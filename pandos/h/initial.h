#ifndef INITIAL
#define INITIAL

#include "../h/const.h"
#include "../h/types.h"

/**************************************************************************** 
 * Written by: Nicolas & Tran
 * The declaration file for the initial.c module
 ****************************************************************************/
int main(); /*main function which is entrypoint to phase 2*/
extern int procCnt; /*integer indicating the number of started, but not yet terminated processes.*/
extern int softBlockCnt; /*Integer representing the number of started, but not terminated processes that in are the “blocked” state due to an I/O or timer request.*/
extern pcb_PTR ReadyQueue; /*Tail pointer to a queue of pcbs that are in the “ready” state.*/
extern pcb_PTR currProc; /*Pointer to the pcb that is in the “running” state, i.e. the current executing process.*/
extern int deviceSemaphores[DEVICE_TYPES][DEVICE_INSTANCES]; /* semaphore integer array that represents each external (sub) device */
extern int semIntTimer; /* semaphore used by the interval timer (pseudo-clock) for timer-related blocking operations (one extra on top of the deviceSemaphores) */
extern void debug_fxn(int i, int p1, int p2, int p3);
void populate_passUpVec(); /*helper method to set up pass up vector*/
void init_proc_state(pcb_PTR firstProc); /*helper method to set up initial state of first proccess*/
#endif