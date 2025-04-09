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
cpu_t timePassed() {
	volatile cpu_t clockTime;
	STCK(clockTime);
	return clockTime - quantum;
}

void* memcpy(void *dest, const void *src, size_t len) {
	char *d = dest;
	const char *s = src;

	while (len--) {
		*d++ = *s++;
	}
	return dest;
}

void blockCurrProc(int *sem){
	currProc->p_s = *((state_t *) BIOSDATAPAGE);
	currProc->p_time += timePassed();
	insertBlocked((int *) sem, currProc);
	currProc = NULL;
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
	   bool blockedOnDevice = 
	   ((processSem >= (int *) deviceSemaphores) &&
		(processSem < ((int *) deviceSemaphores + (sizeof(int) * DEVICE_TYPES * DEVICE_INSTANCES))))
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
void terminateProcess() {
    outChild(currProc);
	recursive_terminate(currProc);
	currProc = NULL;
	switchProcess();
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
    /*compute semIndex based on lineNum. For example, if DISKINT is defined as 3,*/
    /*lineNum values 3-6 correspond to indices 0-3*/
    if (lineNum >= 3 && lineNum <= 6) {
        semIndex = lineNum - DISKINT;  
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

    /*Perform the "passeren" (P or wait) operation on the chosen device semaphore.*/
    passeren(&deviceSemaphores[semIndex][deviceNum]);
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
void getCPUTime(){
	currProc->p_s.s_v0 = currProc->p_time + timePassed();
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
void getSupportData() {
	currProc->p_s.s_v0 = (int) (currProc->p_supportStruct);
}




HIDDEN void passUpOrDie(int index) {
	support_t *supportStructure = currProc->p_supportStruct;
	if (supportStructure == NULL) {
		terminateProcess();
	}
	else {
		supportStructure->sup_exceptState[index] = *EXCSTATE;
		context_t *context = &(supportStructure->sup_exceptContext[index]);
		LDCXT(context->c_stackPtr, context->c_status, context->c_pc);
	}
}

HIDDEN void TLBExceptionHandler() {
	passUpOrDie(PGFAULTEXCEPT);
}


HIDDEN void trapHandler() {
	passUpOrDie(GENERALEXCEPT);
}


HIDDEN void syscallHandler(unsigned int KUp) {
	volatile unsigned int sysId = EXCSTATE->s_a0;

	volatile unsigned int arg1 = EXCSTATE->s_a1;
	volatile unsigned int arg2 = EXCSTATE->s_a2;
	volatile unsigned int arg3 = EXCSTATE->s_a3;
	memaddr resultAddress = (memaddr) &(EXCSTATE->s_v0);

	if (sysId <= 8) {

		if (KUp == 0) {
			EXCSTATE->s_pc += WORDLEN;
			switch (sysId) {
			case CREATEPROCESS:
				createProcess((state_t *) arg1, (support_t *) arg2);
				break;
			case TERMINATEPROCESS:
				terminateProcess();
				break;
			case PASSEREN:
				passeren((semaphore *) arg1);
				break;
			case VERHOGEN:
				verhogen((semaphore *) arg1);
				break;
			case WAITIO:
				waitForIO(arg1, arg2, arg3);
				break;
			case GETTIME:
				getCPUTime();
				break;
			case CLOCKWAIT:
				waitForClock();
				break;
			case GETSUPPORTPTR:
				getSupportData();
				break;
			default:
				terminateProcess();
				break;
			}
			if (currProc == NULL)
			switchProcess();
			else
				LDST(EXCSTATE);
		}
		else {

			EXCSTATE->s_cause &= ~GETEXECCODE;
			EXCSTATE->s_cause |= EXCODESHIFT << CAUSESHIFT;
			trapHandler();
		}
	}
	else {
		passUpOrDie(GENERALEXCEPT);
	}
}


void exceptionHandler()
{

    int exc_code = EXCCODE(EXCSTATE->s_cause);
    if (exc_code == 0) {
        intExceptionHandler(EXCSTATE);
    } else if (((exc_code >= 1) && (exc_code <= TLBS))) {
        TLBExceptionHandler();
    } else if (exc_code == 8) {
		unsigned int KUp = KUP(EXCSTATE->s_status);
        syscallHandler(KUp);
    } else {
        trapHandler();
    }
}
