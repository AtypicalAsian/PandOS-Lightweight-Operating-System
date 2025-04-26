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
#include "/usr/include/umps3/umps/libumps.h"

HIDDEN int delayDaemon_sema4; /*semaphore to support (...)*/
HIDDEN delay_ptr delaydFree_h; /*Ptr to head of free list of event descriptors*/


/*allocate new node for the ADL*/
void alloc_descriptor(){
    return;
}


/*return a node from the ADL to the free pool (of unsued descriptor nodes)*/
void free_descriptor(){
    return;
}

/*Initialize Active Delay List*/
void initADL(){
    /*Set up initial state*/
    state_t base_state;
    base_state.s_pc = (memaddr) delay_daemonProcess;
    base_state.t9 = (memaddr) delay_daemonProcess;
    base_state.s_sp = 0; /*CHANGE TO STARTING ADDRESS ?*/
    base_state.s_status = ALLOFF | IEPON | IMON | TEBITON;
}

/*insert new descriptor into Active Delay List (ADL)*/
int insertADL(){
    return;

}
void removeADL(){
    return;
}

/*implements delay facility*/
void delay_daemonProcess(support_t *currSuppStruct){
    return;
}