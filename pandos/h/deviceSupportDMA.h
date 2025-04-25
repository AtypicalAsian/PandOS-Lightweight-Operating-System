/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for deviceSupportDMA.c module
 * 
 ****************************************************************************/
#ifndef DEVICESUPPORTDMA
#define DEVICESUPPORTDMA


#include "../h/types.h"
#include "../h/const.h"

void disk_put(memaddr *logicalAddr, int diskNo, int sectNo, support_t *support_struct); /*sys14 - disk WRITE*/
void disk_get(memaddr *logicalAddr, int diskNo, int sectNo, support_t *support_struct); /*sys15 - disk READ*/
void flash_put(memaddr *logicalAddr, int flashNo, int blockNo, support_t *support_struct); /*sys16 - flash WRITE*/
void flash_get(memaddr *logicalAddr, int flashNo, int blockNo, support_t *support_struct); /*sys17 - flash READ*/
int flashOperation(memaddr *logicalAddr, int flashNo, int blockNo, int operation, support_t *support_struct) /*Helper method for flash_get and flash_put*/
 


#endif