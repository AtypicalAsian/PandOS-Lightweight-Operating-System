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
/*#include "/usr/include/umps3/umps/libumps.h"*/

/*Support Level Data Structures*/
swap_t swap_pool[UPROCMAX * 2];    /*swap pool table*/
int semaphore_swapPool;              /*swap pool sempahore*/

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
    semaphore_swapPool = 1;
}

/**************************************************************************************************
 * DONE
 * Initialize swap pool table
 **************************************************************************************************/
void initSwapPool() {
    int k;
    for (k = 0; k < POOLSIZE; k++) {
        swapPool[k].sw_asid = NOPROC;
    }
}

/**************************************************************************************************
 * DONE
 * TO-DO  
 * BIG PICTURE - Implement Page Replacement Algorithm (using Round Robin approach)
 **************************************************************************************************/
int find_frame_swapPool(){
    static int frame_no = 0;    /*use static to retain frame_no value inside of the method*/
    frame_no = (frame_no + 1) % POOLSIZE;
    return frame_no;
}

/**************************************************************************************************
 * DONE
 * TO-DO  
 * This function ensures TLB cache consistency after the page table is updated.
 * Reference: POPS 6.4 and PandOS 4.5.2.
 **************************************************************************************************/
void update_tlb_handler(pte_entry_t *ptEntry) {
    /* Save the current value of the CP0 EntryHi register (contains VPN + ASID).
     * We will restore this later to preserve system state.
     */
    unsigned int entry_prev = getENTRYHI();

    /* Load the new page's virtual page number (VPN) and ASID into EntryHi.
     * This is necessary for TLB to find the matching page table entry
     */
    setENTRYHI(ptEntry->entryHI);
    TLBP(); /*probe the TLB to searches for a matching entry using the current EntryHi*/

    /*Check INDEX.P bit (bit 31 of INDEX)*/
    if ((KUSEG & getINDEX()) == 0){ /*If P bit == 0 -> found a matching entry*/
        setENTRYLO(ptEntry->entryLO);  /* Load the updated physical frame number and permissions into EntryLo */
        setENTRYHI(ptEntry->entryHI);  /* Re-load EntryHi just to be safe before issuing TLBWI (DO WE NEED TO DO THIS?) */
        TLBWI(); /* Write to the TLB at the index found by TLBP. This updates the cached entry to match the page table*/
    }

    /* Restore the previously saved EntryHi in CP0 register */
    setENTRYHI(entry_prev);
}

/**************************************************************************************************
 * DONE
 * TO-DO - pandOS [section 4.5.1]
 * BIG PICTURE - To read/write to a flash device, we need to perform these 2 steps:
 *      1. Write the flash device’s DATA0 field with the appropriate starting physical
 *      address of the 4k block to be read (or written); the particular frame’s starting address
 * 
 *      2. Write the flash device’s COMMAND field with the device block number 
 *      (high order three bytes) and the command to read (or write) in the lower order byte.
 * 
 * After these 2 steps, we will perform a SYS5 operation (section 3.5.5)
 * 
 * @note
 * Write the COMMAND field and issue SYS5 atomically (disable interrupts before, enable after) 
 * to ensure interrupt always happen after the SYS5
 * 
 * Each u-proc is associated with its own flash device, already initialized with backing store data.
 * Flash device blocks 0 to 30 store .text and .data, block 31 stores the stack page
 **************************************************************************************************/
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

/**************************************************************************************************
 * DONE
 * TO-DO  
 * BIG PICTURE
 *      1. Determine the page number (denoted as p) of the missing TLB entry by
 *         inspecting EntryHi in the saved exception state located at the start of the
 *         BIOS Data Page. [Section 3.4]
 *      2. Get the Page Table entry for page number p for the Current Process. This
 *         will be located in the Current Process’s Page Table, which is part of its 
 *         Support Structure.
 *      3. Write this Page Table entry into the TLB. This is a three-set process: setENTRYHI, setENTRYLO, TLBWR
 *      4. Return control to current process
 **************************************************************************************************/
void uTLB_RefillHandler() {
    /*Step 1: Determine missing page number*/
    /*virtual addr split into: VPN and Offset -> to isolate the virtual page number, we mask out 
    the offset bits, then shift right by 12 bits to get the page number*/

    state_PTR saved_except_state = (state_PTR) BIOSDATAPAGE; /*get the saved exception state located at start of BIOS data page*/
    unsigned int entryHI = saved_except_state->s_entryHI;   /*get entryHI*/
    unsigned int missing_virtual_pageNum = (entryHI & 0xFFFFF000) >> VPNSHIFT; /*mask offset bits and shift right 12 bits to get VPN*/
    missing_virtual_pageNum %= 32;

    /*WHAT IF VPN exceed size of 32 we defined in types.h? Mod 32?*/

    /*Step 2: Get matching page table entry for missing page number of current process*/
    pte_entry_t page_entry = currProc->p_supportStruct->sup_privatePgTbl[missing_virtual_pageNum];
    /*Technically, uTLB_RefillHandler method is part of phase 2 so we can access currProc global var?*/
    /*Otherwise, we can use sys8 to access the support structure of the current process*/


    /*Step 3: Write page table entry into TLB -> 3-step process: setENTRYHI, setENTRYLO, TLBWR*/
    setENTRYHI(page_entry.entryHI);
    setENTRYLO(page_entry.entryLO);
    TLBWR();

    /*Step 4: Return control to current process (context switch)*/
    LDST(saved_except_state);
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

    SYSCALL(PASSEREN, (int)&semaphore_swapPool, 0, 0);

    int asid = supportStruct->sup_asid;
    int missingPageNum = ((supportStruct->sup_exceptState[PGFAULTEXCEPT].s_entryHI) & MISSINGPAGESHIFT) >> VPNSHIFT;

    frameNum = selectFrame();
    frameAddress = (frameNum * PAGESIZE) + FRAMEADDRSHIFT;

    if (swapPool[frameNum].sw_asid != NOPROC) {
        setSTATUS(INTSOFF);

        swapPool[frameNum].sw_pte->entryLO &= VALIDOFF;
        update_tlb_handler(swapPool[frameNum].sw_pte);

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
    update_tlb_handler(&(supportStruct->sup_privatePgTbl[blockID]));

    setSTATUS(INTSON);

    SYSCALL(VERHOGEN, (int)&semaphore_swapPool, 0, 0);

    returnControlSup(supportStruct, PGFAULTEXCEPT);
}