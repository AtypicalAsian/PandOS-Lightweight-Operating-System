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

int delaySemaphore; 

delayd_PTR delaydFree; 
delayd_PTR delaydFree_h; 
#define RAMTOP(T) ((T) = ((*((int *)RAMBASEADDR)) + (*((int *)RAMBASESIZE))));

void initADL () 
{
    static delayd_t delayEvents[MAXPROC +1];
    state_t initial_state;
    memaddr currentRAMTOP;
    int reset_code;

    delaySemaphore = 1;

    RAMTOP(currentRAMTOP);

    
    initial_state.s_sp = currentRAMTOP;
    initial_state.s_pc = (memaddr) delayDaemon;                      
    initial_state.s_t9 = (memaddr) delayDaemon;                     

    initial_state.s_status = ALLOFF | IEPON | IMON | TEBITON;
    initial_state.s_entryHI = (0 << SHIFT_ASID);             

    reset_code = SYSCALL(CREATEPROCESS, (int) &initial_state, NULL, 0);

    if (reset_code != OK) {
        SYSCALL(TERMINATEPROCESS, 0, 0, 0);
    }

    delaydFree = &delayEvents[0];

    int i;
    for (i = 1; i < MAXPROC; i++) {
        delayEvents[i-1].d_next = &delayEvents[i];
    }

    delayEvents[MAXPROC - 1].d_next = NULL;
    delaydFree_h = &delayEvents[MAXPROC];         
    delaydFree_h->d_next = NULL;
    delaydFree_h->d_supStruct = NULL;
    delaydFree_h->d_wakeTime = 0xFFFFFFFF;      

}


void delayCurrentProc(support_t *current_support) {
    int sleepTime;

    sleepTime = current_support->sup_exceptState[GENERALEXCEPT].s_a2;

    if (sleepTime > 0) {
        SYSCALL(PASSEREN, (int) &delaySemaphore, 0, 0);

        if(insertDelayNode(current_support, sleepTime) == FALSE) {
            get_nuked(NULL);
        }

        setSTATUS(NO_INTS);

        SYSCALL(VERHOGEN, (int) &delaySemaphore, 0, 0);

        SYSCALL(PASSEREN, (int) &current_support->privateSema4, 0, 0);

        setSTATUS(YES_INTS);
    }
    else if (sleepTime < 0) {
        get_nuked(NULL);
    }
}


void delayDaemon () {
    cpu_t currentTime;

    while (TRUE) {
        SYSCALL(CLOCKWAIT, 0, 0, 0);

        SYSCALL(PASSEREN, (int) &delaySemaphore, 0, 0);

        STCK(currentTime);

        while (delaydFree_h->d_wakeTime <= currentTime) {
            SYSCALL(VERHOGEN, (int) &delaydFree_h->d_supStruct->privateSema4, 0, 0);
            freeNode(delaydFree_h);
        }

        SYSCALL(VERHOGEN, (int) &delaySemaphore, 0, 0);

    }
}


int insertDelayNode(support_t *current_support, int sleepTime) {
    delayd_PTR tempNode;
    delayd_PTR newNode;
    cpu_t currentTime;


    newNode = activateASL();

    if (newNode == NULL) {
        return FALSE;
    }


    STCK(currentTime);

    newNode->d_supStruct = current_support;
    newNode->d_wakeTime = (SECOND * sleepTime) + currentTime;
    if(newNode->d_wakeTime < delaydFree_h->d_wakeTime) {
        newNode->d_next = delaydFree_h;
        delaydFree_h = newNode;
    }
    else {
        tempNode = delaydFree_h;

        while (tempNode->d_next->d_wakeTime < newNode->d_wakeTime) {
            tempNode = tempNode->d_next;
        }

        newNode->d_next = tempNode->d_next;
        tempNode->d_next = newNode;
    }

    return(TRUE);

}


delayd_PTR activateASL () {
    delayd_PTR tempNode;

    if (delaydFree == NULL) {
        return NULL;
    }
    else {
        tempNode = delaydFree;
        delaydFree = delaydFree->d_next;

        tempNode->d_next = NULL;
        tempNode->d_wakeTime = 0;
        tempNode->d_supStruct = NULL;

        return tempNode;
    }
}

void freeNode(delayd_PTR delayEvent) {
    delayEvent->d_next = delaydFree;
    delaydFree = delayEvent;
}