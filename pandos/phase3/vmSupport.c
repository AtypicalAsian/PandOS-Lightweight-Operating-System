/**************************************************************************************************  
 * @file vmSupport.c  
 *  
 * @brief  
 * This module implements the TLB exception handler (the Pager) and TLB refill handler for virtual memory support.
 *
 * @details  
 * The vmSupport.c module is responsible for handling page faults by implementing the TLB 
 * exception handler (Pager). It manages the virtual memory system by initializing and operating 
 * the Swap Pool table and its accompanying Swap Pool semaphore—resources. In addition, the module 
 * provides helper functions for:
 *    - Reading from and writing to flash devices used for paging,
 *    - Enabling/disabling interrupts via updates to the status register,
 *    - Gaining and releasing mutual exclusion, and
 *    - Returning control back to a specific process after handling an exception.
 *
 * The test function in phase 3 now invokes a new public function in vmSupport.c, initSwapStructs(), which 
 * initializes both the Swap Pool table and the associated semaphore.
 * 
 * @ref
 * pandOS - sections 4.2, 4.3, 4.4, 4.5
 * 
 * @authors  
 * Nicolas & Tran  
 * View version history and changes: https://github.com/AtypicalAsian/CS372-OS-Project
 * 
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


swap_pool_t swap_pool[MAXUPROCS * 2];    /*swap pool table*/
int semaphore_swapPool;              /*swap pool sempahore*/

/**************************************************************************************************
 * Initialize the swap pool table and associated semaphores
 **************************************************************************************************/
void initSwapStructs(){
    /*Initialize the swap pool table*/
    int i;
    for (i=0; i < MAXUPROCS * 2; i++){
        swap_pool[i].asid = FREE;
    }

    /*Initialize swap pool semaphore*/
    semaphore_swapPool = SWAP_SEMAPHORE_INIT;

    /*Initialize associated semaphores*/
    int j;
    for (j=0; j < DEVICE_TYPES * DEVICE_INSTANCES; j++){
        support_device_sems[j] = SUPP_SEMA4_INIT;
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
void flash_read_write(int deviceNum, int block_num, int op_type, int frame_dest) {
    /*Local variables to thid method*/
    unsigned int device_status; /*status of the device (success or no)*/
    unsigned int max_block;     /*max number of blocks the flash device supports*/
    unsigned int command;       /*command to write to COMMAND field of flash device*/
    device_t* f_device;      /*pointer to the flash device we want to work with*/

    support_t *currSuppStruct = (support_t*) SYSCALL(SYS8,0,0,0);

    /*Method 1: Array indexing (Calculate address of specific flash device register block)*/
    int devIdx = (FLASHINT-DISKINT) * DEVPERINT + deviceNum;
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
    f_device = &busRegArea->devreg[devIdx];

    /*Compute pointer to correct device_t struct representing the selected flash device - method 2*/
    /*f_device = (device_t *) ((DEV_STARTING_REG + ((FLASHINT - DISKINT) * (DEVPERINT * DEVREGSIZE)) + (deviceNum * DEVREGSIZE)));*/

    /*Perform SYS3 to lock flash device semaphore*/
    /*SYSCALL(SYS3, (memaddr)&support_device_sems[1][deviceNum], 0, 0);*/
    SYSCALL(SYS3, (memaddr)&support_device_sems[(1 * DEVICE_INSTANCES) + deviceNum], 0, 0);


    /*Read DATA1 field to get max number of blocks the flash device supports*/
    max_block = f_device->d_data1; /*pops - [section 5.4]*/

    /*Check whether the requested block number is out of bounds*/
    if (block_num >= max_block){
        terminate(NULL);
    }

    /*Build command code*/
    if (op_type == 3){ /*If it's a write operation*/
        command = 3 | (block_num << 8);
    }
    else { /*If it's a read operation*/
        command = 2 | (block_num << 8);
    }

    /*Write the flash device’s DATA0 field with the starting physical address of the 4kb block to be read (or written)*/
    f_device->d_data0 = frame_dest;

    
    setSTATUS(INTSOFF); /*Disable interrupts*/
    f_device->d_command = command;  /*write the flash device's COMMAND field*/
    device_status = SYSCALL(SYS5,FLASHINT,deviceNum,0); /*immediately issue SYS5 (wait for IO) to block the current process*/
    setSTATUS(INTSON); /*enable interrupts*/
    
    /*Perform SYS4 to unlock flash device semaphore*/
    /*SYSCALL(SYS4, (memaddr)&support_device_sems[1][deviceNum], 0, 0);*/
    SYSCALL(SYS4, (memaddr)&support_device_sems[(1 * DEVICE_INSTANCES) + deviceNum], 0, 0);
    /*If operation failed (check device status) -> program trap handler*/
    if (device_status != READY){
        trapExcHandler(currSuppStruct);
    }
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
    unsigned int missing_virtual_pageNum = (entryHI & 0xFFFFF000) >> SHIFT_VPN; /*mask offset bits and shift right 12 bits to get VPN*/
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
void tlb_exception_handler() {
    /*If we're here, page fault has occured*/

    /*--------------Declare local variables---------------------*/
    support_t* currProc_supp_struct;
    int free_frame_num = 0;
    int flash_no;
    unsigned int frame_addr;
    unsigned int exception_cause;
    int asid;
    /*int deviceStatus;*/
    unsigned int missing_page_no;
    /*----------------------------------------------------------*/

    /*Step 1: Obtain current process support structure via syscall number 8*/
    currProc_supp_struct = (support_t*) SYSCALL(SYS8,0,0,0);

    /*Step 2: Identify Cause of the TLB Exception from sup_exceptState field of support structure*/
    exception_cause = (currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT].s_cause & GETEXCPCODE) >> CAUSESHIFT;

    /*Step 3: If the exception code is a "modification" type, treat as program trap*/
    if (exception_cause == 1){
        /*Pass execution to support level program trap handler*/
        trapExcHandler(currProc_supp_struct); /*Define this method in sysSupport.c*/
    }
    else{
        /*Step 4: First, gain mutual exclusion of swap pool via performing a SYS3 operation*/
        SYSCALL(SYS3,(int)&semaphore_swapPool,0,0);

        /*Step 5: Compute missing page number*/
        missing_page_no = (currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT].s_entryHI & 0xFFFFF000) >> SHIFT_VPN;

        /*Step 6: Pick a frame from the swap pool (Page Replacement Algorithm)*/
        free_frame_num = find_frame_swapPool();
        frame_addr = (free_frame_num * PAGESIZE) + POOLBASEADDR; /*Calculate the starting address of the frame (4KB block)*/
        /*We get frame address by multiplying the page size with the frame number then adding the offset which is the starting address of the swap pool*/

        /*Step 7 + 8: If the frame is occupied -> need to evict it (invalidate the page occupying this frame)*/
        if (swap_pool[free_frame_num].asid != -1){
            /*Updating TLB and Swap Pool must be atomic -> DISABLE INTERRUPTS - pandOS [section 4.5.3]*/
            setSTATUS(INTSOFF);

            /*Step 1: Mark old page currently occupying the frame number as invalid*/
            swap_pool[free_frame_num].ownerEntry->entryLO &= VALIDBITOFF; /*go to page table entry of owner process and set valid bit to off*/

            /*Step 2: Update the TLB*/
            update_tlb_handler(swap_pool[free_frame_num].ownerEntry);

            /*ENABLE INTERRUPTS*/
            setSTATUS(INTSON);

            unsigned int occp_pageNum = swap_pool[free_frame_num].pg_number; /*get the page number of the page occupying the frame at swap_pool[frame_number]*/
            occp_pageNum = occp_pageNum % 32; /*mod to map page to range 0-31*/    
            unsigned int occp_asid = swap_pool[free_frame_num].asid; /*get ASID of process whose page owns the frame at swap_pool[frame_number]*/
            flash_no = occp_asid - 1;

            /*Step 3: Write the old (at this point evicted) page back to its backing store (flash device) - pandOS [section 4.5.1]*/
            /*need to input: flash device no, block num, op_type, frame_num (or frame_address)*/
            flash_read_write(flash_no, occp_pageNum,FLASHWRITE, frame_addr);
        }
        /*If frame is not occupied*/
        
        /*section 4.4.1 - [pandOS]*/

        /*Step 9:Load missing page from backing store into the selected frame (at address free_fram_address)*/
        /*For this step, we essentially must do a flash device read operation to load the page into the frame*/
        asid = currProc_supp_struct->sup_asid; /*Get process asid to map to flash device number*/
        flash_no = asid - 1; /*Get flash device number associated with the process asid*/
        missing_page_no = missing_page_no % 32; /*mod to map page to range 0-31*/

        /*First, get flash device number that is associated with the current u-proc*/

        /*Perform flash read operation*/
        flash_read_write(flash_no, missing_page_no, FLASHREAD,frame_addr);

        /*Step 10: Update the Swap Pool Table to reflect the new contents (atomic operations)*/
        /*First, we disable Interrupts by getting current status and clearing the IEc (global interrupt) bit*/
        setSTATUS(INTSOFF); /*section 7.1 - [pops]*/
        

        /*Update swap pool table with new entry*/
        swap_pool[free_frame_num].asid = asid; /*set asid of the u-proc that now owns this frame*/
        swap_pool[free_frame_num].pg_number = missing_page_no; /*record virtual page number that is now occupying this frame*/
        swap_pool[free_frame_num].ownerEntry = &(currProc_supp_struct->sup_privatePgTbl[missing_page_no]); /*store pointer to page table entry for this page*/


        /*Step 11: Update the Page Table for the new process, marking the page as valid (V bit)*/
        currProc_supp_struct->sup_privatePgTbl[missing_page_no].entryLO = frame_addr | V_BIT_SET | D_BIT_SET; /*set the valid bit and dirty bit in entryLO*/

        /*Step 12: Update the TLB to include the new page (optimization)*/
        update_tlb_handler(&(currProc_supp_struct->sup_privatePgTbl[missing_page_no]));

        /*TLBCLR();*/ /*For now, we will do approach 2 - erase ALL the entries in the TLB (OPTIMIZE LATER)*/

        /*Re-enable interrupts*/
        setSTATUS(INTSON); /*section 7.1 - [pops]*/

        /*Step 13: Perform SYS4 to release mutex on swap pool table*/
        SYSCALL(SYS4,(int)&semaphore_swapPool,0,0);

        /*Step 14: Return control (context switch) to the instruction that caused the page fault*/
        LDST(&(currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT]));
    }
}