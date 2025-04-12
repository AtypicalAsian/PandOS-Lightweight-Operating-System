/**************************************************************************************************  
 * @file initProc.c  
 *  
 * 
 * @brief  
 * This module implements test() and exports the Support Level's global variables.
 * 
 * @details  
 * 
 *  
 * @note  
 * 
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

/* GLOBAL VARIABLES DECLARATION */
int deviceSema4s[DEVICE_TYPES * DEVPERINT]; /*array of semaphores, each for a (potentially) shareable peripheral I/O device. These semaphores will be used for mutual exclusion*/
int masterSema4; /* A Support Level semaphore used to ensure that test() terminates gracefully by calling HALT() instead of PANIC() */
int freeSupIndex; /*iterator to index into free support stack*/
support_t *free_support_pool[UPROCMAX+1];
support_t support_structs_pool[UPROCMAX];

/**************************************************************************************************
 * TO-DO
 * Initialize Base (initial) processor state for a user process
 **************************************************************************************************/
/*Initialize base processor state for user-process (define a function for this)*/
void init_base_state(state_t *base_state){
    base_state->s_status = USERPON | IEPON | TEBITON | IMON;
    base_state->s_pc = TEXT_START; /*initialize PC*/
    base_state->s_t9 = TEXT_START; /*have to set t9 register after setting s_pc*/
    base_state->s_sp = SP_START; /*stack pointer*/
}

/**************************************************************************************************
 * TO-DO
 **************************************************************************************************/
void deallocate(support_t *support){
    free_support_pool[freeSupIndex] = support;
    freeSupIndex++;
}

/**************************************************************************************************
 * TO-DO
 **************************************************************************************************/
support_t* allocate() {
    support_t *tempSupport = NULL;
    
    if (freeSupIndex != 0){
        freeSupIndex--;
        tempSupport = free_support_pool[freeSupIndex];
    }
    return tempSupport;
}

/**************************************************************************************************
 * TO-DO
 **************************************************************************************************/
void initSupport() {
    freeSupIndex = 0;
    int i;
    for (i = 0; i < UPROCMAX; i++){
        deallocate(&support_structs_pool[i]);
    }
}

/**************************************************************************************************
 * TO-DO
 **************************************************************************************************/
void init_deviceSema4s(){
    int i, j;
    for (i = 0; i < DEVICE_TYPES; i++) {
        for (j = 0; j < DEVICE_INSTANCES; j++) {
            support_device_sems[i][j] = 1;
        }
    }
    swapSemaphore = 1;
}




/**************************************************************************************************
 * TO-DO  
 * Implement test() function 
 *      1. Initialize I/O device semaphores + master semaphore (done)
 *      2. Initialize Virtual Memory (swap pool table) (working on it!!!)
 *      3. Initialize base processor state for all user processes (working on it!!!)
 *      4. Create and launch max number of processes (working on it!!!)
 *      5. Optimization (perform P op on master sema4 MAXUPROCESS times) (working on it!!!)
 *      6. Terminate test() (working on it!!!)
 **************************************************************************************************/
void test() {
    /*Declare local variables*/
    int process_id; /*unique process id (asid) associated with each user process that's created (instantiated)*/
    state_t base_state;
    support_t *suppStruct;

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

    for (process_id= 1; process_id < UPROCMAX+1; process_id++) {
        suppStruct = allocate(); /*allocate a support structure from the free pool*/
        base_state.s_entryHI = (process_id << ASIDSHIFT);
        
        /*Create exception context per process*/
        suppStruct->sup_asid = process_id;

        /*Set Up General Exception Context*/
        suppStruct->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) &sysSupportGenHandler; 
        suppStruct->sup_exceptContext[GENERALEXCEPT].c_status = IEPON | IMON | TEBITON;
        suppStruct->sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) &(suppStruct->sup_stackGen[STACKSIZE]);

        /*Set Up Page Fault Exception Context*/
        suppStruct->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) &pager;
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
        SYSCALL(SYS1,(memaddr) &base_state,(memaddr)suppStruct,0);
    }
    
    /*Wait for all uprocs to finish*/
    int i;
    for (i = 0; i < UPROCMAX; i++){
        SYSCALL(SYS3, (memaddr) &masterSema4, 0, 0);
    }

    /* Terminate the current process */
    SYSCALL(SYS2, 0, 0, 0);
}
