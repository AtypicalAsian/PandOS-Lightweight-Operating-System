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
swap_pool_t swap_pool[MAXFRAMES];    /*swap pool table*/
int semaphore_swapPool;              /*swap pool sempahore*/

/**************************************************************************************************
 * DONE
 * Initialize swap pool table and accompanying semaphores
 **************************************************************************************************/
void init_swap_structs(){
    /*initialize swap pool semaphore to 1 -> free at first*/
    semaphore_swapPool = 1;
    masterSema4 = 0;

    /*initialize swap pool table*/
    int i;
    for (i=0;i<MAXFRAMES;i++){
        swap_pool[i].asid = FREEFRAME; /*If a frame is unoccupied, its ASID entry is set to -1 [section 4.4]*/
    }
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
int flash_read_write(int deviceNum, int block_num, int op_type, int frame_dest){

    unsigned int command;   
    device_t *flashDevice; 
    unsigned int maxBlock;

    /* Get the pointer to the flash device based on its number */
    flashDevice = (device_t *) (0x10000054 + ((FLASHINT - DISKINT) * (8 * DEVREGSIZE)) + (deviceNum * DEVREGSIZE));
    maxBlock = flashDevice->d_data1;  

    /* Check if the requested sector is valid */
    if (block_num >= maxBlock) {
        terminate(NULL);  
    }

    /* Prepare the command based on the operation type */
    if (op_type == READFLASH) {
        command = READFLASH | (block_num << 8);  
    } else if (op_type == WRITEFLASH) {
        command = WRITEFLASH | (block_num << 8);  
    } else {
        return -1; 
    }


    flashDevice->d_data0 = frame_dest;

    /* Disable interrupts for the operation */
    setSTATUS(INT_OFF);
    flashDevice->d_command = command;
    unsigned int deviceStatus = SYSCALL(SYS5, FLASHINT, deviceNum, 0);
    
    /* Re-enable interrupts */
    setSTATUS(INT_ON);

    return deviceStatus;  /* Return the device status */
}

/**************************************************************************************************
 * DONE
 * TO-DO  
 * BIG PICTURE - Implement Page Replacement Algorithm (using Round Robin approach)
 **************************************************************************************************/
int find_frame_swapPool(){
    static int frame_no = 0;    /*use static to retain frame_no value inside of the method*/
    frame_no = (frame_no + 1) % MAXFRAMES;
    return frame_no;
}

/**************************************************************************************************
 * DONE
 * TO-DO  
 * This function ensures TLB cache consistency after the page table is updated.
 * Reference: POPS 6.4 and PandOS 4.5.2.
 **************************************************************************************************/
void update_tlb_handler(pte_entry_t *new_page_table_entry){

    /* Save the current value of the CP0 EntryHi register (contains VPN + ASID).
     * We will restore this later to preserve system state.
     */
    unsigned int entry_prev = getENTRYHI();

    /* Load the new page's virtual page number (VPN) and ASID into EntryHi.
     * This is necessary for TLB to find the matching page table entry
     */
    setENTRYHI(new_page_table_entry->entryHI);
    TLBP(); /*probe the TLB to searches for a matching entry using the current EntryHi*/

    /*Check INDEX.P bit (bit 31 of INDEX)*/
    if ((getINDEX() & KUSEG) == 0){ /*If P bit == 0 -> found a matching entry*/
        setENTRYHI(new_page_table_entry->entryHI);  /* Re-load EntryHi just to be safe before issuing TLBWI (DO WE NEED TO DO THIS?) */
        setENTRYLO(new_page_table_entry->entryLO);  /* Load the updated physical frame number and permissions into EntryLo */
        TLBWI(); /* Write to the TLB at the index found by TLBP. This updates the cached entry to match the page table*/
    }

    /* Restore the previously saved EntryHi in CP0 register */
    setENTRYHI(entry_prev);
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
void uTLB_RefillHandler(){
    /*Step 1: Determine missing page number*/
    /*virtual addr split into: VPN and Offset -> to isolate the virtual page number, we mask out 
    the offset bits, then shift right by 12 bits to get the page number*/
    int missing_pageNo = (EXCSTATE->s_entryHI & MISSINGPAGESHIFT) >> VPNSHIFT;
    missing_pageNo %= MAX_PAGES;
    /*WHAT IF VPN exceed size of 32 we defined in types.h? Mod 32?*/

    /*Step 2: Get matching page table entry for missing page number of current process*/

    /*Step 3: Write page table entry into TLB -> 3-step process: setENTRYHI, setENTRYLO, TLBWR*/
    setENTRYHI(currProc->p_supportStruct->sup_privatePgTbl[missing_pageNo].entryHI);
    setENTRYLO(currProc->p_supportStruct->sup_privatePgTbl[missing_pageNo].entryLO);
    TLBWR();

    /*Step 4: Return control to current process (context switch)*/
    return_control();
}


/**************************************************************************************************
 * ALMOST DONE
 * BIG PICTURE
 * TLB refills managed by refill handler
 * Pager manages other page faults (page fault on load, page fault on store op, attemp write
 *          to read only page?should not occur in Pandos so treat as program trap)
 * 
 * @details
 * STEPS (Section 4.4 - pandOS)
 *       1.Obtain Current Process’s Support Structure (SYS8)
 *       2.Identify the cause of the TLB exception from `sup_exceptState[0].Cause`
 *       3.If the cause is a "Modification" exception, treat it as a program trap
 * 
 *       Otherwise:
 *       4.Gain mutual exclusion over the Swap Pool Table (SYS3 - P operation)
 *       5.Determine the missing page number (EntryHi in the exception state).
 *       6.Pick a frame from the Swap Pool (determined by the page replacement algorithm).
 *       7.Check if the frame is occupied by another process’s page
 *       8.If occupied, perform the following steps:
 *           - Mark the old page as invalid in the previous process’s Page Table.
 *           - Update the TLB, ensuring it reflects the invalidated page.
 *           - Write the old page back to its backing store (flash device).
 *       9.Load the missing page from the backing store into the selected frame.
 *       10.Update the Swap Pool Table to reflect the new contents.
 *       11.Update the Page Table for the new process, marking the page as valid (V bit)
 *       12.Update the TLB to include the new page.
 *       13.Release mutual exclusion over the Swap Pool Table (SYS4 - V operation).
 *       14.Retry the instruction that caused the page fault using LDST.
 * 
 * @note
 * To update tlb there are 2 approaches
 *      1. Probe the TLB (TLBP) to see if the newly updated TLB entry is indeed cached in the TLB. 
 *         If so (Index.P is 0), rewrite (update) that entry (TLBWI) to match the entry in the Page Table.
 *      2. Erase ALL the entries in the TLB (TLBCLR) - implement this before implementing the first approach
 * 
 * @todo
 *      - implement occupied_frame_handler helper method
 *      - implement program_trap_handler helper method
 *      - implement flash_read_write helper method
 *      - check step 9 for correctness (refer to Pandos docs)
 *      - optimize step 12 (after testing)
 **************************************************************************************************/
void tlb_exception_handler(){ /*--> Otherwise known as the Pager*/
    /*If we're here, page fault has occured*/

    /*--------------Declare local variables---------------------*/
    int frameNum = 0;
    int blockID;
    unsigned int frameAddress;
    pte_entry_t *pageTableEntry;
    support_t *supportStruct;
    int deviceStatus;
    int flashNum;
    /*----------------------------------------------------------*/


    supportStruct = (support_t *)SYSCALL(SYS8, 0, 0, 0);
    int cause = (supportStruct->sup_exceptState[PGFAULTEXCEPT].s_cause & 0x0000007C) >> CAUSESHIFT;

    if (cause == 1) {
        program_trap_handler(supportStruct);
    }

    SYSCALL(SYS3, (int)&semaphore_swapPool, 0, 0);

    int asid = supportStruct->sup_asid;
    int missingPageNum = ((supportStruct->sup_exceptState[PGFAULTEXCEPT].s_entryHI) & MISSINGPAGESHIFT) >> VPNSHIFT;

    frameNum = selectFrame();
    frameAddress = (frameNum * PAGESIZE) + FRAME_SHIFT;

    /* If the frame is occupied, update the Page Table, TLB, and backing store of the occupying process */
    if (swap_pool[frameNum].asid != FREEFRAME) {
        /* Update TLB and swap pool atomically */
        setSTATUS(INT_OFF);

        swap_pool[frameNum].ownerEntry->entryLO &= VALIDBITOFF;
        update_tlb_handler(swap_pool[frameNum].ownerEntry);
        setSTATUS(INT_ON);

        blockID = swap_pool[frameNum].pg_number;
        blockID = blockID % MAX_PAGES;

        flashNum = swap_pool[frameNum].asid - 1;

        /* Mutex on flash device semaphore */
        SYSCALL(SYS3, (memaddr) &devRegSem[DEV_INDEX(4, flashNum, FALSE)], 0, 0);

        /* Perform the flash write operation */
        deviceStatus = flash_read_write(flashNum,blockID,WRITEFLASH,frameAddress);

        /* Release semaphore for the flash device */
        SYSCALL(SYS4, (memaddr) &devRegSem[DEV_INDEX(4, flashNum, FALSE)], 0, 0);

        /* If device failed to execute command, call trap exception handler */
        if (deviceStatus != READY) {
            program_trap_handler(supportStruct);
        }
    }

    /* Calculate Block ID */
    blockID = missingPageNum;
    blockID = blockID % MAX_PAGES;

    flashNum = asid - 1;

    /* Mutex on flash device semaphore */
    SYSCALL(SYS3, (memaddr) &devRegSem[DEV_INDEX(4, flashNum, FALSE)], 0, 0);

    /* Perform the flash read operation */
    deviceStatus = flash_read_write(flashNum, blockID, READFLASH, frameAddress);

    /* Release semaphore for the flash device */
    SYSCALL(SYS4, (memaddr) &devRegSem[DEV_INDEX(4, flashNum, FALSE)], 0, 0);

    /* If device failed to execute command, call trap exception handler */
    if (deviceStatus != READY) {
        program_trap_handler(supportStruct);
    }

    /* Update the Swap Pool table and the Current Process's Page Table */
    pageTableEntry = &(supportStruct->sup_privatePgTbl[blockID]);
    swap_pool[frameNum].asid = asid;
    swap_pool[frameNum].pg_number = missingPageNum;
    swap_pool[frameNum].ownerEntry = pageTableEntry;

    /* Atomically update TLB and page table entry */
    setSTATUS(INT_OFF);

    swap_pool[frameNum].ownerEntry->entryLO = frameAddress | V_BIT_SET | D_BIT_SET;
    update_tlb_handler(&(supportStruct->sup_privatePgTbl[blockID]));

    setSTATUS(INT_ON);

    /* Release mutual exclusion over the Swap Pool table */
    SYSCALL(SYS4, (int)&semaphore_swapPool, 0, 0);

    /* Return control to the current process to retry the instruction that caused the page fault */
    returnControlSup(supportStruct, PGFAULTEXCEPT);
}