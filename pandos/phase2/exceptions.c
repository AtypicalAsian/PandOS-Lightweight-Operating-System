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
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initial.h"

#include "/usr/include/umps3/umps/libumps.h"

/*function prototypes*/
HIDDEN void blockCurrProc(int *sem);
HIDDEN void createProcess(state_PTR stateSYS, support_t *suppStruct);
HIDDEN void terminateProcess(pcb_PTR proc);
HIDDEN void passeren(int *sem);
HIDDEN void verhogen(int *sem);
HIDDEN void waitForIO(int lineNum, int deviceNum, int readBool);
HIDDEN void getCPUTime();
HIDDEN void waitForClock();
HIDDEN void getSupportData();




int syscallNo;


/**************************************************************************** 
 * This function is responsible for handling general exceptions. It determines 
 * the type of exception that occurred and delegates handling to the appropriate 
 * exception handler.
 * 
 * params: None
 * return: None

 *****************************************************************************/
void gen_exception_handler(){

    /**************************************************************************** 
     * BIG PICTURE
     * 1. Retrieves the saved processor state from BIOSDATAPAGE to analyze the exception.
     * 2. Extracts the exception code from the cause register to determine the type of exception.
     * 3. Delegates handling based on exception type:
     *      a. Device Interrupts (Code 0) → processing passed to device interrupt handler -> Calls interruptsHandler().
     *      b. TLB Exceptions (Codes 1-3) → processing passed to TLB exception handler -> Calls 
     *      c. System Calls (Code 8) → processing passed to syscall exception handler -> Calls 
     *      d. Program Traps (Code 4-7,9-12) → processing passed to program trap exception handler → Calls e

    *****************************************************************************/
    
    state_t *saved_state; /* Pointer to the saved processor state at time of exception */  
    int exception_code; /* Stores the extracted exception type */  

    saved_state = (state_t *) BIOSDATAPAGE;  /* Retrieve the saved processor state from BIOS data page */
    exception_code = ((saved_state->s_cause) & GETEXCPCODE) >> CAUSESHIFT; /* Extract exception code from the cause register */

    if (exception_code == INTCONST) {  
        /* Case 1: Exception Code 0 - Device Interrupt */
        interruptsHandler();  /* call the Nucleus' device interrupt handler function */
    }  
    if (exception_code <= CONST3) {  
        /* Case 2: Exception Codes 1-3 - TLB Exceptions */
        tlbTrapHanlder();  /* call the Nucleus' TLB exception handler function */
    }  
    if (exception_code == SYSCONST) {  
        /* Case 3: Exception Code 8 - System Calls */
        sysTrapHandler();  /* call the Nucleus' SYSCALL exception handler function */
    }
    /* Case 4: All Other Exceptions - Program Traps */
    prgmTrapHandler(); /* calling the Nucleus' Program Trap exception handler function because the exception code is not 0-3 or 8*/
 }


/**************************************************************************** 
 * update_pcb_state()
 * Helper function
 * params:
 * return: None

 *****************************************************************************/
void update_pcb_state(){
    copyState(savedExceptState,&(currProc->p_s));
}


/**************************************************************************** 
 * blockCurrProc()
 * Helper function that performs the ops necessary when blocking a process.
 * This involves updating the accumulated CPU time for the current process,
 * insert the current process onto the ASL.
 * params:
 * return: None

 *****************************************************************************/
void blockCurrProc(int *sem){
    STCK(curr_TOD);
    currProc->p_time = currProc->p_time + (curr_TOD - start_TOD);
    insertBlocked(sem,currProc);
    currProc = NULL;
}

/**************************************************************************** 
 * createProcess() - SYS1
 * params:
 *      - stateSYS
 *      - suppStruct: pointer to the pcb
 * return: None

 *****************************************************************************/
void createProcess(state_PTR stateSYS, support_t *suppStruct) {

    /* BIG PICTURE:
    - Allocate a new Process Control Block (PCB) for the new process.
    - If no PCB is available, return an error (-1) in the caller's v0 register.
    - Initialize the PCB fields, including the process state and support structure.
    - Reset CPU time usage and set the process as ready (not blocked on a semaphore).
    - Insert the new process into the process tree as a child of the current process.
    - Add the new process to the Ready Queue for scheduling.
    - Increment the process count to track active processes.
    - Return success (0) in the caller's v0 register.
    */
    pcb_PTR newProc;
    newProc = allocPcb();
    if (newProc != NULL){
        copyState(stateSYS, &(newProc->p_s));        /*perform deep copy of processor state - 31 general and 4 control registers*/
        newProc->p_supportStruct = suppStruct;      
        newProc->p_time = INITIAL_TIME; 
        newProc->p_semAdd = NULL; 
     
        insertChild(currProc, newProc); 
        insertProcQ(&ReadyQueue, newProc); 

        currProc->p_s.s_v0 = SUCCESS;
        procCnt++;
    }
    else{
        currProc->p_s.s_v0 = NULL_PTR_ERROR;
    }

    STCK(curr_TOD);
    currProc->p_time = currProc->p_time + (curr_TOD - start_TOD);
    swContext(currProc);
}

/**************************************************************************** 
 * terminateProcess() - SYS2
 * params:
 * return: None

 *****************************************************************************/
void terminateProcess(pcb_PTR proc) {
    int *processSem; /*ptr to sempahore of the current Process*/
    processSem = proc->p_semAdd;

    /* Recursively terminate all child processes */
    while (!(emptyChild(proc))){
        terminateProcess(removeChild(proc));
    }
    

    /* Remove the process from its current state (Running, Blocked, or Ready) */
    if (proc == currProc) {  
        /* If the process is currently running, detach it from its parent */
        outChild(proc);  
    }  
    else if (processSem != NULL) {  
        /* If the process is blocked, remove it from the ASL */
        outBlocked(proc);
        
        if (!(processSem >= &deviceSemaphores[DEV0] && processSem <= &deviceSemaphores[INDEXCLOCK])){ /* if proc is not blocked on a device semaphore */
			(*(processSem))++; /* incrementing the val of sema4*/
		}
        else {  
            softBlockCnt--;  /* Decrease soft-blocked process count */
        }  
    }  
    else {  
        /* Otherwise, the process was in the Ready Queue, so remove it */
        outProcQ(&ReadyQueue, proc);  
    }  

    /* Free the process and update system counters */
    freePcb(proc);  
    procCnt--;
    proc = NULL;
}



/**************************************************************************** 
 * passeren() - SYS3
 * params:
 * return: None

 *****************************************************************************/
void passeren(int *sem){
    /* BIG PICTURE:
    - If the value of semaphore address is negative -> the resources are not available.
    - Block the process first, then move it to ASL.
    - Switch to a new process that needs less resources to execute by calling Scheduler().
    */

    (*sem)--;  /* Decrement the semaphore */
    
    /*If semaphore value < 0, process is blocked on the ASL (transitions from running to blocked)*/
    if (*sem < SEM4BLOCKED) {
        blockCurrProc(sem); /*block the current process and perform the necessary steps associated with blocking a process*/  
        switchProcess();  /* Call the scheduler to run another process */
    }

    /*return control to the Current Process*/
    STCK(curr_TOD);
    currProc->p_time = currProc->p_time + (curr_TOD -start_TOD);
    swContext(currProc);
}


/**************************************************************************** 
 * verhogen() - SYS4
 * params:
 * return: None

 *****************************************************************************/
void verhogen(int *sem) {
    /* BIG PICTURE:
    - If the value of semaphore address is positive -> there is free resource to allocate for a blocked process.
    - Remove the first blocked process from ASL if there are any.
    - If there exists a process, add it to ReadyQueue to proceed.
    */
    (*sem)++;
    if (*sem <= SEM4BLOCKED) {
        pcb_PTR p = removeBlocked(sem);
        insertProcQ(&ReadyQueue,p);
    }
    STCK(curr_TOD);
    currProc->p_time = currProc->p_time + (curr_TOD - start_TOD);
    swContext(currProc);
}



/**************************************************************************** 
 * waitForIO() - SYS5
 * params:
 * return: None

 *****************************************************************************/
 void waitForIO(int lineNum, int deviceNum, int readBool) {
    /*devAddrBase = ((IntlineNo - 3) * 0x80) + (DevNo * 0x10) (for memory address w. device's device register, not I/O device ???)*/ 
    
    /*
    BIG PICTURE: 
    - Many I/O devices can cause a process to be blocked while waiting for I/O to complete.
    - Each interrupt line (3–7) has up to 8 devices, requiring us to compute `semIndex` to find the correct semaphore.
    - Terminal devices (line 7) have two independent sub-devices: read (input) and write (output), requiring an extra adjustment in `semIndex`.
    - When a process requests I/O, it performs a P operation on `deviceSemaphores[semIndex]`, potentially blocking the process.
    - If blocked, the process is inserted into the Active Semaphore List (ASL) and the system switches to another process.
    - Once the I/O completes, an interrupt will trigger a V operation, unblocking the process.
    - The process resumes and retrieves the device’s status from `deviceStatus[semIndex]`.
    */
    int semIndex;
    semIndex = ((lineNum - OFFSET) * DEVPERINT) + deviceNum;

    if (lineNum == LINENUM7 && readBool != TRUE) {
        semIndex += DEVPERINT;
    }

    softBlockCnt++;
    (deviceSemaphores[semIndex])--;
    blockCurrProc(&deviceSemaphores[semIndex]);
    switchProcess();

 }

/**************************************************************************** 
 * getCPUTime() - SYS6
 * params:
 * return: None

 *****************************************************************************/
void getCPUTime(){
    STCK(curr_TOD);
    currProc->p_s.s_v0 = currProc->p_time + (curr_TOD - start_TOD);
    currProc->p_time = currProc->p_time + (curr_TOD - start_TOD);
    swContext(currProc);
}

/**************************************************************************** 
 * waitForClock() - SYS7
 * params:
 * return: None

 *****************************************************************************/
void waitForClock(){
    (deviceSemaphores[INDEXCLOCK])--;
    blockCurrProc(&deviceSemaphores[INDEXCLOCK]); 
    softBlockCnt++;
	switchProcess();
}


/**************************************************************************** 
 * getSupportData() - SYS8
 * params:
 * return: None

 *****************************************************************************/
void getSupportData(){
    currProc->p_s.s_v0 = (int) (currProc->p_supportStruct);
    STCK(curr_TOD);
    currProc->p_time = currProc->p_time + (curr_TOD - start_TOD);
    swContext(currProc);
}


/**************************************************************************** 
 * exceptionPassUpHandler()
 * params:
 * return: None

 *****************************************************************************/
void exceptionPassUpHandler(int exceptionCode){
    /*If current process has a support structure -> pass up exception to the exception handler */
    if (currProc->p_supportStruct != NULL){
        copyState(savedExceptState, &(currProc->p_supportStruct->sup_exceptState[exceptionCode])); /* copy saved exception state from BIOS Data Page to currProc's sup_exceptState field */
        STCK(curr_TOD); /*get current time on TOD clock*/
        currProc->p_time = currProc->p_time + (curr_TOD - start_TOD); /*update currProc with accumulated CPU time*/
        LDCXT(currProc->p_supportStruct->sup_exceptContext[exceptionCode].c_stackPtr, currProc->p_supportStruct->sup_exceptContext[exceptionCode].c_status,currProc->p_supportStruct->sup_exceptContext[exceptionCode].c_pc);
    }
    /*Else, if no support structure -> terminate the current process and its children*/
    else{
        terminateProcess(currProc);
        currProc = NULL;
        switchProcess();
    }
}


/**************************************************************************** 
 * sysTrapHandler()
 * Entrypoint to exceptions.c module
 * params:
 * return: None

 *****************************************************************************/
void sysTrapHandler(){

    /*Retrieve saved processor state (located at start of the BIOS Data Page) & extract the syscall number to find out which type of exception was raised*/
    savedExceptState = (state_PTR) BIOSDATAPAGE;  
    syscallNo = savedExceptState->s_a0;  

    /*Increment PC by 4 avoid infinite loops*/
    savedExceptState->s_pc = savedExceptState->s_pc + WORDLEN;

    /*Edge case: If request to syscalls 1-8 is made in user-mode will trigger program trap exception response*/
    /*DOUBLE CHECK CONDITION*/
    if (((savedExceptState->s_status) & STATUS_USERPON) != STATUS_ALL_OFF){
        savedExceptState->s_cause = (savedExceptState->s_cause) & RESINSTRCODE; /* Set exception cause to Reserved Instruction */
        prgmTrapHandler();  /* Handle it as a Program Trap */
    } 

    /*Validate syscall number (must be between SYS1NUM and SYS8NUM) */
    if ((syscallNo < SYS1) || (syscallNo > SYS8)) {  
        prgmTrapHandler();  /* Invalid syscall, treat as Program Trap */
    }


    /*save processor state into cur */
    update_pcb_state(currProc);  

    /* Execute the appropriate syscall based on sysNum */
    switch (syscallNo) {  
        case SYS1:  
            createProcess((state_PTR) (currProc->p_s.s_a1), (support_t *) (currProc->p_s.s_a2));

        case SYS2:  
            terminateProcess(currProc);  
            currProc = NULL;  
            switchProcess();

        case SYS3:  
            passeren((int *) (currProc->p_s.s_a1));  

        case SYS4:  
            verhogen((int *) (currProc->p_s.s_a1));  

        case SYS5:  
            waitForIO(currProc->p_s.s_a1, currProc->p_s.s_a2, currProc->p_s.s_a3);  

        case SYS6:  
            getCPUTime();  

        case SYS7:  
            waitForClock();  

        case SYS8:  
            getSupportData();  

    }  

}


/**************************************************************************** 
* tlbTrapHanlder()
 * params:
 * return: None

 *****************************************************************************/
void tlbTrapHanlder(){
    exceptionPassUpHandler(PGFAULTEXCEPT);
}

/**************************************************************************** 
* prgmTrapHandler()
 * params:
 * return: None

 *****************************************************************************/
void prgmTrapHandler(){
    exceptionPassUpHandler(GENERALEXCEPT);
}


