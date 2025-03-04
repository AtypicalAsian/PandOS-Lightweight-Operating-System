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
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initial.h"

#include "/usr/include/umps3/umps/libumps.h"

HIDDEN void nontimerInterruptHandler();
HIDDEN void pltInterruptHandler();
HIDDEN void systemIntervalInterruptHandler();

/*Helper Functions*/
HIDDEN int getInterruptLine();
HIDDEN int getDevNum();



/*Global variables*/
cpu_t at_interrupt_TOD;    /*value of TOD clock when at the time we enter the interrupts module (i.e what is the current time when the interrupt was generated?)*/
cpu_t time_left;    /*Amount of time remaining in the current process' quantum slice (of 5ms) when the interrupt was generated*/


/**************************************************************************** 
 * getInterruptLine()
 * params:
 * return: None

 *****************************************************************************/
int getInterruptLine(){
    if ((savedExceptState->s_cause & LINE3MASK) != STATUS_ALL_OFF) return LINE3;
    else if ((savedExceptState->s_cause & LINE4MASK) != STATUS_ALL_OFF) return LINE4;
    else if ((savedExceptState->s_cause & LINE5MASK) != STATUS_ALL_OFF) return LINE5;
    else if ((savedExceptState->s_cause & LINE6MASK) != STATUS_ALL_OFF) return LINE6;
    else if ((savedExceptState->s_cause & LINE7MASK) != STATUS_ALL_OFF) return LINE7;
    return -1;  /* No interrupt detected */
}



/**************************************************************************** 
 * getDevNum()
 * Helper function to find device number that generated interrupt, after we
 * found the line on which the interrupt was generated on
 * params:
 * return: None

 *****************************************************************************/
int getDevNum(int line_num){
    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR; /* Get the Device Register Area */
    unsigned int bitMap = devRegArea->interrupt_dev[line_num - OFFSET]; /* Get Interrupt Devices Bit Map */
    
    /* Scan through the 8 possible devices using bitwise operations */
    int i;
    for (i = 0; i < DEVPERINT; i++) {
        if (bitMap & (1 << i)) { /* Check if bit i is set */
            return i; /* Return the first (highest-priority) device with a pending interrupt */
        }
    }
    /*No pending interrupts*/
    return -1;
}

/**************************************************************************** 
 * nontimerInterruptHandler()
 * params:
 * return: None

 *****************************************************************************/
void nontimerInterruptHandler() {
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
    
    cpu_t curr_TOD; /*value on time of day clock (currently)*/
    int lineNum;     /* The line number where the highest-priority interrupt occurred */
    int devNum;      /* The device number where the highest-priority interrupt occurred */
    int index;       /* Index in device register array of the interrupting device */
    devregarea_t *devRegPtr; /* Pointer to the device register area */
    int statusCode;  /* Status code from the interrupting device's device register */
    pcb_PTR unblockedPcb; /* Process that originally initiated the I/O request */

    /* Step 1: Identify the interrupt line */
    lineNum = getInterruptLine();

    /* Step 2: Identify the specific device that triggered the interrupt */
    devNum = getDevNum(lineNum);  
    index = ((lineNum - OFFSET) * DEVPERINT) + devNum; /* Compute device index */
    devRegPtr = (devregarea_t *) RAMBASEADDR;  /* Load device register area */

     /* Step 3: Handle Terminal Device Interrupts */
     if ((lineNum == LINE7) && (((devRegPtr->devreg[index].t_transm_status) & TERM_DEV_STATUSFIELD_ON) != READY)) {
        /* Transmission (Write) Interrupt */
        statusCode = devRegPtr->devreg[index].t_transm_status;
        devRegPtr->devreg[index].t_transm_command = ACK; /* Acknowledge interrupt */
        unblockedPcb = removeBlocked(&deviceSemaphores[index + DEVPERINT]); /* Unblock */
        deviceSemaphores[index + DEVPERINT]++; /* Perform V operation */
    } else {
        /* Reception (Read) Interrupt or Non-Terminal Device Interrupt */
        statusCode = devRegPtr->devreg[index].t_recv_status;
        devRegPtr->devreg[index].t_recv_command = ACK; /* Acknowledge interrupt */
        unblockedPcb = removeBlocked(&deviceSemaphores[index]); /* Unblock */
        deviceSemaphores[index]++; /* Perform V operation */
    }

    /* Step 4: If no process was waiting for I/O, return control */
    if (unblockedPcb == NULL) {
        if (currProc != NULL) { /* Resume execution of current process */
            update_pcb_state();
            currProc->p_time = currProc->p_time + (at_interrupt_TOD - start_TOD);
            setTIMER(time_left);
            swContext(currProc);
        }
        switchProcess();  /* Call scheduler if no current process exists */
    }

    /* Step 5: Process was unblocked, update its state */
    unblockedPcb->p_s.s_v0 = statusCode; /* Store status code in `v0` */
    insertProcQ(&ReadyQueue, unblockedPcb); /* Move process to Ready Queue */
    softBlockCnt--; /* Decrement soft-blocked process count */

    /* Step 6: Resume execution */
    if (currProc != NULL) { 
        update_pcb_state();
        setTIMER(time_left);
        currProc->p_time = currProc->p_time + (at_interrupt_TOD - start_TOD);
        STCK(curr_TOD); /* Get current time */
        unblockedPcb->p_time =unblockedPcb->p_time + (curr_TOD - at_interrupt_TOD); /* Charge time */
        swContext(currProc);
    }
    switchProcess(); /* Call the scheduler */
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

    cpu_t curr_TOD;

    /*If there is a running process when the interrupt was generated*/
    if (currProc != NULL){
        setTIMER(LARGETIME);
        update_pcb_state();
        STCK(curr_TOD);
        currProc->p_time = currProc->p_time + (curr_TOD - start_TOD);
        insertProcQ(&ReadyQueue,currProc);
        currProc = NULL;
        switchProcess();
    }
    PANIC();

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
        insertProcQ(&ReadyQueue,unblockedProc);
        softBlockCnt--;
    }

    /*Reset Pseudo-Clock Semaphore*/
    deviceSemaphores[PSEUDOCLOCKIDX] = PSEUDOCLOCKSEM4INIT;

    /*Return control to the current process (if curr proc not NULL)*/
    if (currProc != NULL){
        setTIMER(time_left);
        update_pcb_state();
        currProc->p_time = currProc->p_time + (at_interrupt_TOD - start_TOD);
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
    STCK(at_interrupt_TOD);
    time_left = getTIMER();
    savedExceptState = (state_PTR) BIOSDATAPAGE;

    if (((savedExceptState->s_cause) & LINE1MASK) != STATUS_ALL_OFF){
        pltInterruptHandler();
    }

    if (((savedExceptState->s_cause) & LINE2MASK) != STATUS_ALL_OFF){
        systemIntervalInterruptHandler();
    }
    nontimerInterruptHandler();
}