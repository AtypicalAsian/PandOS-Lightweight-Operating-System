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
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/interrupts.h"
#include "../h/initial.h"

#include "/usr/include/umps3/umps/libumps.h"


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


/**************************************************************************** 
 * swContext()
 *
 * @brief 
 * Switches execution to the specified process by updating the current process 
 * pointer, recording its start time, and loading its processor state.
 *
 * @details
 * This function is responsible for switching execution context to a new process. 
 * It updates currProc to point to the given process, records the time at which
 * the process starts running using the Time of Day (TOD) clock, and restores 
 * the process state by loading the saved processor state stored in the process 
 * control block (p_s). This effectively transfers control to the specified 
 * process.
 *
 * @param curr_process  Pointer to the PCB of the process to switch to.
 *
 * @return None (execution control is transferred to the new process).
 *****************************************************************************/

void swContext(pcb_PTR curr_proccess){
    currProc = curr_proccess;        /* Update the global pointer to track the currently running process */
    STCK(start_TOD);                 /* Store the current Time of Day (TOD) clock value into start_TOD */
    LDST(&(curr_proccess->p_s));     /* Load the processor state of the new current process */
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
    currProc = removeProcQ(&ReadyQueue); /* Remove a process from the ReadyQueue and assign it as the current process */

    /* If the ReadyQueue is not empty, schedule the next process */
    if (currProc != NULL){
        setTIMER(SCHED_TIME_SLICE);     /* Set Process Local Timer (PLT) to 5ms for time-sharing */
        swContext(currProc);            /* Perform context switch to the selected process */
    }

    /* If there are no active processes left in the system, halt execution */
    if (procCnt == INITPROCCNT){
        HALT(); /* No more processes to execute, system stops */
    }

    /* If no ready processes exist but there are blocked processes, enter a wait state */
    if ((procCnt > INITPROCCNT) && (softBlockCnt > INITSBLOCKCNT)){
        setSTATUS(STATUS_ALL_OFF |  STATUS_INT_ON | STATUS_IECON); /* Enable interrupts before waiting */
        setTIMER(LARGETIME); /* Set a large timer value to avoid premature wake-up */
        WAIT(); /* Enter wait state until an interrupt occurs (e.g., I/O completion) */
    }

    /* If the system reaches this point, it means no processes are ready or waiting on I/O */
    /* This indicates a deadlock situation, meaning all processes are blocked with no recovery */
    PANIC();  /* Trigger a system panic as no forward progress can be made */
}
    