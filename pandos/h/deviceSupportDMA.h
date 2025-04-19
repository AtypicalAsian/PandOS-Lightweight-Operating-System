/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for deviceSupportDMA.c module
 * 
 ****************************************************************************/
#ifndef DEVICESUPPORTDMA
#define DEVICESUPPORTDMA


#include "../h/types.h"
#include "../h/const.h"

void disk_put(); /*sys14*/
void disk_get(); /*sys15*/
void flash_put(); /*sys16*/
void flash_get(); /*sys17*/
 


#endif