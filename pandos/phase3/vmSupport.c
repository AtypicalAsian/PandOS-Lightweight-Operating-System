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


/*Helper Methods*/
HIDDEN int find_frame_swapPool(); /*find frame from swap pool (page replacement algo)*/
HIDDEN void occupied_frame_handler(); /*handle ops when frame occupied*/
HIDDEN void update_tlb_handler(); /*Helper function to perform operations related to updating the TLB*/
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
        swap_pool[i].asid = FREEFRAME; /*If a frame is unoccupied, its ASID entry is set to -1 [section 4.4]*/
    }
}


/**************************************************************************************************
 * TO-DO  
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
void flash_read_write(){

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
 * - Mark the old page as invalid in the previous process’s Page Table.
 * - Update the TLB, ensuring it reflects the invalidated page.
 * - Write the old page back to its backing store (flash device).
 **************************************************************************************************/
void occupied_frame_handler(int frame_number){
    /*Updating TLB and Swap Pool must be atomic -> DISABLE INTERRUPTS*/
    setSTATUS();

    /*Step 1: Mark old page currently occupying the frame number as invalid*/
    swap_pool[frame_number].ownerEntry->entryLO &= VALIDBITOFF; /*go to page table entry of owner process and set valid bit to off*/

    /*Step 2: Update the TLB*/
    update_tlb_handler();

    /*Step 3: Write the old (at this point evicted) page back to its backing store (flash device)*/

    /*We know these info: frame number, frame address (start addrs of frame in RAM can be calc)*/
    /*Gain mutex over flash device + disable interrupts*/
    /*Write to flash device registers (bit shifts)*/
    /*Do Sys5 to block until the write operation completes*/
    /*Release mutex (semaphore) of flash device*/
    /*Enable interrupts when we're done*/

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
 * 
 * Note on update tlb: there are 2 approaches
 *      1. Probe the TLB (TLBP) to see if the newly updated TLB entry is indeed cached in the TLB. 
 *         If so (Index.P is 0), rewrite (update) that entry (TLBWI) to match the entry in the Page Table.
 *      2. Erase ALL the entries in the TLB (TLBCLR) - implement this before implementing the first approach
 **************************************************************************************************/
void tlb_exception_handler(){ /*--> Otherwise known as the Pager*/
    /*If we're here, page fault has occured*/

    support_t* currProc_supp_struct = (support_t*) SYSCALL(8,0,0,0);      /*Obtain current process support structure via syscall number 8*/

    /*Identify Cause of the TLB Exception from sup_exceptState field of support structure*/
    unsigned int exception_cause = (currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT].s_cause & GETEXCPCODE) >> CAUSESHIFT;

    /*If the exception code is a "modification" type, treat as program trap*/
    if (exception_cause == MODEXCEPTION){
        /*Pass execution to support level program trap handler*/
        /*trap_handler();*/ /*Define this method in sysSupport.c*/
    }

    else{
        /*First, gain mutual exclusion of swap pool via performing a SYS3 operation*/
        SYSCALL(3,(int)&semaphore_swapPool,0,0);
        
        /*Determine missing page number*/
        unsigned int missing_page_number = (currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT].s_entryHI & PAGESHIFT) >> VPNSHIFTMASK;

        /*Pick a frame from the swap pool (Page Replacement Algorithm)*/
        int free_frame_num = find_frame_swapPool();

        /*If the frame is occupied -> need to evict it (invalidate the page occupying this frame)*/
        if (swap_pool[free_frame_num].asid != FREEFRAME){
            occupied_frame_handler(free_frame_num); /*Call the helper method that performs operations when a frame is occupied*/
        }

        /*If frame is not occupied*/

        /*Load missing page from backing store to the selected frame (means we're performing a flash device read operation)*/


        /*Update curr proc's page table and the swap pool*/
        // currProcEntry = (supportStruct)
        swap_pool[free_frame_num].asid = 0;
        swap_pool[free_frame_num].pg_number = missing_page_number;
        swap_pool[free_frame_num].ownerEntry = NULL;


    }
}