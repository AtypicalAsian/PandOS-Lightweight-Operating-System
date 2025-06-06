/**************************************************************************************************  
 * @file vmSupport.c  
 *  
 * @brief  
 * This module implements the TLB exception handler (the Pager), TLB refill handler alongside other
 * functions that help with initializing swap pool, device semaphores, swap pool semaphores, and 
 * writing/reading to/from flash devices.
 * 
 *  
 * @note  
 * The TLB-Refill event handler code will be implemented in vmSupport.c and referenced for Phase 2 code.
 * The module declares the Swap Pool table and the accompanying Swap Pool semaphore as local  
 * (module-wide) variables in vmSupport.c, rather than as global variables in initProc.c. A public  
 * function, initSwapStructs(), is also defined here and called in initProc.c by test() to initialize both 
 * the Swap Pool table and the semaphore.
 * 
 * @ref
 * pandOS - sections 4.2, 4.3, 4.4, 4.5
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
#include "/usr/include/umps3/umps/libumps.h"

/*Data structures and Variables Declaration*/
int semaphore_swapPool;              /*swap pool sempahore*/
swap_pool_t swap_pool[SWAP_POOL_CAP];    /*swap pool table*/

/**************************************************************************************************
 * @brief Initializes the Swap Pool Table, Swap Pool Semaphore and Device Semaphores
 *
 * @param: None
 * @return: None
 * 
 * @ref
 * pandOS - section 4.11.2
 **************************************************************************************************/
void initSwapStructs(){
    /*Initialize swap pool semaphore*/
    semaphore_swapPool = SWAP_SEMAPHORE_INIT; /*initialize swap pool semaphore to 1*/

    /*Initialize the swap pool table*/
    int i;
    for (i=0; i < SWAP_POOL_CAP; i++){
        swap_pool[i].asid = FREE; /*init swap pool frames as unoccupied (-1)*/
    }

    /*Initialize associated semaphores*/
    int j;
    for (j=0; j < DEVICE_TYPES * DEV_UNITS; j++){
        devSema4_support[j] = SUPP_SEMA4_INIT; /*initialize device semaphores to 1*/
    }
}

/**************************************************************************************************
 * @brief Implements a simple Round Robin page replacement algorithm for the swap pool. As we cycle
 * through frames, we check to see if it's occupied or not to replace the number of write-backs to
 * flash devices
 *
 * @param: None
 * @return: integer index of next frame in swap pool to be used for page replacement
 * 
 * @ref
 * pandOS - section 4.5.4 & 4.10
 **************************************************************************************************/
int find_frame_swapPool(){
    static int last_replaced_idx = 0; /*last index of page swapped*/
    int iterator = 0; 

    /*Search the swap table starting from last_replaced_idx for a free page*/
    while (iterator < SWAP_POOL_CAP && swap_pool[(last_replaced_idx + iterator) % SWAP_POOL_CAP].asid != FREE){
        iterator++;
    }

    /*If no free swap frame was found, set iterator to 1 to evict the next immediate page (unfortunate)*/
    if (iterator == SWAP_POOL_CAP) {
        iterator = 1;
    }

    /*Update and return the new replacement index*/
    last_replaced_idx = (last_replaced_idx + iterator) % SWAP_POOL_CAP;
    return last_replaced_idx;
}

/**************************************************************************************************
 * @brief
 *  Performs a flash device read or write operation
 *
 * @details
 *  This function interacts with a flash device, either reading a block into a memory
 *  frame or writing a block from a memory frame to the flash device. It:
 *    1. Retrieves the current process’s support structure.
 *    2. Computes the address of the flash device register.
 *    3. Locks the flash device semaphore
 *    4. Writes the memory frame address (to be read from or wrriten to) into the flash device’s data register
 *    5. Constructs a command code based on the operation type (read or write).
 *    6. Disables interrupts, sends the command to the flash device, and waits for I/O completion.
 *    7. Re-enables interrupts and unlocks the flash device semaphore.
 *    8. Checks the device status and, if an error occurred, invokes the program trap handler.
 *
 * @params:
 *      1. deviceNum - flash device number
 *      2. block_num - The block number on the flash device (each block = 4KB). This 
 *                     directly corresponds to the page number (missing or occupied)
 *      3. op_type - The operation type: 3 for flash write, or 2 for flash read
 *      4. frame_dest - The physical address of the memory frame that serves as the source (for writes)
 *                      or destination (for reads)
 * @return: None
 * 
 * 
 * @ref
 *  pandos - section 4.5.1
 *  pops   - section 5.4
 **************************************************************************************************/
void flash_read_write(int deviceNum, int block_num, int op_type, int frame_dest) {
    /*Local variables to thid method*/
    unsigned int device_status; /*Status returned by the flash device after the operation*/
    unsigned int command;       /*command to write to COMMAND field of flash device*/
    device_t* f_device;      /*pointer to the flash device reg*/

    /*Retrieve the current process support structure*/
    support_t *currSuppStruct = (support_t*) SYSCALL(SYS8,0,0,0); 

    /*Calculate address of specific flash device register block*/
    int devIdx = (FLASHINT-DISKINT) * DEVPERINT + deviceNum;
    SYSCALL(SYS3, (memaddr)&devSema4_support[(DEV_UNITS) + deviceNum], 0, 0); /*Perform SYS3 to lock flash device semaphore*/
    
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
    f_device = &busRegArea->devreg[devIdx];

    /*Write the flash device’s DATA0 field with the starting physical address of the 4kb block to be read (or written)*/
    f_device->d_data0 = frame_dest;

    /*Build command code*/
    if (op_type == FLASHWRITE){ /*If it's a write operation*/
        command = (block_num << BLOCK_SHIFT)| FLASHWRITE;
    }
    else { /*If it's a read operation*/
        command = (block_num << BLOCK_SHIFT) | FLASHREAD;
    }

    setSTATUS(NO_INTS); /*Disable interrupts*/
    f_device->d_command = command;  /*write the flash device's COMMAND field*/
    device_status = SYSCALL(SYS5,FLASHINT,deviceNum,0); /*immediately issue SYS5 (wait for IO) to block the current process*/
    setSTATUS(YES_INTS); /*enable interrupts*/
    
    /*Perform SYS4 to unlock flash device semaphore*/
    SYSCALL(SYS4, (memaddr)&devSema4_support[(DEV_UNITS) + deviceNum], 0, 0);
    /*If operation failed (check device status) -> program trap handler*/
    if (device_status != READY){
        syslvl_prgmTrap_handler(currSuppStruct);
    }
}

/**************************************************************************************************
 * @brief
 * This function ensures TLB cache consistency (with uprocs' page tables) after page tables are updated.
 * 
 * @details
 * This function is called after the OS updates a page table entry. It performs the following steps:
 *   1. Loads the new page table entry’s EntryHi into CP0, so that the TLB can find the 
 *      corresponding entry.
 *   2. Calls TLBP() to probe the TLB for a matching entry using the updated EntryHi.
 *   3. If a matching entry is found (valid INDEX.P bit), the function:
 *         - Loads the new page's physical frame number into entryLO
 *         - Writes the updated entry back into the TLB using TLBWI()
 * 
 * @param: ptEntry - updated page table entry
 * @return: None
 * 
 * @ref 
 * POPS 6.4, 7.1 and PandOS 4.5.2.
 **************************************************************************************************/
void update_tlb_handler(pte_entry_t *ptEntry) {
    setENTRYHI(ptEntry->entryHI); /* Load the page of interest's virtual page number (VPN) and ASID into EntryHi*/
    TLBP(); /*probe the TLB to searches for a matching entry using the current EntryHi*/

    /*In Index CP0 control register, check INDEX.P bit (bit 31 of INDEX). We perform bitwise AND with 0x8000000 to isolate bit 31*/
    if ((P_BIT_MASK & getINDEX()) == 0){ /*If P bit == 0 -> found a matching entry. Else P bit == 1 if not found*/
        setENTRYLO(ptEntry->entryLO); /*set content of entryLO to write to Index.TLB-INDEX*/
        TLBWI(); /* Write content of entryHI and entryLO CP0 registers into Index.TLB-INDEX -> This updates the cached entry to match the page table*/
    }
}


/**************************************************************************************************
 * @brief Handles TLB-refill exceptions
 * 
 * @details
 * This function performs the following steps:
 *   1. Determines the virtual page number (VPN) of the missing TLB entry
 *   2. Retrieves the corresponding page table entry from the current process's private page table.
 *   3. Writes this page table entry into the TLB using a 3-step procedure: setting CP0 EntryHi,
 *      setting CP0 EntryLo, and issuing TLBWR().
 *   4. Restores the saved exception state & return control to the current process.
 * 
 * @param: None
 * @return: None
 * 
 * @ref
 * pandOS - section 4.3
 * POP - section 6.4, 6.3.2
 **************************************************************************************************/
void uTLB_RefillHandler() {
    /*Step 1: Determine missing page number*/
    state_PTR saved_except_state = (state_PTR) BIOSDATAPAGE; /*get the saved exception state located at start of BIOS data page*/
    unsigned int entryHI = saved_except_state->s_entryHI;   /*get entryHI*/
    /*virtual addr split into: VPN (19 higher bits) and other 12 lower bits -> to isolate the virtual page number, we mask out 
    the lower 12 bits, then shift right by 12 bits*/
    unsigned int missing_virtual_pageNum = (entryHI & VPN_MASK) >> SHIFT_VPN; /*mask offset bits and shift right 12 bits to get VPN*/
    missing_virtual_pageNum %= 32;

    /*Step 2: Get matching page table entry for missing page number of current process*/
    pte_entry_t page_entry = currProc->p_supportStruct->sup_privatePgTbl[missing_virtual_pageNum];
    /*Technically, uTLB_RefillHandler method is part of phase 2 so we can access currProc global var*/
    /*Otherwise, we can use sys8 to access the support structure of the current process (will be slower)*/

    /*Step 3: Write page table entry into TLB -> 3-step process: setENTRYHI, setENTRYLO, TLBWR*/
    setENTRYHI(page_entry.entryHI);
    setENTRYLO(page_entry.entryLO);
    TLBWR();

    /*Step 4: Return control to current process (context switch)*/
    LDST(saved_except_state);
}

/**************************************************************************************************
 * @brief
 * The TLB exception handler (or Pager) handles page faults, including page fault on load, page 
 * fault on store op, attempted write to read only page
 * 
 * @details
 * This function performs the following steps:
 *       1.Obtain Current Process’s Support Structure (SYS8)
 *       2.Identify the cause of the TLB exception from sup_exceptState[0].Cause
 *       3.If the cause is a "Modification" exception, treat it as a program trap
 *       Otherwise:
 *       4.Gain mutual exclusion over the Swap Pool Table (SYS3 - P operation)
 *       5.Determine the missing page number
 *       6.Pick a frame from the Swap Pool (determined by the page replacement algorithm)
 *       7.Check if the frame is occupied by another process’s page
 *       8.If occupied, perform the following steps:
 *           - Mark the old page as invalid in the previous process’s Page Table.
 *           - Update the TLB, ensuring it reflects the invalidated page.
 *           - Write the old page back to its backing store (write to flash device)
 *       9.Load the missing page from the backing store into the selected frame
 *       10.Update the Swap Pool Table to reflect the new contents
 *       11.Update the Page Table for the new process, marking the page as valid (V bit)
 *       12.Update the TLB to include the new page
 *       13.Release mutual exclusion over the Swap Pool Table (SYS4 - V operation)
 *       14.Retry the instruction that caused the page fault using LDST
 * 
 * @param: None
 * @return: None
 * 
 * @note
 * To update tlb there are 2 approaches
 *      1. Probe the TLB (TLBP) to see if the newly updated TLB entry is indeed cached in the TLB. 
 *         If so (Index.P is 0), rewrite (update) that entry (TLBWI) to match the entry in the Page Table.
 *      2. Erase ALL the entries in the TLB (TLBCLR) - implement this before implementing the first approach
 * 
 * @ref
 * pandOS - section 4.4
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
    unsigned int missing_page_no;
    /*----------------------------------------------------------*/

    /*Step 1: Obtain current process support structure via syscall number 8*/
    currProc_supp_struct = (support_t*) SYSCALL(SYS8,0,0,0);

    /*Step 2: Identify Cause of the TLB Exception from sup_exceptState field of support structure*/
    exception_cause = (currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT].s_cause & GETEXCPCODE) >> CAUSESHIFT;

    /*Step 3: If the exception code is a "modification" type, treat as program trap*/
    if (exception_cause == 1){
        syslvl_prgmTrap_handler(currProc_supp_struct); /*support level program trap handler*/
    }
    else{
        /*Step 4: First, gain mutual exclusion of swap pool via SYS3*/
        SYSCALL(SYS3,(int)&semaphore_swapPool,0,0);

        /*Step 5: Compute missing page number*/
        missing_page_no = (currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT].s_entryHI & VPN_MASK) >> SHIFT_VPN;

        /*Step 6: Pick a VICTIM (frame from swap pool)*/
        free_frame_num = find_frame_swapPool();
        frame_addr = (free_frame_num * PAGESIZE) + POOLBASEADDR; /*Calculate the starting address of the frame (4KB block)*/
        /*We get frame address by multiplying the page size with the frame number then adding the offset which is the starting address of the swap pool*/

        /*Step 7 + 8: If the frame is occupied -> need to evict it (invalidate the page occupying this frame)*/
        if (swap_pool[free_frame_num].asid != FREE){
            /*Updating TLB and Swap Pool must be atomic -> DISABLE INTERRUPTS - pandOS [section 4.5.3]*/
            setSTATUS(NO_INTS);

            /*Step 1: Mark old page currently occupying the frame number as invalid*/
            swap_pool[free_frame_num].ownerEntry->entryLO &= VALIDOFF; /*go to page table entry of owner process and set valid bit to off*/

            /*Step 2: Update the TLB*/
            update_tlb_handler(swap_pool[free_frame_num].ownerEntry);

            /*ENABLE INTERRUPTS*/
            setSTATUS(YES_INTS);

            unsigned int occp_pageNum = swap_pool[free_frame_num].pg_number; /*get the page number of the page occupying the frame at swap_pool[frame_number]*/
            occp_pageNum = occp_pageNum % 32; /*mod to map page to range 0-31*/    
            unsigned int occp_asid = swap_pool[free_frame_num].asid; /*get ASID of process whose page owns the frame at swap_pool[frame_number]*/
            flash_no = occp_asid - 1; /*Get corresponding flash device number*/

            /*Step 3: Write the old page back to its backing store (flash device) - pandOS [section 4.5.1]*/
            flash_read_write(flash_no, occp_pageNum,FLASHWRITE, frame_addr);
        }
        /*If frame is not occupied*/

        /*Step 9:Load missing page from backing store into the selected frame*/

        asid = currProc_supp_struct->sup_asid; /*Get process asid to map to flash device number*/
        flash_no = asid - 1; /*Get flash device number associated with the process asid*/
        missing_page_no = missing_page_no % 32; /*mod to map page to range 0-31*/

        /*Perform flash read operation*/
        flash_read_write(flash_no, missing_page_no, FLASHREAD,frame_addr);

        /*Step 10: Update the Swap Pool Table to reflect the new contents (atomic operations)*/
        /*First, we disable Interrupts by getting current status and clearing the IEc (global interrupt) bit*/
        setSTATUS(NO_INTS);

        /*Update swap pool table with new entry*/
        swap_pool[free_frame_num].asid = asid; /*set asid of the u-proc that now owns this frame*/
        swap_pool[free_frame_num].pg_number = missing_page_no; /*record virtual page number that is now occupying this frame*/
        swap_pool[free_frame_num].ownerEntry = &(currProc_supp_struct->sup_privatePgTbl[missing_page_no]); /*store pointer to page table entry for this page*/

        /*Step 11: Update the Page Table for the new process, marking the page as valid (V bit) & mark D bit on (ensure page is dirty)*/
        currProc_supp_struct->sup_privatePgTbl[missing_page_no].entryLO = frame_addr | D_BIT_SET | V_BIT_SET; /*set the valid bit and dirty bit in entryLO*/

        /*Step 12: Update the TLB to include the new page (optimization)*/
        update_tlb_handler(&(currProc_supp_struct->sup_privatePgTbl[missing_page_no]));

        /*TLBCLR();*/ /*old approach - erase ALL the entries in the TLB*/

        /*Re-enable interrupts*/
        setSTATUS(YES_INTS);

        /*Step 13: Perform SYS4 to release mutex on swap pool table*/
        SYSCALL(SYS4,(int)&semaphore_swapPool,0,0);

        /*Step 14: Return control (context switch) to the instruction that caused the page fault*/
        LDST(&(currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT]));
    }
}