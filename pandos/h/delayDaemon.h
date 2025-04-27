/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for delayDaemon.c module
 * 
 ****************************************************************************/
#ifndef DELAYDAEMON
#define DELAYDAEMON

#include "../h/types.h"
#include "../h/const.h"

void initADL(); /*initialize the Active Delay List (ADL)*/
void sys18Handler(support_t *support_struct); /*function to implement syscall 18*/
void delayDaemon(support_t *currSuppStruct); /*code for delay daemon process*/

delayd_PTR alloc_descriptor(); /*allocate new node for the ADL*/
void return_to_ADL(delayd_PTR delayDescriptor); /*return a node from the ADL to the free pool (of unsued descriptor nodes)*/
int insertADL(); /*insert new descriptor into Active Delay List (ADL)*/
delayd_PTR searchADL(int wakeTime);

#endif