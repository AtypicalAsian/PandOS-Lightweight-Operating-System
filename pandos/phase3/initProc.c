/**************************************************************************************************  
 * @file initProc.c  
 *  
 * 
 * @brief  
 * This module implements test() and exports the Support Level’s global variables.
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
/*extern int deviceSema4s[MAXSHAREIODEVS];*/ /*array of semaphores, each for a (potentially) shareable peripheral I/O device. These semaphores will be used for mutual exclusion*/
int masterSema4; /* A Support Level semaphore used to ensure that test() terminates gracefully by calling HALT() instead of PANIC() */
int iterator; /*iterator to index into free support stack*/
support_t *free_support_pool[MAXUPROCESS+1];
support_t support_structs_pool[MAXUPROCESS];

/*HELPER METHODS*/
HIDDEN void init_base_state(state_t base_state);
HIDDEN void summon_process(int pid);
HIDDEN support_t* allocate();    /*Helper method to allocate a support struct from the free stack*/
HIDDEN void deallocate(support_t* supportStruct);   /*Helper method to return support structure to the free stack*/

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
 * Helper method to create a process
 **************************************************************************************************/
void summon_process(int pid){
}

/**************************************************************************************************
 * TO-DO
 **************************************************************************************************/
support_t* allocate(){
    if (iterator == 0){
        return NULL;
    }
    iterator--;
    return free_support_pool[iterator];
}

/**************************************************************************************************
 * TO-DO
 * Helper method to return support structure to the free stack
 **************************************************************************************************/
void deallocate(support_t* supportStruct){
    if (iterator < MAXUPROCESS){
        free_support_pool[iterator] = supportStruct;
        iterator++;
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
    /*Declare local variables*/
    int process_id; /*unique process id (asid) associated with each user process that's created (instantiated)*/
    state_t base_proc_state;
    static support_t supp_struct_array[MAXUPROCESS+1]; /*array of support structures*/
    support_t *suppStruct;

    /*Initialize master semaphore to 0*/
    masterSema4 = 0; /*DEFINE CONSTANT FOR 0*/
    iterator = 0;
    
    /*Initialize I/O device semaphores to 1*/
    int i;
    for (i=0; i<MAXSHAREIODEVS; i++){
        deviceSema4s[i] = 1; /*DEFINE CONSTANT FOR 1*/
    }

    /*initialize free pool (stack) of support structs*/
    int j;
    for (j=0; j < MAXUPROCESS; j++){
        deallocate(&support_structs_pool[j]);
    }


    /*Initialize virtual memory*/
    init_base_state(base_proc_state); /*Initialize base processor state of a user process*/
    init_swap_structs(); /*Initialize swap pool & swap pool semaphore*/

    /*create and launch MAXUPROCESS user processes*/
    /*note: asid (process_id) 0 is reserved for kernl daemons, so the (up to 8) u-procs get assigned asid values from 1-8 instead*/

    for (process_id=1; process_id < MAXUPROCESS + 1; process_id++){
        /*create and launch MAXUPROCESS user processes*/
        suppStruct = allocate();
        base_proc_state.s_entryHI = process_id << 6;

        /*Create exception context per process*/
        suppStruct->sup_asid = process_id;

        /*Set Up General Exception Context*/
        suppStruct->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) gen_exception_handler;    /*assign address of general exception handler*/
        suppStruct->sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) &(suppStruct->sup_stackGen[STACKSIZE]); /*Set Up stack area for support level exception handler*/
        suppStruct->sup_exceptContext[GENERALEXCEPT].c_status = STATUS_IE_ENABLE | STATUS_PLT_ON | STATUS_INT_ON;
        /*Set Up Page Fault Exception Context*/
        suppStruct->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) tlb_exception_handler;  /*assign address of tlb exception handler*/
        suppStruct->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) &(suppStruct->sup_stackTLB[STACKSIZE]); /*Set Up stack area for TLB exception handler*/
        suppStruct->sup_exceptContext[PGFAULTEXCEPT].c_status = STATUS_IE_ENABLE | STATUS_PLT_ON | STATUS_INT_ON;

        /*Set Up process page table*/

        /*Call SYS1*/
        SYSCALL(SYS1,0,0,0);

        /*Wait for all uprocs to finish*/

        /*Terminate current uproc*/

        supp_struct_array[process_id].sup_asid = process_id;  /*set up user process' unique identifier ASID*/
        return;
    }

}