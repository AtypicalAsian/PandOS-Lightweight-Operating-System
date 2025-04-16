/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for initProc.c module
 * 
 ****************************************************************************/
#ifndef INITPROC
#define INITPROC


#include "../h/types.h"
#include "../h/const.h"

void deallocate(support_t* supportStruct);
support_t* allocate();
void initSuppPool();
void init_base_state(state_t *base_state);
void summon_process(int process_id,state_t *base_state);
extern void test(); 

extern int masterSema4; /* A Support Level semaphore used to ensure that test() terminates gracefully by calling HALT() instead of PANIC() */


#endif