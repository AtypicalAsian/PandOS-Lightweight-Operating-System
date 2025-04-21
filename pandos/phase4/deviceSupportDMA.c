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
    device_t *disk = &busRegArea->devreg[diskNo];  /*get pointer to device register */

    /* Read geometry info */
    int maxCyl = disk->d_data1 >> CYLADDRSHIFT;
    int maxHd  = (disk->d_data1 >> HEADADDRSHIFT) & LOWERMASK;
    int maxSect = disk->d_data1 & LOWERMASK;
    int totalSectors = maxCyl * maxHd * maxSect;

    /* Validation: sector bounds and address range */
    if (sectNo < 0 || sectNo >= totalSectors || (int)logicalAddr < KUSEG) {
        SYSCALL(SYS9, 0, 0, 0);
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
    int i;
    for (i = 0; i < BLOCKS_4KB; i++) {
        *dmaBuffer = *logicalAddr;
        dmaBuffer++;
        logicalAddr++;
    }

    /* Seek to the correct cylinder */
    setSTATUS(NO_INTS);
    disk->d_command = (cyl << HEADADDRSHIFT) | SEEKCYL;
    int status = SYSCALL(SYS5, DISKINT, diskNo, 0);

    /*Device is not ready -> error write*/
    if (status != DISKREADY) {
        support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
        setSTATUS(YES_INTS);
        SYSCALL(SYS4, (memaddr)&devSema4_support[diskNo], 0, 0);
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


/**************************************************************************************************  
 * Reads data from sector in target disk device to uproc logical address space
 * 
 * Steps:
 *  1. Extract syscall arguments: user virtual address, disk number, sector number (done in syscall handler)
 *  2. Check invalid memory region access
 *  3. Lock semaphore 
 *  4. Locate flash's DMA buffer in RAM
 *  5. Read from disk sector to device DMA buffer
 *  6. Copy data from DMA buffer into requesting uproc address space starting from provided start address
 *  7. Release semaphore
 *  8. Check status code to see if operation is successful -> write into v0 accordingly
 * 
 * 
 * @ref
 * 5.2 pandos and 5.3 pops
 **************************************************************************************************/
void disk_get(int *logicalAddr, int diskNo, int sectNo, support_t *support_struct) {
    /* Get pointer to device register area */
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
    device_t *disk = &busRegArea->devreg[diskNo];

    /* Extract geometry from d_data1 */
    int maxCyl  = disk->d_data1 >> CYLADDRSHIFT;
    int maxHd   = (disk->d_data1 >> HEADADDRSHIFT) & LOWERMASK;
    int maxSect = disk->d_data1 & LOWERMASK;
    int totalSectors = maxCyl * maxHd * maxSect;

    /* Validate address and bounds */
    if (sectNo < 0 || sectNo >= totalSectors || (int)logicalAddr < KUSEG) {
        SYSCALL(SYS9, 0, 0, 0);
        return;
    }

    /* Convert flat sector number into (cylinder, head, sector) */
    int cyl  = sectNo / (maxHd * maxSect);
    int temp = sectNo % (maxHd * maxSect);
    int hd   = temp / maxSect;
    int sect = temp % maxSect;

    /* Acquire disk semaphore (mutual exclusion) */
    SYSCALL(SYS3, (memaddr)&devSema4_support[diskNo], 0, 0);

    /* Use physical buffer assigned to this disk */
    memaddr *dmaBuffer = (memaddr *)(DISKSTART + (diskNo * PAGESIZE));

    /* Step 1: Seek to the correct cylinder */
    setSTATUS(NO_INTS);
    disk->d_command = (cyl << HEADADDRSHIFT) | SEEKCYL;
    int status = SYSCALL(SYS5, DISKINT, diskNo, 0);
    setSTATUS(YES_INTS);

    if (status != DISKREADY) {
        support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
        SYSCALL(SYS4, (memaddr)&devSema4_support[diskNo], 0, 0);
        return;
    }

    /* Step 2: Set DMA buffer address and initiate READ */
    setSTATUS(NO_INTS);
    disk->d_data0 = (memaddr)dmaBuffer;
    disk->d_command = (hd << RESETACKSHIFT) | (sect << CYLADDRSHIFT) | READBLK;
    status = SYSCALL(SYS5, DISKINT, diskNo, 0);
    setSTATUS(YES_INTS);

    if (status != DISKREADY) {
        support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = -status;
        SYSCALL(SYS4, (memaddr)&devSema4_support[diskNo], 0, 0);
        return;
    }

    /* Step 3: Copy from DMA buffer to logical address in user memory */
    memaddr *dst = (memaddr *)logicalAddr;
    int i;
    for (i = 0; i < BLOCKS_4KB; i++) {
        *dst = *dmaBuffer;
        dst++;
        dmaBuffer++;
    }

    /* Step 4: Release semaphore and return status */
    SYSCALL(SYS4, (memaddr)&devSema4_support[diskNo], 0, 0);
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = DISKREADY;
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
void flash_put(int *logicalAddr, int flashNo, int blockNo, support_t *suppStruct){
    /*logicalAddr = a1, flashNo = a2, blockNo = a3*/

    /*Local Variables*/
    memaddr *dmaBuffer; /*pointer to appropriate dmaBuffer for the given flash device*/
    int dev_status; /*stores flash device status*/
    device_t *flash_dev; /*pointer to target flash device*/
    unsigned int command; /*command to write to COMMAND field of flash device*/
    unsigned int max_blockCnt; /*stores the target flash device's max block number*/

    /*Check invalid memory region access*/
    if ((int) logicalAddr < KUSEG){
        SYSCALL(SYS9,0,0,0);
        return;
    }

    /*Lock flash device semaphore*/
    SYSCALL(SYS3, (memaddr) &devSema4_support[(DEV_UNITS) + flashNo], 0,0);

    /*Locate flash's dma buffer in RAM*/
    dmaBuffer = (memaddr *) (FLASHSTART + (PAGESIZE * flashNo));

    /*Copy data from uproc address space to dma buffer (prepare for write operation)*/
    int i;
    for (i=0; i < BLOCKS_4KB; i++){
        *dmaBuffer = *logicalAddr; /*copy the word into buffer*/
        dmaBuffer++; 
        logicalAddr++; 
    }

    /*Perform write from DMA buffer to target flash block*/
    /*needs flashNo, block, buffer, write op*/
    int devIdx = (FLASHINT-DISKINT) * DEVPERINT + flashNo;
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
    flash_dev = &busRegArea->devreg[devIdx]; /*get pointer to target flash device*/

    max_blockCnt = flash_dev->d_data1; /*get max block number for this target flash device [0...maxBlock-1]*/

    if (blockNo > max_blockCnt-1){ /*if requested block is outside the range of max_blockCnt -> terminate*/
        SYSCALL(SYS9,0,0,0);
    }

    
    flash_dev->d_data0 = dmaBuffer;/*Write the flash device’s DATA0 field with the starting physical address of the 4kb block to be written*/
    command = (blockNo << BLOCK_SHIFT)| FLASHWRITE; /*build WRITE command to write to COMMAND field of device register*/

    setSTATUS(NO_INTS); /*Disable interrupts*/
    flash_dev->d_command = command;  /*write the flash device's COMMAND field*/
    dev_status = SYSCALL(SYS5,FLASHINT,flashNo,0); /*immediately issue SYS5 (wait for IO) to block the current process*/
    setSTATUS(YES_INTS); /*Enable interrupts*/

    /*Release flash device semaphore*/
    SYSCALL(SYS4,(memaddr) &devSema4_support[(DEV_UNITS) + flashNo],0,0);

    /*Check status code to write appropriate value into v0 register*/
    
    if (dev_status != READY){ /*If operation failed (check device status) -> program trap handler*/
        suppStruct->sup_exceptState[GENERALEXCEPT].s_v0 = -(dev_status); /*return negative of device status in v0*/
        syslvl_prgmTrap_handler(suppStruct); /*call support level program trap handler*/
    }

    suppStruct->sup_exceptState[GENERALEXCEPT].s_v0 = dev_status; /*write device status into v0*/
    
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
void flash_get(int *logicalAddr, int flashNo, int blockNo, support_t *suppStruct){
    /*logicalAddr = a1, flashNo = a2, blockNo = a3*/

    /*Local Variables*/
    memaddr *dmaBuffer; /*pointer to appropriate dmaBuffer for the given flash device*/
    int dev_status; /*stores flash device status*/
    device_t *flash_dev; /*pointer to target flash device*/
    unsigned int command; /*command to write to COMMAND field of flash device*/
    unsigned int max_blockCnt; /*stores the target flash device's max block number*/

    /*Check invalid memory region access*/
    if ((int) logicalAddr < KUSEG){
        SYSCALL(SYS9,0,0,0);
        return;
    }

    /*Lock device semaphore*/
    SYSCALL(SYS3,(memaddr)&devSema4_support[DEV_UNITS + flashNo],0,0);

    /*Locate flash device's DMA buffer*/
    dmaBuffer = (memaddr *) (FLASHSTART + (PAGESIZE * flashNo));

    /*Perform read operation to read from target flash block to device DMA buffer*/
    /*needs flashNo, block, buffer, write op*/
    int devIdx = (FLASHINT-DISKINT) * DEVPERINT + flashNo;
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
    flash_dev = &busRegArea->devreg[devIdx]; /*get pointer to target flash device*/

    max_blockCnt = flash_dev->d_data1; /*get max block number for this target flash device [0...maxBlock-1]*/

    if (blockNo > max_blockCnt-1){ /*if requested block is outside the range of max_blockCnt -> terminate*/
        SYSCALL(SYS9,0,0,0);
    }

    flash_dev->d_data0 = dmaBuffer;/*Write the flash device’s DATA0 field with the starting physical address of the 4kb block to be written*/
    command = (blockNo << BLOCK_SHIFT)| FLASHREAD; /*build WRITE command to write to COMMAND field of device register*/

    setSTATUS(NO_INTS); /*Disable interrupts*/
    flash_dev->d_command = command;  /*write the flash device's COMMAND field*/
    dev_status = SYSCALL(SYS5,FLASHINT,flashNo,0); /*immediately issue SYS5 (wait for IO) to block the current process*/
    setSTATUS(YES_INTS); /*Enable interrupts*/

    /*Release flash device semaphore*/
    SYSCALL(SYS4,(memaddr)&devSema4_support[DEV_UNITS + flashNo],0,0);
    
    if (dev_status != READY){
        suppStruct->sup_exceptState[GENERALEXCEPT].s_v0 = -(dev_status); /*return negative of device status in v0*/
        syslvl_prgmTrap_handler(suppStruct); /*call support level program trap handler*/
    }

    /*Successful read -> Copy data from DMA buffer into uproc address space*/
    int j;
    for (j=0; j < BLOCKS_4KB; j++){
        *logicalAddr = *dmaBuffer; /*copy the word from buffer to the uproc address*/
        logicalAddr++; /*move onto next address*/
        dmaBuffer++; /*move onto next word*/
    }

    /*Check device status code to write appropriate value into v0 register*/
    suppStruct->sup_exceptState[GENERALEXCEPT].s_v0 = dev_status;
}