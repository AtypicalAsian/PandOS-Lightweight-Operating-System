#ifndef INTERRUPTS
#define INTERRUPTS

/**************************************************************************** 
 * Nicolas & Tran
 * Declaration file for interrupts handler module
 ****************************************************************************/
#include "../h/types.h"

#define GETIP   0x0000FE00
#define IPSHIFT 8
#define TERMSTATUSMASK 0x000000FF

#define DEVREGADDR ((devregarea_t *)RAMBASEADDR)
void interruptsHandler(state_t *exceptionState);

#endif