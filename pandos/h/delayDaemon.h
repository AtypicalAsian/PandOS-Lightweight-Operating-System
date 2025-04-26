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
void sys18Handler(int sleepTime, support_t *support_struct); /*function to implement syscall 18*/
void delayDaemon(support_t *currSuppStruct); /*code for delay daemon process*/

void alloc_descriptor(); /*allocate new node for the ADL*/
void remove_descriptor(); /*return a node from the ADL to the free pool (of unsued descriptor nodes)*/
int insertADL(); /*insert new descriptor into Active Delay List (ADL)*/
int removeADL(); /*remove descriptor from Active Delay List (ADL)*/

#endif