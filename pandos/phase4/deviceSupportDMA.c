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
#include "../h/deviceSupportDMA.h"
// #include "/usr/include/umps3/umps/libumps.h"


/**************************************************************************************************  
 * Writes data from given memory address to specific disk device (diskNo)
 * 
 * Steps:
 *  1. Extract syscall arguments: user virtual address, disk number, sector number.
 *  2. Access DATA1 field to get maxcyl,maxhead,maxsect
 *  3. Find sectCnt (convert to 1D view of disk)
 *  4. Check invalid access
 *  5. Convert sector number to (cylinder, platter, sector) triplet.
 *  6. Lock semaphore 
 *  7. Locate disk's DMA buffer in RAM
 *  8. Copy 4KB data to write from uproc to DMA buffer we just found
 *  9. Perform the write op (seek correct cylinder, write from buffer to correct sector, etc..)
 *  10. Release semaphore
 *  11. Check status code to see if operation is successful -> write into v0 accordingly
 * 
 * 
 * @ref
 * 5.2 pandos and 5.3 pops
 **************************************************************************************************/
void disk_put(int *logicalAddr, int diskNo, int sectNo, support_t *support_struct) {

    /* Pointers */
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
    device_t *disk = &busRegArea->devreg[diskNo];  // get pointer to device register

    /* Read geometry info */
    int maxCyl = disk->d_data1 >> CYLADDRSHIFT;
    int maxHd  = (disk->d_data1 >> HEADADDRSHIFT) & LOWERMASK;
    int maxSect = disk->d_data1 & LOWERMASK;
    int totalSectors = maxCyl * maxHd * maxSect;

    /* Validation: sector bounds and address range */
    if (sectNo < 0 || sectNo >= totalSectors || (int)logicalAddr < KUSEG) {
        SYSCALL(SYS9, 0, 0, 0);  // terminate process
        return;
    }

    /* Compute cylinder, head, sector from 1D index */
    int cyl = sectNo / (maxHd * maxSect);
    int temp = sectNo % (maxHd * maxSect);
    int hd = temp / maxSect;
    int sect = temp % maxSect;

    /* Acquire disk semaphore (mutual exclusion) */
    SYSCALL(SYS3, (memaddr)&devSema4_support[diskNo], 0, 0);

    /* Copy user memory to DMA buffer */
    memaddr *dmaBuffer = (memaddr *)(DISKSTART + (PAGESIZE * diskNo));
    memaddr *src = (DISKSTART + (diskNo * PAGESIZE));
    for (int i = 0; i < BLOCKS_4KB; i++) {
        *dmaBuffer++ = *logicalAddr++;
    }

    /* Seek to the correct cylinder */
    setSTATUS(NO_INTS);
    disk->d_command = (cyl << HEADADDRSHIFT) | SEEKCYL;
    int status = SYSCALL(SYS5, DISKINT, diskNo, 0);

    /*Device is not ready -> error write*/
    if (status != DISKREADY) {
        support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
        setSTATUS(YES_INTS);
        SYSCALL(SYS4, (memaddr)&devSema4_support[diskNo], 0, 0);  // release lock
        return;
    }

    /* Set up DMA transfer from buffer to disk */
    setSTATUS(NO_INTS);
    disk->d_data0 = (memaddr)dmaBuffer;
    disk->d_command = (hd << RESETACKSHIFT) | (sect << CYLADDRSHIFT) | WRITEBLK;
    status = SYSCALL(SYS5, DISKINT, diskNo, 0);
    setSTATUS(YES_INTS);

    /* Return result */
    if (status != DISKREADY) {
        support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
    } else {
        support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = DISKREADY;
    }

    /* Release disk semaphore */
    SYSCALL(SYS4, (memaddr)&devSema4_support[diskNo], 0, 0);
}




/*reads data from given location in disk
* 5.2 pandos and 5.3 pops
*/
void disk_get(int *logicalAddr, int diskNo, int sectNo, support_t *support_struct) {
    
    /* Find the disk device register with given diskNo */
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
    
}
