/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for deviceSupportDMA.c module
 * 
 ****************************************************************************/
#ifndef DEVICESUPPORTDMA
#define DEVICESUPPORTDMA


#include "../h/types.h"
#include "../h/const.h"

void disk_put(int *logicalAddr, int diskNo, int sectNo, support_t *support_struct); /*sys14*/
void disk_get(int *logicalAddr, int diskNo, int sectNo, support_t *support_struct); /*sys15*/
void flash_put(); /*sys16*/
void flash_get(); /*sys17*/
 


#endif