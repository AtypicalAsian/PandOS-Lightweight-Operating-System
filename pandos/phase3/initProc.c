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
support_t *free_support_pool[MAXUPROCS+1]; /*Array of pointers to free support structures. One extra slot is allocated to serve as a sentinel*/
support_t support_structs_pool[MAXUPROCS]; /*Array of support structure objects for user procs*/

/**************************************************************************************************
 * @brief
 *
 * This function sets up the initial state for a user process by initializing:
 *  - The status register
 *  - The program counter (PC) and register t9
 *  - The stack pointer (SP)
 *
 * @param base_state Pointer to a state_t structure that will be initialized with the base state
 * @return None
 **************************************************************************************************/
/*Initialize base processor state for user-process (define a function for this)*/
void init_base_state(state_t *base_state){
    base_state->s_status = IMON | TEBITON | USERPON | IEPON; /*Enable timer, user mode, interrupts*/
    base_state->s_pc = TEXT_START; /*initialize PC*/
    base_state->s_t9 = TEXT_START; /*have to set t9 register after setting s_pc*/
    base_state->s_sp = SP_START; /*stack pointer*/
}

/**************************************************************************************************
 * @brief Return a support structure to the free pool
 *
 * This function return the given support structure to the free_support_pool array. It effectively
 * "deallocates" the support structure so that it can be reused by future processes
 *
 * @param support Pointer to the support_t structure to be deallocated
 * @return None
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
 * @return Pointer to a support_t structure if available, otherwise NULL
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
 * TO-DO
 **************************************************************************************************/
void initSupport() {
    freeSupIndex = 0;
    int i;
    for (i = 0; i < MAXUPROCS; i++){
        deallocate(&support_structs_pool[i]);
    }
}

/**************************************************************************************************
 * TO-DO
 * Helper method to create a process
 **************************************************************************************************/
void summon_process(int process_id, state_t *base_state){
    support_t *suppStruct = allocate();
    if (suppStruct == NULL){
        PANIC();
    }
    state_t base_state_copy = *base_state; /*make a copy of base state to avoid modifying the same struct for multiple processes*/
    base_state_copy.s_entryHI = (process_id << ASIDSHIFT);
        
    suppStruct->sup_asid = process_id;

    /*Set Up General Exception Context*/
    suppStruct->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) &sysSupportGenHandler; 
    suppStruct->sup_exceptContext[GENERALEXCEPT].c_status = IEPON | IMON | TEBITON;
    suppStruct->sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) &(suppStruct->sup_stackGen[STACKSIZE]);

    /*Set Up Page Fault Exception Context*/
    suppStruct->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) &tlb_exception_handler;
    suppStruct->sup_exceptContext[PGFAULTEXCEPT].c_status = IEPON | IMON | TEBITON;
    suppStruct->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) &(suppStruct->sup_stackTLB[STACKSIZE]);
        
    /*Set Up process page table*/
    int k;
    for (k=0; k < 31; k++){
        suppStruct->sup_privatePgTbl[k].entryHI = 0x80000000 + (k << VPNSHIFT) + (process_id << ASIDSHIFT);
        suppStruct->sup_privatePgTbl[k].entryLO = DIRTYON;
    }
        
    /*Entry 31 of page table = stack*/
    suppStruct->sup_privatePgTbl[31].entryHI = 0xBFFFF000 + (process_id << ASIDSHIFT);
    suppStruct->sup_privatePgTbl[31].entryLO = DIRTYON;

    /*Call SYS1*/
    SYSCALL(SYS1,(memaddr) &base_state_copy,(memaddr)suppStruct,0);
}

/**************************************************************************************************
 * TO-DO  
 * Implement test() function 
 *      1. Initialize I/O device semaphores + master semaphore 
 *      2. Initialize Virtual Memory (swap pool table) 
 *      3. Initialize base processor state for all user processes 
 *      4. Create and launch max number of processes 
 *      5. Optimization (perform P op on master sema4 MAXUPROCESS times) 
 *      6. Terminate test() 
 **************************************************************************************************/
void test() {
    /*Declare local variables*/
    int process_id; /*unique process id (asid) associated with each user process that's created (instantiated)*/
    state_t base_state;
    /*support_t *suppStruct;*/

    /*Initialize master semaphore to 0*/
    masterSema4 = 0; /*DEFINE CONSTANT FOR 0*/

    /* Initalise device reg semaphores */
    init_deviceSema4s(); /*initialize device semaphores array*/
    initSwapPool(); /*Initialize swap pool & swap pool semaphore*/
    initSupport(); /*Initialize support structs*/
 
    /*Set up initial proccessor state*/
    init_base_state(&base_state);

    /*create and launch MAXUPROCESS user processes*/
    /*note: asid (process_id) 0 is reserved for kernl daemons, so the (up to 8) u-procs get assigned asid values from 1-8 instead*/

    for (process_id= 1; process_id < MAXUPROCS+1; process_id++) {
        summon_process(process_id,&base_state); /*Helper method to set up asid, exception contexts and page tables for each process*/
    }
    
    /*Wait for all uprocs to finish*/
    int i;
    for (i = 0; i < MAXUPROCS; i++){
        SYSCALL(SYS3, (memaddr) &masterSema4, 0, 0);
    }

    /* Terminate the current process */
    SYSCALL(SYS2, 0, 0, 0);
}
