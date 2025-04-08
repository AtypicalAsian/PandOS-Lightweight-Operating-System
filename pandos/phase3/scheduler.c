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

void scheduler() {
	if (emptyProcQ(ready_tp)) {
		if (process_count == 0) {
			HALT();
		}

		if (process_count > 0 && soft_block_count > 0) {
			unsigned int status = getSTATUS();
            setTIMER(TICKCONVERT(MAXPLT));
            setSTATUS((status) | IECON | IMON);

			WAIT();

			setSTATUS(status);
		}

		if (process_count > 0 && soft_block_count == 0) {
			PANIC();
		}
	}

	current_proc = removeProcQ(&ready_tp);

	setTIMER(TICKCONVERT(TIMESLICE));

	STCK(timeSlice);

	LDST(&(current_proc->p_s));
}

