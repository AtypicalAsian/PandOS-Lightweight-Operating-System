/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for vmSupport.c module
 * 
 ****************************************************************************/

#ifndef VMSUPPORT
#define VMSUPPORT
#include "../h/types.h"
#include "../h/const.h"

void init_swap();

void TLB_exceptionHandler();

void killProc(int *sem);

void clearSwap(int asid);

void updateTLB(pte_entry_t *updatedEntry);

int findReplace();

void writeReadFlash(unsigned int RoW, unsigned int devNumber, unsigned int occupiedPageNumber, int swap_pool_index);

void dirtyPage(unsigned int currASID, unsigned int occupiedPageNumber, unsigned int swap_pool_index);

pte_entry_t *findEntry(unsigned int pageNumber);

void uTLB_refillHandler();
#endif