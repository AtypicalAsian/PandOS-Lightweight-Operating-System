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
void disk_get(support_t *support_struct); /*sys15 - disk READ*/
void flash_put(support_t *suppStruct); /*sys16 - flash WRITE*/
void flash_get(support_t *suppStruct); /*sys17 - flash READ*/
 


#endif