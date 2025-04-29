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

void unblockLoad(int deviceType, int deviceInstance, unsigned int status) {
	pcb_PTR unblockedProc;
	unblockedProc = verhogen(&(deviceSemaphores[deviceType*8 + deviceInstance]));
	if (unblockedProc != NULL) {
		unblockedProc->p_s.s_v0 = status;
		softBlockCnt--;
	}
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
	int instanceMap = DEVREGADDR->interrupt_dev[deviceType];

	instanceMap &= -instanceMap;
	int deviceInstance = getInterruptLine(instanceMap);
	unsigned int status;

	if (deviceType == (TERMINT-DISKINT)) {
		device_t *termStatus = &(DEVREGADDR->devreg[deviceType*8 + deviceInstance]);

		if ((termStatus->d_status & TERMSTATUSMASK) == RECVD_CHAR) {
			status = termStatus->d_status;
			DEVREGADDR->devreg[deviceType*8 + deviceInstance].d_command = ACK;
			unblockLoad(deviceType, deviceInstance, status);
		}
		if ((termStatus->d_data0 & TERMSTATUSMASK) == TRANS_CHAR) {
			status = termStatus->d_data0;
			DEVREGADDR->devreg[deviceType*8 + deviceInstance].d_data1 = ACK;
			unblockLoad(deviceType + 1, deviceInstance, status);
		}
	}
	else {
		status = DEVREGADDR->devreg[deviceType*8 + deviceInstance].d_status;
		DEVREGADDR->devreg[deviceType*8 + deviceInstance].d_command = ACK;
		unblockLoad(deviceType, deviceInstance, status);
	}
	if (currProc == NULL)
		switchProcess();
	else
		LDST(EXCSTATE);
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
	currProc->p_s = *savedState; /*Saves the current process state (from the BIOS Data Page)*/
	currProc->p_time += get_elapsed_time(); /*Updates the CPU time used by the current process*/
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
	pcb_t *unblockedProc = NULL; /*pointer to a process being unblocked*/
	LDIT(INITTIMER);       /* Load the Interval Timer with 100ms to maintain periodic interrupts */
	while ((unblockedProc = removeBlocked(&semIntTimer)) != NULL){ /* Unblock all processes waiting on the pseudo-clock semaphore */
		insertProcQ(&ReadyQueue, unblockedProc); /* Move it to the Ready Queue */
	}
	softBlockCnt += semIntTimer;
	semIntTimer = 0;

	state_t *savedState = (state_t *) BIOSDATAPAGE;

	/* If there is a currently running process, resume execution */
	if (currProc != NULL){
		LDST(savedState);
	}
	else{
		switchProcess(); /*If no curr process to return to -> call scheduler to run next job*/
	}
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

	switch (interrupt) {
		case LOCALTIMERINT:
			pltInterruptHandler();
			break;
		case TIMERINTERRUPT:
			systemIntervalInterruptHandler();
			break;
		case DISKINTERRUPT:
		case FLASHINTERRUPT:
		case NETWINTERRUPT:
		case PRINTINTERRUPT:
		case TERMINTERRUPT:
			nontimerInterruptHandler(getInterruptLine(interrupt >> IPSHIFT) - DISKINT);
			break;
		default:
			break;
	}
}