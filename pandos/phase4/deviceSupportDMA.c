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




/*re-usable*/
int diskOp(int diskNum, int track, int head, int sector, int buffer, int operation) {
    unsigned int command;           
    int device_status;               
    devregarea_t *diskDevice;        

    
    diskDevice = (devregarea_t *) RAMBASEADDR; 

    setSTATUS(NO_INTS);
    
   
    command = (track << 8) | 2;
    diskDevice->devreg[diskNum].d_command = command;  
    

    device_status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    setSTATUS(YES_INTS);  


    if(device_status != READY) {
        device_status = -(device_status);  
    } else {

        setSTATUS(NO_INTS);
        diskDevice->devreg[diskNum].d_data0  = buffer;  
        

        command = (head << 16) | (sector << 8) | operation; 
        diskDevice->devreg[diskNum].d_command = command;  
        
       
        device_status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
        setSTATUS(YES_INTS);  


        if(device_status != READY) {
            device_status = -(device_status);  
        }
    }

    return device_status;  
}


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
void disk_put(support_t *current_support) {
    int maxPlatter, maxSector, maxCylinder, diskPhysicalGeometry, maxCount; 
    int seekCylinder, platterNum, device_status; 
    int diskNum, sectorNum;                       
    memaddr *buffer;                             
    memaddr *virtualAddr;                         
    devregarea_t *devReg;                      


    devReg = (devregarea_t *) RAMBASEADDR; 


    virtualAddr = (memaddr *) current_support->sup_exceptState[GENERALEXCEPT].s_a1;
    diskNum = current_support->sup_exceptState[GENERALEXCEPT].s_a2;
    sectorNum = current_support->sup_exceptState[GENERALEXCEPT].s_a3;


    diskPhysicalGeometry = devReg->devreg[diskNum].d_data1;

    maxCylinder = (diskPhysicalGeometry >> 16);
    maxPlatter = (diskPhysicalGeometry & 0x0000FF00) >> 8;
    maxSector = (diskPhysicalGeometry & 0x000000FF);
    maxCount = maxCylinder * maxPlatter * maxSector;

    if(((int) virtualAddr < KUSEG) || (sectorNum > maxCount)) {
        get_nuked(NULL); 
    }

    seekCylinder = sectorNum / (maxPlatter * maxSector);
    sectorNum = sectorNum % (maxPlatter * maxSector);
    platterNum = sectorNum / maxSector;
    sectorNum = sectorNum % maxSector;

    SYSCALL(PASSEREN, (memaddr)&devSema4_support[diskNum], 0, 0);
    
    buffer = (memaddr *)(DISKSTART + (diskNum * PAGESIZE));
    memaddr *originBuff = (DISKSTART + (diskNum * PAGESIZE));
    
    int i;
    for (i = 0; i < PAGESIZE / WORDLEN; i++) {
        *buffer++ = *virtualAddr++;  
    }

    device_status = diskOp(diskNum, seekCylinder, platterNum, sectorNum, originBuff, 4);

    SYSCALL(VERHOGEN, (memaddr)&devSema4_support[diskNum], 0, 0);

    current_support->sup_exceptState[GENERALEXCEPT].s_v0 = device_status;
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


    devReg = (devregarea_t *) RAMBASEADDR; 

    virtualAddr = (memaddr *) current_support->sup_exceptState[GENERALEXCEPT].s_a1;
    diskNum = current_support->sup_exceptState[GENERALEXCEPT].s_a2;
    sectorNum = current_support->sup_exceptState[GENERALEXCEPT].s_a3;
    
    diskPhysicalGeometry = devReg->devreg[diskNum].d_data1;

    maxCylinder = (diskPhysicalGeometry >> 16);
    maxPlatter = (diskPhysicalGeometry & 0x0000FF00) >> 8;
    maxSector = (diskPhysicalGeometry & 0x000000FF);
    maxCount = maxCylinder * maxPlatter * maxSector;

    if(((int) virtualAddr < KUSEG) || (sectorNum > maxCount)) {
        get_nuked(NULL); 
    }

    seekCylinder = sectorNum / (maxPlatter * maxSector);
    sectorNum = sectorNum % (maxPlatter * maxSector);
    platterNum = sectorNum / maxSector;
    sectorNum = sectorNum % maxSector;

    SYSCALL(PASSEREN, (memaddr)&devSema4_support[diskNum], 0, 0);

    buffer = (memaddr *)(DISKSTART + (diskNum * PAGESIZE));
    memaddr *originBuff = (DISKSTART + (diskNum * PAGESIZE));

    device_status = diskOp(diskNum, seekCylinder, platterNum, sectorNum, originBuff, 3);

    if(device_status == READY) {
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

    virtualAddr = (memaddr *) current_support->sup_exceptState[GENERALEXCEPT].s_a1;
    flashNum = current_support->sup_exceptState[GENERALEXCEPT].s_a2;
    sector = current_support->sup_exceptState[GENERALEXCEPT].s_a3;

    if ((int) virtualAddr < KUSEG) {
        get_nuked(NULL);  
    }

    SYSCALL(PASSEREN, (memaddr)&devSema4_support[DEV_UNITS + flashNum], 0, 0);

    buffer = (memaddr *) FLASHSTART + (flashNum * PAGESIZE);

    int i;
    for (i = 0; i < PAGESIZE / WORDLEN; i++) {
        *buffer++ = *virtualAddr++;
    }

    device_status = flashOp(flashNum, sector, (memaddr)buffer, FLASHWRITE);

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

    
    virtualAddr = (memaddr *) current_support->sup_exceptState[GENERALEXCEPT].s_a1;
    flashNum = current_support->sup_exceptState[GENERALEXCEPT].s_a2;
    sector = current_support->sup_exceptState[GENERALEXCEPT].s_a3;

    
    if ((int) virtualAddr < KUSEG) {
        get_nuked(NULL);  
    }

   
    SYSCALL(PASSEREN, (memaddr)&devSema4_support[DEV_UNITS + flashNum], 0, 0);

   
    buffer = (memaddr *) FLASHSTART + (flashNum * PAGESIZE);

    
    device_status = flashOp(flashNum, sector, (memaddr) buffer, FLASHREAD);

    if (device_status == READY) {
        int i;
        for (i = 0; i < PAGESIZE / WORDLEN; i++) {
            *virtualAddr++ = *buffer++;  
        }
    }

    SYSCALL(VERHOGEN, (memaddr)&devSema4_support[DEV_UNITS + flashNum], 0, 0);

    current_support->sup_exceptState[GENERALEXCEPT].s_v0 = device_status;
}




int flashOp(int flashNum, int sector, int buffer, int operation) {
    unsigned int command;   
    device_t *flashDevice; 
    unsigned int maxBlock; 

    flashDevice = (device_t *) (DEVICEREGSTART + ((FLASHINT - DISKINT) * (DEV_UNITS * DEVREGSIZE)) + (flashNum * DEVREGSIZE));
    maxBlock = flashDevice->d_data1; 

    if (sector >= maxBlock) {
        get_nuked(NULL); 
    }

    if (operation == FLASHREAD) {
        command = FLASHREAD | (sector << FLASHADDRSHIFT);  
    } else if (operation == FLASHWRITE) {
        command = FLASHWRITE | (sector << FLASHADDRSHIFT);
    } else {
        return -1; 
    }

    flashDevice->d_data0 = buffer;


    setSTATUS(NO_INTS);
    flashDevice->d_command = command; 
    
    unsigned int deviceStatus = SYSCALL(WAITIO, FLASHINT, flashNum, 0);
    
    setSTATUS(YES_INTS);

    return deviceStatus;  
}

