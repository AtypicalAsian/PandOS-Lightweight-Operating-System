/************************************************************************************************ 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

This module contains the entry point for Phase 2 - the main() function, which 
initializes Phase 1 data structures, consisting of the free semaphore descriptor
list, Active Semaphore List (ASL), and process queue (contains processes 
ready to be scheduled). With regards to the process queue, the module will
create an intial process in the ready queue to in order for the scheduler 
to begin execution.

Additional global variables for phase 2 are defined and specified as needed with in-line 
comments, and the general exception handler is implemented in this model, whose job is to 
pass up control to the device interrupt handler during interruptions or to the
appropriate function in the exceptions.c module to handle the particular type
of exception.

Last but not least, this module sets up four words in BIOS data page, each 
for the following item: TLB-refill handler, its stack pointer, 
the general exception handler and its stack pointer.

 Once main() calls the Scheduler its task is complete. At this point the only mechanism for 
 re-entering the Nucleus is through an exception; which includes device interrupts. As long as there 
 are processes to run, the processor is executing instructions on their behalf and only 
 temporarily enters the Nucleus long enough to handle a device interrupt or exception when they occur.

To view version history and changes:
    - Remote GitHub Repo: https://github.com/AtypicalAsian/CS372-OS-Project
************************************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "/usr/include/umps3/umps/libumps.h"

#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"


/**************************************************************************** 
 * GLOBAL VARIABLES DECLARATIONS
 *****************************************************************************/
int procCnt; /*integer indicating the number of started, but not yet terminated processes.*/
int softBlockCnt; /*This integer is the number of started, but not terminated processes that in are the “blocked” state due to an I/O or timer request.*/
pcb_PTR ReadyQueue; /*Tail pointer to a queue of pcbs that are in the “ready” state.*/
pcb_PTR currProc; /*Pointer to the pcb that is in the “running” state, i.e. the current executing process.*/
int deviceSemaphores[MAXDEVICECNT]; /*Integer array of device semaphores that is associated with external (sub) devices, plus one semd for the Pseudo-clock*/
state_PTR savedExceptState; /*ptr to saved exception state*/
cpu_t time_of_day_start; /*current time from the system’s Time of Day (TOD) clock when a process starts running. Used to calculate difference between current time of day value and the start time when the process is interrupted*/



/**************************************************************************** 
 * METHOD DECLARATIONS
 *****************************************************************************/
 HIDDEN void exception_handler(); /* hidden function that is responsible for handling general exceptions */
 extern void uTLB_RefillHandler(); /*this function is a placeholder function not implemented in Phase 2 and whose code is provided. This function implementation will be replaced when the support level is implemented*/
 extern void test(); /*Function to help debug the Nucleus, defined in the test file for this module*/

/**************************************************************************** 
 * METHOD IMPLEMENTATIONS
 *****************************************************************************/

/**************************************************************************** 
 * This function is responsible for handling general exceptions. It determines 
 * the type of exception that occurred and delegates handling to the appropriate 
 * exception handler.
 * 
 * params: None
 * return: None

 *****************************************************************************/
 void exception_handler(){

    /**************************************************************************** 
     * BIG PICTURE
     * 1. Retrieves the saved processor state from BIOSDATAPAGE to analyze the exception.
     * 2. Extracts the exception code from the cause register to determine the type of exception.
     * 3. Delegates handling based on exception type:
     *      a. Device Interrupts (Code 0) → processing passed to device interrupt handler -> Calls interruptsHandler().
     *      b. TLB Exceptions (Codes 1-3) → processing passed to TLB exception handler -> Calls 
     *      c. System Calls (Code 8) → processing passed to syscall exception handler -> Calls 
     *      d. Program Traps (Code 4-7,9-12) → processing passed to program trap exception handler → Calls e

    *****************************************************************************/
    
    
    int exception_code; /* Stores the extracted exception type */  
    state_t *saved_state; /* Pointer to the saved processor state at time of exception */  

    saved_state = (state_t *) BIOSDATAPAGE;  /* Retrieve the saved processor state from BIOS data page */
    exception_code = ((saved_state->s_cause) & GETEXCPCODE) >> CAUSESHIFT; /* Extract exception code from the cause register */

    if (exception_code == INTCONST) {  
        /* Case 1: Exception Code 0 - Device Interrupt */
        interruptsHandler();  /* call the Nucleus' device interrupt handler function */
    }  
    else if (exception_code >= CONST1 && exception_code <= CONST3) {  
        /* Case 2: Exception Codes 1-3 - TLB Exceptions */
        tlbTrapHanlder();  /* call the Nucleus' TLB exception handler function */
    }  
    else if (exception_code == SYSCONST) {  
        /* Case 3: Exception Code 8 - System Calls */
        sysTrapHandler();  /* call the Nucleus' SYSCALL exception handler function */
    }  
    else {  
        /* Case 4: All Other Exceptions - Program Traps */
        prgmTrapHandler();  /* calling the Nucleus' Program Trap exception handler function because the exception code is not 0-3 or 8*/
    }
 }


/**************************************************************************** 
 * This function serves as the starting point of the program. It sets up the necessary 
 * data structures for phase 1, including the ASL, the free list of PCBs, and the queue 
 * that will hold processes ready for execution. Additionally, it configures four specific 
 * words in the BIOS data page: one for the general exception handler, one for 
 * its associated stack pointer, one for the TLB-Refill handler, and one for its stack 
 * pointer. The function then creates a single process and hands it over to the Scheduler 
 * for execution. It also initializes the module's global variables.
 * 
 * The main() function is only called once. Once main() calls the Scheduler 
 * its task is complete.
 * 
 * params: None
 * return: None

 *****************************************************************************/
 int main(){

    /**************************************************************************** 
     * BIG PICTURE
     * 
     * 1. Set Up Exception Handling - Populate Processor 0 Pass-Up Vector
     * - Set the Nucleus TLB-Refill event handler address
     * - Set Stack Pointer for Nucleus TLB-Refill event handler to top of Nucleus stack page (0x20001000)
     * - Set the Nucleus exception handler address to the address of your Level 3 Nucleus function that is to be the entry point for exception (and interrupt) handling
     * - Set Stack Pointer for Nucleus exception handler to top of Nucleus stack page: 0x20001000
     * 
     * 2. Initialize Level 2 data structures
     * - Calls initPcbs() to set up the Process Control Block (PCB) free list.
     * - Calls initASL() to set up the Active Semaphore List (ASL).
     * 
     * 3. Initialize all Nucleus maintained variables: Process Count (0), Soft-block Count (0), Ready Queue (mkEmptyProcQ()), and Current Process (NULL), device semaphores (all set to zero at first)
     * 
     * 4. Configure System Timer (Load the system-wide Interval Timer with 100 milliseconds)
     * 
     * 5. Create and Launch the First Process
     * - Allocates a new process (PCB).
     * - Initializes its stack pointer, program counter (PC), and status register (enables interrupts & kernel mode).
     * - Inserts the process into the Ready Queue and increments procCnt.
     * - Calls switchProcess() to begin execution.
     * 
     * 6. If no PCB is available, the system calls PANIC() to halt execution. 

    *****************************************************************************/

    /*1. Set Up Exception Handling*/
    
    pcb_PTR first_proc; /* a pointer to the first process in the ready queue to be created so that the scheduler can begin execution */
    passupvector_t *proc0_passup_vec; /*Pointer to Processor 0 Pass-Up Vector */
    memaddr topRAM; /* the address of the last RAM frame */
    devregarea_t *dra; /* device register area that used to determine RAM size */

    /*Initialize passUp vector fields*/
    proc0_passup_vec = (passupvector_t *) PASSUPVECTOR;                     /*Init processor 0 pass up vector pointer to the address (0x0FFFF900) defined in const.h*/
    proc0_passup_vec->tlb_refll_handler = (memaddr) uTLB_RefillHandler;     /*Initialize address of the nucleus TLB-refill event handler*/
    proc0_passup_vec->tlb_refll_stackPtr = TOPSTKPAGE;                      /*Set stack pointer for the nucleus TLB-refill event handler to the top of the Nucleus stack page */
    proc0_passup_vec->execption_handler = (memaddr) exception_handler;      /*Set the Nucleus exception handler address to the address of function that is to be the entry point for exception (and interrupt) handling*/
    proc0_passup_vec->exception_stackPtr = TOPSTKPAGE;                      /*Set the Stack pointer for the Nucleus exception handler to the top of the Nucleus stack page*/


    /*2. Initialize Level 2 data structures*/
    initPcbs(); /*Set up the Process Control Block (PCB) free list.*/
    initASL(); /*Set up the Active Semaphore List (ASL).*/


    /*3. Initialize nucleus maintained variables*/

    /*Initialize device semaphores*/
    int i;
    for (i = 0; i < MAXDEVICECNT; i++) {
        deviceSemaphores[i] = INITDEVICESEM;
    }
    ReadyQueue = mkEmptyProcQ();  /*Initialize the Ready Queue*/
    currProc = NULL;  /*No process is running initially */
    procCnt = INITPROCCNT;  /*No active processes yet*/
    softBlockCnt = INITSBLOCKCNT;  /*No soft-blocked processes*/

    /*4. Configure System Timer (Load the system-wide Interval Timer with 100 milliseconds)*/
    LDIT(INITTIMER);  /*Set interval timer to 100ms*/

    /*5. Create and Launch the First Process*/
    /**************************************************************************** 
     * In particular this process will have interrupts enabled, the processor Local Timer enabled, kernel-mode on, the SP set to RAMTOP, and its PC set to the address of test. 
     * The remaining pcb fields as follows:
     *      Set all the Process Tree fields to NULL.
     *      Set the accumulated time field (p time) to zero.
     *      Set the blocking semaphore address (p semAdd) to NULL.
     *      Set the Support Structure pointer (p supportStruct) to NULL.

    *****************************************************************************/
    first_proc = allocPcb();    /*allocate a PCB for the first process*/
    
    if (first_proc != NULL){
        dra = (devregarea_t *) RAMBASEADDR;     /*Set the base address of the device register area */
        topRAM = dra->rambase + dra->ramsize;   /*Calculate the top of RAM by adding the base address and total RAM size*/

        /*Initialize the process state*/
        first_proc->p_s.s_status = STATUS_ALL_OFF | STATUS_IE_ENABLE | STATUS_PLT_ON | STATUS_INT_ON; /*configure initial process state to run with interrupts, local timer enabled, kernel-mode on*/
        first_proc->p_s.s_sp = topRAM; /*Stack pointer set to top of RAM*/
        first_proc->p_s.s_pc = (memaddr) test; /*Set PC to test()*/ 
        first_proc->p_s.s_t9 = (memaddr) test; /*Set t9 register to test(). For technical reasons, whenever one assigns a value to the PC one must also assign the same value to the general purpose register t9.*/

        /*Add process to Ready Queue & update process count */
        insertProcQ(&ReadyQueue, first_proc);       /*insert first process into ready queue*/
        procCnt++;                                  /*increase the total process count to 1*/

        /*Start execution with the scheduler*/  
        switchProcess();  
        return (0);  
    }

    /*7. If no PCB is available, the system calls PANIC() to halt execution*/
    PANIC();
    return (0);
 }
