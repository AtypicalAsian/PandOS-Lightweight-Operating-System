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
void tlb_exception_handler(){

}