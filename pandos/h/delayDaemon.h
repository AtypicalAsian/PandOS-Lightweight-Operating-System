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
void sys18Handler(int sleepTime, support_t *support_struct); /*function to implement syscall 18*/
void delayDaemon(); /*code for delay daemon process*/

delayd_PTR alloc_descriptor(); /*allocate new node for the ADL*/
void return_to_ADL(delayd_PTR delayDescriptor); /*return a node from the ADL to the free pool (of unsued descriptor nodes)*/
int insertADL(int time_asleep, support_t *supStruct); /*insert new descriptor into Active Delay List (ADL)*/
delayd_PTR searchADL(int wakeTime);

#endif