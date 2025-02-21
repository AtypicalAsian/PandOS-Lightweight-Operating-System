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


/* 
BIG PICTURE: 
There are 4 main types of exception handler: 

(??? implement in interrupts.c)
• For exception code 0 (Interrupts), processing should be passed along to your
Nucleus’s device interrupt handler. [Section 3.6] 

(already written in pandos.pdf)
• For exception codes 1-3 (TLB exceptions), processing should be passed
along to your Nucleus’s TLB exception handler. [Section 3.7.3]


• For exception codes 4-7, 9-12 (Program Traps), processing should be passed
along to your Nucleus’s Program Trap exception handler. [Section 3.7.2]

(
Instructions in pandos, focus on state_t defined in types.h to get register array and read address from this array
Set Processor 0 address as 0x0FFF.F000 (BIOS Datapage)
)
• For exception code 8 (SYSCALL), processing should be passed along to
your Nucleus’s SYSCALL exception handler. [Section 3.5]

*/


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