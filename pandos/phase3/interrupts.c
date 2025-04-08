#include "../h/const.h"
#include "../h/types.h"

#include "../h/asl.h"
#include "../h/pcb.h"

#include "../h/exceptions.h"
#include "../h/initial.h"
#include "../h/interrupts.h"
#include "../h/scheduler.h"
#include "../h/syscalls.h"

#include <umps3/umps/libumps.h>


int findIntLine(unsigned int map) {
    int i;
	for (i = 0; i < 32; i++) {
        if (map & (1 << i)) {
            return i;
        }
    }
    PANIC();
    return -1;
}


void unblockLoad(int deviceType, int deviceInstance, unsigned int status) {
	pcb_PTR unblockedProc;

	unblockedProc = verhogen(&(device_sems[deviceType][deviceInstance]));

	if (unblockedProc != NULL) {
		unblockedProc->p_s.s_v0 = status;
		soft_block_count--;
	}
}


void nonTimerInterrupt(int deviceType) {
	int instanceMap = DEVREGADDR->interrupt_dev[deviceType];

	instanceMap &= -instanceMap;
	int deviceInstance = findIntLine(instanceMap);
	unsigned int status;

	if (deviceType == (TERMINT-DISKINT)) {
		device_t *termStatus = &(DEVREGADDR->devreg[deviceType][deviceInstance]);

		if ((termStatus->d_status & TERMSTATUSMASK) == RECVD_CHAR) {
			status = termStatus->d_status;
			DEVREGADDR->devreg[deviceType][deviceInstance].d_command = ACK;
			unblockLoad(deviceType, deviceInstance, status);
		}
		if ((termStatus->d_data0 & TERMSTATUSMASK) == TRANS_CHAR) {
			status = termStatus->d_data0;
			DEVREGADDR->devreg[deviceType][deviceInstance].d_data1 = ACK;
			unblockLoad(deviceType + 1, deviceInstance, status);
		}
	}
	else {
		status = DEVREGADDR->devreg[deviceType][deviceInstance].d_status;
		DEVREGADDR->devreg[deviceType][deviceInstance].d_command = ACK;
		unblockLoad(deviceType, deviceInstance, status);
	}
	if (current_proc == NULL)
		scheduler();
	else
		LDST(EXCSTATE);
}


void pltInterrupt() {
	setTIMER(TICKCONVERT(MAXPLT));
	current_proc->p_s = *EXCSTATE;
	current_proc->p_time += timePassed();
	insertProcQ(&ready_tp, current_proc);
	current_proc = NULL;
	scheduler();
}


void intervalTimerInterrupt() {
	LDIT(INTIMER);
	pcb_t *blockedProcess = NULL;

	while ((blockedProcess = removeBlocked(&IntervalTimerSem)) != NULL) {
		insertProcQ(&ready_tp, blockedProcess);
	}

	soft_block_count += IntervalTimerSem;
	IntervalTimerSem = 0;

	if (current_proc == NULL)
		scheduler();
	else
		LDST(EXCSTATE);
}

void intExceptionHandler(state_t *exceptionState) {
	int pending_int = (exceptionState->s_cause & GETIP);

	pending_int &= -pending_int;

	switch (pending_int) {
	case LOCALTIMERINT:
		pltInterrupt();
		break;
	case TIMERINTERRUPT:
		intervalTimerInterrupt();
		break;
	case DISKINTERRUPT:
	case FLASHINTERRUPT:
	case NETWINTERRUPT:
	case PRINTINTERRUPT:
	case TERMINTERRUPT:
		nonTimerInterrupt(findIntLine(pending_int >> IPSHIFT) - DISKINT);
		break;
	default:
		break;
	}
}