/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for delayDaemon.c module
 * 
 ****************************************************************************/
#ifndef DELAYDAEMON
#define DELAYDAEMON

#include "../h/types.h"
#include "../h/const.h"

extern int delaySemaphore;
extern delayd_PTR delaydFree;
extern delayd_PTR delaydFree_h;

void initADL();
void delayCurrentProc(support_t *current_support);
int insertDelayNode(support_t *current_support, int sleepTime);
void delayDaemon();
delayd_PTR activateASL();
void freeNode(delayd_PTR delayEvent);

#endif