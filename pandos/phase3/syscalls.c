#include <umps3/umps/libumps.h>

#include "../h/const.h"

#include "../h/asl.h"
#include "../h/pcb.h"

#include "../h/exceptions.h"
#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/syscalls.h"

HIDDEN void termProcRecursive(pcb_t *p);


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