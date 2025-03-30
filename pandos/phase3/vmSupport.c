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

/*GLOBAL VARIABLES/DATA STRUCTRES DECLARATION*/
HIDDEN swap_pool_t swap_pool[MAXFRAMES];    /*swap pool table*/
HIDDEN int semaphore_swapPool;              /*swap pool sempahore*/

extern pcb_PTR currentProc; /*pointer to current proces PCB (defined in case we cannot access global currProc variable)*/


/*Helper Methods*/
HIDDEN int find_frame_swapPool(); /*find frame from swap pool (page replacement algo) - DONE*/
HIDDEN void occupied_frame_handler(); /*handle ops when frame occupied - NOT DONE*/
HIDDEN void update_tlb_handler(); /*Helper function to perform operations related to updating the TLB (optimization) - NOT DONE*/
HIDDEN void flash_read_write(); /*perform read or write to flash device - NOT DONE*/

/**************************************************************************************************
 * Initialize swap pool table and accompanying semaphores
 **************************************************************************************************/
void init_swap_structs(){
    /*initialize swap pool semaphore to 1 -> free at first*/
    semaphore_swapPool = 1;

    /*initialize swap pool table*/
    int i;
    for (i=0;i<MAXFRAMES;i++){
        swap_pool[i].asid = FREEFRAME; /*If a frame is unoccupied, its ASID entry is set to -1 [section 4.4]*/
    }
}


/**************************************************************************************************
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
void flash_read_write(int deviceNum, unsigned int block_num, int op_type, int frame_num){
    /*SYS3 to gain mutex on flash device*/


    /*SYS4 to release flash device*/

    
    /*If operation failed (check device status) -> program trap handler*/
    unsigned int device_status = 0;
    if (device_status != READY){
        program_trap_handler();
    }
}

/**************************************************************************************************
 * TO-DO  
 * BIG PICTURE - Implement Page Replacement Algorithm (using Round Robin approach)
 **************************************************************************************************/
int find_frame_swapPool(){
    static int frame_no = 0;    /*use static to retain frame_no value inside of the method, we don't
    want to declare frame_no as global var*/
    frame_no = (frame_no + 1) % MAXFRAMES;
    return frame_no;
}

/**************************************************************************************************
 * TO-DO  
 * Helper function to perform ops to update the tlb (part of 4.10 optimizations)
 **************************************************************************************************/
void update_tlb_handler(pte_entry_t *new_page_table_entry){
    return;
}

/**************************************************************************************************
 * ALMOST DONE
 * TO-DO  
 * - Mark the old page as invalid in the previous process’s Page Table.
 * - Update the TLB, ensuring it reflects the invalidated page.
 * - Write the old page back to its backing store (flash device).
 * 
 * @note
 * If frame i is occupied, assume it's occupied by logical page number k belonging to process x (ASID)
 * and that it is "dirty" (dirty bit ON) - pandOS [section 4.4]
 **************************************************************************************************/
void occupied_frame_handler(int frame_number){
    /*Updating TLB and Swap Pool must be atomic -> DISABLE INTERRUPTS - pandOS [section 4.5.3]*/
    setSTATUS(STATUS_IECOFF);

    /*Step 1: Mark old page currently occupying the frame number as invalid*/
    swap_pool[frame_number].ownerEntry->entryLO &= VALIDBITOFF; /*go to page table entry of owner process and set valid bit to off*/

    /*Step 2: Update the TLB*/
    update_tlb_handler(swap_pool[frame_number].ownerEntry);

    /*ENABLE INTERRUPTS*/
    setSTATUS(STATUS_IECON);

    unsigned int occupiedASID = swap_pool[frame_number].asid;
    unsigned int occupiedPageNumber = swap_pool[frame_number].pg_number;    

    /*Step 3: Write the old (at this point evicted) page back to its backing store (flash device) - pandOS [section 4.5.1]*/
    flash_read_write(3,occupiedASID-1,occupiedPageNumber,frame_number);
}


/**************************************************************************************************
 * ALMOST DONE
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
void tlb_refill_handler(){
    /*Step 1: Determine missing page number*/
    /*virtual addr split into: VPN and Offset -> to isolate the virtual page number, we mask out 
    the offset bits, then shift right by 12 bits to get the page number*/

    state_PTR saved_except_state = (state_PTR) BIOSDATAPAGE; /*get the saved exception state located at start of BIOS data page*/
    unsigned int entryHI = saved_except_state->s_entryHI;   /*get entryHI*/
    unsigned int missing_virtual_pageNum = (entryHI & VPNMASK) >> VPNSHIFT; /*mask offset bits and shift right 12 bits to get VPN*/

    /*WHAT IF VPN exceed size of 32 we defined in types.h? Mod 32?*/

    /*Step 2: Get matching page table entry for missing page number of current process*/
    pte_entry_t page_entry = currProc->p_supportStruct->sup_privatePgTbl[missing_virtual_pageNum];

    /*Step 3: Write page table entry into TLB -> 3-step process: setENTRYHI, setENTRYLO, TLBWR*/
    setENTRYHI(page_entry.entryHI);
    setENTRYLO(page_entry.entryLO);
    TLBWR();

    /*Step 4: Return control to current process (context switch)*/
    LSDT(saved_except_state);
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

    /*Step 1: Obtain current process support structure via syscall number 8*/
    support_t* currProc_supp_struct = (support_t*) SYSCALL(SYS8,0,0,0);

    /*Step 2: Identify Cause of the TLB Exception from sup_exceptState field of support structure*/
    unsigned int exception_cause = (currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT].s_cause & GETEXCPCODE) >> CAUSESHIFT;

    /*Step 3: If the exception code is a "modification" type, treat as program trap*/
    if (exception_cause == MODEXCEPTION){
        /*Pass execution to support level program trap handler*/
        program_trap_handler(); /*Define this method in sysSupport.c*/
    }

    else{
        /*Step 4: First, gain mutual exclusion of swap pool via performing a SYS3 operation*/
        SYSCALL(SYS3,(int)&semaphore_swapPool,0,0);
        
        /*Step 5: Determine missing page number*/
        unsigned int missing_page = (currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT].s_entryHI & VPNMASK) >> VPNSHIFT;

        /*Step 6: Pick a frame from the swap pool (Page Replacement Algorithm)*/
        int free_frame_num = find_frame_swapPool();

        /*Step 7 + 8: If the frame is occupied -> need to evict it (invalidate the page occupying this frame)*/
        if (swap_pool[free_frame_num].asid != FREEFRAME){
            occupied_frame_handler(free_frame_num); /*Call the helper method that performs operations when a frame is occupied*/
        }

        /*If frame is not occupied*/
        unsigned int free_frame_address;
        free_frame_address = (free_frame_num * PAGESIZE) + POOLBASEADDR; /*First, we calculate the address of the free frame in swap pool*/
        /*section 4.4.1 - [pandOS]*/

        /*Step 9:Load missing page from backing store into the selected frame (at address free_fram_address)*/
        /*For this step, we essentially must do a flash device read operation to load the page into the frame*/

        /*First, get flash device number that is associated with the current u-proc*/

        /*Perform flash read op (helper function)*/
        flash_read_write();

        /*Step 10: Update the Swap Pool Table to reflect the new contents (atomic operations)*/
        /*First, we disable Interrupts by getting current status and clearing the IEc (global interrupt) bit*/
        unsigned int status = getSTATUS(); /*get current status*/
        status = status & (STATUS_IECOFF); /*clear IEc (global interrupt) bit*/
        setSTATUS(status); /*section 7.1 - [pops]*/
        

        /*Update swap pool table with new entry*/
        swap_pool[free_frame_num].asid = currProc_supp_struct->sup_asid; /*set asid of the u-proc that now owns this frame*/
        swap_pool[free_frame_num].pg_number = missing_page; /*record virtual page number that is now occupying this frame*/
        swap_pool[free_frame_num].ownerEntry = &(currProc_supp_struct->sup_privatePgTbl[missing_page]); /*store pointer to page table entry for this page*/


        /*Step 11: Update the Page Table for the new process, marking the page as valid (V bit)*/
        currProc_supp_struct->sup_privatePgTbl[missing_page].entryLO = free_frame_address | V_BIT_SET | D_BIT_SET; /*set the valid bit and dirty bit in entryLO*/

        /*Step 12: Update the TLB to include the new page*/
        /*update_tlb_handler();*/

        TLBCLR(); /*For now, we will do approach 2 - erase ALL the entries in the TLB (OPTIMIZE LATER)*/

        /*Re-enable interrupts*/
        status = getSTATUS(); /*get current status*/
        status = status | STATUS_IE_ENABLE; /*set IEc (global interrupt) bit*/
        setSTATUS(status); /*section 7.1 - [pops]*/

        /*Step 13: Perform SYS4 to release mutex on swap pool table*/
        SYSCALL(SYS4,(int)&semaphore_swapPool,0,0);

        /*Step 14: Return control (context switch) to the instruction that caused the page fault*/
        LDST(&(currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT]));
    }
}