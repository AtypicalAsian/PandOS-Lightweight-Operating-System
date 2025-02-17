/************************************************************************************************ 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

This module contains the entry point for Phase 2 - the main() function, which 
initializes Phase 1 data structures, consisting of the free semaphore descriptor
list, Active Semaphore List (ASL), and process queue (contains processes 
ready to be scheduled). With regards to the process queue, the module will
create an intial process in the ready queue to in order for the scheduler 
to begin execution.

Additional global variables for phase 2 are defined and specified as needed with in-line 
comments, and the general exception handler is implemented in this model, whose job is to 
pass up control to the device interrupt handler during interruptions or to the
appropriate function in the exceptions.c module to handle the particular type
of exception.

Last but not least, this module sets up four words in BIOS data page, each 
for the following item: TLB-refill handler, its stack pointer, 
the general exception handler and its stack pointer.

To view version history and changes:
    - Remote GitHub Repo: https://github.com/AtypicalAsian/CS372-OS-Project
************************************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "/usr/include/umps3/umps/libumps.h"

#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"