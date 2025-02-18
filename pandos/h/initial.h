#ifndef INITIAL
#define INITIAL

#include "../h/const.h"
#include "../h/types.h"
/**************************************************************************** 
 * Written by: Nicolas & Tran
 * The declaration file for the initial.c module
 ****************************************************************************/

extern int procCnt;
extern int softBlockCnt;
extern pcb_PTR currProc;
extern pcb_PTR ReadyQueue;
extern cpu_t start_tod;
extern int deviceSemaphores[MAXDEVICECNT];


#endif