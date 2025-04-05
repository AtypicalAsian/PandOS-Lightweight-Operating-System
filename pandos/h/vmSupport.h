/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for vmSupport.c module
 * 
 ****************************************************************************/

#ifndef VMSUPPORT
#define VMSUPPORT
#include "../h/types.h"
#include "../h/const.h"

extern void tlb_exception_handler(); /*TLB exception event handler or the pager*/
extern void init_swap_structs(); /*initialize the swap pool and accompanying sempahore*/
extern void tlb_refill_handler(); /*TLB refill event handler*/
extern void update_tlb_hanlder(pte_entry_t *new_page_table_entry);

extern swap_pool_t swap_pool[MAXFRAMES];
#endif