#ifndef VMSUPPORT
#define VMSUPPORT

#include "const.h"
#include "types.h"

extern swap_t swapPool[POOLSIZE];

void updateTLB(pte_entry_t *updatedEntry);
void initSwapPool();
void pager();

#endif