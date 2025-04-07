/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for vmSupport.c module
 * 
 ****************************************************************************/

#ifndef VMSUPPORT
#define VMSUPPORT
#include "../h/types.h"
#include "../h/const.h"

void init_swap_structs();
void uTLB_RefillHandler();
void nuke_til_it_glows(int *semaphore);
void reset_swap(int asid);
void update_tlb_handler(pte_entry_t *new_page_table_entry);
int find_frame_swapPool();
void flash_read_write(unsigned int read_write_bool, unsigned int device_no, unsigned int occp_pageNo, int swap_pool_index);
void occupied_handler(unsigned int asid, unsigned int occp_pageNo, unsigned int swap_pool_index);
void tlb_exception_handler(); 
#endif