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
// cpu_t start_tod;
// state_PTR savedExceptState;
// int deviceSemaphores[MAXDEVICECNT];


/**************************************************************************** 
 * METHOD DECLARATIONS
 *****************************************************************************/

 extern void test(); /*Function to help debug the Nucleus, defined in the test file for this module*/
 HIDDEN void generalExceptionHandler(); /* hidden function that is responsible for handling general exceptions */
 extern void uTLB_RefillHandler(); /*this function is a placeholder function not implemented in Phase 2 and whose code is provided. This function implementation will be replaced when the support level is implemented*/


/**************************************************************************** 
 * METHOD IMPLEMENTATIONS
 *****************************************************************************/

/**************************************************************************** 
 * This function is responsible for handling general exceptions. It determines 
 * the type of exception that occurred and delegates handling to the appropriate 
 * exception handler.
 * 
 * params: None
 * return: 

 *****************************************************************************/
 void generalExceptionHandler(){
    
    /**************************************************************************** 
     * BIG PICTURE
     * 1. Retrieves the saved processor state from BIOSDATAPAGE to analyze the exception.
     * 2. Extracts the exception code from the cause register to determine the type of exception.
     * 3. Delegates handling based on exception type:
     *      a. Device Interrupts (Code 0) → processing passed to device interrupt handler -> Calls intTrapH().
     *      b. TLB Exceptions (Codes 1-3) → processing passed to TLB exception handler -> Calls tlbTrapH().
     *      c. System Calls (Code 8) → processing passed to syscall exception handler -> Calls sysTrapH().
     *      d. Program Traps (Code 4-7,9-12) → processing passed to program trap exception handler → Calls pgmTrapH().

    *****************************************************************************/
    
    
    int exceptionReason; /* the exception code */
    state_t *oldState; /* the saved exception state for Processor 0 */

    *oldState = (state_t *) BIOSDATAPAGE; /*BIOSDATAPAGE stores the saved processor state at the moment of an exception. We let oldState point to this saved state to analyze the cause of the exception*/
    exceptionReason = ((oldState->s_cause) & GETEXCEPCODE) >> CAUSESHIFT; /*s_cause contains the cause register, which stores the reason for the exception. The exception code is extracted by masking and shifting the relevant bits.*/

    return -1;
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
 * return: 

 *****************************************************************************/
 int main(){

    /**************************************************************************** 
     * BIG PICTURE
     * 1. Declare Global Data Variables (proccess count, soft-b count, readyQ, currProc, device semaphores)
     * 
     * 2. Set Up Exception Handling - Populate Processor 0 Pass-Up Vector
     * - Set the Nucleus TLB-Refill event handler address
     * - Set Stack Pointer for Nucleus TLB-Refill event handler to top of Nucleus stack page (0x2000.1000)
     * - Set the Nucleus exception handler address to the address of your Level 3 Nucleus function that is to be the entry point for exception (and interrupt) handling
     * - Set Stack Pointer for Nucleus exception handler to top of Nucleus stack page: 0x2000.1000
     * 
     * 3. Initialize Level 2 data structures
     * - Calls initPcbs() to set up the Process Control Block (PCB) free list.
     * - Calls initASL() to set up the Active Semaphore List (ASL).
     * 
     * 4. Initialize all Nucleus maintained variables: Process Count (0), Soft-block Count (0), Ready Queue (mkEmptyProcQ()), and Current Process (NULL), device semaphores (all set to zero at first)
     * 
     * 5. Configure System Timer (Load the system-wide Interval Timer with 100 milliseconds)
     * 
     * 6. Create and Launch the First Process
     * - Allocates a new process (PCB).
     * - Initializes its stack pointer, program counter (PC), and status register (enables interrupts & kernel mode).
     * - Inserts the process into the Ready Queue and increments procCnt.
     * - Calls switchProcess() to begin execution.
     * 
     * 7. If no PCB is available, the system calls PANIC() to halt execution. 

    *****************************************************************************/

    passupvector_t *procVec; /* a pointer to the Process 0 Pass Up Vector to be initialized */
    pcb_PTR p; /* a pointer to the intial process in the ready queue to in order for the scheduler to begin execution. */
	memaddr ramtop; /* the address of the last RAM frame */
	devregarea_t *temp; /* device register area that we can we use to determine the last RAM frame */


    ReadyQueue = mkEmptyProcQ();
    procCnt = INITIALPROCCNT;
    softBlockCnt = INITIALSFTBLKCNT;
    currProc = NULL; 

    // int i;
    // for (i=0; i < MAXDEVICECNT; i++){
    //     /*init device semaphores*/
    // }

    initPcbs();
    initASL();



    return -1;
 }
