/**************************************************************************** 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

This module contains the implementation of the exception handler

To view version history and changes:
    - Remote GitHub Repo: https://github.com/AtypicalAsian/CS372-OS-Project
****************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "/usr/include/umps3/umps/libumps.h"

#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initial.h"


/*function prototypes*/
HIDDEN void blockCurrProc(int *sem);
HIDDEN void createProcess(state_PTR stateSYS, support_t *suppStruct);
HIDDEN void terminateProcess(pcb_PTR proc);
HIDDEN void waitOp(int *sem);
HIDDEN void signalOp(int *sem);
HIDDEN void waitForIO(int lineNum, int deviceNum, int readBool);
HIDDEN void getCPUTime();
HIDDEN void waitForPClock();
HIDDEN void getSupportData();

cpu_t curr_time;    /*stores the current time in the TOD clock*/


/**************************************************************************** 
 * blockCurrProc()
 * Helper function that performs the ops necessary when blocking a process.
 * This involves updating the accumulated CPU time for the current process,
 * insert the current process onto the ASL.
 * params:
 * return: None

 *****************************************************************************/
void blockCurrProc(int *sem){
    STCK(curr_time);
    currProc->p_time = currProc->p_time + (curr_time - time_of_day_start);
    insertBlocked(sem,currProc);
    currProc = NULL;
}

/**************************************************************************** 
 * createProcess() - SYS1
 * params:
 *      - stateSYS
 *      - suppStruct: pointer to the pcb
 * return: None

 *****************************************************************************/
void createProcess(state_PTR stateSYS, support_t *suppStruct) {

    /* BIG PICTURE:
    - Allocate a new Process Control Block (PCB) for the new process.
    - If no PCB is available, return an error (-1) in the caller's v0 register.
    - Initialize the PCB fields, including the process state and support structure.
    - Reset CPU time usage and set the process as ready (not blocked on a semaphore).
    - Insert the new process into the process tree as a child of the current process.
    - Add the new process to the Ready Queue for scheduling.
    - Increment the process count to track active processes.
    - Return success (0) in the caller's v0 register.
    */
    pcb_PTR newProc = allocPcb(); 
    if (newProc == NULL) {
        currProc->p_s.s_v0 = NULL_PTR_ERROR;
    }
    
    copyState(stateSYS, &(newProc->p_s))        /*perform deep copy of processor state - 31 general and 4 control registers*/
    newProc->p_supportStruct = suppStruct;      
    newProc->p_time = INITIAL_TIME; 
    newProc->p_semAdd = NULL; 
     
    insertChild(currProc, newProc); 
    insertProcQ(&ReadyQueue, newProc); 

    procCnt++;
    currProc->p_s.s_v0 = READY;

    STCK(curr_time);
    currProc->p_time = currProc->p_time + (curr_time - time_of_day_start);
    swContext(currProc);
}

/**************************************************************************** 
 * terminateProcess() - SYS2
 * params:
 * return: None

 *****************************************************************************/
void terminateProcess(pcb_PTR proc) {  
    /* Recursively terminate all child processes */
    while (!emptyChild(proc)) {  
        pcb_PTR child = removeChild(proc);  
        terminateProcess(child);  
    }  

    /* Remove the process from its current state (Running, Blocked, or Ready) */
    if (proc == currentProc) {  
        /* If the process is currently running, detach it from its parent */
        outChild(proc);  
    }  
    else if (proc->p_semAdd != NULL) {  
        /* If the process is blocked, remove it from the ASL */
        outBlocked(proc);  

        /* If the process was NOT blocked on a device semaphore, increment the semaphore */
        if (proc->p_semAdd < &deviceSemaphores[DEV0] || proc->p_semAdd > &deviceSemaphores[INDEXCLOCK]) {  
            (*(proc->p_semAdd))++;  
        }  
        else {  
            softBlockCnt--;  /* Decrease soft-blocked process count */
        }  
    }  
    else {  
        /* Otherwise, the process was in the Ready Queue, so remove it */
        outProcQ(&ReadyQueue, proc);  
    }  

    /* Free the process and update system counters */
    freePcb(proc);  
    procCnt--;  
}



/**************************************************************************** 
 * passeren() - SYS3
 * params:
 * return: None

 *****************************************************************************/
void passeren(int *sem){
    /* BIG PICTURE:
    - If the value of semaphore address is negative -> the resources are not available.
    - Block the process first, then move it to ASL.
    - Switch to a new process that needs less resources to execute by calling Scheduler().
    */

    (*sem)--;  /* Decrement the semaphore */
    
    /*If semaphore value < 0, process is blocked on the ASL (transitions from running to blocked)*/
    if (*sem < SEM4BLOCKED) {
        blockCurrProc(sem); /*block the current process and perform the necessary steps associated with blocking a process*/  
        switchProcess();  /* Call the scheduler to run another process */
    }

    /*return control to the Current Process*/
    STCK(curr_time);
    currProc->p_time = currProc->p_time + (curr_time -time_of_day_start);
    STCK(time_of_day_start);
    LDST(&(currProc->p_s));
}


/**************************************************************************** 
 * verhogen() - SYS4
 * params:
 * return: None

 *****************************************************************************/
void verhogen(int *sem) {
    /* BIG PICTURE:
    - If the value of semaphore address is positive -> there is free resource to allocate for a blocked process.
    - Remove the first blocked process from ASL if there are any.
    - If there exists a process, add it to ReadyQueue to proceed.
    */

    pcb_PTR p;
    (*sem) ++;
    if (*sem <= SEM4BLOCKED) {
        p = removeBlocked(sem);
        if (p != NULL) {
            insertProcQ(&ReadyQueue,p);
        }
    }
    STCK(curr_time);
    currProc->p_time = currProc->p_time + (curr_time - time_of_day_start);
    STCK(time_of_day_start);
    LDST(&(currProc->p_s));
}



/**************************************************************************** 
 * waitForIO() - SYS5
 * params:
 * return: None

 *****************************************************************************/
void waitForIO(int lineNum, int deviceNum, int readBool) {
    /*devAddrBase = ((IntlineNo - 3) * 0x80) + (DevNo * 0x10) (for memory address w. device's device register, not I/O device ???)*/ 
    
    /*
    BIG PICTURE: 
    - Many I/O devices can cause a process to be blocked while waiting for I/O to complete.
    - Each interrupt line (3–7) has up to 8 devices, requiring us to compute `semIndex` to find the correct semaphore.
    - Terminal devices (line 7) have two independent sub-devices: read (input) and write (output), requiring an extra adjustment in `semIndex`.
    - When a process requests I/O, it performs a P operation on `deviceSemaphores[semIndex]`, potentially blocking the process.
    - If blocked, the process is inserted into the Active Semaphore List (ASL) and the system switches to another process.
    - Once the I/O completes, an interrupt will trigger a V operation, unblocking the process.
    - The process resumes and retrieves the device’s status from `deviceStatus[semIndex]`.
    */
    int semIndex = (lineNum - OFFSET) * DEVPERINT + deviceNum;

    if (lineNum == LINENUM7 && readBool != TRUE) {
        index += DEVPERINT;
    }

    (deviceSemaphores[semIndex])--;
    blockCurrProc(&deviceSemaphores[semIndex]);
    softBlockCnt++;
    switchProcess();
}


/**************************************************************************** 
 * getCPUTime() - SYS6
 * params:
 * return: None

 *****************************************************************************/
void getCPUTime(){
    STCK(curr_time);
    currProc->p_s.s_v0 = currProc->p_time + (curr_time - time_of_day_start);
    currProc->p_time = currProc->p_time + (curr_time - time_of_day_start);
    swContext(currProc);
}

/**************************************************************************** 
 * waitForClock() - SYS7
 * params:
 * return: None

 *****************************************************************************/
void waitForClock(){
    (deviceSemaphores[INDEXCLOCK])--;
    softBlockCnt++; 
	blockCurr(&deviceSemaphores[INDEXCLOCK]); 
	switchProcess();
}


/**************************************************************************** 
 * getSupportData() - SYS7
 * params:
 * return: None

 *****************************************************************************/
void getSupportData(){
    currProc->p_s.s_v0 = (int) (currProc->p_supportStruct);
    STCK(curr_time);
    currProc->p_time = currProc->p_time + (curr_time - time_of_day_start);
    swContext(currProc);
}


/**************************************************************************** 
* exceptionHandlerSwitch()
 * params:
 * return: None

 *****************************************************************************/
void exceptionHandlerSwitch(int exceptionCode){
    /*If current process has a support structure -> pass up exception to the exception handler */


    /*Else, if no support structure -> terminate the current process and its children*/
}


/**************************************************************************** 
* sysTrapHandler()
 * params:
 * return: None

 *****************************************************************************/
void sysTrapHandler(){
    return -1;
}


/**************************************************************************** 
* tlbTrapHanlder()
 * params:
 * return: None

 *****************************************************************************/
void tlbTrapHanlder(){
    exceptionHandlerSwitch(PGFAULTEXCEPT);
}

/**************************************************************************** 
* prgmTrapHandler()
 * params:
 * return: None

 *****************************************************************************/
void prgmTrapHandler(){
    exceptionHandlerSwitch(PGFAULTEXCEPT);
}


