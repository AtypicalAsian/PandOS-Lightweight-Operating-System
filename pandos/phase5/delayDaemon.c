/**************************************************************************************************  
 * @file delayDaemon.c  
 * 
 * 
 * @ref
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
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "../h/deviceSupportDMA.h"
#include "../h/delayDaemon.h"
// #include "/usr/include/umps3/umps/libumps.h"

HIDDEN int delayDaemon_sema4; /*semaphore to provided mutual exclusion over the ADL*/
HIDDEN delayd_PTR delaydFree_h; /*Ptr to head of free list of event descriptors*/


/*allocate new node for the ADL*/
void alloc_descriptor(){
    return;
}


/*return a node from the ADL to the free pool (of unsued descriptor nodes)*/
void remove_descriptor(){
    return;
}

/*Initialize Active Delay List*/
void initADL(){

    static delayd_t delayDescriptors[MAXUPROCS+1]; /*add one more to use as dummy node for ADL*/

    /*Set up initial state for delay daemon*/
    state_t base_state;
    base_state.s_entryHI = (0 << SHIFT_ASID); /*set asid for delay daemon process (0)*/
    base_state.s_pc = (memaddr) delayDaemon;
    base_state.s_t9 = (memaddr) delayDaemon;
    base_state.s_sp = 0; /*CHANGE TO STARTING ADDRESS ?*/
    base_state.s_status = ALLOFF | IEPON | IMON | TEBITON; /*kernel mode + interrupts enabled*/

    int status;
    status = SYSCALL(SYS1,(int) &base_state,NULL,0);

    if (status != 0){
        get_nuked(NULL);
    }

}

/*insert new descriptor into Active Delay List (ADL)*/
int insertADL(){
    return;

}

int removeADL(){
    return;
}

/*implements delay facility (delay daemon process)*/
void delayDaemon(support_t *currSuppStruct){
    cpu_t curr_time; /*store current time on TOD clock*/

    while (TRUE){ /*infite loop*/
        SYSCALL(SYS7,0,0,0); /*wait for 100ms to pass*/
        SYSCALL(SYS3,(int) &delayDaemon_sema4,0,0); /*obtain mutual exclusion over the ADL*/
        STCK(curr_time); /*get current time when we finally wake up from Wait Clock syscall*/
        while (delaydFree_h->d_wakeTime <= curr_time){
            SYSCALL(SYS4,(int)&delaydFree_h->d_supStruct->privateSema4,0,0); /*Perform SYS4 on uproc private semaphore*/
            remove_descriptor();/*Deallocate delay event descriptor node and return it to the free list*/
        }
        /*Release mutual exclusion over the ADL*/
        SYSCALL(SYS4,(int)&delayDaemon_sema4,0,0);
    }
}

/*code for implementing syscall 18 - DELAY*/
sys18Handler(int sleepTime, support_t *support_struct){
    return;
}