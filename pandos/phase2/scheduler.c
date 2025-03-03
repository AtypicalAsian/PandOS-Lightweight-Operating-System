/**************************************************************************** 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

This module implements process scheduling and deadlock detection to ensure the 
system maintains progress and prevents indefinite waiting. It employs a preemptive 
round-robin scheduling algorithm with a five-millisecond time slice, ensuring that 
each process in the Ready Queue gets a fair share of CPU time. 

If a process is available, it is removed from the Ready Queue, assigned to the Current Process, 
and executed. If no ready processes exist, the system evaluates different conditions: 
if no processes remain, it halts execution; if processes are blocked on I/O, the 
system enters a wait state; otherwise, if processes exist but are stuck (deadlock), 
the system triggers a panic.

To view version history and changes:
    - Remote GitHub Repo: https://github.com/AtypicalAsian/CS372-OS-Project
****************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "/usr/include/umps3/umps/libumps.h"

#include "../h/scheduler.h"
#include "../h/interrupts.h"
#include "../h/initial.h"



/**************************************************************************** 
 * This function is a helper function that is responsible for copying processor state
 * 
 * params: 
 *      - src: pointer to processor state to be copied
 *      - dst: pointer to procssor state to copy to
 * return: None

 *****************************************************************************/
void copyState(state_PTR src, state_PTR dst){

    /*Copy 31 general purpose registers*/
    int i;
    for (i=0;i<STATEREGNUM;i++){
        dst->s_reg[i] = src->s_reg[i];
    }

    /*Copy all 4 control registers*/
    dst->s_pc = src->s_pc;
    dst->s_cause = src->s_cause;
    dst->s_entryHI = src->s_entryHI;
    dst->s_status = src->s_status;

}


/**************************************************************************** 
 * Helper function
 * 
 * params: 
 *      - src: pointer to processor state to be copied
 *      - dst: pointer to procssor state to copy to
 * return: None

 *****************************************************************************/
void swContext(pcb_PTR curr_proccess){
    currProc = curr_proccess;
    STCK(time_of_day_start);
    LDST(&(curr_proccess->p_s));
}



/**************************************************************************** 
 * switchProcess()
 * params: 
 * return: None

 *****************************************************************************/

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
    currProc = removeProcQ(&ReadyQueue); /* Remove from ReadyQueue and set as current process */
    if (currProc != NULL){
        setTIMER(SCHED_TIME_SLICE);
        swContext(currProc);
    }

    if (procCnt == INITPROCCNT){
        HALT();
    }

    if ((softBlockCnt > INITSBLOCKCNT) && (procCnt > INITPROCCNT)){
        setSTATUS(STATUS_ALL_OFF |  STATUS_INT_ON | STATUS_IECON);
        setTIMER(LARGETIME);
        WAIT();
    }
    PANIC();
}
    