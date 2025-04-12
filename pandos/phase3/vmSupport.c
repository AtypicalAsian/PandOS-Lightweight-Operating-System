/**************************************************************************************************  
 * @file vmSupport.c  
 *  
 * 
 * @brief  
 * This module implements the TLB exception handler (The Pager). Additionally, the Swap Pool 
 * table and Swap Pool semaphore are local to this module. Instead of declaring them globally 
 * in initProc.c they are declared module-wide in vmSupport.c
 * 
 * @details  
 * 
 *  
 * @note  
 * 
 *  
 * @authors  
 * Nicolas & Tran  
 * View version history and changes: https://github.com/AtypicalAsian/CS372-OS-Project
 * 
 * 
 * TO-DO
 * This module implements the TLB exception handler (The Pager). Since reading and writing to each U-proc’s flash device is limited 
 * to supporting paging, this module should also contain the function(s) for 
 * reading and writing flash devices. Additionally, the Swap Pool table and Swap Pool semaphore are local to 
 * this module. Instead of declaring them globally in initProc.c they can be 
 * declared module-wide in vmSupport.c. The test function will now invoke a new “public” function initSwapStructs which will do the work 
 * of initializing both the Swap Pool table and accompanying semaphore.
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

swap_t swapPool[POOLSIZE];
semaphore swapSemaphore;

/**************************************************************************************************
 * TO-DO
 **************************************************************************************************/
void init_deviceSema4s(){
    int i, j;
    for (i = 0; i < DEVICE_TYPES; i++) {
        for (j = 0; j < DEVICE_INSTANCES; j++) {
            support_device_sems[i][j] = 1;
        }
    }
    swapSemaphore = 1;
}

void initSwapPool() {
    int k;
    for (k = 0; k < POOLSIZE; k++) {
        swapPool[k].sw_asid = NOPROC;
    }
}


int selectFrame() {
    static int selectFrame = 0;
    selectFrame = (selectFrame + 1) % POOLSIZE;
    return selectFrame;
}

void updateTLB(pte_entry_t *pageTableEntry) {
    unsigned int prevEntry = getENTRYHI();

    setENTRYHI(pageTableEntry->entryHI);
    TLBP();

    if ((getINDEX() & KUSEG) == 0) {
        setENTRYHI(pageTableEntry->entryHI);
        setENTRYLO(pageTableEntry->entryLO);
        TLBWI();
    }

    setENTRYHI(prevEntry);
}

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

void uTLB_RefillHandler() {
    int missingPageNum = (EXCSTATE->s_entryHI & MISSINGPAGESHIFT) >> VPNSHIFT;
    missingPageNum %= MAXPAGES;

    setENTRYHI(currProc->p_supportStruct->sup_privatePgTbl[missingPageNum].entryHI);
    setENTRYLO(currProc->p_supportStruct->sup_privatePgTbl[missingPageNum].entryLO);

    TLBWR();
    returnControl();
}

void pager() {
    int frameNum = 0;
    int blockID;
    unsigned int frameAddress;
    pte_entry_t *pageTableEntry;
    support_t *supportStruct;
    int deviceStatus;
    int flashNum;

    supportStruct = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    int cause = (supportStruct->sup_exceptState[PGFAULTEXCEPT].s_cause & GETEXECCODE) >> CAUSESHIFT;

    if (cause == 1) {
        trapExcHandler(supportStruct);
    }

    SYSCALL(PASSEREN, (int)&swapSemaphore, 0, 0);

    int asid = supportStruct->sup_asid;
    int missingPageNum = ((supportStruct->sup_exceptState[PGFAULTEXCEPT].s_entryHI) & MISSINGPAGESHIFT) >> VPNSHIFT;

    frameNum = selectFrame();
    frameAddress = (frameNum * PAGESIZE) + FRAMEADDRSHIFT;

    if (swapPool[frameNum].sw_asid != NOPROC) {
        setSTATUS(INTSOFF);

        swapPool[frameNum].sw_pte->entryLO &= VALIDOFF;
        updateTLB(swapPool[frameNum].sw_pte);

        setSTATUS(INTSON);

        blockID = swapPool[frameNum].sw_pageNo;
        blockID = blockID % MAXPAGES;

        flashNum = swapPool[frameNum].sw_asid - 1;

        SYSCALL(PASSEREN, (memaddr)&support_device_sems[1][flashNum], 0, 0);

        deviceStatus = flashOp(flashNum, blockID, frameAddress, FLASHWRITE);

        SYSCALL(VERHOGEN, (memaddr)&support_device_sems[1][flashNum], 0, 0);

        if (deviceStatus != READY) {
            trapExcHandler(supportStruct);
        }
    }

    blockID = missingPageNum;
    blockID = blockID % MAXPAGES;

    flashNum = asid - 1;

    SYSCALL(PASSEREN, (memaddr)&support_device_sems[1][flashNum], 0, 0);

    deviceStatus = flashOp(flashNum, blockID, frameAddress, FLASHREAD);

    SYSCALL(VERHOGEN, (memaddr)&support_device_sems[1][flashNum], 0, 0);

    if (deviceStatus != READY) {
        trapExcHandler(supportStruct);
    }

    pageTableEntry = &(supportStruct->sup_privatePgTbl[blockID]);
    swapPool[frameNum].sw_asid = asid;
    swapPool[frameNum].sw_pageNo = missingPageNum;
    swapPool[frameNum].sw_pte = pageTableEntry;

    setSTATUS(INTSOFF);

    swapPool[frameNum].sw_pte->entryLO = frameAddress | VALIDON | DIRTYON;
    updateTLB(&(supportStruct->sup_privatePgTbl[blockID]));

    setSTATUS(INTSON);

    SYSCALL(VERHOGEN, (int)&swapSemaphore, 0, 0);

    returnControlSup(supportStruct, PGFAULTEXCEPT);
}