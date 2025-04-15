/**************************************************************************************************  
 * @file initProc.c  
 *  
 * 
 * @brief  
 * This module implements test() (instantiator process) and exports the Support Level's global variables.
 * 
 * @details  
 * The module is responsible for setting up the initial processor state for user processes,
 * initializing the I/O device semaphores, support structure free pool, and virtual memory
 * (swap pool). It also creates and launches (up to) 8 user processes by setting up their ASIDs,
 * exception contexts, page tables and calling phase 2's SYS1. Finally, it synchronizes the 
 * termination of user processes using a master semaphore. 
 * 
 * @ref
 * pandOS - section 4.9 & 4.10
 *  
 * @authors  
 * Nicolas & Tran  
 * View version history and changes: https://github.com/AtypicalAsian/CS372-OS-Project
 **************************************************************************************************/
#include "../h/types.h"
#include "../h/const.h"
#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/* GLOBAL VARIABLES & DATA STRUCTURES */
int deviceSema4s[DEVICE_TYPES * DEVPERINT]; /*array of semaphores, each for a (potentially) shareable peripheral I/O device. These semaphores will be used for mutual exclusion*/
int masterSema4; /* A Support Level semaphore used to ensure that test() terminates gracefully */
int freeSupIndex; /*Iterator to index into the free support pool stack*/
support_t *free_support_pool[MAX_FREE_POOL]; /*Array of pointers to free support structures. One extra slot is allocated to serve as a sentinel*/
support_t support_structs_pool[MAXUPROCS]; /*Array of support structure objects for user procs*/



/**************************************************************************************************
 * @brief Return a support structure to the free pool
 *
 * This function return the given support structure to the free_support_pool array. It effectively
 * "deallocates" the support structure so that it can be reused by future processes
 *
 * @param: Pointer to the support_t structure to be deallocated
 * @return: None
 * 
 * @ref
 * pandOS - section 4.10
 **************************************************************************************************/
void deallocate(support_t *supStruct){
    if (freeSupIndex < MAX_SUPPORTS){ /*Verify that we're not overflowing the pool*/
        free_support_pool[freeSupIndex++] = supStruct; /*Return the support struct to the free pool*/
    }
}

/**************************************************************************************************
 * @brief Retrieve a support structure from the free pool.
 *
 * This function returns a pointer to a support structure from the free_support_pool if one is
 * available. If the pool is empty (freeSupIndex is 0), it returns NULL.
 *
 * @param: None
 * @return: Pointer to a support_t structure if available, otherwise NULL
 * 
 * @ref
 * pandOS - section 4.10
 **************************************************************************************************/
support_t* allocate() {    
    if (freeSupIndex != 0){ /*If we still have available support structs from the free pool*/
        support_t *newSupStruct = NULL;
        freeSupIndex--;
        newSupStruct = free_support_pool[freeSupIndex];
        return newSupStruct;
    }
    else{
        return NULL; /*no more support structs available*/
    }
}

/**************************************************************************************************
 * @brief Initializes the support structure pool
 *
 * @details This function resets the free index (freeSupIndex) to 0 and then iterates over 
 *          the pool of support structures. Each support structure is deallocated (pushed
 *          onto the free support pool) using the deallocate() helper function.
 * 
 * @param: None
 * @return: None
 **************************************************************************************************/
void initSuppPool() {
    freeSupIndex = 0;
    int i;
    for (i = 0; i < MAXUPROCS; i++){
        deallocate(&support_structs_pool[i]);
    }
}

/**************************************************************************************************
 * @brief
 *
 * This function sets up the initial state for a user process by initializing:
 *  - The status register
 *  - The program counter (PC) and register t9
 *  - The stack pointer (SP)
 *
 * @param: Pointer to a state_t structure that will be initialized with the base state
 * @return None
 * 
 * @ref
 * pandOS - section 4.9.1
 **************************************************************************************************/
/*Initialize base processor state for user-process (define a function for this)*/
void init_base_state(state_t *base_state){
    base_state->s_status = IMON | TEBITON | USERPON | IEPON; /*Enable timer, user mode, interrupts*/
    base_state->s_pc = TEXT_START; /*initialize PC*/
    base_state->s_t9 = TEXT_START; /*have to set t9 register after setting s_pc*/
    base_state->s_sp = SP_START; /*stack pointer*/
}

/**************************************************************************************************
 * @brief Initialize a u-proc's support structure and process
 *        state, and then calling SYS1 to create and launch the process.
 *
 * @details
 * 1. Allocates a support structure from the free pool
 * 2. Sets up the unique ASID for the new process
 * 3. Configures two exception contexts in the support structure:
 *       - The general exception context uses sysSupportGenHandler
 *       - The page fault exception context uses the TLB exception handler
 * 4. Initializes the process's private page table:
 *       - The first 31 entries are for program pages
 *       - The last entry (index 31) is reserved for the process stack
 * 5. Finally, invokes SYS1 to create and launch the process
 * 
 * @param: 1.process_id - unique id assigned to each uproc
 *         2.base_state - pointer to state_t struct representing the base processor 
 *                        state for the process
 * @return: None
 * 
 * @ref
 * pandOS - section 4.9.1
 **************************************************************************************************/
void summon_process(int process_id, state_t *base_state){
    support_t *suppStruct = allocate(); /*allocate a support struct from the free pool*/
    if (suppStruct == NULL){
        PANIC();
    }
    state_t base_state_copy = *base_state; /*make a copy of base state to avoid modifying the same struct for multiple processes*/
    base_state_copy.s_entryHI = (process_id << SHIFT_ASID); /*Set unique ASID in state*/
        
    suppStruct->sup_asid = process_id; /*Set unique ASID in support structure*/

    /*Set Up General Exception Context*/
    suppStruct->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) &sysSupportGenHandler; /*set to address of support level's gen exception handler*/
    suppStruct->sup_exceptContext[GENERALEXCEPT].c_status = IEPON | IMON | TEBITON; /*kernel mode, interrupts & PLT enabled*/
    suppStruct->sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) &(suppStruct->sup_stackGen[STACKSIZE]); /*set sp to stack space allocated in support struct*/

    /*Set Up Page Fault Exception Context*/
    suppStruct->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) &tlb_exception_handler; /*set to address of support level's TLB exception handler*/
    suppStruct->sup_exceptContext[PGFAULTEXCEPT].c_status = IEPON | IMON | TEBITON; /*kernel mode, interrupts & PLT enabled*/
    suppStruct->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) &(suppStruct->sup_stackTLB[STACKSIZE]); /*set sp to stack space allocated in support struct*/
        
    /*Initialize the private page table: pages 0-30 for data, entry 31 for stack*/
    int k;
    for (k=0; k < PAGE_TABLE_MAX; k++){
        suppStruct->sup_privatePgTbl[k].entryHI = PT_START + (k << SHIFT_VPN) + (process_id << SHIFT_ASID); /*pandos - 4.2.1*/
        suppStruct->sup_privatePgTbl[k].entryLO = D_BIT_SET; /*mark the page as dirty by default - each page will be write-enabled*/
    }
        
    /*Entry 31 of page table = stack*/
    suppStruct->sup_privatePgTbl[PAGE_TABLE_MAX].entryHI = PAGE31_ADDR + (process_id << SHIFT_ASID); /*pandos - 4.2.1*/
    suppStruct->sup_privatePgTbl[PAGE_TABLE_MAX].entryLO = D_BIT_SET; /*mark D bit on*/

    /*Call SYS1 to create and launch the u-proc*/
    SYSCALL(SYS1,(memaddr) &base_state_copy,(memaddr)suppStruct,0);
}

/************************************************************************************************** 
 * @details
 * The Instantiator Process (named “test”) is created in phase2.This process is responsible for:
 *   1. Initializing Level 4/Phase 3 data structures:
 *      - Setting up the Swap Pool table and initializing the Swap Pool semaphore.
 *      - Initializing mutual exclusion semaphores for all sharable peripheral I/O devices.
 *        For terminal devices, two semaphores are set up – one for reading and one for writing.
 * 
 *   2. Creating and launching between 1 and 8 user processes
 * 
 *   3. Waiting for all U-proc child processes to finish:
 *      - This is done by performing the P (SYS3) operation on a master semaphore
 * 
 *   4. Terminating itself via SYS2
 * 
 * @param: None
 * @return: None
 * 
 * @ref
 * pandos - section 4.9 & 4.10
 **************************************************************************************************/
void test() {
    /*Declare local variables*/
    int process_id; /*unique process id (asid) associated with each user process that's created*/
    state_t base_state; /*Base processor state for user processes*/

    /*Initialize master semaphore to 0*/
    masterSema4 = MASTER_SEMA4_START;

    /* Initalise device reg semaphores */
    initSwapStructs(); /*Initialize swap pool table, swap pool semaphore and associated device semaphores - vmSupport.c function*/
    initSuppPool(); /*Initialize support structs free pool*/
 
    /*Set up initial proccessor state*/
    init_base_state(&base_state);

    /*create and launch 8 user processes*/
    /*note: asid (process_id) 0 is reserved for kernl daemons, so the (up to 8) u-procs get assigned asid values from 1-8 instead*/

    for (process_id= 1; process_id < MAXUPROCS+1; process_id++) {
        summon_process(process_id,&base_state);
    }
    
    /*Perform P operation MAXUPROCS times on the master semaphore*/
    int i;
    for (i = 0; i < MAXUPROCS; i++){
        SYSCALL(SYS3, (memaddr) &masterSema4, 0, 0);
    }

    /* Terminate the instantiator process */
    SYSCALL(SYS2, 0, 0, 0);
}
