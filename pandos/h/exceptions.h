#ifndef EXCEPTIONS
#define EXCEPTIONS

/**************************************************************************** 
 * Nicolas & Tran
 * The externals declaration file for exceptions.c module
 * 
 * 
 ****************************************************************************/

#include "../h/types.h"

extern void sysTrapHandler();
extern void tlbTrapHanlder();
extern void prgmTrapHandler();
extern void update_pcb_state();
extern void uTLB_RefillHandler();
extern void gen_exception_handler();

#endif