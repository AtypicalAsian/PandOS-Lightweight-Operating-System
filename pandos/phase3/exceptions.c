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
HIDDEN void termProcRecursive(pcb_t *p);


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

HIDDEN void passUpOrDie(int index) {
	support_t *supportStructure = currProc->p_supportStruct;
	if (supportStructure == NULL) {
		terminateProc();
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
				terminateProc();
				break;
			case PASSEREN:
				passeren((semaphore *) arg1);
				break;
			case VERHOGEN:
				verhogen((semaphore *) arg1);
				break;
			case WAITIO:
				waitIO(arg1, arg2, arg3);
				break;
			case GETTIME:
				cpuTime((cpu_t *) resultAddress);
				break;
			case CLOCKWAIT:
				waitClk();
				break;
			case GETSUPPORTPTR:
				getSupportData((support_t **) resultAddress);
				break;
			default:
				terminateProc();
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


void terminateProc() {
	outChild(currProc);
	termProcRecursive(currProc);

	currProc = NULL;

	switchProcess();
}


HIDDEN void termProcRecursive(pcb_t *p) {
	pcb_PTR child;

	while ((child = removeChild(p)) != NULL) {
		outProcQ(&ReadyQueue, child);
		termProcRecursive(child);
	}


	bool blockedOnDevice =
		(p->p_semAdd >= (int *) deviceSemaphores &&
		 p->p_semAdd <
		 ((int *) deviceSemaphores +
		  (sizeof(int) * DEVICE_TYPES * DEVICE_INSTANCES)))
		|| (p->p_semAdd == (int *) &semIntTimer);

	pcb_PTR removedPcb = outBlocked(p);

	if (!blockedOnDevice && removedPcb != NULL) {
		(*(p->p_semAdd))++;
	}

	freePcb(p);
	procCnt--;
}


void passeren(int *semAdd) {
	(*semAdd)--;

	if (*semAdd < 0) {
		currProc->p_s = *EXCSTATE;
		currProc->p_time += timePassed();
		insertBlocked((int *) semAdd, currProc);
		currProc = NULL;
		switchProcess();
	}
}


pcb_PTR verhogen(int *semAdd) {
	(*semAdd)++;

	pcb_PTR unblockedProcess = NULL;

	if (*semAdd <= 0) {

		unblockedProcess = removeBlocked(semAdd);
		if (unblockedProcess != NULL) {
			insertProcQ(&ReadyQueue, unblockedProcess);
		}
	}
	return unblockedProcess;
}


void waitIO(int intLine, int deviceNum, bool waitForTermRead) {
	
	currProc->p_s = *EXCSTATE;
	softBlockCnt++;

	switch (intLine) {
	case DISKINT:
	case FLASHINT:
	case NETWINT:
	case PRNTINT:
		passeren(&deviceSemaphores[intLine - DISKINT][deviceNum]);
		break;
	case TERMINT:
		if (waitForTermRead)
			passeren(&deviceSemaphores[4][deviceNum]);
		else
			passeren(&deviceSemaphores[5][deviceNum]);
		break;
	default:
		terminateProc();
		break;
	}
}



void cpuTime(cpu_t *resultAddress) {
	*resultAddress = currProc->p_time + timePassed();
}


void waitClk() {
	softBlockCnt++;
	passeren(&semIntTimer);
}


void getSupportData(support_t **resultAddress) {
	*resultAddress = currProc->p_supportStruct;
}
