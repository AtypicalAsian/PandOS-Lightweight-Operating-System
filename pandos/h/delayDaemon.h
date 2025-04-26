/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for delayDaemon.c module
 * 
 ****************************************************************************/
#ifndef DELAYDAEMON
#define DELAYDAEMON

 #include "../h/types.h"
#include "../h/const.h"

void alloc_descriptor(); /*allocate new node for the ADL*/
void free_descriptor(); /*return a node from the ADL to the free pool (of unsued descriptor nodes)*/
void initADL(); /*initialize the Active Delay List (ADL)*/
int insertADL(); /*insert new descriptor into Active Delay List (ADL)*/
void removeADL(); 
void delay_syscallHandler(support_t *currSuppStruct); /*implements delay facility*/



#endif