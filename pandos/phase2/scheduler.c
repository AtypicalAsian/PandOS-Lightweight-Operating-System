/**************************************************************************** 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

This module contains the implementation of the deadlock detector and the 
scheduler.

To view version history and changes:
    - Remote GitHub Repo: https:ithub.com/AtypicalAsian/CS372-OS-Project
****************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "/usr/include/umps3/umps/libumps.h"

#include "../h/scheduler.h"
#include "../h/interrupts.h"
#include "../h/initial.h"

/*BIG PICTURE

1. Check if the ReadyQueue is empty:
    If procCnt == 0, there are no processes → HALT.
    If there are processes blocked on I/O (softBlockCnt > 0), enter Wait State.
    If no processes are running but also not waiting for I/O, we have Deadlock → PANIC.

2. Pick the next process from the Ready Queue and set it as currProc.

3. Set the Process Local Timer (PLT) to 5ms to ensure fair scheduling.

4. Load the process state into the CPU (LDST) so the process can run.

*/

void switchProcess() {
    /* Step 1: Check if ReadyQueue is empty */
    if (emptyProcQ(ReadyQueue)) {
        if (procCnt == 0) {
            /* No processes left -> HALT */
            HALT();
        } else if (softBlockCnt > 0) {
            /* There are blocked processes, enter Wait State */
            unsigned int status = getSTATUS(); /* Get current status */
            status |= (1 << 0);  /* Enable Global Interrupts (IEc = 1) */
            status |= (1 << 27); /* Enable Local Timer (TE = 1) */
            setSTATUS(status);  
            WAIT();
        } else {
            /* Deadlock condition -> PANIC */
            PANIC();
        }
    }
    
    /* Step 2: Select the next process to run */
    currProc = removeProcQ(&ReadyQueue); /* Remove from ReadyQueue and set as current process */
    
    /* Step 3: Set the Process Local Timer (PLT) to 5ms */
    setTIMER(SCHED_TIME_SLICE);
    
    /* Step 4: Load the process state and execute it */
    LDST(&(currProc->p_s));
}

