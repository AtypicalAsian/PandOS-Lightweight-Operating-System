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
 * Writes data from given memory address to specific flash device (flashNo)
 * 
 * Steps:
 *  1. Extract syscall arguments: user virtual address, disk number, sector number (done in syscall handler)
 *  2. Check invalid memory region access
 *  3. Lock semaphore 
 *  4. Locate flash's DMA buffer in RAM
 *  5. Copy data from uproc address space to dma buffer
 *  5. Perform write to target flash block using starting address of dma buffer (4Kb)
 *  6. Release semaphore
 *  7. Check status code to see if operation is successful -> write into v0 accordingly
 * 
 * 
 * @ref
 * 5.3 pandos and 5.4 pops
 **************************************************************************************************/
void flash_put(memaddr *logicalAddr, int flashNo, int blockNo, support_t *support_struct) {
    /*Local Variables*/
    memaddr *dmaBuffer;                 /* Pointer to disk's DMA buffer in RAM */
    device_t *f_device;                 /* Pointer to target flash device*/
    int status;                         /* Flash device operation status code */    
    unsigned int maxBlock;              /* Maximum blocks of the disk */
    

    /*Check for invalid memory access*/
    if ((int)logicalAddr < KUSEG) {
        get_nuked(NULL);
    }

    /*Lock target flash device semaphore*/
    SYSCALL(SYS3, (memaddr)&devSema4_support[DEV_UNITS + flashNo], 0, 0);

    /*Calculate the base address of the flash's DMA buffer*/
    dmaBuffer = (memaddr *)(FLASHSTART + (flashNo * PAGESIZE));
    memaddr *originBuff = dmaBuffer;

    /*Copy 4KB data from user process memory to flash DMA buffer*/
    int i;
    for (i = 0; i < BLOCKS_4KB; i++) {
        *dmaBuffer = *logicalAddr;
        dmaBuffer++;
        logicalAddr++;
    }

    f_device = (device_t *)(DEVICEREGSTART + ((FLASHINT - DISKINT) * (DEV_UNITS * DEVREGSIZE)) + (flashNo * DEVREGSIZE));
    maxBlock = f_device->d_data1;

    /*Check invalid block number request*/
    if (blockNo >= maxBlock){
        get_nuked(NULL);
    }

    f_device->d_data0 = (memaddr)originBuff;

    setSTATUS(NO_INTS);
    f_device->d_command = FLASHWRITE | (blockNo << FLASHADDRSHIFT); /*issue flash write command*/
    status = SYSCALL(SYS5, FLASHINT, flashNo, 0); /*block uproc on ASL until flash WRITE op completes*/
    setSTATUS(YES_INTS);

    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = status; 
    SYSCALL(SYS4, (memaddr)&devSema4_support[DEV_UNITS + flashNo], 0, 0); /*Unlock target flash device semaphore*/
}




/**************************************************************************************************  
 * Reads data from block in target flash device
 * 
 * Steps:
 *  1. Extract syscall arguments: user virtual address, disk number, sector number (done in syscall handler)
 *  2. Check invalid memory region access
 *  3. Lock semaphore 
 *  4. Locate flash's DMA buffer in RAM
 *  5. Read from flash block to device DMA buffer
 *  6. Copy data from DMA buffer into requesting uproc address space starting from provide start address
 *  7. Release semaphore
 *  8. Check status code to see if operation is successful -> write into v0 accordingly
 * 
 * @ref
 * 5.3 pandos and 5.4 pops
 **************************************************************************************************/
void flash_get(memaddr *logicalAddr, int flashNo, int blockNo, support_t *support_struct) {
    /*Local Variables*/
    memaddr *dmaBuffer;                 /* Pointer to disk's DMA buffer in RAM */
    device_t *f_device;                 /* Pointer to target flash device*/
    int status;                         /* Flash device operation status code */    
    unsigned int maxBlock;              /* Maximum blocks of the disk */

    /*Check for invalid memory access*/
    if ((int)logicalAddr < KUSEG) {
        get_nuked(NULL);
    }

    /*Lock target flash device semaphore*/
    SYSCALL(SYS3, (memaddr)&devSema4_support[DEV_UNITS + flashNo], 0, 0);

    /*Calculate the base address of the flash's DMA buffer*/
    dmaBuffer = (memaddr *)(FLASHSTART + (flashNo * PAGESIZE));
    memaddr *originBuff = dmaBuffer;

    f_device = (device_t *)(DEVICEREGSTART + ((FLASHINT - DISKINT) * (DEV_UNITS * DEVREGSIZE)) + (flashNo * DEVREGSIZE));
    maxBlock = f_device->d_data1;

    /*Check invalid block number request*/
    if (blockNo >= maxBlock) {
        get_nuked(NULL);
    }

    f_device->d_data0 = (memaddr)originBuff;

    setSTATUS(NO_INTS);
    f_device->d_command = FLASHREAD | (blockNo << FLASHADDRSHIFT); /*issue flash READ command*/
    status = SYSCALL(SYS5, FLASHINT, flashNo, 0); /*block uproc on ASL until flash READ op completes*/
    setSTATUS(YES_INTS);

    /*if READ op successful*/
    if (status == READY) {
        int i;
        /*Copy 4KB data from DMA buffer to uproc logical address space*/
        for (i = 0; i < BLOCKS_4KB; i++) {
            *logicalAddr = *dmaBuffer;
            logicalAddr++;
            dmaBuffer++;
        }
    }

    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = status;
    SYSCALL(SYS4, (memaddr)&devSema4_support[DEV_UNITS + flashNo], 0, 0); /*Unlock flash device semaphore*/
}




