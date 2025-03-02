/**************************************************************************** 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

To view version history and changes:
    - Remote GitHub Repo: https://github.com/AtypicalAsian/CS372-OS-Project
****************************************************************************/

#include <string.h>  /* Required for memcpy */
#include <stdio.h>

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
// #include "/usr/include/umps3/umps/libumps.h"


#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initial.h"

HIDDEN void nontimerInterruptHandler();
HIDDEN void pltInterruptHandler();
HIDDEN void systemIntervalInterruptHandler();
cpu_t curr_time;    /*value of TOD clock when at the time we enter the interrupts module (i.e what is the current time when the interrupt was generated?)*/
cpu_t time_left;    /*Amount of time remaining in the current process' quantum slice (of 5ms) when the interrupt was generated*/

void nontimerInterruptHandler(state_PTR procState) {
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

    /* NOTE: 
    - consider to change to memaddr 
    - need to consider dereferencing
    */

    memaddr registerCause = procState->s_cause;
    memaddr pendingInterrupts = (registerCause & 0x0000FF00) >> 8;

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

    memaddr devicePending = *(memaddr*)(0x1000003C + ((lineNum - 3) * 0x80));

    int deviceNum = 0;
    while (deviceNum < 8 && !(devicePending & (1 << deviceNum))) {
        deviceNum += 1;
    }

    /* Compute the device's device register from lineNum & deviceNum */
    memaddr* deviceAddrBase = (memaddr*)(0x10000054 + ((lineNum - 3) * 0x80) + (deviceNum * 0x10));
    memaddr deviceStatus = *deviceAddrBase;

    /* Set ACK status for terminal device to signal the controller about a pending interrupt */
    if (deviceStatus == TRANSTATUS || deviceStatus == RECVSTATUS) {
        /* We set deviceAddrBase, not deviceStatus since it's memory-mapped register */
        *deviceAddrBase = ACK;
    }

    /* After the interrupt, set it back to RESET state ??? */
    *deviceAddrBase = RESET;

    int semIndex = (lineNum - OFFSET) * DEVPERINT + deviceNum;
    verhogen(&deviceSemaphores[semIndex]);
    waitForIO(lineNum,deviceNum,(lineNum == 7) ? TRUE: FALSE);

    procState->s_v1 = deviceStatus;

    pcb_PTR unblockedProc = removeBlocked(&deviceSemaphores[semIndex]);

    if (unblockedProc != NULL) {
        insertProcQ(&ReadyQueue, unblockedProc);  
    }

    LDST(&(unblockedProc->p_s));
}



/**************************************************************************** 
 * pltInterruptHandler()
 * params:
 * return: None

 *****************************************************************************/
void pltInterruptHandler() {
    /* 
    BIG PICTURE: 
    1. Acknowledge the PLT interrupt by reloading the timer.
    2. Save the current process state (from BIOS Data Page) into the process control block (pcb).
    3. Update the CPU time for the current process.
    4. Move the current process to the Ready Queue (since it used up its time slice).
    5. Call the scheduler to select the next process to run.
    */

    cpu_t curr_time;
    /*If there is no running process when the interrupt was generated*/
    if (currProc == NULL){
        PANIC(); /*stop the system*/
    }

    /*If there is a running process when the interrupt was generated*/
    setTIMER(LARGETIME);
    update_pcb_state();
    STCK(curr_time);
    currProc->p_time += (curr_time - time_of_day_start);
    insertProcQ(&ReadyQueue,currProc);
    currProc = NULL;
    switchProcess();
}



/**************************************************************************** 
 * systemIntervalInterruptHandler()
 * params:
 * return: None

 *****************************************************************************/
void systemIntervalInterruptHandler() {
    /* 
    BIG PICTURE: 
    1. Load Interval Timer with 100ms
    2. Unblock ALL pcbs blocked on the Pseudo-clock semaphore.
    3. Reset the Pseudo-clock semaphore to zero. This insures that all SYS7 calls
        block and that the Pseudo-clock semaphore does not grow positive
    4. Perform a LDST on the saved exception state -> return control to curr process

    5. If no currProc to return control to -> executes WAIT()
    */

    pcb_PTR unblockedProc; /*pointer to a process being unblocked*/
    LDIT(INITTIMER);


    /*unblock (wake-up) all pcbs blocked on the Pseudo-Clock Semaphore*/
    while (headBlocked(&deviceSemaphores[PSEUDOCLOCKIDX]) != NULL){
        unblockedProc = removeBlocked(&deviceSemaphores[PSEUDOCLOCKIDX]);
        softBlockCnt--;
        insertProcQ(&ReadyQueue,unblockedProc);
    }

    /*Reset Pseudo-Clock Semaphore*/
    deviceSemaphores[PSEUDOCLOCKIDX] = PSEUDOCLOCKSEM4INIT;

    /*Return control to the current process (if curr proc not NULL)*/
    if (currProc != NULL){
        setTIMER(time_left);
        update_pcb_state();
        currProc->p_time += (curr_time - time_of_day_start);
        swContext(currProc);    /*return control to current process (switch back to context of current process)*/
    }

    /*If no curr process to return to -> call scheduler to run next job*/
    switchProcess();

}


/**************************************************************************** 
 * interruptsHandler()
 * This is the entrypoint to the interrupts.c module when handling interrupts
 * params:
 * return: None

 *****************************************************************************/
void interruptsHandler(){
    STCK(curr_time);
    time_left = getTIMER();
    savedExceptState = (state_PTR) BIOSDATAPAGE;

    if (((savedExceptState->s_cause) & LINE1MASK) != STATUS_ALL_OFF){
        pltInterruptHandler();
    }

    if (((savedExceptState->s_cause) & LINE2MASK) != STATUS_ALL_OFF){
        systemIntervalInterruptHandler();
    }
    /*nontimerInterruptHandler();*/
}