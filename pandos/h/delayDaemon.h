/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for delayDaemon.c module
 * 
 ****************************************************************************/
#ifndef DELAYDAEMON
#define DELAYDAEMON

#include "../h/types.h"
#include "../h/const.h"

extern int delayDaemon_sema4;
extern delayd_PTR delayd_h;
extern delayd_PTR delaydFree_h;

void initADL(); /*initialize the Active Delay List (ADL)*/
void sys18Handler(int sleep_time, support_t *support_struct); /*function to implement syscall 18*/
void delayDaemon(); /*code for delay daemon process*/

delayd_PTR alloc_descriptor(); /*allocate new node for the ADL*/
void free_descriptor(delayd_PTR delayDescriptor); /*remove a node from the ADL and return it to the free pool (of unsued descriptor nodes)*/
int insertADL(int time_asleep, support_t *supStruct); /*insert new descriptor into Active Delay List (ADL)*/
delayd_PTR find_insert_position(int wakeTime); /*helper method to find insert position for ADL*/
void removeADL(int currTime); /*remove all procs that need to be woken up at currTime from ADL*/

#endif