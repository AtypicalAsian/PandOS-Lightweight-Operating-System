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
/*int deviceSema4s[MAXSHAREIODEVS];*/ /*array of semaphores, each for a (potentially) shareable peripheral I/O device. These semaphores will be used for mutual exclusion*/
/*int masterSema4;*/ /* A Support Level semaphore used to ensure that test() terminates gracefully by calling HALT() instead of PANIC() */
/*int iterator;*/ /*iterator to index into free support stack*/
/*support_t *free_support_pool[MAXUPROCESS+1];*/
/*support_t support_structs_pool[MAXUPROCESS];*/

static support_t supportStruct_pool[MAXUPROCESS];
int masterSema4;
support_t* supp_struct_free;
int devRegSem[MAXSHAREIODEVS];


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
/*void summon_process(int pid){
    return;
}*/

/**************************************************************************************************
 * TO-DO
 **************************************************************************************************/
support_t* allocate(){
    support_t* temp;
    temp = supp_struct_free;
    if (temp == NULL){
        return NULL;
    }
    else{
        supp_struct_free = supp_struct_free->next;
        temp->next = NULL;
        return temp;
    }
}

/**************************************************************************************************
 * TO-DO
 * Helper method to return support structure to the free stack
 **************************************************************************************************/
void deallocate(support_t* supportStruct){
    support_t *temp;
    temp = supp_struct_free;
    if (temp == NULL){
        supp_struct_free = supportStruct;
        supp_struct_free->next = NULL;
    }
    else{
        while (temp->next != NULL){ temp = temp->next;}
        temp->next = supportStruct;
        temp = temp->next;
        temp->next = NULL;
    }
}

void init_supp_struct_Sema4(){
    int i;
    for (i=0; i<MAXDEVICECNT;i++){
        devRegSem[i] = 1;
    }
}

void summonProc(int pid){
    memaddr ramTOP;
    RAMTOP(ramTOP);
    memaddr topStack = ramTOP - (2*pid*PAGESIZE);

    /*Init process state*/
    state_t newState;
    init_base_state(newState);
    newState.s_entryHI = pid << 6;

    /*go to free list -> alloc free support struct*/
    support_t* supportStruct = allocate();
    if (supportStruct != NULL){
        supportStruct->sup_asid = pid;

        /*General Exception Init*/
        supportStruct->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) gen_excp_handler;
        supportStruct->sup_exceptContext[GENERALEXCEPT].c_status= STATUS_IE_ENABLE | STATUS_PLT_ON | STATUS_INT_ON;
        supportStruct->sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) topStack;


        /*Page Exception Init*/
        supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) tlb_exception_handler;
        supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_status= STATUS_IE_ENABLE | STATUS_PLT_ON | STATUS_INT_ON;
        supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) (topStack + PAGESIZE);


        /*TLB ENTRY INIT*/
        int i;
        for (i=0;i<MAX_PAGES;i++){
            supportStruct->sup_privatePgTbl[i].entryHI = 0x80000000 + (i << VPNSHIFT) + (pid << 6);
            supportStruct->sup_privatePgTbl[i].entryLO = D_BIT_SET;
        }
        supportStruct->sup_privatePgTbl[MAX_PAGES-1].entryHI = 0xBFFFF000 + (pid << 6);
        supportStruct->sup_privatePgTbl[MAX_PAGES-1].entryHI = D_BIT_SET;

        /*Run SYS1 to create process after init support struct*/
        int status = SYSCALL(SYS1,(memaddr) &newState, (memaddr) supportStruct, 0);
        if (status != 0){
            SYSCALL(SYS2,0,0,0);
        }
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
    init_supp_struct_Sema4();
    masterSema4 = 0;
    supp_struct_free = NULL;

    int i;
    for (i=0;i<MAXUPROCESS;i++){
        deallocate(&supportStruct_pool[i]);
    }

    int id;
    for (id=0;id<MAXUPROCESS;id++){
        summonProc(id+1);
    }
    
    int j;
    for (j=0;j<MAXUPROCESS;j++){
        SYSCALL(SYS3, (int) &masterSema4, 0,0);
    }
    SYSCALL(SYS2,0,0,0);
}