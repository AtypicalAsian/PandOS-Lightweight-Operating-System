/**************************************************************************** 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

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

HIDDEN void nontimerInterruptHandler();
HIDDEN void pdltInterruptHandler();

void nontimerInterruptHandler(state_t *stateSYS) {
    /* 
    BIG PICTURE: 
    1. Find the pending interrupts from Cause Register (Processor 0, given its address is 0x0FFF.F000)
    2. Get the first interrupt (highest priority -- the lower the interrupt line and device number,
    the higher the priority of the interrupt), since the Interrupting Devices Bit Map will
    indicate which devices on each of these interrupt lines have a pending interrupt.
    3. Perform signal operation to notify CPU once an interrupt triggers (verhogen)
    4. Since the process terminates temporarily because of an I/O request, waitForIO is called.
    5. After I/O request finishes, the blocked process is moved out of ASL, and resumes its execution on ReadyQueue.
    6. LDST is called to restore the state of the unblocked process. This involves loading the saved context (stored in the process control block, `p_s`) of the unblocked process, which contains all the CPU register values (such as program counter, status, and general-purpose registers). This action effectively resumes the execution of the process, restoring it to the exact point where it was interrupted (before the I/O operation). The **LDST** function performs a context switch to this unblocked process, allowing it to continue from the last known state.
    */
    unsigned int registerCause = stateSYS->s_cause;
    unsigned int pendingInterrupts = (registerCause & 0x0000FF00) >> 8;

    int lineNum = -1;
    for (int i = 3; i <= 7; i ++) {
        if (pendingInterrupts & (1 << i)) {
            lineNum = i;
            break;
        }
    }

    if (lineNum < 0) {
        /* No interrupts found */
        return;
    }

    unsigned int devicePending = *(unsigned int*)(0x1000003C + ((lineNum - 3) * 0x80));

    int deviceNum = 0;
    while (deviceNum < 8 && !(devicePending & (1 << deviceNum))) {
        deviceNum += 1;
    }

    /* Compute the device's device register from lineNum & deviceNum */
    unsigned int* deviceAddrBase = (unsigned int*)(0x1000.0054 + ((lineNum - 3) * 0x80) + (deviceNum * 0x10));
    unsigned int deviceStatus = *deviceAddrBase;

    int semIndex = (lineNum - OFFSET) * DEVPERINT + deviceNum;
    verhogen(&deviceSemaphores[semIndex]);
    waitForIO(lineNum,deviceNum,(lineNum == 7) ? TRUE: FALSE);

    stateSYS->s_v1 = deviceStatus;

    pcb_PTR unblockedProc = removeBlocked(&deviceSemaphores[semIndex]);

    if (unblockedProc != NULL) {
        insertProcQ(&ReadyQueue, unblockedProc);  
    }

    LDST(&(unblockedProc->p_s));
}