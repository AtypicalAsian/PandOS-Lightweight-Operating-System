/**************************************************************************************************  
 * @file deviceSupportDMA.c  
 * 
 * This module provides support for block I/O operations, functionalities core to syscalls 14-17,
 * using DMA devices, specifically disk and flash storage. It implements read and write 
 * operations for 4KB blocks of data between user process address space and device-specific 
 * DMA buffers. 
 * 
 * Core functionalities include:
 *  - Disk read/write (disk_get, disk_put)
 *  - Flash read/write using block addressing (flash_get, flash_put)
 * 
 * @ref
 * PandOS - Chapter 5
 * Pops - 5.3 & 5.4
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
#include "/usr/include/umps3/umps/libumps.h"

/**************************************************************************************************  
 * Writes data from given memory address to specific disk device (diskNo) - SYS14
 * 
 * Steps:
 *  1. Extract disk geometry from device register DATA1 field: maxcyl, maxhead, maxsect
 *  2. Validate sector number to ensure it's within disk capacity (prevent invalid access)
 *  3. Lock target disk device semaphore
 *  4. Compute cylinder,head,sector from linear sector number requested by calling uproc
 *  5. Copy 4KB data from uproc address space to disk's DMA buffer
 *  6. Seek to the correct cylinder (issue seek command + block uproc on ASL until seek completes)
 *  6. If disk SEEK successful:
 *     a. Put starting address of DMA buffer into register v0
 *     b. Issue WRITE command to requested sector
 *     c. Block uproc on ASL until WRITE op completes
 *  7. If operation succeeded, return status in v0; otherwise return negative status code.
 *  8. Release device semaphore
 * 
 *  @param logicalAddr Pointer to 4KB data in uproc logical address to be written to disk.
 *  @param diskNo      Disk device number to write to
 *  @param sectNo      Linear sector number on disk where data will be written
 *  @param support_struct Pointer to the calling process's support structure
 * 
 * @ref
 * 5.2 pandos and 5.3 pops
 **************************************************************************************************/
void disk_put(memaddr *logicalAddr, int diskNo, int sectNo, support_t *support_struct) {
    /*Local Variables*/
    memaddr *dmaBuffer;                  /* Pointer to disk's DMA buffer in RAM */
    devregarea_t *busRegArea;            /* Pointer to device register area */
    int maxCyl;                          /* Maximum cylinders of the disk */
    int maxHead;                         /* Maximum heads (platters) of the disk */
    int maxSect;                         /* Maximum sectors*/
    int headNum, cylNum;                 /* Computed head and cylinder number for this sector */
    int status;                          /* Disk device operation status code */                             

    busRegArea = (devregarea_t *) RAMBASEADDR; /*Access device register base*/

    /*Extract disk geometry values*/
    maxSect = (busRegArea->devreg[diskNo].d_data1 & LOWERMASK);
    maxCyl = (busRegArea->devreg[diskNo].d_data1 >> CYLADDRSHIFT);
    maxHead = (busRegArea->devreg[diskNo].d_data1 & HEADMASK) >> HEADADDRSHIFT;

    /*Validate sector number and requested adddress to check for invalid access*/
    if ( (sectNo < 0) || ((int)logicalAddr < KUSEG) || (sectNo > (maxCyl * maxHead * maxSect))) {
        get_nuked(NULL); 
    }
    
    /*Lock target disk device semaphore*/
    SYSCALL(SYS3, (memaddr)&devSema4_support[diskNo], 0, 0); 

    /*Calculate the base address of the disk's DMA buffer for this disk unit*/
    dmaBuffer = (memaddr *)(DISKSTART + (diskNo * PAGESIZE));

    /* Convert linear sector number into Cylinder-Head-Sector triplet */
    cylNum = sectNo / (maxHead * maxSect);
    sectNo = sectNo % (maxHead * maxSect);
    headNum = sectNo / maxSect;
    sectNo = sectNo % maxSect;

    /*Copy 4KB data from user process memory to disk DMA buffer*/
    int i;
    for (i = 0; i < BLOCKS_4KB; i++) {
        *dmaBuffer = *logicalAddr;
        dmaBuffer++;
        logicalAddr++;
    }

    dmaBuffer = (memaddr *)(DISKSTART + (diskNo * PAGESIZE)); /*reset dmaBuffer to issue correct starting address for later WRITE op*/


    setSTATUS(NO_INTS);
    busRegArea->devreg[diskNo].d_command = (cylNum << LEFTSHIFT8) | SEEK_CMD; /*issue SEEK command*/
    status = SYSCALL(SYS5, DISKINT, diskNo, 0); /*Block current proc until SEEK op completes*/
    setSTATUS(YES_INTS);

    if (status != READY){ /*If seek unsucessful*/
        support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
        SYSCALL(SYS4, (memaddr)&devSema4_support[diskNo], 0, 0);
        return;
    } else{ /*If seek successful*/
        setSTATUS(NO_INTS);
        busRegArea->devreg[diskNo].d_data0 = (unsigned int) dmaBuffer; /*Put starting address of DMA buffer into register v0*/
        unsigned int headField = headNum << LEFTSHIFT16;
        unsigned int sectorField = sectNo << LEFTSHIFT8;
        busRegArea->devreg[diskNo].d_command = headField | sectorField | WRITEBLK; /*Issue WRITE command */
        status = SYSCALL(SYS5, DISKINT, diskNo, 0); /*Block uproc on ASL until WRITE op completes*/
        setSTATUS(YES_INTS);

        if (status == READY) { /*If WRITE op successful*/
            support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = status;
        } 
        else{ /*If WRITE op unsuccessful*/
            support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
        }
        SYSCALL(SYS4, (memaddr)&devSema4_support[diskNo], 0, 0); /*Release disk device semaphore*/
    } 
}


/**************************************************************************************************  
 * Reads data from sector in target disk device to uproc logical address space - SYS15
 * 
 * Steps:
 *  1. Extract disk geometry from device register DATA1 field: maxcyl, maxhead, maxsect
 *  2. Validate sector number to ensure it's within disk capacity (prevent invalid access)
 *  3. Lock target disk device semaphore
 *  4. Locate appropriate disk DMA buffer in RAM
 *  5. Compute cylinder,head,sector from linear sector number requested by calling uproc 
 *  6. Seek to the correct cylinder (issue seek command + block uproc on ASL until seek completes)
 *  7. If disk SEEK successful:
 *     a. Put starting address of DMA buffer into register v0
 *     b. Issue READ command to read in correct disk sector into dma buffer
 *     c. Block uproc on ASL until READ op completes
 *  8. If operation succeeded, copy content of dma buffer to uproc's logical address space
 *  9. Return status in v0
 * 10. Release target disk device semaphore
 * 
 *  @param logicalAddr Pointer to 4KB data in uproc logical address space where disk data will be stored
 *  @param diskNo      Disk device number to read from
 *  @param sectNo      Linear sector number on disk to read from
 *  @param support_struct Pointer to the calling process's support structure
 * 
 * @ref
 * 5.2 pandos and 5.3 pops
 **************************************************************************************************/

 void disk_get(memaddr *logicalAddr, int diskNo, int sectNo, support_t *support_struct) {
    /*Local Variables*/
    memaddr *dmaBuffer;                  /* Pointer to disk's DMA buffer in RAM */
    devregarea_t *busRegArea;            /* Pointer to device register area */
    int maxCyl;                          /* Maximum cylinders of the disk */
    int maxHead;                         /* Maximum heads (platters) of the disk */
    int maxSect;                         /* Maximum sectors*/
    int headNum, cylNum;                 /* Computed head and cylinder number for this sector */
    int status;                          /* Disk device operation status code */    

    busRegArea = (devregarea_t *) RAMBASEADDR; /*Access device register base*/

    /*Extract disk geometry values*/
    maxCyl = (busRegArea->devreg[diskNo].d_data1 >> CYLADDRSHIFT);
    maxHead = (busRegArea->devreg[diskNo].d_data1 & HEADMASK) >> HEADADDRSHIFT;
    maxSect = (busRegArea->devreg[diskNo].d_data1 & LOWERMASK);

    /*Validate sector number and requested adddress to check for invalid access*/
    if ( (sectNo < 0) || ((int)logicalAddr < KUSEG) || (sectNo > (maxCyl * maxHead * maxSect))) {
        get_nuked(NULL); 
    }

    /*Lock target disk device semaphore*/
    SYSCALL(SYS3, (memaddr)&devSema4_support[diskNo], 0, 0);

    /*Calculate the base address of the disk's DMA buffer for this disk unit*/
    dmaBuffer = (memaddr *)(DISKSTART + (diskNo * PAGESIZE));
    memaddr *originBuff = (memaddr *)(DISKSTART + (diskNo * PAGESIZE));

    /* Convert linear sector number into Cylinder-Head-Sector triplet */
    cylNum = sectNo / (maxHead * maxSect);
    sectNo = sectNo % (maxHead * maxSect);
    headNum = sectNo / maxSect;
    sectNo = sectNo % maxSect;

    setSTATUS(NO_INTS);
    busRegArea->devreg[diskNo].d_command = (cylNum << LEFTSHIFT8) | SEEK_CMD; /*issue SEEK command*/
    status = SYSCALL(SYS5, DISKINT, diskNo, 0); /*Block uproc on ASL until WRITE op completes*/
    setSTATUS(YES_INTS);

    if (status != READY) { /*If seek unsucessful*/
        support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
        SYSCALL(SYS4, (memaddr)&devSema4_support[diskNo], 0, 0);
        return;
    } else { /*If seek sucessful*/
        setSTATUS(NO_INTS);
        busRegArea->devreg[diskNo].d_data0 = (unsigned int) originBuff; /*Put starting address of DMA buffer into register v0*/
        unsigned int headField = headNum << LEFTSHIFT16;
        unsigned int sectorField = sectNo << LEFTSHIFT8;
        busRegArea->devreg[diskNo].d_command = headField | sectorField | READBLK; /*Issue READ command to read in correct disk sector into dma buffer*/
        status = SYSCALL(SYS5, DISKINT, diskNo, 0); /*Block uproc on ASL until WRITE op completes*/
        setSTATUS(YES_INTS);
    
        if (status != READY) { /*If READ op unsuccessful*/
            support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
            SYSCALL(SYS4, (memaddr)&devSema4_support[diskNo], 0, 0);
            return;
        }
        /*If READ op successful*/
        int i;
        /*Copy 4kb data from dma buffer to uproc's logical address space*/
        for (i = 0; i < BLOCKS_4KB; i++) {
            *logicalAddr = *dmaBuffer;
            logicalAddr++;
            dmaBuffer++;
        }

        support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = status;
        SYSCALL(SYS4, (memaddr)&devSema4_support[diskNo], 0, 0); /*release disk device semaphore*/
    }
}

/**************************************************************************************************  
 * Writes a 4KB block of data from the given user logical address to a specific block on a flash device
 * via DMA buffer - SYS16
 * @param logicalAddr Pointer to 4KB data in uproc logical address space to be written to flash
 * @param flashNo     Flash device number to write to
 * @param blockNo     Flash block number to write to
 * @param support_struct Pointer to the calling process's support structure
 * 
 * @ref
 * 5.3 pandos and 5.4 pops
 **************************************************************************************************/
void flash_put(memaddr *logicalAddr, int flashNo, int blockNo, support_t *support_struct) {
    flashOperation(logicalAddr, flashNo, blockNo, FLASHWRITE, support_struct);
}


/**************************************************************************************************  
 * Reads data from 4kb block in target flash device into uproc's logical address space via DMA buffer
 * - SYS17
 * 
 *  @param logicalAddr Pointer to 4KB buffer in uproc logical address space where flash data will be stored
 *  @param flashNo     Flash device number to read from
 *  @param blockNo     Flash block number to read from
 *  @param support_struct Pointer to the calling process's support structure
 * 
 * @ref
 * 5.3 pandos and 5.4 pops
 **************************************************************************************************/
void flash_get(memaddr *logicalAddr, int flashNo, int blockNo, support_t *support_struct) {
    flashOperation(logicalAddr, flashNo, blockNo, FLASHREAD, support_struct);
}


/**************************************************************************************************  
 * Helper for flash read/write operations
 * 
 * Steps:
 *  1. Validate user logical address (must be in KUSEG)
 *  2. Lock flash device semaphore
 *  3. Compute DMA buffer address and device register addresses
 *  4. Validate block number against device's maxBlock
 *  5. Set device's data0 to DMA buffer
 *  6. Handle 2 cases:
 *      a. If WRITE operations, copy data from uproc logical address space to DMA buffer
 *      b. Issue WRITE command and block uproc until WRITE completes
 *  
 *      a. If READ operation, issue READ command and block uproc until READ completes
 *      b. Copy data from DMA buffer to uproc logical address space
 * 
 *  7. Store status in v0 and release flash device semaphore
 * 
 * @param logicalAddr Pointer to uproc logical starting address
 * @param flashNo     Flash device number
 * @param blockNo     Block number to read/write to
 * @param operation   FLASHREAD or FLASHWRITE code
 * @param support_struct Pointer to support struct of uproc requesting the syscall
 * 
 * @return None
 * 
 * @ref
 * 5.3, 5.5.2 pandos and 5.4 pops
 **************************************************************************************************/
void flashOperation(memaddr *logicalAddr, int flashNo, int blockNo, int operation, support_t *support_struct) {
    /*Local Variables*/
    memaddr *dmaBuffer;                 /* Pointer to disk's DMA buffer in RAM */
    device_t *f_device;                 /* Pointer to target flash device*/
    int status;                         /* Flash device operation status code */    
    unsigned int maxBlock;              /* Maximum blocks of the disk */

    /* Validate uproc memory access */
    if ((int)logicalAddr < KUSEG) {
        get_nuked(NULL);
    }

    /* Lock target flash device semaphore */
    SYSCALL(SYS3, (memaddr)&devSema4_support[DEV_UNITS + flashNo], 0, 0);

    /* Calculate the base address of the flash's DMA buffer */
    dmaBuffer = (memaddr *)(FLASHSTART + (flashNo * PAGESIZE));

    /* Calculate pointer to target flash device */
    int devIdx = (FLASHINT - DISKINT) * DEVPERINT + flashNo;
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
    f_device = &busRegArea->devreg[devIdx];
    maxBlock = f_device->d_data1; /*Retrieve max number of valid blocks supported by the target device*/

    /* Validate block number */
    if (blockNo >= maxBlock) {
        get_nuked(NULL);
    }

    /*Set flash device's DATA0 register to DMA buffer starting address*/
    f_device->d_data0 = (memaddr)dmaBuffer;

    /* If WRITE operation, copy data from uproc logical address space to DMA buffer */
    if (operation == FLASHWRITE) {
        int i;
        for (i = 0; i < BLOCKS_4KB; i++) {
            *dmaBuffer++ = *logicalAddr++;
        }
        dmaBuffer = (memaddr *)(FLASHSTART + (flashNo * PAGESIZE)); /* Reset for device use */
        f_device->d_data0 = (memaddr)dmaBuffer;
    }

    /* Issue flash command (READ or WRITE) */
    setSTATUS(NO_INTS);
    f_device->d_command = operation | (blockNo << FLASHADDRSHIFT);
    status = SYSCALL(SYS5, FLASHINT, flashNo, 0); /* Wait for operation to complete */
    setSTATUS(YES_INTS);

    /* Step 9: If operation is READ and status is READY, copy data from DMA buffer to logicalAddr */
    if ((operation == FLASHREAD) && (status == READY)) {
        int i;
        for (i = 0; i < BLOCKS_4KB; i++) {
            *logicalAddr++ = *dmaBuffer++;
        }
    }

    /* Store status in v0 */
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = status;

    /*Unlock flash device semaphore */
    SYSCALL(SYS4, (memaddr)&devSema4_support[DEV_UNITS + flashNo], 0, 0);
}


