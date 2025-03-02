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

/*Helper Functions*/
HIDDEN int getInterruptLine();
HIDDEN void getDevNum();
HIDDEN pcb_PTR unblockProcess(int semIndex, int statusCode);
HIDDEN pcb_PTR handleDeviceInterrupt(int lineNum, int devNum, int semIdx, devregarea_t *devRegPtr);
HIDDEN void handleNoUnblockedProcess(); /*might not need to  break into separate function for this one!!!!*/
HIDDEN void resumeCurrentProcess();



/*Global variables*/
cpu_t curr_time_enter_interrupt;    /*value of TOD clock when at the time we enter the interrupts module (i.e what is the current time when the interrupt was generated?)*/
cpu_t time_left;    /*Amount of time remaining in the current process' quantum slice (of 5ms) when the interrupt was generated*/



/**************************************************************************** 
 * getInterruptLine()
 * params:
 * return: None

 *****************************************************************************/
int getInterruptLine(){
    if ((savedExceptState->s_cause & LINE3MASK) != STATUS_ALL_OFF) return LINE3;
    if ((savedExceptState->s_cause & LINE4MASK) != STATUS_ALL_OFF) return LINE4;
    if ((savedExceptState->s_cause & LINE5MASK) != STATUS_ALL_OFF) return LINE5;
    if ((savedExceptState->s_cause & LINE6MASK) != STATUS_ALL_OFF) return LINE6;
    if ((savedExceptState->s_cause & LINE7MASK) != STATUS_ALL_OFF) return LINE7;
    return -1;  /* No interrupt detected */
}



/**************************************************************************** 
 * getDevNum()
 * Helper function to find device number that generated interrupt, after we
 * found the line on which the interrupt was generated on
 * params:
 * return: None

 *****************************************************************************/
void getDevNum(line_num){
    return -1;
}

/**************************************************************************** 
 * unblockProcess()
 * Helper function to unblock a process waiting on a device semaphore
 * params:
 * return:

 *****************************************************************************/
pcb_PTR unblockProcess(int semIndex, int statusCode){
    pcb_PTR proc = removeBlocked(&deviceSemaphores[semIndex]);
    if (proc != NULL){
        deviceSemaphores[semIndex]++;
        proc->p_s.s_v0 = statusCode;
        softBlockCnt--;
        insertProcQ(&ReadyQueue,proc);
    }
    return proc;
}


/**************************************************************************** 
 * handleDeviceInterrupt()
 * params:
 * return: pointer to PCB of the unblocked process if the process was waiting
 *         else NULL if no process was blocked

 *****************************************************************************/
pcb_PTR handleDeviceInterrupt(int lineNum, int devNum, int semIdx, devregarea_t *devRegPtr){
    int status_code;
     /* Handle Terminal Devices (Line 7) */
     if ((lineNum == LINE7) && ((devRegPtr->devreg[semIdx].t_transm_status & TERM_DEV_STATUSFIELD_ON) != READY)) {
        status_code = devRegPtr->devreg[semIdx].t_transm_status;
        devRegPtr->devreg[semIdx].t_transm_command = ACK;
        return unblockProcess(semIdx + DEVPERINT, status_code);
    }

    /* Handle Other Devices and Terminal Reception */
    status_code = devRegPtr->devreg[semIdx].t_recv_status;
    devRegPtr->devreg[semIdx].t_recv_command = ACK;
    return unblockProcess(semIdx, semIdx);
}


/**************************************************************************** 
 * handleNoUnblockedProcess()
 * params:
 * return: pointer to PCB of the unblocked process if the process was waiting
 *         else NULL if no process was blocked

 *****************************************************************************/
void handleNoUnblockedProcess(){
    if (currProc == NULL){
        switchProcess();
    } else{
        update_pcb_state();
        currProc->p_time += (curr_time_enter_interrupt - time_of_day_start);
        setTIMER(time_left);
        swContext(currProc);
    }
}

/**************************************************************************** 
 * nontimerInterruptHandler()
 * params:
 * return: None

 *****************************************************************************/
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

    int deviceNum;
    int interrupt_line; /*stores which line generated the interrupt*/
    int status_code;
    cpu_t curr_time;    /*value on time of day clock (currently)*/
    pcb_PTR unblockedProc;
    devregarea_t *dev_reg_ptr; /*ptr to dev register area*/
    int idx;

    /*Find out which line caused the Interrupt (Highest Prio First)*/
    if (((savedExceptState->s_cause) & LINE3MASK) != STATUS_ALL_OFF){
        interrupt_line = LINE3;
    }
    else if (((savedExceptState->s_cause) & LINE4MASK) != STATUS_ALL_OFF){
        interrupt_line = LINE4;
    }
    else if (((savedExceptState->s_cause) & LINE5MASK) != STATUS_ALL_OFF){
        interrupt_line = LINE5;
    }
    else if (((savedExceptState->s_cause) & LINE6MASK) != STATUS_ALL_OFF){
        interrupt_line = LINE6;
    }
    else {
        interrupt_line = LINE7;
    }

    /*no valid interrupt line*/
    if (interrupt_line < 0){
        return;
    }

    /*Find out which device on the line generated the interrupt*/
    /*deviceNum = getDevNum(interrupt_line);*/
    dev_reg_ptr = (devregarea_t *) RAMBASEADDR;
    idx = (interrupt_line - OFFSET) * DEVPERINT + deviceNum;

    /*Get device status & acknowledge interrupt*/
    
    /*Line 7 has terminal devices, which contain 2 sub-devices - one for transmission, one for reception -> have to handle transmission separately*/
    if ((interrupt_line == LINE7) && ((dev_reg_ptr->devreg[idx].t_transm_status & TERM_DEV_STATUSFIELD_ON) != READY)) {
        status_code = dev_reg_ptr->devreg[idx].t_transm_status;
        dev_reg_ptr->devreg[idx].t_transm_command = ACK;
        unblockedProc = removeBlocked(&deviceSemaphores[idx + DEVPERINT]);
        deviceSemaphores[idx+DEVPERINT]++;
    }
    /*Line 7 Reception or Other Lines*/
    else{
        status_code = dev_reg_ptr->devreg[idx].t_recv_status;
        dev_reg_ptr->devreg[idx].t_recv_command = ACK;
        unblockedProc = removeBlocked(&deviceSemaphores[idx]);
        deviceSemaphores[idx]++;
    }

    /*If there was a process unblocked*/
    if (unblockedProc != NULL){
        unblockedProc->p_s.s_v0 = status_code;
        softBlockCnt--;
        insertProcQ(&ReadyQueue,unblockedProc);
    }

    /*Special Case: No process unblocked*/
    if (unblockedProc == NULL){
        if (currProc == NULL){
            switchProcess();
        }
        else{
            update_pcb_state();
            currProc->p_time += (curr_time_enter_interrupt - time_of_day_start);
            setTIMER(time_left);
            swContext(currProc);
        }
    }

    if (currProc != NULL){
        update_pcb_state();
        setTIMER(time_left);
        currProc->p_time += (curr_time_enter_interrupt - time_of_day_start);
        STCK(curr_time);
        unblockedProc->p_time += (curr_time - curr_time_enter_interrupt);
        swContext(currProc);
    }
    switchProcess();
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
        currProc->p_time += (curr_time_enter_interrupt - time_of_day_start);
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
    STCK(curr_time_enter_interrupt);
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