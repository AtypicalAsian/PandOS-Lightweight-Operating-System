/**************************************************************************************************  
 * @file delayDaemon.c  
 * 
 * This module implements a kernel-level delay facility.
 * It allows user-level processes (U-procs) to suspend their execution for a specified
 * number of seconds via the SYS18 system call. The core components of this module include:
 * 
 *      - A statically allocated pool of delay descriptor nodes, maintained through a free list.
 *      - An Active Delay List (ADL), implemented as a singly linked list with dummy head and tail 
 *        nodes, sorted in ascending order by wakeTime for efficient insertion and traversal.
 *      - A delay daemon process that periodically (every 100ms) checks the ADL, wakes up 
 *        processes whose delay period has expired by performing SYS4 on their private semaphores, and 
 *        returns the corresponding descriptors to the free list.
 *      - Support for SYS18, which inserts sleeping U-procs into the ADL and blocks 
 *        them using atomic operations on semaphores.
 * 
 * @note
 * The ADL is protected by its own semaphore to ensure mutual exclusion during concurrent access.
 * 
 * @ref
 * pandos Chapter 6
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

int delayDaemon_sema4; /*semaphore to provide mutual exclusion over the ADL*/
delayd_PTR delaydFree_h; /*Head pointer of the free list of delay descriptor nodes*/
delayd_PTR delayd_h; /*Pointer to the dummy head node of the Active Delay List (ADL)*/
delayd_PTR delayd_tail; /*Pointer to the dummy tail node of the ADL*/
static delayd_t delayDescriptors[MAXUPROCS + 2]; /*Static pool of delay descriptors (+2 for dummy head and tail)*/


/**************************************************************************************************  
 * @brief Allocates a delay event descriptor from the free list
 * 
 * This function manages a statically allocated pool of delay descriptors using a singly linked
 * free list. It returns a node from the head of the free list and initializes the node's fields
 * 
 * @param: None
 * @return 
 *    - Pointer to a newly allocated delay descriptor, or
 *    - NULL if no free descriptors are available.
 * 
 * @ref 
 *    - PANDOS Section 6.2.2, 6.3.4 & asl.c
 **************************************************************************************************/
delayd_PTR alloc_descriptor(){ /*similar logic to ASL*/
    delayd_PTR newDescriptor; /*pointer to new descriptor to be allocated from free list*/
    if (delaydFree_h != NULL){
        newDescriptor = delaydFree_h; /*take the descriptor at the head of the free list*/
        delaydFree_h = delaydFree_h->d_next; /*move head to next node*/

        /*Initialize descriptor node fields*/
        newDescriptor->d_next = NULL;
        newDescriptor->d_supStruct = NULL; /*reset to ensure no stale support structure*/
        newDescriptor->d_wakeTime = 0;
        return newDescriptor;
    }
    return NULL; /*in case we run out of free descriptor nodes*/
}

/**************************************************************************************************  
 * @brief Deallocates a delay descriptor node and returns it to the free list.
 * 
 * This function reclaims a delay event descriptor that is no longer in use (after a delayed process 
 * has been awakened). It inserts the node back at the head of the free list for reuse.
 * 
 * @param delayDescriptor 
 *    - Pointer to the delay descriptor to be returned to the free list.
 * 
 * @return None
 * 
 * @ref 
 *    - PANDOS Section 6.2.2, 6.3.4 & asl.c
 **************************************************************************************************/
void free_descriptor(delayd_PTR delayDescriptor){
    /*Insert the descriptor node at the front of the free list*/
    delayDescriptor->d_next = delaydFree_h;
    delaydFree_h = delayDescriptor;
}


/**************************************************************************************************
 * @brief Initializes the free list of delay event descriptor nodes.
 *
 * This function sets up the free list used by the Active Delay List (ADL) by linking together
 * the statically allocated pool of delay descriptor nodes.
 *
 * @param None
 * @return None
 *
 * @ref
 *    - PANDOS Section 6.2.2, 6.3.3 & 6.3.4
 **************************************************************************************************/
void initFreeList(){
    delaydFree_h = &delayDescriptors[2];
    int i;
    for (i = 3; i < MAXUPROCS + 2; i++){ /*Link each node in the free list to the next*/
        delayDescriptors[i - 1].d_next = &delayDescriptors[i];
    }
    delayDescriptors[MAXUPROCS + 1].d_next = NULL; /*mark end of free list*/
}


/**************************************************************************************************
 * @brief Set up the initial processor state for launching the delay daemon.
 *
 * This function sets up a state_t structure for the delay daemon with:
 * - s_pc and s_t9 pointing to the delayDaemon function [Sec 6.3.2].
 * - s_sp set to the top of RAM [Sec 4.10].
 * - s_status enabling kernel mode and all interrupts.
 * - s_entryHI ASID set to 0 for the kernel.
 *
 * @param: None
 * @return state_t: Initialized processor state for SYS1.
 *
 * @ref
 * PANDOS Sections 4.10, 6.3.2, 6.3.3
 **************************************************************************************************/
state_t daemon_setUp(){
    memaddr topRAM = *((int *)RAMBASEADDR) + *((int *)RAMBASESIZE);
    state_t base_state;
    base_state.s_entryHI = (DAEMONID << SHIFT_ASID); /*set entryHI ASID to 0*/
    base_state.s_pc = (memaddr) delayDaemon; /*PC point to delayDaemon function*/
    base_state.s_t9 = (memaddr) delayDaemon; /*Set t9 everytime we set PC*/
    base_state.s_sp = topRAM; /*set stack pointer to top of RAM*/
    base_state.s_status = ALLOFF | IEPON | IMON | TEBITON; /*kernel mode + interrupts enabled*/
    return base_state; 
}

/**************************************************************************************************  
 * Initialize Active Delay List (ADL)
 * Steps:
 * 1. Add each element from the static array of delay event descriptor nodes to the free list and 
 *    initialize the active list (zero, one or two dummy nodes)
 * 2. Initialize and launch (SYS1) the Delay Daemon.
 * 3. Set the Support Structure SYS1 parameter to be NULL
 * 
 * @param: None
 * @return: None
 * 
 * @ref:
 * pandos
 **************************************************************************************************/
void initADL(){
    delayDaemon_sema4 = 1;

    /*Initialize Free List*/
    initFreeList();

    /*Initialize ADL*/
    delayd_h = &delayDescriptors[0];
    delayd_tail = &delayDescriptors[1];
    delayd_h->d_next = delayd_tail;
    delayd_tail->d_next = NULL;

    delayd_h->d_supStruct = NULL;
    delayd_tail->d_supStruct = NULL;

    delayd_h->d_wakeTime = 0;
    delayd_tail->d_wakeTime = 0xFFFFFFFF;

    /*initialize base state for daemon and launch the daemon*/
    state_t daemon_initState;
    daemon_initState = daemon_setUp();
    int status = SYSCALL(SYS1, (int)&daemon_initState, (int)NULL, 0); /*launch delay daemon process*/
    if (status != 0) get_nuked(NULL);
}


/**************************************************************************************************  
 * Finds the appropriate insert position in the Active Delay List (ADL) for a new node.
 * 
 * The ADL is sorted in ascending order of wakeTime, and this function returns the pointer to 
 * the node that should precede the to-be-inserted descriptor
 * 
 * @param wakeTime: The time at which the new event descriptor is set to wake.
 * @return: Pointer to the descriptor node that will precede the new one.
 *          If the new descriptor has the earliest wakeTime, this will return the dummy head node.
 * 
 * @ref:
 * pandos 6.2.2, 6.3.4
 **************************************************************************************************/
delayd_PTR find_insert_position(int wakeTime){
    delayd_PTR prev = delayd_h;
    delayd_PTR curr = delayd_h->d_next;

    /*Traverse ADL until we find a node with wakeTime greater than or equal to the new one*/
    while (curr != delayd_tail && curr->d_wakeTime <= wakeTime){
        prev = curr;
        curr = curr->d_next;
    }
    return prev;
}

/**********************************************************************************************************************************************  
 * Inserts a new delay descriptor into the Active Delay List (ADL) in sorted order by wakeTime.
 * 
 * @param time_asleep: Delay duration in seconds.
 * @param supStruct: Pointer to the support structure of the calling user process.
 * @return TRUE if insertion was successful, FALSE if no free descriptor is available.
 * 
 * @ref:
 * pandos 6.2.2, 6.3.4
 **********************************************************************************************************************************************/
int insertADL(int time_asleep, support_t *supStruct){
    int currTime;
    delayd_PTR newDescriptor;

    newDescriptor = alloc_descriptor(); /*Allocate a descriptor node from the free list*/
    if (newDescriptor == NULL){
        return FALSE;
    }
    STCK(currTime); /*Get current time from TOD clock*/
    newDescriptor->d_wakeTime = currTime + SECONDS(time_asleep); /*Set waketime for new descriptor node*/
    newDescriptor->d_supStruct = supStruct;

    delayd_PTR prev = find_insert_position(newDescriptor->d_wakeTime); /*Find correct position in ADL to insert new descriptor node*/
    newDescriptor->d_next = prev->d_next;
    prev->d_next = newDescriptor;

    return TRUE;
}


/**************************************************************************************************  
 * Remove descriptor nodes that are to be woken up from the ADL and subsequently free them
 * 
 * @param:
 * @return:
 * 
 * @ref:
 * pandos
 **************************************************************************************************/
void removeADL(cpu_t currTime){
    delayd_PTR prev = delayd_h; /*dummy head*/
    delayd_PTR curr = delayd_h->d_next; /*actual head*/

    while (curr != delayd_tail && curr->d_wakeTime <= currTime) {
        if (curr->d_supStruct != NULL){
            SYSCALL(SYS4, (int)&curr->d_supStruct->privateSema4, 0, 0);
        }
        prev->d_next = curr->d_next;
        free_descriptor(curr);
        curr = prev->d_next; /*advance to next node*/
    }
}

/**************************************************************************************************  
 * Implements delay facility (delay daemon process)
 * 
 * @param: None
 * @return: None
 * 
 * @ref:
 * pandos
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
            free_descriptor(curr);
            curr = delayd_h->d_next;
        }
        /*removeADL(curr_time);*/
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
 * @param:
 * @return:
 * 
 * @ref
 * pandos
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

