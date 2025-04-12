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
void init_base_state(state_t base_state){
    base_state.s_status = USERPON | IEPON | TEBITON | IMON;
    base_state.s_pc = TEXT_START; /*initialize PC*/
    base_state.s_t9 = TEXT_START; /*have to set t9 register after setting s_pc*/
    base_state.s_sp = SP_START; /*stack pointer*/
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


void test() {
    /*Declare local variables*/
    int process_id; /*unique process id (asid) associated with each user process that's created (instantiated)*/
    state_t base_state;
    support_t *suppStruct;

    /* Initalise device reg semaphores */
    initSwapPool();
    initSupport();

    /*Set up initial proccessor state*/
    base_state.s_status = USERPON | IEPON | TEBITON | IMON;
    base_state.s_pc = TEXT_START; /*initialize PC*/
    base_state.s_t9 = TEXT_START; /*have to set t9 register after setting s_pc*/
    base_state.s_sp = SP_START; /*stack pointer*/

    support_t *supportStruct;
    int proc;
    for (proc= 1; proc <= UPROCMAX; proc++) {
        base_state.s_entryHI = (proc << ASIDSHIFT);

        supportStruct = allocate();
        
        /* Set up exception context */
        supportStruct->sup_asid = proc;
        supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) &pager;
        supportStruct->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) &sysSupportGenHandler; 
        
        supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_status = IEPON | IMON | TEBITON;
        supportStruct->sup_exceptContext[GENERALEXCEPT].c_status = IEPON | IMON | TEBITON;

        supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) &(supportStruct->sup_stackTLB[STACKSIZE]);
        supportStruct->sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) &(supportStruct->sup_stackGen[STACKSIZE]);
        
        /* Initalise page table */
        int pgTblSize;
        for(pgTblSize = 0; pgTblSize<USERPGTBLSIZE-1; pgTblSize++) {
            supportStruct->sup_privatePgTbl[pgTblSize].pte_entryHI = VPNBASE + (pgTblSize << VPNSHIFT) + (proc << ASIDSHIFT);
            supportStruct->sup_privatePgTbl[pgTblSize].pte_entryLO = DIRTYON;
        }
        
        /* Set last entry in page table to the stack*/
        supportStruct->sup_privatePgTbl[USERPGTBLSIZE-1].pte_entryHI = UPROCSTACKPG + (proc << ASIDSHIFT);
        supportStruct->sup_privatePgTbl[USERPGTBLSIZE-1].pte_entryLO = DIRTYON;

         /* Create the process*/
        SYSCALL(CREATEPROCESS, (memaddr) &base_state, (memaddr)supportStruct, 0);
    }
    
    int i;
    for (i = 0; i < UPROCMAX; i++){
        SYSCALL(PASSEREN, (memaddr) &masterSema4, 0, 0);
    }

    /* Terminate the current process */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0);
}
