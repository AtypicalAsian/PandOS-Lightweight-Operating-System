#include "/usr/include/umps3/umps/libumps.h"

#include "../h/exceptions.h"
#include "../h/initial.h"

#include "../h/initProc.h"
#include "../h/sysSupport.h"
#include "../h/vmSupport.h"
#include "../h/deviceSupportDMA.h"  


#define FLASHSEM 1  

swap_t swapPool[POOLSIZE];
semaphore swapSemaphore;


void initSwapPool() {
    int i, j, k;
    for (i = 0; i < DEVICE_TYPES; i++) {
        for (j = 0; j < DEVICE_INSTANCES; j++) {
            support_device_sems[i][j] = 1;
        }
    }

    swapSemaphore = 1;
    masterSema4 = 0;

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

        SYSCALL(PASSEREN, (memaddr)&support_device_sems[FLASHSEM][flashNum], 0, 0);

        deviceStatus = flashOp(flashNum, blockID, frameAddress, FLASHWRITE);

        SYSCALL(VERHOGEN, (memaddr)&support_device_sems[FLASHSEM][flashNum], 0, 0);

        if (deviceStatus != READY) {
            trapExcHandler(supportStruct);
        }
    }

    blockID = missingPageNum;
    blockID = blockID % MAXPAGES;

    flashNum = asid - 1;

    SYSCALL(PASSEREN, (memaddr)&support_device_sems[FLASHSEM][flashNum], 0, 0);

    deviceStatus = flashOp(flashNum, blockID, frameAddress, FLASHREAD);

    SYSCALL(VERHOGEN, (memaddr)&support_device_sems[FLASHSEM][flashNum], 0, 0);

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