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