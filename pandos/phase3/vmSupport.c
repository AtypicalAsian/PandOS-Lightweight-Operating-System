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

/*Support Level Data Structures*/
HIDDEN swap_pool_t swap_pool[MAXFRAMES];    /*swap pool table*/
HIDDEN int semaphore_swapPool;              /*swap pool sempahore*/

extern int devRegSem[MAXSHAREIODEVS+1];
extern int masterSema4;


/*Helper Methods*/

/**************************************************************************************************
 * DONE
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
void flash_read_write(unsigned int read_write_bool, unsigned int device_no, unsigned int occp_pageNo, int swap_pool_index){
    memaddr pfn = (swap_pool_index << 12) + POOLBASEADDR;
    unsigned int command = (occp_pageNo << 8) | read_write_bool;
    SYSCALL(SYS3,(memaddr) &devRegSem[DEV_INDEX(4, device_no, FALSE)],0,0);

    dtpreg_t* flashaddr = (dtpreg_t*) DEV_REG_ADDR(4,device_no);
    flashaddr->data0 = pfn;

    IEDISABLE;
    flashaddr->command = command;
    unsigned int device_status = SYSCALL(SYS5,4,device_no,FALSE);
    IEENABLE;

    SYSCALL(SYS4,(memaddr) &devRegSem[DEV_INDEX(4,device_no,FALSE)],0,0);
    if (device_status != READY){
        nuke_til_it_glows(&semaphore_swapPool);
    }
}

void occupied_handler(unsigned int asid, unsigned int occp_pageNo, unsigned int swap_pool_index){
    IEDISABLE;
    swap_pool[swap_pool_index].ownerEntry->entryLO &= VALIDBITOFF;
    update_tlb_handler(swap_pool[swap_pool_index].ownerEntry);
    IEENABLE;
    flash_read_write(WRITEFLASH,asid-1,occp_pageNo,swap_pool_index);
}

void reset_swap(int asid){
    int i;
    for (i=0;i<MAXFRAMES;i++){
        if (swap_pool[i].asid == asid){swap_pool[i].asid = FREEFRAME;}
    }
}

void nuke_til_it_glows(int *semaphore){
    support_t* currProc_supp_struct;
    currProc_supp_struct = (support_t*) SYSCALL(SYS8,0,0,0);
    reset_swap(currProc_supp_struct->sup_asid);
    if (semaphore != NULL){
        SYSCALL(SYS4,(int) semaphore,0,0);
    }

    deallocate(currProc_supp_struct);
    SYSCALL(SYS4,(int) &masterSema4, 0,0);
    SYSCALL(SYS2,0,0,0);
}

/**************************************************************************************************
 * DONE
 * TO-DO  
 * BIG PICTURE - Implement Page Replacement Algorithm (using Round Robin approach)
 **************************************************************************************************/
int find_frame_swapPool(){
    static int currentReplacementIndex = 0;
    int i = 0;
    //SEARCHING FOR A FREE SWAP PAGE
    while((swap_pool[(currentReplacementIndex + i) % MAXFRAMES].asid != FREEFRAME) && (i < MAXFRAMES))
        ++i;
    //CASE == NO FREE SWAP PAGE    
    i = (i == MAXFRAMES) ? 1 : i;
    
    return currentReplacementIndex = (currentReplacementIndex + i) % MAXFRAMES;
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
    /*unsigned int entry_prev = getENTRYHI();*/

    /* Load the new page's virtual page number (VPN) and ASID into EntryHi.
     * This is necessary for TLB to find the matching page table entry
     */
    setENTRYHI(new_page_table_entry->entryHI);
    TLBP(); /*probe the TLB to searches for a matching entry using the current EntryHi*/

    /*Check INDEX.P bit (bit 31 of INDEX)*/
    if ((KUSEG & getINDEX()) == 0){ /*If P bit == 0 -> found a matching entry*/
        setENTRYLO(new_page_table_entry->entryLO);  /* Load the updated physical frame number and permissions into EntryLo */
        TLBWI(); /* Write to the TLB at the index found by TLBP. This updates the cached entry to match the page table*/
    }

    /* Restore the previously saved EntryHi in CP0 register */
    /*setENTRYHI(entry_prev);*/
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

    state_PTR saved_except_state = (state_PTR) BIOSDATAPAGE; /*get the saved exception state located at start of BIOS data page*/
    int page_no = GETVPN(saved_except_state->s_entryHI);
    support_t* currProc_supp_struct;
    currProc_supp_struct = (support_t*) SYSCALL(SYS8,0,0,0);

    /*Step 3: Write page table entry into TLB -> 3-step process: setENTRYHI, setENTRYLO, TLBWR*/
    setENTRYHI(currProc_supp_struct->sup_privatePgTbl[page_no].entryHI);
    setENTRYLO(currProc_supp_struct->sup_privatePgTbl[page_no].entryLO);
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
void tlb_exception_handler(){ /*--> Otherwise known as the Pager*/
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
    if (exception_cause == MODEXCEPTION){
        /*Pass execution to support level program trap handler*/
        nuke_til_it_glows(NULL);
    }
    else{
        /*Step 4: First, gain mutual exclusion of swap pool via performing a SYS3 operation*/
        SYSCALL(SYS3,(memaddr)&semaphore_swapPool,0,0);
        
        /*Step 5: Compute missing page number*/
        missing_page_no = GETVPN(currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT].s_entryHI);

        /*Step 6: Pick a frame from the swap pool (Page Replacement Algorithm)*/
        free_frame_num = find_frame_swapPool();

        if (swap_pool[free_frame_num].asid != FREEFRAME){
            unsigned int occp_asid = swap_pool[free_frame_num].asid;
            unsigned int occp_pageNo = swap_pool[free_frame_num].pg_number;
            occupied_handler(occp_asid,occp_pageNo,free_frame_num);
        }
        flash_read_write(READFLASH,currProc_supp_struct->sup_asid-1,missing_page_no,free_frame_num);
        IEDISABLE;
        swap_pool[free_frame_num].asid = currProc_supp_struct->sup_asid;
        swap_pool[free_frame_num].pg_number = missing_page_no;
        swap_pool[free_frame_num].asid = &(currProc_supp_struct->sup_privatePgTbl[missing_page_no]);

        currProc_supp_struct->sup_privatePgTbl[missing_page_no].entryLO |= VALID_BIT_ON;
        currProc_supp_struct->sup_privatePgTbl[missing_page_no].entryLO = (currProc_supp_struct->sup_privatePgTbl[missing_page_no].entryLO & ~ENTRYLO_PFN_MASK) | ((free_frame_num << 12) + POOLBASEADDR);

        update_tlb_handler(&(currProc_supp_struct->sup_privatePgTbl[missing_page_no]));
        IEENABLE;
        SYSCALL(SYS4,(memaddr) &semaphore_swapPool,0,0);
        LDST(&(currProc_supp_struct->sup_exceptState[PGFAULTEXCEPT]));        
    }
}