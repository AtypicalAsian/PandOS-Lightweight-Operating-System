/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for initProc.c module
 * 
 ****************************************************************************/

#ifndef INITPROC
#define INITPROC


#include "../h/types.h"
#include "../h/const.h"

/*extern int masterSema4;*/ /* A Support Level semaphore used to ensure that test() terminates gracefully by calling HALT() instead of PANIC() */
void deallocate(support_t*  toDeallocate);

support_t* allocate();

void init_supLevSem();

void createProc(int id);

void test();


#endif