/**************************************************************************************************  
 * @file delayDaemon.c  
 * 
 * This module implements a kernel-level delay mechanism using a singly linked list of event descriptors.
 * Key components include:
 *   - A statically allocated pool of delay descriptor nodes managed via a free list.
 *   - An Active Delay List (ADL) using dummy head and tail nodes to maintain sorted order by wakeTime.
 *   - A delay daemon process that wakes up delayed user processes after the appropriate time interval.
 *   - Support for SYS18 (DELAY) system call, which blocks user processes for a specified duration.
 * 
 * @ref
 * pandos
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

int delayDaemon_sema4; /*semaphore to provided mutual exclusion over the ADL*/
delayd_PTR delaydFree_h; /*Ptr to head of free list of event descriptors*/
delayd_PTR delayd_h; /*dummy head ptr*/
delayd_PTR delayd_tail; /*dummy tail ptr*/


/**************************************************************************************************  
 * Allocate new node for the ADL from free list
 * 
 * @ret:
 *     - pointer to preceding event descriptor
 **************************************************************************************************/
delayd_PTR alloc_descriptor(){ /*similar logic to ASL*/
    delayd_PTR newDescriptor; /*pointer to new descriptor to be allocated from free list*/
    if (delaydFree_h != NULL){
        newDescriptor = delaydFree_h; /*set to current head*/
        delaydFree_h = delaydFree_h->d_next; /*move head to next node*/

        newDescriptor->d_next = NULL;
        newDescriptor->d_supStruct = NULL; /*do we have to init this to NULL?*/
        newDescriptor->d_wakeTime = 0;
        return newDescriptor;
    }
    return NULL;
}

/**************************************************************************************************  
 * Remove a node from the ADL and return it to the free pool
 **************************************************************************************************/
void removeADL(delayd_PTR delayDescriptor){ /*similar logic to ASL*/
    delayDescriptor->d_next = delaydFree_h;
    delaydFree_h = delayDescriptor;
}


/**************************************************************************************************  
 * Initialize Active Delay List (ADL)
 * Steps:
 * 1. Add each element from the static array of delay event descriptor nodes to the free list and 
 *    initialize the active list (zero, one or two dummy nodes)
 * 2. Initialize and launch (SYS1) the Delay Daemon.
 * 3. Set the Support Structure SYS1 parameter to be NULL
 **************************************************************************************************/
void initADL(){
    static delayd_t delayDescriptors[MAXUPROCS + 2]; /*+2 for dummy head and tail*/
    delayDaemon_sema4 = 1;

    delaydFree_h = &delayDescriptors[2];
    int i;
    for (i = 3; i < MAXUPROCS + 2; i++){
        delayDescriptors[i - 1].d_next = &delayDescriptors[i];
    }
    delayDescriptors[MAXUPROCS + 1].d_next = NULL;

    delayd_h = &delayDescriptors[0];
    delayd_tail = &delayDescriptors[1];
    delayd_h->d_next = delayd_tail;
    delayd_tail->d_next = NULL;

    delayd_h->d_supStruct = NULL;
    delayd_tail->d_supStruct = NULL;

    delayd_h->d_wakeTime = 0;
    delayd_tail->d_wakeTime = 0xFFFFFFFF;

    memaddr topRAM = *((int *)RAMBASEADDR) + *((int *)RAMBASESIZE);
    state_t base_state;
    base_state.s_entryHI = (0 << SHIFT_ASID);
    base_state.s_pc = (memaddr) delayDaemon;
    base_state.s_t9 = (memaddr) delayDaemon;
    base_state.s_sp = topRAM;
    base_state.s_status = ALLOFF | IEPON | IMON | TEBITON;

    int status = SYSCALL(SYS1, (int)&base_state, (int)NULL, 0);
    if (status != 0) get_nuked(NULL);
}


/**************************************************************************************************  
 * Helper method that searches the ADL for the event descriptor that directly precedes 
 * where the new descriptor should belong (ADL sorted by wakeTime)
 * 
 * @ret:
 *     - pointer to preceding event descriptor
 **************************************************************************************************/
delayd_PTR find_insert_position(int wakeTime){
    delayd_PTR prev = delayd_h;
    delayd_PTR curr = delayd_h->d_next;

    while (curr != delayd_tail && curr->d_wakeTime < wakeTime){
        prev = curr;
        curr = curr->d_next;
    }
    return prev;
}

/**************************************************************************************************  
 * Insert new descriptor into Active Delay List (ADL)
 * 
 **************************************************************************************************/
int insertADL(int time_asleep, support_t *supStruct){
    int currTime;
    delayd_PTR newDescriptor;

    newDescriptor = alloc_descriptor();
    if (newDescriptor == NULL){
        return FALSE;
    }
    STCK(currTime);
    newDescriptor->d_wakeTime = currTime + (time_asleep * 1000000);
    newDescriptor->d_supStruct = supStruct;

    delayd_PTR prev = find_insert_position(newDescriptor->d_wakeTime);
    newDescriptor->d_next = prev->d_next;
    prev->d_next = newDescriptor;

    return TRUE;
}

/**************************************************************************************************  
 * Implements delay facility (delay daemon process)
 * 
 **************************************************************************************************/
void delayDaemon(){
    cpu_t curr_time;

    while (TRUE){
        SYSCALL(SYS7,0,0,0);
        SYSCALL(SYS3,(int) &delayDaemon_sema4,0,0);
        STCK(curr_time);

        delayd_PTR curr = delayd_h->d_next;
        while (curr != delayd_tail && curr->d_wakeTime <= curr_time){
            SYSCALL(SYS4,(int)&curr->d_supStruct->privateSema4,0,0);
            delayd_h->d_next = curr->d_next;
            removeADL(curr);
            curr = delayd_h->d_next;
        }

        SYSCALL(SYS4,(int)&delayDaemon_sema4,0,0);
    }
}

/**************************************************************************************************  
 * Code for implementing syscall 18 - DELAY
 * Steps:
 * 1. Check the seconds parameter and terminate (SYS9) the U-proc if the wait time is negative
 * 2. Obtain mutual exclusion over the ADL - SYS3/P on the ADL semaphore
 * 3. Allocate a delay event descriptor node from the free list, populate it and insert it into 
 *    its proper location on the active list. If this operation is unsuccessful, terminate (SYS9) 
 *    the U-proc – after releasing mutual exclusion over the ADL.
 * 4. Release mutual exclusion over the ADL - SYS4/V on the ADL semaphore AND execute a SYS3/P 
 *    on the U-proc’s private semaphore atomically. This will block the executing U-proc.
 * 5. Return control (LDST) to the U-proc at the instruction immediately following the SYS18. 
 *    This step will not be executed until after the U-proc is awoken
 * 
 * @ref
 * 
 **************************************************************************************************/
void sys18Handler(int sleepTime, support_t *support_struct){
    if (sleepTime == 0) return;
    else if (sleepTime < 0){
        get_nuked(NULL);
    }
    else{
        SYSCALL(SYS3,(int) &delayDaemon_sema4,0,0);
        if (insertADL(sleepTime,support_struct) == FALSE){
            get_nuked(NULL);
        }

        setSTATUS(NO_INTS);
        SYSCALL(SYS4,(int) &delayDaemon_sema4,0,0);
        SYSCALL(SYS3,(int)&support_struct->privateSema4,0,0); 
        setSTATUS(YES_INTS);
    }
}

