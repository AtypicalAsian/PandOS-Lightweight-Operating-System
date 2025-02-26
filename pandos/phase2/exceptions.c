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
HIDDEN void blockCurr(int *sem);
HIDDEN void createProcess(state_PTR stateSYS, support_t *suppStruct);  
HIDDEN void terminateProcess(pcb_PTR proc); 
HIDDEN void waitOp(int *sem);
HIDDEN void signalOp(int *sem);
HIDDEN void waitForIO(int lineNum, int deviceNum, int readBool);
HIDDEN void getCPUTime();
HIDDEN void waitForPClock();
HIDDEN void getSupportData();

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
        currProc->p_s.s_v0 = -1;
        return;
    }
    
    newProc->p_s = *stateSYS; 
    newProc->p_supportStruct = suppStruct; 
    newProc->p_time = 0; 
    newProc->p_semAdd = NULL; 
     
    insertChild(currProc, newProc); 
    insertProcQ(&ReadyQueue, newProc); 
    
    procCnt++;
 
    currProc->p_s.s_v0 = 0;
}

void terminateProcess(pcb_PTR proc) {
    pcb_PTR childProc;

    /* BIG PICTURE:
    - Recursively terminate all child processes before terminating the current process.
    - There are 2 cases: 
        - If the process is blocked on a semaphore, remove it from the Active Semaphore List (ASL).
        - If the process is in the Ready Queue, remove it.
    - Remove the process from its parent's child list.
    - Free the Process Control Block (PCB) to make it available for future processes.
    - If the terminated process is the currently running process, reset `currProc` and switch to a new process using `Scheduler()`.
    */
 
    while ((childProc = removeChild(proc)) != NULL) {
        terminateProcess(childProc);
    }
    
    if (proc->p_semAdd != NULL) {
        outBlocked(proc);  
    }
    
    outProcQ(&ReadyQueue, proc);

    if (proc->p_parent != NULL) {
        outChild(proc);
    }

    freePcb(proc);

    if (proc == currProc) {
        currProc = NULL;
        switchProcess(); 
    }
}

void waitOp(int *sem) {
    (*sem)--;  /* Decrement the semaphore */ 

    /* BIG PICTURE:
    - If the value of semaphore address is negative -> the resources are not available.
    - Block the process first, then move it to ASL.
    - Switch to a new process that needs less resources to execute by calling Scheduler().
    */
    if (*sem < 0) {  
        insertBlocked(sem, currProc);
        switchProcess();  /* Call the scheduler to run another process */
    }
}

void signalOp(int *sem) {
    pcb_PTR p;
    (*sem) ++;

    /* BIG PICTURE:
    - If the value of semaphore address is positive -> there is free resource to allocate for a blocked process.
    - Remove the first blocked process from ASL if there are any.
    - If there exists a process, add it to ReadyQueue to proceed.
    */
    if (*sem >= 0) {
        p = removeBlocked(sem);
        if (p != NULL) {
            insertProcQ(&ReadyQueue,p);
        }
    }
}

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
    int semIndex = (lineNum - 3) * 8 + deviceNum;

    if (lineNum == 7) {
        semIndex = semIndex * 2 + (readBool ? 0 : 1);
    }

    int *deviceSem = &deviceSemaphores[semIndex];

    if (*deviceSem < 0) {
        currProc->p_semAdd = deviceSem;
        insertBlocked(deviceSem,currProc);
        switchProcess();
    }

    currProc->p_s.s_v0 = deviceStatus[semIndex];
}