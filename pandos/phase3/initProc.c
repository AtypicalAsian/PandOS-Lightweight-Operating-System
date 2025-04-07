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
/*#include "/usr/include/umps3/umps/libumps.h"*/

/* GLOBAL VARIABLES DECLARATION */
int masterSema4;/* A Support Level semaphore used to ensure that test() terminates gracefully by calling HALT() instead of PANIC() */
support_t support_structs_pool[MAXUPROCESS];
support_t *free_support_pool[MAXUPROCESS+1];
int stk_iterator; /*iterator to index into free support stack*/
int devRegSem[48];

void init_devRegSem(){
    int i;
    for (i=0;i<49;i++){
        devRegSem[i] = 1;
    }
}


/**************************************************************************************************
 * TO-DO
 * Initialize Base (initial) processor state for a user process
 **************************************************************************************************/
/*Initialize base processor state for user-process (define a function for this)*/
void init_base_state(state_t base_state){
    base_state.s_status = STATUS_USERPON | STATUS_IE_ENABLE | STATUS_PLT_ON | STATUS_INT_ON;
    base_state.s_pc = TEXT_START; /*initialize PC*/
    base_state.s_t9 = TEXT_START; /*have to set t9 register after setting s_pc*/
    base_state.s_sp = SP_START; /*stack pointer*/
}

/**************************************************************************************************
 * TO-DO
 **************************************************************************************************/
support_t* allocate(){
    support_t *temp = NULL;
    if (stk_iterator != 0){
        stk_iterator--;
        temp = free_support_pool[stk_iterator];
    }
    return temp;
}

/**************************************************************************************************
 * TO-DO
 * Helper method to return support structure to the free stack
 **************************************************************************************************/
void deallocate(support_t* supportStruct){
    free_support_pool[stk_iterator] = supportStruct;
    stk_iterator++;
}

/*Initialize the support structure free list (stack)*/
void initSupportStruct(){
    stk_iterator = 0;
    int i;
    for (i=0;i<MAXUPROCESS;i++){
        deallocate(&support_structs_pool[i]);
    }
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
void test(){
    init_swap_structs();
    initSupportStruct();

    state_t base_proc_state;
    init_base_state(base_proc_state);

    support_t *supportStruct;
    int pid;
    for (pid=1;pid<=MAXUPROCESS;pid++){
        base_proc_state.s_entryHI = (pid << SHIFT_ASID);
        supportStruct = allocate();

        /*Init exception context*/
        supportStruct->sup_asid = pid;

        /*Set Up General Exception Context*/
        supportStruct->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) sp_level_gen_handler;    /*assign address of general exception handler*/
        supportStruct->sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) &(supportStruct->sup_stackGen[STACKSIZE]); /*Set Up stack area for support level exception handler*/
        supportStruct->sup_exceptContext[GENERALEXCEPT].c_status = STATUS_IE_ENABLE | STATUS_PLT_ON | STATUS_INT_ON;
        /*Set Up Page Fault Exception Context*/
        supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) tlb_exception_handler;  /*assign address of tlb exception handler*/
        supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) &(supportStruct->sup_stackTLB[STACKSIZE]); /*Set Up stack area for TLB exception handler*/
        supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_status = STATUS_IE_ENABLE | STATUS_PLT_ON | STATUS_INT_ON;

        /*Set Up uproc page table*/
        /*Set Up process page table*/
        int k;
        for (k=0; k < MAX_PAGES-1; k++){
            supportStruct->sup_privatePgTbl[k].entryHI = 0x80000000 + (k << VPNSHIFT) + (pid << ID_SHIFT);
            supportStruct->sup_privatePgTbl[k].entryLO = D_BIT_SET;
        }

        /*Entry 31 of page table = stack*/
        supportStruct->sup_privatePgTbl[31].entryHI = 0xBFFFF000 + (pid << ID_SHIFT);
        supportStruct->sup_privatePgTbl[31].entryLO = D_BIT_SET;

        /*Call SYS1*/
        SYSCALL(SYS1,(memaddr) &base_proc_state,(memaddr)supportStruct,0);
    }
    /*Wait for all uprocs to finish*/
    int l;
    for (l=0;l<MAXUPROCESS;l++){
        SYSCALL(SYS3,(memaddr) &masterSema4,0,0);
    }
    /*Terminate current uproc*/
    SYSCALL(SYS2,0,0,0);
}