/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for initProc.c module
 * 
 ****************************************************************************/
#ifndef INITPROC
#define INITPROC


#include "../h/types.h"
#include "../h/const.h"

extern int masterSema4; /* A Support Level semaphore used to ensure that test() terminates gracefully by calling HALT() instead of PANIC() */
extern void test(); 
void deallocate(support_t* supportStruct);
support_t* allocate();
 
#endif