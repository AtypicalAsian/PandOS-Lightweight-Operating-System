#ifndef SCHEDULER
#define SCHEDULER


/**************************************************************************** 
 * Written by: Nicolas & Tran
 * The declaration file for the scheduler.c module
 ****************************************************************************/
extern void switchProcess();
extern void copyState(state_PTR src, state_PTR dst);
extern void swContext(pcb_PTR curr_process);


#endif