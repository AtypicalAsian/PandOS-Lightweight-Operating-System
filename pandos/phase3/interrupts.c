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
void nontimerInterruptHandler();
void pltInterruptHandler();
void systemIntervalInterruptHandler();
int getInterruptLine();
int getDevNum();

int findIntLine(unsigned int map);
void unblockLoad(int deviceType, int deviceInstance, unsigned int status);
void nonTimerInterrupt(int deviceType);
void pltInterrupt();
void intervalTimerInterrupt();
void interruptsHandler(state_t *exceptionState);

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

	state_t *savedState = (state_t *)BIOSDATAPAGE;
    if ((savedState->s_cause & LINE3MASK) != ALLOFF) return 3;        /* Check if an interrupt is pending on line 3 */
    else if ((savedState->s_cause & LINE4MASK) != ALLOFF) return 4;   /* Check if an interrupt is pending on line 4 */
    else if ((savedState->s_cause & LINE5MASK) != ALLOFF) return 5;   /* Check if an interrupt is pending on line 5 */
    else if ((savedState->s_cause & LINE6MASK) != ALLOFF) return 6;   /* Check if an interrupt is pending on line 6 */
    else if ((savedState->s_cause & LINE7MASK) != ALLOFF) return 7;   /* Check if an interrupt is pending on line 7 */
	PANIC();
    return -1;  /* No interrupt detected */
}


int findIntLine(unsigned int map) {
    int i;
	for (i = 0; i < 32; i++) {
        if (map & (1 << i)) {
            return i;
        }
    }
    PANIC();
    return -1;
}


void unblockLoad(int deviceType, int deviceInstance, unsigned int status) {
	pcb_PTR unblockedProc;

	unblockedProc = verhogen(&(deviceSemaphores[deviceType][deviceInstance]));

	if (unblockedProc != NULL) {
		unblockedProc->p_s.s_v0 = status;
		softBlockCnt--;
	}
}


void nonTimerInterrupt(int deviceType) {
	devregarea_t *deviceRegisters = (devregarea_t *)RAMBASEADDR;  /*get pointer to devreg struct*/
	int device_intMap = deviceRegisters->interrupt_dev[deviceType]; /*retrieve interrupt status bitmap for specific device type*/
	/*device_intMap &= -device_intMap;*/

	int mask = 1;  // Start with the least significant bit.
	while (!(device_intMap & mask)) {
		mask <<= 1;  // Shift the mask one bit to the left.
	}
	device_intMap = mask;  // Now device_intMap contains only the lowest set bit.
	int deviceInstance = findIntLine(device_intMap);
	/*int deviceInstance = getInterruptLine();*/
	unsigned int status;

	if (deviceType == (TERMINT-DISKINT)) {
		device_t *termStatus = &(DEVREGADDR->devreg[deviceType][deviceInstance]);

		if ((termStatus->d_status & TERMSTATUSMASK) == RECVD_CHAR) {
			status = termStatus->d_status;
			DEVREGADDR->devreg[deviceType][deviceInstance].d_command = ACK;
			unblockLoad(deviceType, deviceInstance, status);
		}
		if ((termStatus->d_data0 & TERMSTATUSMASK) == TRANS_CHAR) {
			status = termStatus->d_data0;
			DEVREGADDR->devreg[deviceType][deviceInstance].d_data1 = ACK;
			unblockLoad(deviceType + 1, deviceInstance, status);
		}
	}
	else {
		status = DEVREGADDR->devreg[deviceType][deviceInstance].d_status;
		DEVREGADDR->devreg[deviceType][deviceInstance].d_command = ACK;
		unblockLoad(deviceType, deviceInstance, status);
	}
	if (currProc == NULL)
		switchProcess();
	else
		LDST(EXCSTATE);
}


void pltInterrupt() {
	setTIMER(TIME_TO_TICKS(PLT_HIGHEST_VAL));
	currProc->p_s = *EXCSTATE;
	currProc->p_time += get_elapsed_time();
	insertProcQ(&ReadyQueue, currProc);
	currProc = NULL;
	switchProcess();
}


void intervalTimerInterrupt() {
	LDIT(INTIMER);
	pcb_t *blockedProcess = NULL;

	while ((blockedProcess = removeBlocked(&semIntTimer)) != NULL) {
		insertProcQ(&ReadyQueue, blockedProcess);
	}

	softBlockCnt += semIntTimer;
	semIntTimer = 0;

	if (currProc == NULL)
		switchProcess();
	else
		LDST(EXCSTATE);
}

void interruptsHandler(state_t *exceptionState) {
	int pending_int = (exceptionState->s_cause & GETIP);

	pending_int &= -pending_int;

	switch (pending_int) {
	case LOCALTIMERINT:
		pltInterrupt();
		break;
	case TIMERINTERRUPT:
		intervalTimerInterrupt();
		break;
	case DISKINTERRUPT:
	case FLASHINTERRUPT:
	case NETWINTERRUPT:
	case PRINTINTERRUPT:
	case TERMINTERRUPT:
		nonTimerInterrupt(findIntLine(pending_int >> IPSHIFT) - DISKINT);
		break;
	default:
		break;
	}
}