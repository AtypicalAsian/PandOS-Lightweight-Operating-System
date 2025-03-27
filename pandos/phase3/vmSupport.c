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
HIDDEN swap_pool_t swap_pool[MAXFRAMES];
HIDDEN int semaphore_swapPool;


/*Helper Methods*/
HIDDEN void find_missing_page();
HIDDEN void find_frame_swapPool(); /*find frame from swap pool (page replacement algo)*/
HIDDEN void is_occupied_frame(); /*handle ops when frame occupied*/
HIDDEN void flash_read_write(); /*perform read or write to flash device*/
HIDDEN void page_table_lookup(); /*find Pg Table entry via page number*/

/**************************************************************************************************
 * Initialize swap pool table and accompanying semaphores
 * NEED TO DEFINE CONSTANT FOR -1 LATER
 **************************************************************************************************/
void init_swap_structs(){
    /*initialize swap pool semaphore to 1 -> free at first*/
    semaphore_swapPool = 1;

    /*initialize swap pool table*/
    int i;
    for (i=0;i<MAXFRAMES;i++){
        swap_pool[i].asid = -1;
    }
}


/**************************************************************************************************
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

}


/**************************************************************************************************
 * TO-DO  
 * BIG PICTURE
 * TLB refills managed by refill handler
 * Pager manages other page faults (page fault on load, page fault on store op, attemp write
 *          to read only page?should not occur in Pandos so treat as program trap)
 * 
 * STEPS
 *       1.Obtain Current Process’s Support Structure (SYS8)
 *       2.Identify the cause of the TLB exception from `sup_exceptState[0].Cause`
 *       3.If the cause is a "Modification" exception, treat it as a program trap
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
 * 
 * Note on update tlb: there are 2 approaches
 *      1. Probe the TLB (TLBP) to see if the newly updated TLB entry is indeed cached in the TLB. 
 *         If so (Index.P is 0), rewrite (update) that entry (TLBWI) to match the entry in the Page Table.
 *      2. Erase ALL the entries in the TLB (TLBCLR) - implement this before implementing the first approach
 **************************************************************************************************/
void tlb_exception_handler(){

}