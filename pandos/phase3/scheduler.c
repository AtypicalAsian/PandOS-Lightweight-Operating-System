#include "../h/types.h"
#include "../h/const.h"

#include "../h/pcb.h"
#include "../h/asl.h"

#include "../h/scheduler.h"
#include "../h/initial.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"

#include "/usr/include/umps3/umps/libumps.h"

volatile cpu_t timeSlice;

void switchProcess() {
	if (emptyProcQ(ReadyQueue)) {
		if (procCnt == 0) {
			HALT();
		}

		if (procCnt > 0 && softBlockCnt > 0) {
			unsigned int status = getSTATUS();
            setTIMER(TICKCONVERT(MAXPLT));
            setSTATUS((status) | IECON | IMON);

			WAIT();

			setSTATUS(status);
		}

		if (procCnt > 0 && softBlockCnt == 0) {
			PANIC();
		}
	}

	currProc = removeProcQ(&ReadyQueue);

	setTIMER(TICKCONVERT(TIMESLICE));

	STCK(timeSlice);

	LDST(&(currProc->p_s));
}

