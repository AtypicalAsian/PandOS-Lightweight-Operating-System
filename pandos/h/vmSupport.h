
/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for vmSupport.c module
 * 
 ****************************************************************************/
#ifndef VMSUPPORT
#define VMSUPPORT
#include "../h/types.h"
#include "../h/const.h"
void initSwapStructs(); /*init swap pool, device semaphores + swap pool semaphore*/
int find_frame_swapPool(); /*page replacement*/
void update_tlb_handler(pte_entry_t *ptEntry); /*maintain TLB and page table consistency*/
void flash_read_write(int deviceNum, int block_num, int op_type, int frame_dest); /*write or read to flash device (backing store)*/
void uTLB_RefillHandler();
void tlb_exception_handler();
extern swap_pool_t swap_pool[SWAP_POOL_CAP]; /*declare swap pool*/
#endif