/************************************************************************************************ 
 * CS372 - Dr. Goldweber
 * 
 * @file scheduler.c
 * 
 * 
 * @brief
 * This module implements the process scheduling mechanism and deadlock detection to ensure 
 * system progress and prevent indefinite waiting. It employs a preemptive round-robin 
 * scheduling algorithm with a fixed time slice of 5ms to ensure fair CPU allocation among processes.
 * 
 * 
 * @details
 * The scheduler is responsible for:
 * - Selecting the next process from the Ready Queue and assigning it as the current process.
 * - Managing CPU time allocation through a process-local timer (PLT) to enforce time slices.
 * - Handling scenarios when no ready processes exist:
 *   - If there are no active processes, the system halts.
 *   - If all processes are blocked on I/O, the system enters a wait state.
 *   - If processes remain but no progress is possible (deadlock), the system triggers a panic.
 * - Correctly maintain process execution metadata by updating the accumulated CPU time of the 
 *   currently executing process before switching to the next.
 * 
 * 
 * @authors Nicolas & Tran
 * View version history and changes: https://github.com/AtypicalAsian/CS372-OS-Project
 ************************************************************************************************/
#include "../h/types.h"
#include "../h/const.h"

#include "../h/pcb.h"
#include "../h/asl.h"

#include "../h/scheduler.h"
#include "../h/initial.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"

#include "/usr/include/umps3/umps/libumps.h"

#define LARGETIME        0xFFFFFFFF
volatile cpu_t quantum;

/***********************HELPER METHODS***************************************/

/**************************************************************************** 
 * copyState()
 *
 * 
 * @brief 
 * This helper fucntion copies the entire processor state from one state 
 * structure to another.This includes all 31 general-purpose registers and 
 * 4 control registers.
 *
 * 
 * @details
 * The function performs a deep copy of the processor state, ensuring that the 
 * destination (dst) state matches the source (src) exactly. This is commonly 
 * used when dealing with context switches, exception/interrupt handling, or 
 * process creation to ensure that the current process can continue 
 * executing once the exception or interrupt is handled.
 *
 * 
 * @param src  Pointer to the source processor state to be copied.
 * @param dst  Pointer to the destination processor state where the data will be copied.
 *
 * @return None
 *****************************************************************************/
void copyState(state_PTR src, state_PTR dst){
    /*Copy 31 general purpose registers*/
    int i;
    for (i=0;i<STATEREGNUM;i++){
        dst->s_reg[i] = src->s_reg[i];
    }

    /*Copy all 4 control registers*/
    dst->s_entryHI = src->s_entryHI;
    dst->s_cause = src->s_cause;
    dst->s_status = src->s_status;
    dst->s_pc = src->s_pc;

}

/***************************SCHEDULER*************************************/

/**************************************************************************** 
 * switchProcess()
 *
 * 
 * @brief 
 * Selects and switches execution to the next ready process or handles system 
 * idle states (halt, wait, or panic) when no ready processes exist.
 *
 * 
 * @details
 * This function is responsible for process scheduling and context switching. 
 * It selects the next process from the ReadyQueue to be executed (sounds fun!). If the 
 * ReadyQueue is empty, it evaluates system conditions to determine whether 
 * to halt execution, enter a wait state, or trigger a system panic due to 
 * deadlock.
 *
 * 
 * @protocol
 * 1.Check ReadyQueue:
 *    - If a process is available, remove it from the queue and set it as currProc.
 *    - If no processes are ready:
 *      - If no processes exist, halt the system
 *      - If there are processes blocked on I/O, enter a wait state
 *      - If no ready or blocked processes exist, a deadlock has occurred
 *  
 * 2. Schedule Next Process:
 *    - Set currProc to the next process in the ReadyQueue.
 *    - Start its execution by restoring its processor state.
 *    - Set the Process Local Timer (PLT) to enforce time-sharing (5ms time slice).
 *
 * 3. Handle Empty ReadyQueue:
 *    - If no ready processes exist but some are waiting on I/O, enter a wait state.
 *    - If no processes are running or waiting for I/O, a deadlock has occurred, and the system panics.
 *
 * @note 
 * This function never returns. It either transfers control to a process via swContext(), 
 * halts execution, enters a wait state, or triggers a panic due to deadlock.
 *
 * @param None
 * @return None
 *****************************************************************************/

void switchProcess() {
	if (emptyProcQ(ReadyQueue)) { /* Check if the ReadyQueue is empty */
		if (procCnt == 0) { /* If no processes are active, halt the system */
			HALT(); /* System halt: no processes to run */
		}
		/* If there are active processes but none are blocked, deadlock has occurred */
		if (procCnt > 0 && softBlockCnt == 0) {
			PANIC();
		}
		/* If there are active processes but some are blocked, wait for an external interrupt */
		if (procCnt > 0 && softBlockCnt > 0) {
			unsigned int status = getSTATUS();
            setTIMER(TIME_TO_TICKS(PLT_HIGHEST_VAL));
            setSTATUS((status) | IECON | IMON);
			WAIT();
			setSTATUS(status);
		}
	}
	currProc = removeProcQ(&ReadyQueue); /* Remove the next ready process from the ReadyQueue and assign it as the current process */
	setTIMER(TIME_TO_TICKS(TIMESLICE));
	STCK(quantum);
	LDST(&(currProc->p_s));
}

