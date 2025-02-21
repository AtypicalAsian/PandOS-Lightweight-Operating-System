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

void switchProcess(){
    return -1;
}