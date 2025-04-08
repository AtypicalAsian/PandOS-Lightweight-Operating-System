

#include "../h/const.h"

#include "../h/exceptions.h"
#include "../h/initial.h"
#include "../h/interrupts.h"
#include "../h/scheduler.h"
#include "../h/pcb.h"
#include "../h/asl.h"

#include <umps3/umps/libumps.h>

HIDDEN void termProcRecursive(pcb_t *p);

cpu_t timePassed() {
	volatile cpu_t clockTime;

	STCK(clockTime);
	return clockTime - timeSlice;
}

HIDDEN void passUpOrDie(int index) {
	support_t *supportStructure = current_proc->p_supportStruct;
	if (supportStructure == NULL) {
		terminateProc();
	}
	else {
		supportStructure->sup_exceptState[index] = *EXCSTATE;
		context_t *context = &(supportStructure->sup_exceptContext[index]);
		LDCXT(context->c_stackPtr, context->c_status, context->c_pc);
	}
}

void* memcpy(void *dest, const void *src, size_t len) {
	char *d = dest;
	const char *s = src;

	while (len--) {
		*d++ = *s++;
	}
	return dest;
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
				createProc((state_t *) arg1, (support_t *) arg2);
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
			if (current_proc == NULL)
				scheduler();
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


void createProc(state_t * statep, support_t * supportp) {
	pcb_PTR newProc = allocPcb();
	unsigned int retValue = -1;

	if (newProc != NULL) {
		process_count++;
		newProc->p_supportStruct = supportp;
		newProc->p_s = *statep;
		insertChild(current_proc, newProc);
		insertProcQ(&ready_tp, newProc);
		retValue = 0;
	}
	EXCSTATE->s_v0 = retValue;
}

void terminateProc() {
	outChild(current_proc);
	termProcRecursive(current_proc);

	current_proc = NULL;

	scheduler();
}


HIDDEN void termProcRecursive(pcb_t *p) {
	pcb_PTR child;

	while ((child = removeChild(p)) != NULL) {
		outProcQ(&ready_tp, child);
		termProcRecursive(child);
	}


	bool blockedOnDevice =
		(p->p_semAdd >= (int *) device_sems &&
		 p->p_semAdd <
		 ((int *) device_sems +
		  (sizeof(int) * DEVICE_TYPES * DEVICE_INSTANCES)))
		|| (p->p_semAdd == (int *) &IntervalTimerSem);

	pcb_PTR removedPcb = outBlocked(p);

	if (!blockedOnDevice && removedPcb != NULL) {
		(*(p->p_semAdd))++;
	}

	freePcb(p);
	process_count--;
}


void passeren(int *semAdd) {
	(*semAdd)--;

	if (*semAdd < 0) {
		current_proc->p_s = *EXCSTATE;
		current_proc->p_time += timePassed();
		insertBlocked((int *) semAdd, current_proc);
		current_proc = NULL;
		scheduler();
	}
}


pcb_PTR verhogen(int *semAdd) {
	(*semAdd)++;

	pcb_PTR unblockedProcess = NULL;

	if (*semAdd <= 0) {

		unblockedProcess = removeBlocked(semAdd);
		if (unblockedProcess != NULL) {
			insertProcQ(&ready_tp, unblockedProcess);
		}
	}
	return unblockedProcess;
}


void waitIO(int intLine, int deviceNum, bool waitForTermRead) {
	
	current_proc->p_s = *EXCSTATE;
	soft_block_count++;

	switch (intLine) {
	case DISKINT:
	case FLASHINT:
	case NETWINT:
	case PRNTINT:
		passeren(&device_sems[intLine - DISKINT][deviceNum]);
		break;
	case TERMINT:
		if (waitForTermRead)
			passeren(&device_sems[4][deviceNum]);
		else
			passeren(&device_sems[5][deviceNum]);
		break;
	default:
		terminateProc();
		break;
	}
}



void cpuTime(cpu_t *resultAddress) {
	*resultAddress = current_proc->p_time + timePassed();
}


void waitClk() {
	soft_block_count++;
	passeren(&IntervalTimerSem);
}


void getSupportData(support_t **resultAddress) {
	*resultAddress = current_proc->p_supportStruct;
}
