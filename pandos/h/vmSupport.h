/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for vmSupport.c module
 * 
 ****************************************************************************/

#ifndef VMSUPPORT
#define VMSUPPORT
#include "../h/types.h"
#include "../h/const.h"

extern void tlb_exception_handler(); /*TLB refill event handler*/
extern void init_swap_structs(); /*initialize the swap pool and accompanying sempahore*/

#endif