/****************************************************************************  
 * @file exceptions.c  
 *  
 * 
 * @brief  
 * This module defines exception-handling functions for managing SYSCALLs,  
 * TLB exceptions, and program traps. It is responsible for appropriately  
 * handling system calls, passing up exceptions when necessary, and ensuring  
 * correct process termination when errors occur.  
 *  
 * @details  
 * The exceptions module is an essential part of the Nucleus, allowing processes  
 * to request system services through SYSCALLs and ensuring that memory and  
 * program exceptions are managed correctly. The three main exception handlers  
 * are:  
 *  
 * - sysTrapHandler(): Handles SYSCALL exceptions, routing them to the appropriate  
 *   syscall handler function (SYS1 - SYS8). Ensures syscalls are made in kernel mode  
 *   and that invalid syscalls are treated as program traps.
 *   
 * - pgmTrapHandler(): Handles program traps, such as illegal memory accesses  
 *   or invalid instructions. If the process has a support structure, the exception  
 *   is passed up; otherwise, the process is terminated.  
 * 
 * - tlbTrapHandler(): Handles TLB exceptions, which occur when a memory access is 
 *   made to an unmapped page. Like program traps, the exception is passed up if 
 *   a support structure exists; otherwise, the process is terminated.  
 *  
 * Additionally, this module implements Pass Up or Die logic:  
 * - If a process has a support structure, the exception is passed up  
 *   to its handler.  
 * - If not, the process dies (is terminated), along with its children.  
 *  
 *  
 *  CPU Timing Policy
 *  A key design decision in this module is how CPU time is accounted for 
 *  when a process makes a SYSCALL request.  
 *  
 * - When a SYSCALL is made, the system transitions into exception-handling mode,
 *   transferring control to the kernel. We have decided that the time spent executing 
 *   the exception handler will be charged to the requesting process. This means that 
 *   while the kernel is handling the system call, the process that made the request 
 *   is still considered "running" and is accumulating CPU time. We made this decision because
 *   the current process is the process that requested to spend part of its time slice
 *   to handle the SYSCALL request, so it's logical to charge this time as part of the
 *   accumulated CPU time for the current process.
 *  
 *  
 * @note  
 * The exception handler assumes that all processes executing SYSCALLs are  
 * running in kernel mode. If a SYSCALL is made from user mode, it is  
 * treated as a program trap, and the process is terminated.  
 *  
 * @authors  
 * Nicolas & Tran  
 * View version history and changes: https://github.com/AtypicalAsian/CS372-OS-Project  
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

HIDDEN void blockCurrProc(int *sem); /* Block the current process on the given semaphore (helper method) */
int syscallNo; /*stores the syscall number (1-8)*/
HIDDEN void recursive_terminate(pcb_PTR proc);



/**************** HELPER METHODS ***************************/

/*Helper method to calculate elapsed time since process quantum began*/
cpu_t get_elapsed_time() {
	volatile cpu_t clockTime;
	STCK(clockTime); /*Read the current clock tick*/
	return clockTime - quantum; /*Return the elapsed time relative to the process's quantum*/
}

/*Helper method to copy 'len' bytes from the source memory block 'src' to the destination memory block 'dest' */
void* memcpy(void *dest, const void *src, unsigned int len) {
	char *d = dest;
	const char *s = src;
	while (len--) {
		*d++ = *s++;
	}
	return dest;
}

/*Helper method to block the currently running process (currProc) on a specified semaphore.*/
void blockCurrProc(int *sem){
	currProc->p_s = *((state_t *) BIOSDATAPAGE); /*get current processor state*/
	currProc->p_time += get_elapsed_time(); /*update process's accumulated CPU time by adding elapsed time since quantum began*/
	insertBlocked((int *) sem, currProc); /*insert current proc into blocked queue associated with the given semaphore*/
	currProc = NULL; /*reset currProc global variable*/
}

/*Recurisve function - helper to SYS2 terminateProcess()*/
void recursive_terminate(pcb_PTR proc){
	pcb_PTR child_proc; /*point to current child (pcb) of process to be terminated*/
	int *processSem;
	processSem = proc->p_semAdd;

	/* Terminate all child processes of proc */
    while ((child_proc = removeChild(proc)) != NULL) {
        outProcQ(&ReadyQueue, child_proc);  /*Remove child from the Ready Queue*/
        recursive_terminate(child_proc);       /*Recursively terminate the child process*/
    }

	/* Check if process p is blocked on a device semaphore.
       It is considered blocked on a device if its semaphore pointer (p->p_semAdd)
       falls within the range of deviceSemaphores[] or is equal to the address of semIntTimer. */
	   int blockedOnDevice = 
	   ((processSem >= (int *) deviceSemaphores) &&
		(processSem < ((int *) deviceSemaphores + (sizeof(int) * DEVICE_TYPES * DEV_UNITS))))
		|| (processSem == (int *) &semIntTimer);

   /* Remove p from its blocked queue (if it is currently blocked) */
   pcb_PTR removedPcb = outBlocked(proc);

   /* If the process was removed from a blocking queue and is not blocked on a device,
	  then increment the associated semaphore value (signaling that a resource has been freed). */
   if (!blockedOnDevice && removedPcb != NULL) {
	   (*(processSem))++;
   }

   /* Free the process control block for p and update the global process count */
   freePcb(proc);
   procCnt--;
}



/****************************************************************************  
 * createProcess() - SYS1  
 *  
 * 
 * @brief  
 * Creates a new process (as a child of the current process) by allocating a 
 * new PCB, initializing it, and adding it to the process tree and Ready 
 * Queue for execution.  
 *  
 * 
 * @details  
 * - Allocates a new PCB using allocPcb()  
 * - If no PCB is available, sets v0 to -1 and returns.  
 * - Copies the state of the requesting process (stateSYS) into the new PCB.  
 * - Initializes process fields (support structure, CPU time, etc.).  
 * - Inserts the new process as a child of the current process.  
 * - Adds the new process to the Ready Queue.  
 * - Increments procCnt
 * - Returns 0 in v0 if successful.  
 *  
 * @param state_PTR stateSYS - Pointer to the initial state of the new process.  
 * @param support_t *suppStruct - Pointer to the process's support structure.  
 * 
 * 
 * @return None  
 *****************************************************************************/
void createProcess(state_PTR stateSYS, support_t *suppStruct) {
    pcb_PTR newProc;  /* Pointer to the new process' PCB */
    newProc = allocPcb(); /* Allocate a new PCB from the free PCB list */

     /* If a new PCB was successfully allocated */
    if (newProc != NULL){
		/*newProc->p_s = *stateSYS;*/
        copyState(stateSYS, &(newProc->p_s));        /* Copy the given processor state to the new process */
        newProc->p_supportStruct = suppStruct;       /* Assign the provided support structure */
        newProc->p_time = 0;              			 /* Initialize CPU time usage to 0 */
        newProc->p_semAdd = NULL;                    /* New process is not blocked on a semaphore */
     
        insertChild(currProc, newProc);              /* Insert the new process as a child of the current process */
        insertProcQ(&ReadyQueue, newProc);           /* Add the new process to the Ready Queue for scheduling */

        currProc->p_s.s_v0 = 0;                		 /* Indicate success (0) in the caller's v0 register */
        procCnt++;                                   /* Increment the active process count */
    }
    /* If no PCB was available (Free PCB Pool exhausted) */
    else{
        currProc->p_s.s_v0 = -1;         /* Indicate failure (assign -1) in v0 */
    }
}



/****************************************************************************  
 * terminateProcess() - SYS2  
 *  
 * 
 * @brief  
 * Recursively terminates a process and all of its children, removing them  
 * from the system and freeing their PCBs.  
 *  
 * 
 * @details  
 * - If the process has children, it recursively terminates them first.  
 * - If the process is running (currProc), it is detached from its parent.  
 * - If the process is blocked, it is removed from the ASL.  
 * - If the process is waiting on a device semaphore, it decrements softBlockCnt.  
 * - If the process is in the Ready Queue, it is removed from the queue.  
 * - Frees the process PCB and decrements procCnt.  
 * - If terminating currProc, it calls switchProcess() to schedule a new process.  
 *  
 * 
 * @param pcb_PTR proc - Pointer to the process control block of the process to terminate.  
 *  
 * @return None  
 *****************************************************************************/
<<<<<<< HEAD
void terminateProcess(pcb_PTR proc) {
    int *processSem; /* Pointer to the semaphore that the process is blocked on (if any) */
    processSem = proc->p_semAdd;  /* Retrieve the semaphore the process is currently waiting on */

    /* Recursively terminate all child processes */
    while (!(emptyChild(proc))){
        terminateProcess(removeChild(proc)); /* Recursively remove and terminate child processes */
    }
    

    /* Remove the process from its current state (Running, Blocked, or Ready) */
    if (proc == currProc) {  
        /* If the process is currently running, detach it from its parent */
        outChild(proc);  
    }  
    else if (processSem != NULL) {  
        /* If the process is blocked, remove it from the ASL */
        outBlocked(proc);
        
        /*  If the process was NOT blocked on a device semaphore.  */
        if (!(processSem >= &deviceSemaphores[DEV0] && processSem <= &deviceSemaphores[INDEXCLOCK])){ 
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

    /* Free the process's PCB and update system process count */
    freePcb(proc);   /* Return the PCB to the free list */
    procCnt--;       /* Decrement the count of active processes */
    proc = NULL;     /* Nullify the pointer */
=======
void terminateProcess() {
    outChild(currProc);
	recursive_terminate(currProc);
	currProc = NULL;
	switchProcess();
>>>>>>> fafd3b43fb303831ee72b8e7787e119e265fbed2
}


/****************************************************************************  
 * passeren() - SYS3  
 *  
 * 
 * @brief  
 * Performs a P (wait) operation on a semaphore. If the semaphore value is  
 * negative, the calling process is blocked and moved to the ASL.  
 * 
 *  
 * @details  
 * - Decrements the semaphore value.  
 * - If the value is negative, the process is blocked and inserted into the ASL.  
 * - The process is removed from execution and switchProcess() is called.  
 * - If the process is not blocked, execution resumes immediately.  
 * 
 *  
 * @param int *sem - Pointer to the semaphore to be decremented.  
 * 
 *  
 * @return None  
 *****************************************************************************/
void passeren(int *sem){
    (*sem)--;  /* Decrement the semaphore */
    
    /*If semaphore value < 0, process is blocked on the ASL (transitions from running to blocked)*/
    if (*sem < 0) {
        blockCurrProc(sem);
        switchProcess();  /* Call the scheduler to run another process */
    }
}

/****************************************************************************  
 * verhogen() - SYS4  
 *  
 * 
 * @brief  
 * Performs a V (signal) operation on a semaphore. If there are blocked  
 * processes, the first one is unblocked and added to the Ready Queue.  
 *  
 * 
 * @details  
 * - Increments the semaphore value.  
 * - If a process is blocked on the semaphore, it is removed from the ASL.  
 * - The unblocked process is added to the Ready Queue for execution.  
 *  
 * 
 * @param int *sem - Pointer to the semaphore to be incremented.  
 *  
 * @return None  
 *****************************************************************************/
pcb_PTR verhogen(int *sem) {
	pcb_PTR p = NULL;
    (*sem)++; /* Increment the semaphore value, signaling that a resource is available */
    /* Check if there were processes blocked on this semaphore */

    if (*sem <= 0) { 
        p = removeBlocked(sem); /* Unblock the first process waiting on this semaphore */
		if (p != NULL){insertProcQ(&ReadyQueue,p);}    /* Add the unblocked process to the Ready Queue */
    }
    return p; /*return pointer to unblocked process pcb*/
}

/****************************************************************************  
 * waitForIO(int lineNum, int deviceNum, int readBool) - SYS5  
 *  
 * 
 * @brief  
 * Blocks a process until an I/O operation on a specific device completes.  
 * 
 *  
 * @details  
 * - Computes the semaphore index for the given device.  
 * - If the device is a terminal, determines if it's a read or write request.  
 * - The process performs a P operation on the device semaphore.  
 * - The process is blocked and inserted into the ASL.  
 * - The scheduler switches to another process while waiting for I/O completion.  
 * - Once the I/O operation completes, the process is unblocked and resumes execution.  
 * 
 * @note
 *  Many I/O devices can cause a process to be blocked while waiting for I/O to complete.
 *  Each interrupt line (3–7) has up to 8 devices, requiring us to compute `semIndex` to find the correct semaphore.
 *  Terminal devices (line 7) have two independent sub-devices: read (input) and write (output), requiring an extra adjustment (add 8 to semIndex)
 * 
 * @param int lineNum - The interrupt line number (3-7).  
 * @param int deviceNum - The device number (0-7).  
 * @param int readBool - 1 if waiting for a read operation, 0 if writing.  
 *  
 * @return None  
 *****************************************************************************/
void waitForIO(int lineNum, int deviceNum, int readBool) {
	softBlockCnt++;  /*Increment count of soft-blocked (waiting) processes*/
    
    /*Save the current process state from the BIOS Data Page*/
    currProc->p_s = *((state_t *) BIOSDATAPAGE);

    int semIndex;  /*This will hold the index into the deviceSemaphores array*/

    /*For device interrupts (assumed to be in the range [DISKINT, ...]),*/
    /*compute semIndex based on lineNum*/
    if (lineNum >= 3 && lineNum <= 6) {
        semIndex = lineNum - OFFSET;  
    }
    /*If lineNum corresponds to a terminal interrupt (e.g., TERMINT == 7),*/
    /*choose the semaphore index based on the readBool flag:*/
    /* - If readBool is true, use index 4.*/
    /* - Otherwise, use index 5.*/
    else if (lineNum == 7) {
        semIndex = (readBool) ? 4 : 5;
    }
    /*If the lineNum doesn't match expected values, terminate the process.*/
    else {
        terminateProcess();
        return;
    }
	int index = semIndex * DEVPERINT + deviceNum;
    /*Perform the "passeren" (P or wait) operation on the chosen device semaphore.*/
    passeren(&deviceSemaphores[index]);
}

/****************************************************************************  
 * getCPUTime() - SYS6  
 *  
 * @brief  
 * Returns the total CPU time used by the calling process.  
 *  
 * @details  
 * - The process's CPU time is stored in its PCB (p_time).  
 * - The function reads the current Time of Day (TOD) clock and calculates  
 *   the time since the process last started executing.  
 * - The result is returned in the v0 register.  
 *  
 * @return None (CPU time is stored in v0).  
 *****************************************************************************/
void getCPUTime(state_t *savedState){
	cpu_t totalTime;
    totalTime = currProc->p_time + get_elapsed_time();
	savedState->s_v0 = totalTime;
    currProc->p_s.s_v0 = totalTime;
}


/****************************************************************************  
 * waitForClock() - SYS7  
 *  
 * @brief  
 * Blocks the calling process until the next interval timer interrupt occurs.  
 *  
 * @details  
 * - Performs a P operation on the clock semaphore (decrement semaphore by 1)  
 * - The process is inserted into the ASL and blocked.  
 * - The scheduler switches to another process.  
 * - The blocked process will be unblocked at the next interval timer interrupt.  
 *  
 * @return None  
 *****************************************************************************/
void waitForClock() {
	softBlockCnt++;
	passeren(&semIntTimer);
}

/****************************************************************************  
 * getSupportData() - SYS8  
 *  
 * @brief  
 * Retrieves the support structure pointer for the calling process.  
 *  
 * @details  
 * - The support_t structure contains exception-handling information for user-mode processes.  
 * - The pointer to this structure is stored in the process's PCB (p_supportStruct).  
 * - This function simply returns the pointer in v0.  
 *  
 * @return None (Support structure pointer is stored in `v0`).  
 *****************************************************************************/
void getSupportData(state_t *savedState) {
	savedState->s_v0 = (int) currProc->p_supportStruct;
}



/****************************************************************************  
 * exceptionPassUpHandler()  
 *  
 * @brief  
 * Implements the pass up or die mechanism, which means we handle exceptions 
 * by either passing control to the user-level exception handler  
 * (if one is defined) or terminating the process if no handler exists.  
 *  
 * @details  
 * - If the current process has a support structure, the exception is "passed up"  
 *   to the corresponding user-defined handler.  
 * - The function copies the saved processor state from the BIOS Data Page into  
 *   the appropriate exception state field of the process's support structure.  
 * - CPU time used up to the exception is recorded and charged to the process.  
 * - The process context is then switched to the user-level exception handler.  
 * - If the process does not have a support structure, it is terminated  
 *   along with any of its child processes.  
 *  
 * @param exceptionCode - The type of exception that occurred (e.g., TLB, SYSCALL, Program Trap).  
 *  
 * @return None (This function either transfers control to the user-level handler  
 *               or terminates the process and schedules another one).  
 *****************************************************************************/
HIDDEN void exceptionPassUpHandler(int exceptionCode) {
	/*If current process has a support structure -> pass up exception to the exception handler */
	if (currProc->p_supportStruct != NULL){
		copyState(((state_t *) BIOSDATAPAGE),&(currProc->p_supportStruct->sup_exceptState[exceptionCode]));
		context_t *ctx = &(currProc->p_supportStruct->sup_exceptContext[exceptionCode]);
		LDCXT(ctx->c_stackPtr, ctx->c_status, ctx->c_pc);
	}
	/* No user-level handler defined, so terminate the process */
	else{
		terminateProcess();
	}
}


/****************************************************************************  
 * prgmTrapHandler()  
 *  
 * @brief  
 * Handles program-related exceptions, including illegal operations,  
 * invalid memory accesses, and arithmetic errors.  
 *  
 * @details  
 * - This function is called when a General Exception occurs.  
 * - Instead of resolving the exception itself, it delegates the task to  
 *   exceptionPassUpHandler(), which determines if the process has a  
 *   user-defined exception handler using the GENERALEXCEPT index value  
 * - If a handler is available, the exception is passed up for processing.  
 * - If no handler exists, the process is terminated.  
 *  
 * @return None (This function does not return, as it either transfers control  
 *               to a user-defined handler or terminates the process).  
 *****************************************************************************/  
void prgmTrapHandler() {
	exceptionPassUpHandler(GENERALEXCEPT);
}

/****************************************************************************  
 * tlbTrapHandler()  
 *  
 * 
 * @brief  
 * Handles TLB exceptions, typically triggered when a process accesses an 
 * invalid or unmapped virtual address.  
 *  
 * 
 * @details  
 * - This function is called when a Page Fault Exception occurs.  
 * - Instead of handling the exception directly, it delegates the handling  
 *   to exceptionPassUpHandler(), which determines whether the process  
 *   has a user-defined exception handler using the PGFAULTEXCEPT index value
 * - If a handler exists, the exception state is passed up to user space.  
 * - If no handler is present, the process is terminated.  
 *  
 * 
 * @return None  
 *****************************************************************************/ 
void tlbTrapHanlder() {
	exceptionPassUpHandler(PGFAULTEXCEPT);
}


/****************************************************************************  
 * sysTrapHandler()  
 *  
 * @brief  
 * Handles system call (SYSCALL) exceptions by determining the system call number  
 * and executing the corresponding system call function.  
 *  
 * 
 * @details  
 * - Retrieves the saved processor state from the BIOS data page to analyze the exception.  
 * - Extracts the system call number from register a0 to determine the requested SYSCALL.  
 * - Increments the program counter (PC) to prevent infinite loops.  
 * - Checks if the SYSCALL was made in user mode:  
 *   - If true, it is considered an illegal instruction and is handled as a program trap.  
 * - Validates the system call number:  
 *   - If it is not between SYS1 and SYS8, it is treated as a program trap.  
 * - If valid, updates the current process state and executes the corresponding system call.  
 *  
 * @note  
 * SYSCALL requests should only be made in kernel mode. If a user-mode process attempts  
 * a system call, the process is terminated via the program trap handler.  
 *  
 * @return None  
 *****************************************************************************/  
void sysTrapHandler() {
	/*Retrieve saved processor state (located at start of the BIOS Data Page) & extract the syscall number to find out which type of exception was raised*/
	state_t *savedState = (state_t *)BIOSDATAPAGE;
	syscallNo = savedState->s_a0;
	unsigned int reg_a1 = savedState->s_a1;
	unsigned int reg_a2 = savedState->s_a2;
	unsigned int reg_a3 = savedState->s_a3;

	/*Increment PC by 4 avoid infinite loops*/
    savedState->s_pc = savedState->s_pc + WORDLEN;

	/*If request to syscalls 1-8 is made in user-mode will trigger program trap exception response*/
    if (((savedState->s_status) & USERPON) != ALLOFF){
        savedState->s_cause = (savedState->s_cause) & 0xFFFFFF28; /* Set exception cause to Reserved Instruction */
        prgmTrapHandler();  /* Handle it as a Program Trap */
    }

	/*Validate syscall number (must be between SYS1NUM and SYS8NUM) */
    if ((syscallNo < 1) || (syscallNo > 8)) {  
        exceptionPassUpHandler(GENERALEXCEPT);  /* Invalid syscall, try pass up or die to see if we can handle it */
    }
	unsigned int kup_check = ((savedState->s_status) & 0x00000008) >> 3; /*KUp bit, which checks whether the process is in user or kernel mode*/
	/*Phase 2 requires one to be in kernel mode*/

	/*If we're in kernel mode -> safe to proceed*/
	if (kup_check == 0) {
		switch (syscallNo) {
		case SYS1:
			createProcess((state_t *) reg_a1, (support_t *) reg_a2);
			break;
		case SYS2:
			terminateProcess();
			break;
		case SYS3:
			passeren((int *) reg_a1);
			break;
		case SYS4:
			verhogen((int *) reg_a1);
			break;
		case SYS5:
			waitForIO(reg_a1, reg_a2, reg_a3);
			break;
		case SYS6:
			getCPUTime(savedState);
			break;
		case SYS7:
			waitForClock();
			break;
		case SYS8:
			getSupportData(savedState);
			break;
		default:
			terminateProcess();
			break;
		}
	
    /* 
     * After handling the system call, check if there is a current process.
     * If no process is available (i.e., currProc is NULL), switch to the next available process.
     * Otherwise, load the processor state from the saved state to resume execution.
     */
	if (currProc == NULL)
		switchProcess();
	else
		LDST(savedState);
	}
	else {
	/* 
     * If kup_check is not 0, the process is not in kernel mode
     * which is an error condition since privileged system calls must be executed in kernel mode.
     */
		savedState->s_cause &= ~GETEXECCODE; /* Clear the current exception code bits from the cause field */
		savedState->s_cause |= 10 << CAUSESHIFT; /* Set the new cause by shifting the exception code into place */
		prgmTrapHandler(); /*Handle as program trap*/
	}
}

/****************************************************************************  
 * gen_exception_handler()  
 *  
 * @brief  
 * Handles all general exceptions that occur in the system by determining  
 * the exception type and delegating it to the appropriate handler.  
 *  
 * @details  
 * - The function retrieves the saved processor state from the BIOS data page.  
 * - It extracts the exception code from the cause register to identify  
 *   the type of exception that occurred.  
 * - Based on the exception type, it redirects control to the corresponding  
 *   handler function:  
 *     - Interrupts (Code 0) → interruptsHandler() (handles device I/O)  
 *     - TLB Exceptions (Codes 1-3) → tlbTrapHandler() (handles memory page faults)  
 *     - System Calls (Code 8) → sysTrapHandler() (handles user mode system calls)  
 *     - Program Traps (Codes 4-7) → prgmTrapHandler() (handles invalid operations)  
 *  
 * @note  
 * This function does not return to the caller. Instead, control is  
 * passed to the appropriate exception handler. If an exception occurs  
 * in a process without an appropriate handler, the process is terminated.  
 *  
 * @return None  
 *****************************************************************************/
void gen_exception_handler()
{	
	state_t *saved_state; /* Pointer to the saved processor state at time of exception */  
    int exception_code; /* Stores the extracted exception type */  

    saved_state = (state_t *) BIOSDATAPAGE;  /* Retrieve the saved processor state from BIOS data page */
    exception_code = ((saved_state->s_cause) & GETEXCPCODE) >> CAUSESHIFT; /* Extract exception code from the cause register */

	if (exception_code == 0) {  
        /* Case 1: Exception Code 0 - Device Interrupt */
        interruptsHandler();  /* call the Nucleus' device interrupt handler function */
    }  
    else if ((exception_code <= 3) && (exception_code >= 1)) {  
        /* Case 2: Exception Codes 1-3 - TLB Exceptions */
        tlbTrapHanlder();  /* call the Nucleus' TLB exception handler function */
    }  
    else if (exception_code == 8) {  
        /* Case 3: Exception Code 8 - System Calls */
        sysTrapHandler();  /* call the Nucleus' SYSCALL exception handler function */
    }
    /* Case 4: All Other Exceptions - Program Traps */
    prgmTrapHandler(); /* calling the Nucleus' Program Trap exception handler function because the exception code is not 0-3 or 8*/
}
