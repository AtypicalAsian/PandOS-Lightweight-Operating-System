/**************************************************************************************************  
 * @file deviceSupportDMA.c  
 * 
 * @ref
 * 
 * 
 * @authors  
 * Nicolas & Tran  
 * View version history and changes: https://github.com/AtypicalAsian/CS372-OS-Project
 **************************************************************************************************/

#include "../h/types.h"
#include "../h/const.h"
#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

int devSema4_support[DEVICE_TYPES * DEV_UNITS]; 

hidden void disk_put(int *logicalAddr, int diskNo, int sectNo, support_t *support_struct);
hidden void disk_get(int *logicalAddr, int diskNo, int sectNo, support_t *support_struct);
hidden void flash_put(int *logicalAddr, int flashNo, int blockNo, support_t *support_struct);
hidden void flash_get(int *logicalAddr, int flashNo, int blockNo, support_t *support_struct);

/*
 * Ref: princOfOperations - section 5.3, pandOS - section 5.2.1, 5.1 (fig 5.1)
 */
void disk_put (int *logicalAddr, int diskNo, int sectNo, support_t *support_struct) {

    /* Find the disk device register with given diskNo */
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
    device_t d_device = &busRegArea->devreg[diskNo];

    int maxCyl = d_device->d_data1 >> CYLADDRSHIFT;
    int maxHd = (d_device->d_data1 >> HEADADDRSHIFT) & LOWERMASK;
    int maxSect = d_device->d_data1 & LOWERMASK;

    /* Validate the sector address, where we perform WRITE operation into 
     * if it's not outside of U's proc logical address 
     */
    if (sectNo < 0 || sectNo >= (maxCyl * maxHd * maxSect)) {
        SYSCALL(SYS9, 0, 0, 0);
        return;
    }

    int cyl = sectNo / (maxHd * maxSect);
    int temp = sectNo % (maxHd * maxSect);
    int hd = temp / maxSect;
    int sect = temp % maxSect;

    setSTATUS(NO_INTS);

    disk->d_command = (cyl << HEADADDRSHIFT) | SEEKCYL;
    /* Issue I/O Request to suspend current U's proc until the disk WRITE/READ operation is done */
    int status = SYSCALL(SYS5, DISKINT, diskNo, 0);

    /* If the operation ends with a status other than “Device Ready”
     * (1), the negative of the completion status is returned in v0*/
    if (status != DISKREADY) {
        supportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
        setSTATUS(YES_INTS);
        return;
    }

    /* Set DMA address for WRITE operation: "A disk drive DATA0 device register field is 
     * read/writable and is used to specify the starting physical address 
     * for a read or write DMA operation" 
     */
    disk->d_data0 = (memaddr)logicalAddr;
    /* Set WRITEBLK in "d_command" to address WRITE operation in a3 */
    disk->d_command = (hd << RESETACKSHIFT) | (sect << CYLADDRSHIFT) | WRITEBLK;

    /* Issue I/O Request again to check whether WRITE operation ends with SUCCESS status */
    status = SYSCALL(SYS5, DISKINT, diskNo, 0);
    if (status != DISKREADY) {
        supportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
        setSTATUS(YES_INTS);
        return;
    }

    setSTATUS(YES_INTS);
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = DISKREADY;
}

void disk_get (int *logicalAddr, int diskNo, int sectNo, support_t *support_struct) {
    
    /* Find the disk device register with given diskNo */
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
    device_t d_device = &busRegArea->devreg[diskNo];

    int maxCyl = d_device->d_data1 >> CYLADDRSHIFT;
    int maxHd = (d_device->d_data1 >> HEADADDRSHIFT) & LOWERMASK;
    int maxSect = d_device->d_data1 & LOWERMASK;

    /* Validate the sector address, where we perform WRITE operation into 
     * if it's not outside of U's proc logical address 
     */
    if (sectNo < 0 || sectNo >= (maxCyl * maxHd * maxSect)) {
        SYSCALL(SYS9, 0, 0, 0);
        return;
    }

    int cyl = sectNo / (maxHd * maxSect);
    int temp = sectNo % (maxHd * maxSect);
    int hd = temp / maxSect;
    int sect = temp % maxSect;

    setSTATUS(NO_INTS);

    disk->d_command = (cyl << HEADADDRSHIFT) | SEEKCYL;
    /* Issue I/O Request to suspend current U's proc until the disk WRITE/READ operation is done */
    int status = SYSCALL(SYS5, DISKINT, diskNo, 0);

    /* If the operation ends with a status other than “Device Ready”
     * (1), the negative of the completion status is returned in v0*/
    if (status != DISKREADY) {
        supportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
        setSTATUS(YES_INTS);
        return;
    }

    /* Set DMA address for WRITE operation: "A disk drive DATA0 device register field is 
     * read/writable and is used to specify the starting physical address 
     * for a read or write DMA operation" 
     */
    disk->d_data0 = (memaddr)logicalAddr;
    /* Read disk sector number by setting READBLK in a3 */
    disk->d_command = (hd << RESETACKSHIFT) | (sect << CYLADDRSHIFT) | READBLK;

    /* Issue I/O Request again to check whether WRITE operation ends with SUCCESS status */
    status = SYSCALL(SYS5, DISKINT, diskNo, 0);
    if (status != DISKREADY) {
        supportStruct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
        setSTATUS(YES_INTS);
        return;
    }

    setSTATUS(YES_INTS);
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = DISKREADY;
}