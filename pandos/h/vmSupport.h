#ifndef VMSUPPORT
#define VMSUPPORT

#include "const.h"
#include "types.h"

extern swap_t swapPool[POOLSIZE];

void updateTLB(pte_entry_t *updatedEntry);
void initSwapPool();
void init_deviceSema4s();
void pager();
int flashOp(int flashNum, int sector, int buffer, int operation);

#endif