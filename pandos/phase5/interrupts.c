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
int getInterruptLine(unsigned int interruptMap);
void nontimerInterruptHandler(int deviceType);
void pltInterruptHandler();
void systemIntervalInterruptHandler();
void interruptsHandler();

/****************************************************************************
 * getInterruptLine(unsigned int interruptMap)
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
 * @param - interruptMap - bitmap representing the status of potential interrupt lines
 * 
 * @return int - The interrupt line number, or -1 if no interrupt is pending.  
 *****************************************************************************/
int getInterruptLine(unsigned int interruptMap){
    int k;
	for (k=0; k<32;k++){
		if (interruptMap & (1<<k)) {return k;}
	}
	PANIC();
	return -1;
}

/**************************************************************************** 
 * nontimerInterruptHandler(int deviceType)
 * 
 * @brief 
 * Handles all non-timer interrupts (I/O device and terminal interrupts).  
 * This function identifies the interrupt source, acknowledges the interrupt,  
 * unblocks the waiting process if necessary, and resumes execution.
 * 
 * @details  
 * 1. Calculate the address for this device’s device register. [Section 5.1-pops]
 * 2. Save off the status code from the device’s device registers
 * 3. Acknowledge the outstanding interrupt. This is accomplished by writing 
 *    the acknowledge command code in the interrupting device’s device register.
 * 4. Perform a V operation on the Nucleus maintained semaphore associated
 *    with this (sub)device. This operation should unblock the process (pcb)
 *    which initiated this I/O operation and then requested to wait for its 
 *    completion via a SYS5 operation.
 * 5. Place the stored off status code in the newly unblocked pcb’s v0 register.
 * 6. Insert the newly unblocked pcb on the Ready Queue, transitioning this process from 
 *    the “blocked” state to the “ready” state.
 * 7. Return control to the Current Process: Perform a LDST on the saved exception 
 *    state (located at the start of the BIOS Data Page [Section 3.4]). 
 * 
 * @param - int corresponding to device type that gen the interrupt
 * @return None
 *****************************************************************************/
void nontimerInterruptHandler(int deviceType){
	devregarea_t *deviceRegisters = (devregarea_t *)RAMBASEADDR;  /*get pointer to devreg struct*/
	int device_intMap = deviceRegisters->interrupt_dev[deviceType]; /*retrieve interrupt status bitmap for specific device type*/
	unsigned int statusCode;

	int mask = 1;  /*Start with the least significant bit*/
	while (!(device_intMap & mask)) {
		mask <<= 1;  /*Shift the mask one bit to the left*/
	}
	device_intMap = mask;  /*device_intMap contains only the lowest set bit*/
	int deviceInstance = getInterruptLine(device_intMap);

	/*Case 1: Interrupt device is not terminal devs*/
	if (deviceType != 4){
		int regIndex = deviceType * 8 + deviceInstance;
		statusCode = deviceRegisters->devreg[regIndex].d_status; /*Save off the status code from the device’s device registers*/
		deviceRegisters->devreg[regIndex].d_command = ACK; /*Acknowledge the interrupt*/
		int semIndex = deviceType * 8 + deviceInstance;
		pcb_t *pcb_unblocked = verhogen(&(deviceSemaphores[semIndex])); /*perform v op on semaphore*/
		if (pcb_unblocked != NULL){
			softBlockCnt--;
			pcb_unblocked->p_s.s_v0 = statusCode; /*Place the stored off status code in the newly unblocked pcb’s v0 register*/
		}
	}
	/*Case 2: Handle Terminal devices separately*/
	else{
		int regIndex = deviceType * 8 + deviceInstance;
		device_t *tStat = &(deviceRegisters->devreg[regIndex]);
		/*Terminal devices have 2 subdevices: transmission and reception*/
		/*Case 1: If device is transmission*/
        if ((tStat->d_data0 & 0x000000FF) == 5) {
            statusCode = tStat->d_data0;  /*Retrieve the status*/
            deviceRegisters->devreg[regIndex].d_data1 = ACK; /*ACK the interrupt*/
            
            /*For transmit interrupts, use the next device type slot (deviceType + 1)*/
			/*Transmission device semaphores are 8 bits behind reception for terminal devices (plus 8 to index)*/
			int semIndex = (deviceType + 1) * 8 + deviceInstance;
			pcb_t *pcb_unblocked = verhogen(&(deviceSemaphores[semIndex])); /*do v op on device semaphore*/
            if (pcb_unblocked != NULL) {
				softBlockCnt--;
                pcb_unblocked->p_s.s_v0 = statusCode;
            }
        }
		/*Case 2: If device is reception*/
		if ((tStat->d_status & 0x000000FF) == 5) {
            statusCode = tStat->d_status;  /*Retrieve the status*/
            deviceRegisters->devreg[regIndex].d_command = ACK; /*ACK the interrupt*/
			int semIndex = deviceType * 8 + deviceInstance;
            pcb_PTR pcb_unblocked = verhogen(&(deviceSemaphores[semIndex])); /*do v op on device semaphore*/
            if (pcb_unblocked != NULL) {
				softBlockCnt--;
                pcb_unblocked->p_s.s_v0 = statusCode;                   
            }
        }
	}
	state_PTR savedState = (state_t *) BIOSDATAPAGE;
	if (currProc != NULL){
		LDST(savedState);
	}
	switchProcess();
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
	state_t *savedState = (state_t *) BIOSDATAPAGE;

	/*If there is a running process when the interrupt was generated*/
	setTIMER(TICKCONVERT(0xFFFFFFFFUL));  /*Reset the timer*/
	current_proc->p_s = *savedState; /*Saves the current process state (from the BIOS Data Page)*/
	current_proc->p_time += timePassed(); /*Updates the CPU time used by the current process*/
	insertProcQ(&ReadyQueue, currProc);  /* Move the current process back to the Ready Queue since it used up its time slice */
	currProc = NULL; /* Clear the current process pointer switch to the next process */
	switchProcess(); /* Call the scheduler to select and run the next process */
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
	pcb_PTR unblockedProc = NULL; /*pointer to a process being unblocked*/
	LDIT(INITTIMER);       /* Load the Interval Timer with 100ms to maintain periodic interrupts */

	/* Unblock all processes waiting on the pseudo-clock semaphore */
	while (headBlocked(&semIntTimer) != NULL){
		unblockedProc = removeBlocked(&semIntTimer); /* Remove a blocked process */
		insertProcQ(&ReadyQueue, unblockedProc); /* Move it to the Ready Queue */
		softBlockCnt--; /* Decrease the count of soft-blocked processes */
	}
	semIntTimer = 0; /* Reset the pseudo-clock semaphore to 0 */
	state_t *savedState = (state_t *) BIOSDATAPAGE;

	/* If there is a currently running process, resume execution */
	if (currProc != NULL){
		LDST(savedState);
	}
	switchProcess(); /*If no curr process to return to -> call scheduler to run next job*/
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
 * 
 * @return None
 *****************************************************************************/
void interruptsHandler() {
	state_t *savedState = (state_t *)BIOSDATAPAGE; /*Get saved processor state*/
	unsigned int causeReg = savedState->s_cause; /*Extract value from cause register*/
	int interrupt = (causeReg & 0x0000FE00); /*Mask out the bits corresponding to the pending interrupts*/
	interrupt &= -interrupt; /*Isolate the lowest set bit (highest priority interrupt)*/

	/* Check if the interrupt came from the Process Local Timer (PLT) (Line 1) */
    if (((savedState->s_cause) & LINE1MASK) != ALLOFF){
        pltInterruptHandler();  /* Call method to handle the Process Local Timer (PLT) interrupt */
    }

    /* Check if the interrupt came from the System-Wide Interval Timer (Line 2) */
    if (((savedState->s_cause) & LINE2MASK) != ALLOFF){
        systemIntervalInterruptHandler(); /* Call method to the System Interval Timer interrupt */
    }
	
	int nonTimerDeviceType = getInterruptLine(interrupt >> 8) - OFFSET; /*subtract offset since interrupts start at 3-7*/
    /*Handle non-timer interrupts*/
    nontimerInterruptHandler(nonTimerDeviceType);  /* Call method to the handle non-timer interrupts */
}