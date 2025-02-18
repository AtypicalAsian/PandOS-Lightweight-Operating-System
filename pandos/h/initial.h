#ifndef INITIAL
#define INITIAL

#include "../h/const.h"
#include "../h/types.h"
/**************************************************************************** 
 * Written by: Nicolas & Tran
 * The declaration file for the initial.c module
 ****************************************************************************/

extern int procCnt; /*integer indicating the number of started, but not yet terminated processes.*/
extern int softBlockCnt; /*This integer is the number of started, but not terminated processes that in are the “blocked” state due to an I/O or timer request.*/
extern pcb_PTR ReadyQueue; /*Tail pointer to a queue of pcbs that are in the “ready” state.*/
extern pcb_PTR currProc; /*Pointer to the pcb that is in the “running” state, i.e. the current executing process.*/
// extern cpu_t start_tod; /*the value on the time of day clock that the Current Process begins executing at*/
// extern int deviceSemaphores[MAXDEVICECNT]; /* array of integer semaphores that correspond to each external (sub) device, plus one semd for the Pseudo-clock, located 
// at the last index of the array (PCLOCKIDX). Note that this array will be implemented so that terminal device semaphores are last and terminal device semaphores
// associated with a read operation in the array come before those associated with a write op */
// extern state_PTR savedExceptState; /* a pointer to the saved exception state */

#endif