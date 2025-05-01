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

int delayDaemon_sema4; /*semaphore to provided mutual exclusion over the ADL*/
delayd_PTR delaydFree_h; /*Ptr to head of free list of event descriptors*/
delayd_PTR delayd_h; /*ptr to head of active delay list ADL*/


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
 * Return a node from the ADL to the free pool (of unsued descriptor nodes)
 * 
 **************************************************************************************************/
void return_to_ADL(delayd_PTR delayDescriptor){ /*similar logic to ASL*/
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

    static delayd_t delayDescriptors[MAXUPROCS+1]; /*add one more to use as dummy node for ADL*/
    delayDaemon_sema4 = 1;

    /*Init Active Delay List (ADL)*/
    delaydFree_h = &delayDescriptors[0];
    int i;
    for (i=1;i<MAXPROC;i++){
        delayDescriptors[i-1].d_next = &delayDescriptors[i];
    }
    /*init dummy tail*/
    delayDescriptors[MAXPROC - 1].d_next = NULL;
    delayd_h = &delayDescriptors[MAXPROC];         
    delayd_h->d_next = NULL;
    delayd_h->d_supStruct = NULL;
    delayd_h->d_wakeTime = 0xFFFFFFFF;  

    /*Set up initial state for delay daemon*/
    memaddr topRAM = (*((int *)RAMBASEADDR) + *((int *)RAMBASESIZE));
    state_t base_state;
    base_state.s_entryHI = (0 << SHIFT_ASID); /*set asid for delay daemon process (0)*/
    base_state.s_pc = (memaddr) delayDaemon;
    base_state.s_t9 = (memaddr) delayDaemon;
    base_state.s_sp = topRAM; /*CHANGE TO STARTING ADDRESS ?*/
    base_state.s_status = ALLOFF | IEPON | IMON | TEBITON; /*kernel mode + interrupts enabled*/

    int status;
    status = SYSCALL(SYS1,(int) &base_state,(int) NULL,0);

    if (status != 0){
        get_nuked(NULL);
    }
}


/**************************************************************************************************  
 * Helper method that searches the ADL for the event descriptor that directly precedes 
 * where the new descriptor should belong (ADL sorted by wakeTime)
 * 
 * @ret:
 *     - pointer to preceding event descriptor
 **************************************************************************************************/
delayd_PTR searchADL(int wakeTime){
    if (delayd_h == NULL){return NULL;}

    delayd_PTR prev = NULL;
    delayd_PTR curr = delayd_h;

    /*Traverse ADL to find correct insert position*/
    while (curr != NULL && curr->d_wakeTime < wakeTime){
        /*Stop at tail dummy node*/
        if (curr->d_wakeTime == 0xFFFFFFFF){ /*Define LARGE value for dummy WAKETIME value*/
            return prev;
        }
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
    delayd_PTR tempNode;
    delayd_PTR newNode;
    cpu_t currentTime;
    newNode = alloc_descriptor();

    if (newNode == NULL) {
        return FALSE;
    }

    /* Get the current time */
    STCK(currentTime);

    newNode->d_supStruct = supStruct;
    newNode->d_wakeTime = (1000000 * time_asleep) + currentTime;

    /* Insert the new node into the ADL in ascending order of wake-up time */
    if(newNode->d_wakeTime < delaydFree_h->d_wakeTime) {
        newNode->d_next = delaydFree_h;
        delaydFree_h = newNode;
    }
    else {
        /* Traverse the ADL to find the correct position */
        tempNode = delaydFree_h;

        while (tempNode->d_next->d_wakeTime < newNode->d_wakeTime) {
            tempNode = tempNode->d_next;
        }

        newNode->d_next = tempNode->d_next;
        tempNode->d_next = newNode;
    }

    return(TRUE);
}

/**************************************************************************************************  
 * Implements delay facility (delay daemon process)
 * 
 **************************************************************************************************/
void delayDaemon(support_t *currSuppStruct){
    cpu_t curr_time; /*store current time on TOD clock*/

    while (TRUE){ /*infite loop*/
        SYSCALL(SYS7,0,0,0); /*wait for 100ms to pass*/
        SYSCALL(SYS3,(int) &delayDaemon_sema4,0,0); /*obtain mutual exclusion over the ADL*/
        STCK(curr_time); /*get current time when we finally wake up from Wait Clock syscall*/
        while (delayd_h->d_wakeTime <= curr_time){
            SYSCALL(SYS4,(int)&delayd_h->d_supStruct->privateSema4,0,0); /*Perform SYS4 on uproc private semaphore*/
            return_to_ADL(delayd_h); /*Deallocate delay event descriptor node and return it to the free list*/
        }
        /*Release mutual exclusion over the ADL*/
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
    int sleep_time = support_struct->sup_exceptState[GENERALEXCEPT].s_a2;
    /*Negative wait time -> terminate uproc with SYS9*/
    /*if (sleep_time < 0){
        get_nuked(NULL);
    }*/

    /*Lock semaphore to obtain mutual exclusion over ADL*/
    SYSCALL(SYS3,(int) &delayDaemon_sema4,0,0);
    if (insertADL(sleep_time,support_struct) == FALSE){
        get_nuked(NULL);
    }

    /*Perform SYS4 on ADL semaphore then SYS3 on uproc private semaphore atomically*/
    setSTATUS(NO_INTS);
    SYSCALL(SYS4,(int) &delayDaemon_sema4,0,0);
    SYSCALL(SYS3,(int)&support_struct->privateSema4,0,0); 
    setSTATUS(YES_INTS);
}

