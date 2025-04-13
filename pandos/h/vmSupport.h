
/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for vmSupport.c module
 * 
 ****************************************************************************/
#ifndef VMSUPPORT
#define VMSUPPORT
#include "../h/types.h"
#include "../h/const.h"
void update_tlb_handler(pte_entry_t *ptEntry);
void initSwapPool();
void init_deviceSema4s();
void tlb_exception_handler();
void flash_read_write(int flashNum, int sector, int buffer, int operation);
extern swap_pool_t swap_pool[POOLSIZE];
#endif