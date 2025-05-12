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
 * This function initialize Active Delay List (ADL) and is called inside of test() in initProc.c
 * Steps:
 * 1. Initializes the delay descriptor free list used to manage available nodes.
 * 2. Creates an empty singly linked Active Delay List with dummy head and tail nodes.
 * 3. Sets up and launches the Delay Daemon process (via SYS1)
 * 
 * @param: None
 * @return: None
 * 
 * @ref:
 * pandos 6.3.3
 **************************************************************************************************/
void initADL(){
    delayDaemon_sema4 = 1; /*initialize ADL semaphore*/

    /*Initialize Free List*/
    initFreeList();

    /*Initialize empty ADL*/
    delayd_h = &delayDescriptors[0]; /*dummy head*/
    delayd_tail = &delayDescriptors[1]; /*dummy tail*/
    delayd_h->d_next = delayd_tail;
    delayd_tail->d_next = NULL;

    delayd_h->d_supStruct = NULL;
    delayd_tail->d_supStruct = NULL;

    delayd_h->d_wakeTime = 0; /*set dummy head wakeTime*/
    delayd_tail->d_wakeTime = LARGETIME; /*Dummy tail wakeTime marks the logical end*/

    /* Set up initial state for the Delay Daemon and launch it via SYS1*/
    state_t daemon_initState;
    daemon_initState = daemon_setUp(); /*Populate fields for delay daemon base state (PC, SP, etc.)*/
    int status = SYSCALL(SYS1, (int)&daemon_initState, (int)NULL, 0); /*launch delay daemon process*/
    if (status != 0) get_nuked(NULL); /*terminate if SYS1 fails*/
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
 * This function implements the delay daemon - an OS created process that periodically wakes up 
 * user processes whose sleep time has expired. The daemon blocks until a 100ms clock interrupt 
 * occurs, then checks the Active Delay List (ADL) for any descriptors ready to wake and signals 
 * their private semaphores.
 * 
 * @note: Freed descriptors are returned to the free list.
 * 
 * @param: None
 * @return: None
 * 
 * @ref:
 * pandos 6.2.2, 6.3.2
 **************************************************************************************************/
void delayDaemon(){
    cpu_t curr_time; /* Stores current time from TOD clock */

    while (TRUE){ /*inifinite loop*/
        SYSCALL(SYS7,0,0,0); /* Wait for 100ms clock tick */
        SYSCALL(SYS3,(int) &delayDaemon_sema4,0,0); /*Acquire mutex on ADL (lock ADL)*/
        STCK(curr_time); /* Get current time from TOD clock */

        /*Traverse ADL and wake up all processes whose wakeTime has expired */
        delayd_PTR curr = delayd_h->d_next;
        while (curr != delayd_tail && curr->d_wakeTime <= curr_time){
            SYSCALL(SYS4,(int)&curr->d_supStruct->privateSema4,0,0);  /* Unblock the sleeping process */
            delayd_h->d_next = curr->d_next; /* Remove current descriptor from ADL */
            free_descriptor(curr);  /* Return descriptor to the free list */
            curr = delayd_h->d_next; /* Move to next descriptor in ADL */
        }
        SYSCALL(SYS4,(int)&delayDaemon_sema4,0,0); /*Release mutex on ADL (unlock ADL)*/
    }
}

/**************************************************************************************************  
 * This function implements syscall 18 - DELAY. The syscall will block the requesting process for
 * a specified number of seconds until the delay period expires at which the process will be 
 * woken up from its "sleep"
 * 
 * Steps:
 * 1. If the requested sleepTime is zero, return immediately (no delay).
 * 2. If the requested sleepTime is negative, the U-proc is terminated via SYS9.
 * 3. Otherwise:
 *    a. Acquire mutual exclusion over the Active Delay List (ADL) using SYS3 (P).
 *    b. Attempt to allocate and insert a descriptor node into the ADL.
 *       - If allocation fails, release the semaphore and terminate the process.
 *    c. Atomically release the ADL semaphore (SYS4/V) and perform a P (SYS3) on the 
 *       U-proc's private semaphore to block the process until it is woken by the daemon.
 * 
 * @note: It is important that we perform the SYS4 on the ADL semaphore before putting the 
 *        user process to sleep via a SYS3 to make sure that the uproc does not go to sleep
 *        holding the semaphore lock.
 *        If the user process is blocked before releasing the ADL semaphore, the delay daemon 
 *        would be unable to access the ADL, causing a deadlock in the system.
 * 
 * @param: sleepTime – number of seconds to delay
 *         support_struct – pointer to U-proc’s support structure
 * @return: None
 * 
 * @ref:
 * pandos 6.1, 6.2, 6.3.1
 **************************************************************************************************/
void sys18Handler(int sleepTime, support_t *support_struct){
    if (sleepTime == 0) return; 
    else if (sleepTime < 0){ /*invalid delay request -> terminate*/
        get_nuked(NULL);
    }
    else{
        SYSCALL(SYS3,(int) &delayDaemon_sema4,0,0); /*Acquire mutex on the ADL (lock ADL)*/
        if (insertADL(sleepTime,support_struct) == FALSE){ /*Attempt to insert a new descriptor node onto ADL*/
            get_nuked(NULL);
        }
        setSTATUS(NO_INTS); /*Disable interrupts*/
        SYSCALL(SYS4,(int) &delayDaemon_sema4,0,0); /*Release mutex on the ADL (unlock ADL)*/
        SYSCALL(SYS3,(int)&support_struct->privateSema4,0,0); /*Block uproc on private semaphore (go to sleep)*/
        setSTATUS(YES_INTS); /*Re-enable interrupts*/
    }
}

