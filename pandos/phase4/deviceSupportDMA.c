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
void disk_put(memaddr *logicalAddr, int diskNo, int sectNo, support_t *support_struct) {
    /*logAddr,diskNo,sectNo,suppStruct*/
    int maxPlatter, maxSector, maxCylinder, diskPhysicalGeometry, maxCount; 
    int seekCylinder, platterNum, device_status; 
    memaddr *buffer;                              
    memaddr *virtualAddr;                         
    devregarea_t *devReg;                         
    unsigned int command;                        

    devReg = (devregarea_t *) RAMBASEADDR; 

    /*virtualAddr = (memaddr *) support_struct->sup_exceptState[GENERALEXCEPT].s_a1;*/
    /*diskNo = support_struct->sup_exceptState[GENERALEXCEPT].s_a2;*/
    /*sectNo = support_struct->sup_exceptState[GENERALEXCEPT].s_a3;*/

    diskPhysicalGeometry = devReg->devreg[diskNo].d_data1;

    maxCylinder = (diskPhysicalGeometry >> 16);
    maxPlatter = (diskPhysicalGeometry & 0x0000FF00) >> 8;
    maxSector = (diskPhysicalGeometry & 0x000000FF);
    maxCount = maxCylinder * maxPlatter * maxSector;

    if (((int)virtualAddr < KUSEG) || (sectNo > maxCount)) {
        get_nuked(NULL); 
    }

    seekCylinder = sectNo / (maxPlatter * maxSector);
    sectNo = sectNo % (maxPlatter * maxSector);
    platterNum = sectNo / maxSector;
    sectNo = sectNo % maxSector;

    SYSCALL(PASSEREN, (memaddr)&devSema4_support[diskNo], 0, 0);

    buffer = (memaddr *)(DISKSTART + (diskNo * PAGESIZE));
    memaddr *originBuff = (DISKSTART + (diskNo * PAGESIZE));

    int i;
    for (i = 0; i < PAGESIZE / WORDLEN; i++) {
        *buffer++ = *virtualAddr++;
    }

    /*Perform disk seek operation + read contents of disk sector*/
    setSTATUS(NO_INTS);
    command = (seekCylinder << 8) | 2;
    devReg->devreg[diskNo].d_command = command;
    device_status = SYSCALL(WAITIO, DISKINT, diskNo, 0);

    setSTATUS(YES_INTS);

    if (device_status == READY) {
        setSTATUS(NO_INTS);
        devReg->devreg[diskNo].d_data0 = originBuff;

        command = (platterNum << 16) | (sectNo << 8) | 4;
        devReg->devreg[diskNo].d_command = command;

        device_status = SYSCALL(WAITIO, DISKINT, diskNo, 0);
        setSTATUS(YES_INTS);

        if (device_status != READY) {
            device_status = -device_status;
        }
    } else {
        device_status = -device_status;
    }

    SYSCALL(VERHOGEN, (memaddr)&devSema4_support[diskNo], 0, 0);

    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = device_status;
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

 void disk_get(support_t *current_support) {
    int maxPlatter, maxSector, maxCylinder, diskPhysicalGeometry, maxCount; 
    int seekCylinder, platterNum, device_status; 
    int diskNum, sectorNum;                       
    memaddr *buffer;                              
    memaddr *virtualAddr;                         
    devregarea_t *devReg;                         
    unsigned int command;

    devReg = (devregarea_t *) RAMBASEADDR;

    virtualAddr = (memaddr *) current_support->sup_exceptState[GENERALEXCEPT].s_a1;
    diskNum = current_support->sup_exceptState[GENERALEXCEPT].s_a2;
    sectorNum = current_support->sup_exceptState[GENERALEXCEPT].s_a3;

    diskPhysicalGeometry = devReg->devreg[diskNum].d_data1;
    maxCylinder = (diskPhysicalGeometry >> 16);
    maxPlatter = (diskPhysicalGeometry & 0x0000FF00) >> 8;
    maxSector = (diskPhysicalGeometry & 0x000000FF);
    maxCount = maxCylinder * maxPlatter * maxSector;

    if (((int)virtualAddr < KUSEG) || (sectorNum > maxCount)) {
        get_nuked(NULL); 
    }

    seekCylinder = sectorNum / (maxPlatter * maxSector);
    sectorNum = sectorNum % (maxPlatter * maxSector);
    platterNum = sectorNum / maxSector;
    sectorNum = sectorNum % maxSector;

    SYSCALL(PASSEREN, (memaddr)&devSema4_support[diskNum], 0, 0);

    buffer = (memaddr *)(DISKSTART + (diskNum * PAGESIZE));
    memaddr *originBuff = (DISKSTART + (diskNum * PAGESIZE));

    setSTATUS(NO_INTS);

    command = (seekCylinder << 8) | 2;
    devReg->devreg[diskNum].d_command = command;
    device_status = SYSCALL(WAITIO, DISKINT, diskNum, 0);

    setSTATUS(YES_INTS);

    if (device_status == READY) {
        setSTATUS(NO_INTS);

        devReg->devreg[diskNum].d_data0 = originBuff;

        command = (platterNum << 16) | (sectorNum << 8) | 3;
        devReg->devreg[diskNum].d_command = command;

        device_status = SYSCALL(WAITIO, DISKINT, diskNum, 0);

        setSTATUS(YES_INTS);

        if (device_status != READY) {
            device_status = -device_status;
        }
    } else {
        device_status = -device_status;
    }

    if (device_status == READY) {
        int i;
        for (i = 0; i < PAGESIZE / WORDLEN; i++) {
            *virtualAddr++ = *buffer++; 
        }
    }

    SYSCALL(VERHOGEN, (memaddr)&devSema4_support[diskNum], 0, 0);

    current_support->sup_exceptState[GENERALEXCEPT].s_v0 = device_status;
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
void flash_put(support_t *current_support) {
    int flashNum, sector, device_status;
    memaddr *buffer;
    memaddr *virtualAddr;
    device_t *flashDevice;
    unsigned int command, maxBlock;

    virtualAddr = (memaddr *) current_support->sup_exceptState[GENERALEXCEPT].s_a1;
    flashNum = current_support->sup_exceptState[GENERALEXCEPT].s_a2;
    sector = current_support->sup_exceptState[GENERALEXCEPT].s_a3;

    if ((int)virtualAddr < KUSEG) {
        get_nuked(NULL);
    }

    SYSCALL(PASSEREN, (memaddr)&devSema4_support[DEV_UNITS + flashNum], 0, 0);

    buffer = (memaddr *)(FLASHSTART + (flashNum * PAGESIZE));
    memaddr *originBuff = buffer;

    int i;
    for (i = 0; i < PAGESIZE / WORDLEN; i++) {
        *buffer++ = *virtualAddr++;
    }

    flashDevice = (device_t *)(DEVICEREGSTART + ((FLASHINT - DISKINT) * (DEV_UNITS * DEVREGSIZE)) + (flashNum * DEVREGSIZE));
    maxBlock = flashDevice->d_data1;

    if (sector >= maxBlock) {
        get_nuked(NULL);
    }

    command = FLASHWRITE | (sector << FLASHADDRSHIFT);
    flashDevice->d_data0 = (memaddr)originBuff;

    setSTATUS(NO_INTS);
    flashDevice->d_command = command;
    device_status = SYSCALL(WAITIO, FLASHINT, flashNum, 0);
    setSTATUS(YES_INTS);

    SYSCALL(VERHOGEN, (memaddr)&devSema4_support[DEV_UNITS + flashNum], 0, 0);

    current_support->sup_exceptState[GENERALEXCEPT].s_v0 = device_status;
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
void flash_get(support_t *current_support) {
    int flashNum, sector, device_status;
    memaddr *buffer;
    memaddr *virtualAddr;
    device_t *flashDevice;
    unsigned int command, maxBlock;

    virtualAddr = (memaddr *) current_support->sup_exceptState[GENERALEXCEPT].s_a1;
    flashNum = current_support->sup_exceptState[GENERALEXCEPT].s_a2;
    sector = current_support->sup_exceptState[GENERALEXCEPT].s_a3;

    if ((int)virtualAddr < KUSEG) {
        get_nuked(NULL);
    }

    SYSCALL(PASSEREN, (memaddr)&devSema4_support[DEV_UNITS + flashNum], 0, 0);

    buffer = (memaddr *)(FLASHSTART + (flashNum * PAGESIZE));
    memaddr *originBuff = buffer;

    flashDevice = (device_t *)(DEVICEREGSTART + ((FLASHINT - DISKINT) * (DEV_UNITS * DEVREGSIZE)) + (flashNum * DEVREGSIZE));
    maxBlock = flashDevice->d_data1;

    if (sector >= maxBlock) {
        get_nuked(NULL);
    }

    command = FLASHREAD | (sector << FLASHADDRSHIFT);
    flashDevice->d_data0 = (memaddr)originBuff;

    setSTATUS(NO_INTS);
    flashDevice->d_command = command;
    device_status = SYSCALL(WAITIO, FLASHINT, flashNum, 0);
    setSTATUS(YES_INTS);

    if (device_status == READY) {
        int i;
        for (i = 0; i < PAGESIZE / WORDLEN; i++) {
            *virtualAddr++ = *buffer++;
        }
    }

    SYSCALL(VERHOGEN, (memaddr)&devSema4_support[DEV_UNITS + flashNum], 0, 0);

    current_support->sup_exceptState[GENERALEXCEPT].s_v0 = device_status;
}




