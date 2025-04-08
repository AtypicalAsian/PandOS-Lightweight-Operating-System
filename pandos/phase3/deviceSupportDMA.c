#include "../h/types.h"
#include "../h/const.h"

#include <umps3/umps/libumps.h>

#include "../h/initProc.h"
#include "../h/sysSupport.h"
#include "../h/vmSupport.h"
#include "../h/deviceSupportDMA.h"

/* 
 * Read or Write Flash Device
 * 
 * Returns:
 *   Device status indicating success or failure.
 */
int flashOp(int flashNum, int sector, int buffer, int operation) {
    unsigned int command;   
    device_t *flashDevice;  
    unsigned int maxBlock;

    
    flashDevice = (device_t *) (DEVICEREGSTART + ((FLASHINT - DISKINT) * (DEVICE_INSTANCES * DEVREGSIZE)) + (flashNum * DEVREGSIZE));
    maxBlock = flashDevice->d_data1; 
    
    if (sector >= maxBlock) {
        terminate(NULL); 
    }
    
    if (operation == FLASHREAD) {
        command = FLASHREAD | (sector << FLASHADDRSHIFT); 
    } else if (operation == FLASHWRITE) {
        command = FLASHWRITE | (sector << FLASHADDRSHIFT); 
    } else {
        return -1; 
    }
   
    flashDevice->d_data0 = buffer;
    setSTATUS(INTSOFF);
    flashDevice->d_command = command;  
    unsigned int deviceStatus = SYSCALL(WAITIO, FLASHINT, flashNum, 0);
    setSTATUS(INTSON);
    return deviceStatus;
}

