/**************************************************************************** 
 * @file interrupts.c
 * 
 * 
 * @brief
 * This module is responsible for handling interrupt exceptions that occur during 
 * process execution. The interrupt handler functions defined here are invoked by 
 * the Nucleus whenever an interrupt is detected. 
 * 
 * @details
 * 
 * - interruptsHandler() → Entry point for handling all types of interrupts.
 * - getInterruptLine() → Determines which interrupt line triggered the exception.
 * - getDevNum() → Identifies the specific device that generated an interrupt.
 * - pltInterruptHandler() → Handles **Process Local Timer (PLT) interrupts**.
 * - systemIntervalInterruptHandler() → Handles **System-wide Interval Timer interrupts**.
 * - nontimerInterruptHandler() → Manages **I/O device interrupts** (lines 3-7).
 * 
 * @note
 * @interrupt_priority
 * - If multiple interrupts occur simultaneously, they are handled one at a time 
 *   based on priority. The lower the line number, the higher the priority.
 * - The highest-priority pending interrupt is resolved first, before handling any  
 *   remaining lower-priority interrupts.
 * 
 * @cpu_time_accounting
 * A key decision in this module is how*CPU time is charged when handling interrupts:
 * - I/O Interrupts (Lines 3-7)  
 *   - The CPU time spent handling an I/O interrupt is charged to the process that  
 *     generated the interrupt, rather than the process that was running when the  
 *     interrupt occurred. We made this decision because it is appropriate to charge
 *     the process that generated the interrupt CPU time instead of penalizing the
 *     process that was running when the interrupt occurred.
 * 
 * 
 * - Handling Process Local Timer (PLT) Interrupts  
 *   - The time between when the Current Process began executing and when the PLT  
 *     interrupt occurred is charged to the Current Process.
 *   - Additionally, the time spent **handling the PLT interrupt** is also charged to  
 *     the Current Process, since it was responsible for exhausting its time slice.
 * 
 * - Handling System-wide Interval Timer Interrupt  
 *   - The Current Process is charged for the time it executed before the interrupt.  
 *   - However, the time spent handling the interrupt is not charged to any  
 *     process, since it is a global system event, not directly caused by any process.
 * 
 * @authors
 * - Nicolas & Tran
 * 
 * View version history and changes: https://github.com/AtypicalAsian/CS372-OS-Project
 ****************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initial.h"

#include "/usr/include/umps3/umps/libumps.h"

/**************** METHOD DECLARATIONS***************************/ 
HIDDEN void nontimerInterruptHandler();
HIDDEN void pltInterruptHandler();
HIDDEN void systemIntervalInterruptHandler();
HIDDEN int getInterruptLine();
HIDDEN int getDevNum();


cpu_t time_left;    /*Amount of time remaining in the current process' quantum slice (of 5ms) when the interrupt was generated*/


/****************************************************************************
 * getInterruptLine()
 * 
 * @brief Identifies the highest-priority pending interrupt line.  
 * 
 * @details
 * - This function checks the s_cause register in the saved processor state  
 *   to determine which interrupt line (3-7) has an active interrupt.  
 * - Interrupts are checked in ascending order, ensuring that the lowest-numbered  
 *   line (highest priority) is returned first.
 * - If no interrupts are detected, the function returns -1.  
 * 
 * @return int - The interrupt line number (3-7), or -1 if no interrupt is pending.  
 *****************************************************************************/
int getInterruptLine(){
    if ((savedExceptState->s_cause & LINE3MASK) != STATUS_ALL_OFF) return LINE3;        /* Check if an interrupt is pending on line 3 */
    else if ((savedExceptState->s_cause & LINE4MASK) != STATUS_ALL_OFF) return LINE4;   /* Check if an interrupt is pending on line 4 */
    else if ((savedExceptState->s_cause & LINE5MASK) != STATUS_ALL_OFF) return LINE5;   /* Check if an interrupt is pending on line 5 */
    else if ((savedExceptState->s_cause & LINE6MASK) != STATUS_ALL_OFF) return LINE6;   /* Check if an interrupt is pending on line 6 */
    else if ((savedExceptState->s_cause & LINE7MASK) != STATUS_ALL_OFF) return LINE7;   /* Check if an interrupt is pending on line 7 */
    return NO_INTERRUPTS;  /* No interrupt detected */
}



/****************************************************************************
 * getDevNum()
 * 
 * 
 * @brief Determines the specific device that generated an interrupt on a given line.
 * 
 * @details  
 * - This function is called after identifying the interrupt line (via getInterruptLine()).  
 * - It accesses the device register area to retrieve a bit map that represents  
 *   pending interrupts for up to 8 devices on the specified interrupt line.  
 * - Using bitwise operations, the function scans through the devices from highest priority (lowest index) to lowest priority,  
 *   returning the first device number with a pending interrupt.  
 * - If no device on the given line has a pending interrupt, the function returns -1.  
 * 
 * @param int line_num - The interrupt line number (3-7) on which an interrupt was detected.  
 * @return int - The device number (0-7) that generated the interrupt, or -1 if none are pending.  
 *****************************************************************************/
int getDevNum(int line_num){
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;  /* Get a pointer to the Device Register Area */
    unsigned int bitMap = devRegArea->interrupt_dev[line_num - OFFSET]; /* Retrieve the pending interrupt bit map for the given line */
    
    /* Iterate through all possible devices (8 per line) to find the highest-priority pending interrupt */
    int i;
    for (i = 0; i < DEVPERINT; i++) {
        if (bitMap & (INTERRUPT_BITMASK_INITIAL << i)) { /* Check if the i-th bit is set, meaning this device triggered an interrupt */
            return i; /* Return the first (highest-priority) device with a pending interrupt */
        }
    }
    /*No pending interrupts*/
    return NO_INTERRUPTS;
}

/**************************************************************************** 
 * nontimerInterruptHandler()
 * 
 * @brief 
 * Handles all non-timer interrupts (I/O device and terminal interrupts).  
 * This function identifies the interrupt source, acknowledges the interrupt,  
 * unblocks the waiting process if necessary, and resumes execution.
 * 
 * @details  
 * - Step 1: Identify the interrupt line where the highest-priority interrupt occurred.  
 * - Step 2: Identify the specific device that generated the interrupt on that line  
 * - Step 3: If a process was blocked on this device, unblock it.  
 * - Step 4: If no process was waiting, restore and continue executing the current process.  
 * - Step 5: If a process was unblocked, move it to the Ready Queue.  
 * - Step 6: Perform a context switch to resume execution where we left off.  
 * 
 * 
 * @return None
 *****************************************************************************/
void nontimerInterruptHandler() {
    /* 
    BIG PICTURE: 
    1. Find the pending interrupts from Cause Register (Processor 0, given its address is 0x0FFF.F000)
    2. Get the first interrupt (highest priority -- the lower the interrupt line and device number,
    the higher the priority of the interrupt), since the Interrupting Devices Bit Map will
    indicate which devices on each of these interrupt lines have a pending interrupt.
    3. Perform signal operation to notify CPU once an interrupt triggers (verhogen)
    4. Since the process terminates temporarily because of an I/O request, waitForIO is called.
    5. After I/O request finishes, the blocked process is moved out of ASL, and resumes its execution on ReadyQueue.
    6. LDST is called to restore the state of the unblocked process. This involves loading the saved context (stored in the process control block, `p_s`) of the unblocked process, which contains all the CPU register values (such as program counter, status, and general-purpose registers). This action effectively resumes the execution of the process, restoring it to the exact point where it was interrupted (before the I/O operation). The **LDST** function performs a context switch to this unblocked process, allowing it to continue from the last known state.
    */

    /* NOTE: 
    - consider to change to memaddr 
    - need to consider dereferencing
    */
    
    int lineNum;     /* The line number where the highest-priority interrupt occurred */
    int devNum;      /* The device number where the highest-priority interrupt occurred */
    int index;       /* Index in device register array of the interrupting device */
    devregarea_t *devRegPtr; /* Pointer to the device register area */
    int statusCode;  /* Status code from the interrupting device's device register */
    pcb_PTR unblockedPcb; /* Process that originally initiated the I/O request */

    /* Step 1: Identify the interrupt line */
    lineNum = getInterruptLine(); /* Retrieve the highest-priority interrupt line */

    /* Step 2: Identify the specific device that triggered the interrupt */
    devNum = getDevNum(lineNum);  /*get the device number of the device that generated the interrupt*/
    index = ((lineNum - OFFSET) * DEVPERINT) + devNum; /* Compute device index */
    devRegPtr = (devregarea_t *) RAMBASEADDR;  /* Load the base address of the device register area */

     /* ---------------- Step 3: Handle Terminal Device Interrupts (Line 7) ---------------- */
     if ((lineNum == LINE7) && (((devRegPtr->devreg[index].t_transm_status) & TERM_DEV_STATUSFIELD_ON) != READY)) {
        /* Transmission (Write) Interrupt */
        statusCode = devRegPtr->devreg[index].t_transm_status; /* Read status of transmission */
        devRegPtr->devreg[index].t_transm_command = ACK; /* Acknowledge the transmission interrupt */
        unblockedPcb = removeBlocked(&deviceSemaphores[index + DEVPERINT]); /* Unblock the process waiting on transmission */
        deviceSemaphores[index + DEVPERINT]++;  /* Perform V operation to release the semaphore */
    } 
    else {
        /* Reception (Read) Interrupt or Non-Terminal Device Interrupt */
        statusCode = devRegPtr->devreg[index].t_recv_status; /* Read status of reception */
        devRegPtr->devreg[index].t_recv_command = ACK; /* Acknowledge interrupt */
        unblockedPcb = removeBlocked(&deviceSemaphores[index]); /* Unblock the process waiting on reception */
        deviceSemaphores[index]++;  /* Perform V operation to release the semaphore */
    }

    /* ---------------- Step 4: If no process was waiting, return control ---------------- */
    if (unblockedPcb == NULL) {
        if (currProc != NULL) { /* If a process is already running, continue its execution */
            update_pcb_state(); /* Save the current process state */
            update_accumulated_CPUtime(start_TOD,at_interrupt_TOD,currProc); /* Update CPU time usage */
            setTIMER(time_left); /* Restore remaining time slice */
            swContext(currProc);  /* Resume execution */
        }
        switchProcess();  /* Call scheduler if no current process exists */
    }

    /* ---------------- Step 5: If a process was unblocked, move it to Ready Queue ---------------- */
    unblockedPcb->p_s.s_v0 = statusCode; /* Store the device status in the v0 register of unblocked process */
    insertProcQ(&ReadyQueue, unblockedPcb); /* Move the unblocked process to the Ready Queue */
    softBlockCnt--; /* Decrease soft-blocked process count */

    /* ---------------- Step 6: Resume Execution ---------------- */
    /*If there is a current process to return to*/
    if (currProc != NULL) { 
        update_pcb_state(); /* Save the current process state */
        setTIMER(time_left); /* Restore process time slice */
        update_accumulated_CPUtime(start_TOD,at_interrupt_TOD,currProc);  /* Update CPU time accounting */
        STCK(curr_TOD); /* Store the current Time-of-Day clock value */
        update_accumulated_CPUtime(at_interrupt_TOD,curr_TOD,unblockedPcb); /* Charge CPU time to unblocked process */
        swContext(currProc); /* Restore execution context */
    }
    switchProcess(); /* Call the scheduler if there is no current process to return to -> schedule next process */
}

/**************************************************************************** 
 * pltInterruptHandler()
 * 
 * @brief 
 * Handles a Process Local Timer (PLT) interrupt to enforce time-sharing.
 * 
 * @details  
 * - The PLT is a timer used to enforce preemptive scheduling by 
 *   periodically interrupting the running process.
 * - When a PLT interrupt occurs, this function:  
 *   1. Acknowledges the interrupt by resetting the timer.  
 *   2. Saves the current process state (from the BIOS Data Page).  
 *   3. Updates the CPU time used by the current process.  
 *   4. Moves the current process back to the Ready Queue.  
 *   5. Calls the scheduler to select the next process to run.  
 * - If no process is running, this function triggers a kernel panic.
 * 
 * @return None
 *****************************************************************************/
void pltInterruptHandler() {
    cpu_t curr_TOD; /* Stores the current Time-of-Day clock value */

    /*If there is a running process when the interrupt was generated*/
    if (currProc != NULL){
        setTIMER(LARGETIME);   /* Reset the Process Local Timer (PLT) with a large value to prevent further interrupts */
        update_pcb_state();    /* Save the current process state into its PCB */
        STCK(curr_TOD);        /* Store the current Time-of-Day clock value */
        update_accumulated_CPUtime(start_TOD,curr_TOD,currProc);   /* Update the accumulated CPU time for the current process */
        insertProcQ(&ReadyQueue,currProc);  /* Move the current process back to the Ready Queue since it used up its time slice */
        currProc = NULL;       /* Clear the current process pointer switch to the next process */
        switchProcess();       /* Call the scheduler to select and run the next process */
    }
    PANIC(); /* If no process was running when the interrupt occurred, trigger a kernel panic */

}



/**************************************************************************** 
 * systemIntervalInterruptHandler()
 * 
 * @brief 
 * Handles a System-Wide Interval Timer interrupt, which occurs every 100ms.
 * 
 * @details  
 * - The System Interval Timer is used to manage pseudo-clock-based process wakeups.
 * - When an interrupt occurs, this function:  
 *   1. Reloads the Interval Timer with 100ms to reset the Pseudo-Clock
 *   2. Unblocks all processes waiting on the pseudo-clock semaphore  
 *      (these processes were waiting via SYS7 - waitForClock()).  
 *   3. Resets the pseudo-clock semaphore to zero
 *   4. Restores execution of the current process if one exists.  
 *   5. Calls the scheduler if no process is available to run.  
 * 
 * @note The pseudo-clock semaphore is used for time-based process blocking.  
 *       Each process that calls SYS7 (waitForClock) is blocked on this semaphore until  
 *       the next interval timer tick, at which point it is unblocked.
 * 
 * @return None
 *****************************************************************************/
void systemIntervalInterruptHandler() {
    /* 
    BIG PICTURE: 
    1. Load Interval Timer with 100ms
    2. Unblock ALL pcbs blocked on the Pseudo-clock semaphore.
    3. Reset the Pseudo-clock semaphore to zero. This insures that all SYS7 calls
        block and that the Pseudo-clock semaphore does not grow positive
    4. Perform a LDST on the saved exception state -> return control to curr process

    5. If no currProc to return control to -> executes WAIT()
    */

    pcb_PTR unblockedProc; /*pointer to a process being unblocked*/
    LDIT(INITTIMER);       /* Load the Interval Timer with 100ms to maintain periodic interrupts */


    /* Unblock all processes waiting on the pseudo-clock semaphore */
    while (headBlocked(&deviceSemaphores[PSEUDOCLOCKIDX]) != NULL){
        unblockedProc = removeBlocked(&deviceSemaphores[PSEUDOCLOCKIDX]);  /* Remove a blocked process */
        insertProcQ(&ReadyQueue,unblockedProc);   /* Move it to the Ready Queue */
        softBlockCnt--;  /* Decrease the count of soft-blocked processes */
    }

    /* Reset the pseudo-clock semaphore to 0 */
    deviceSemaphores[PSEUDOCLOCKIDX] = PSEUDOCLOCKSEM4INIT;

    /* If there is a currently running process, resume execution */
    if (currProc != NULL){
        setTIMER(time_left);    /* Restore the remaining process quantum */
        update_pcb_state();     /* Save the updated processor state */
        update_accumulated_CPUtime(start_TOD,at_interrupt_TOD,currProc);  /* Update accumulated CPU time */
        swContext(currProc);    /*return control to current process (switch back to context of current process)*/
    }

    /*If no curr process to return to -> call scheduler to run next job*/
    switchProcess();

}


/**************************************************************************** 
 * interruptsHandler()
 * 
 * @brief 
 * Handles all hardware interrupts by determining the interrupt source  
 * and delegating processing to the appropriate handler.
 * 
 * @note  
 * - Interrupts are triggered by hardware events, such as timers and devices.  
 * - This function determines which interrupt occurred and calls the appropriate handler.  
 * - The Process Local Timer (PLT) and System Interval Timer have dedicated handlers.  
 * - All other interrupts (I/O devices, terminal, disk, etc.) are handled by nontimerInterruptHandler().
 * 
 * @details
 * Interrupt Handling Flow:
 * 1. Store the current Time-of-Day (TOD) clock value to track execution time.  
 * 2. Retrieve the remaining process quantum (time slice) before servicing the interrupt.  
 * 3. Check for Process Local Timer (PLT) interrupts (Line 1) → Call pltInterruptHandler().  
 * 4. Check for System-Wide Interval Timer interrupts (Line 2) → Call systemIntervalInterruptHandler().  
 * 5. If neither of the above, assume an I/O interrupt and call nontimerInterruptHandler().  
 * 
 * @return None
 *****************************************************************************/
void interruptsHandler(){
    STCK(at_interrupt_TOD); /* Store the current Time-of-Day (TOD) clock value for CPU timing purposes */
    time_left = getTIMER(); /* Get the remaining time from the current process quantum (if any) */
    savedExceptState = (state_PTR) BIOSDATAPAGE; /* Retrieve the saved processor state from BIOS Data Page */

    /* Check if the interrupt came from the Process Local Timer (PLT) (Line 1) */
    if (((savedExceptState->s_cause) & LINE1MASK) != STATUS_ALL_OFF){
        pltInterruptHandler();  /* Call method to handle the Process Local Timer (PLT) interrupt */
    }

    /* Check if the interrupt came from the System-Wide Interval Timer (Line 2) */
    if (((savedExceptState->s_cause) & LINE2MASK) != STATUS_ALL_OFF){
        systemIntervalInterruptHandler(); /* Call method to the System Interval Timer interrupt */
    }

    /*Handle non-timer interrupts*/
    nontimerInterruptHandler();  /* Call method to the handle non-timer interrupts */
}