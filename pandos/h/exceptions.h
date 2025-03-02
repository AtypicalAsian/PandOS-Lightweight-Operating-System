#ifndef EXCEPTIONS
#define EXCEPTIONS

/**************************************************************************** 
 *
 * The externals declaration file for exceptions.c module
 * 
 * 
 ****************************************************************************/

#include "../h/types.h"

extern void sysTrapHandler();
extern void tlbTrapHanlder();
extern void prgmTrapHandler();
extern void update_pcb_state();

#endif