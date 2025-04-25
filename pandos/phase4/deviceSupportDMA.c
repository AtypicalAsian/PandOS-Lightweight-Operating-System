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
#include "/usr/include/umps3/umps/libumps.h"

/**************************************************************************************************  
 * Writes data from given memory address to specific disk device (diskNo)
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
        busRegArea->devreg[diskNo].d_data0 = dmaBuffer; /*Put starting address of DMA buffer into register v0*/
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
 * Reads data from sector in target disk device to uproc logical address space
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
 * 
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
    memaddr *originBuff = (DISKSTART + (diskNo * PAGESIZE));

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
        busRegArea->devreg[diskNo].d_data0 = originBuff; /*Put starting address of DMA buffer into register v0*/
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
 * Writes a 4KB block of data from the given user logical address to a specific block on a flash device.
 * 
 * Steps:
 *  1. Validate uproc logical address access (make sure to be within KUSEG)
 *  2. Lock semaphore for target flash device
 *  3. Locate appropriate DMA buffer in RAM
 *  4. Copy 4kb data from uproc logical address space into flash device's DMA buffer
 *  5. Determine target flash device register
 *  6. Retrieve max number of valid blocks supported by the target device
 *  7. Validate requested block number to make sure it's within valid flash block range
 *  8. Set flash device's DATA0 register to DMA buffer starting address
 *  9. Issue WRITE command with target flash block number
 * 10. Block uproc until WRITE completes 
 * 11. Store status of the operation (success or failure) into v0 of the exception state  
 * 12. Release semaphore for the target flash device
 * 
 * 
 * @ref
 * 5.3 pandos and 5.4 pops
 **************************************************************************************************/
void flash_put(memaddr *logicalAddr, int flashNo, int blockNo, support_t *support_struct) {
    flashOperation(logicalAddr, flashNo, blockNo, FLASHWRITE, support_struct);
}


/**************************************************************************************************  
 * Reads data from block in target flash device
 * 
 * Steps:
 *  1. Validate uproc logical address access (make sure to be within KUSEG)
 *  2. Lock semaphore for target flash device
 *  3. Locate appropriate DMA buffer in RAM
 *  4. Determine the target flash device's register
 *  5. Extract max number of valid blocks supported by the flash device
 *  6. Validate the requested block number to ensure it is within valid flash block range
 *  7. Set the flash device's DATA0 register to the DMA buffer address (destination for the read)
 *  8. Issue the READ command with the target block number
 *  9. Block uproc until READ completes 
 * 10. If the operation was successful, copy 4KB of data from the flash's DMA buffer to uproc logical address space.
 * 11. Store the status of the operation (success or failure) into v0 of the exception state.
 * 12. Release the semaphore for the target flash device
 * 
 * 
 * @ref
 * 5.3 pandos and 5.4 pops
 **************************************************************************************************/
void flash_get(memaddr *logicalAddr, int flashNo, int blockNo, support_t *support_struct) {
    flashOperation(logicalAddr, flashNo, blockNo, FLASHREAD, support_struct);
}


/**************************************************************************************************  
 * flashOperation Helper
 * 
 * 
 * 
 * @ref
 * 5.3, 5.5.2 pandos and 5.4 pops
 **************************************************************************************************/
int flashOperation(memaddr *logicalAddr, int flashNo, int blockNo, int operation, support_t *support_struct) {
    memaddr *dmaBuffer;
    device_t *f_device;
    int status;
    unsigned int maxBlock;

    /* Step 1: Validate user memory access */
    if ((int)logicalAddr < KUSEG) {
        get_nuked(NULL);
    }

    /* Step 2: Lock target flash device semaphore */
    SYSCALL(SYS3, (memaddr)&devSema4_support[DEV_UNITS + flashNo], 0, 0);

    /* Step 3: Calculate the base address of the flash's DMA buffer */
    dmaBuffer = (memaddr *)(FLASHSTART + (flashNo * PAGESIZE));

    /* Step 4: Access flash device register */
    int devIdx = (FLASHINT - DISKINT) * DEVPERINT + flashNo;
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
    f_device = &busRegArea->devreg[devIdx];
    maxBlock = f_device->d_data1;

    /* Step 5: Validate block number */
    if (blockNo >= maxBlock) {
        get_nuked(NULL);
    }

    /* Step 6: Set data0 to DMA buffer */
    f_device->d_data0 = (memaddr)dmaBuffer;

    /* Step 7: If writing, copy data from logicalAddr to DMA buffer */
    if (operation == FLASHWRITE) {
        int i;
        for (i = 0; i < BLOCKS_4KB; i++) {
            *dmaBuffer++ = *logicalAddr++;
        }
        dmaBuffer = (memaddr *)(FLASHSTART + (flashNo * PAGESIZE)); /* Reset for device use */
        f_device->d_data0 = (memaddr)dmaBuffer;
    }

    /* Step 8: Issue flash command (READ or WRITE) */
    setSTATUS(NO_INTS);
    f_device->d_command = operation | (blockNo << FLASHADDRSHIFT);
    status = SYSCALL(SYS5, FLASHINT, flashNo, 0); /* Wait for operation to complete */
    setSTATUS(YES_INTS);

    /* Step 9: If reading and status is READY, copy data from DMA buffer to logicalAddr */
    if ((operation == FLASHREAD) && (status == READY)) {
        int i;
        for (i = 0; i < BLOCKS_4KB; i++) {
            *logicalAddr++ = *dmaBuffer++;
        }
    }

    /* Step 10: Store status in v0 */
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = status;

    /* Step 11: Unlock flash device semaphore */
    SYSCALL(SYS4, (memaddr)&devSema4_support[DEV_UNITS + flashNo], 0, 0);
    return status;
}

/*flash_get*/


/*flash_put*/


