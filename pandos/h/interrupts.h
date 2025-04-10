#ifndef INTERRUPTS
#define INTERRUPTS

/**************************************************************************** 
 * Nicolas & Tran
 * Declaration file for interrupts handler module
 ****************************************************************************/
#include "../h/types.h"
void interruptsHandler(state_t *exceptionState);
#define TERMSTATUSMASK 0x000000FF
#define DEVREGADDR ((devregarea_t *)RAMBASEADDR)
#define GETIP   0x0000FE00
#define IPSHIFT 8


#endif