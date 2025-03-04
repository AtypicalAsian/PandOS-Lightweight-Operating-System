
/************************************************************************************************ 
 * CS372 - Dr. Goldweber
 * 
 * @file initial.c
 * 
 * 
 * @brief
 * This module serves as the entry point for Phase 2 - the Nucleus via the main() function. It is responsible 
 * for initializing core data structures, setting up exception handling, and launching the first process.
 * Once initialization is complete, the system enters the scheduler to begin process execution.
 * 
 * 
 * @def
 * The Nucleus in the Pandos OS functions as the core of the kernel. The purpose of the Nucleus is 
 * to provide an environment in which asynchronous sequential processes exist, each making forward 
 * progress as they take turns sharing the processor. Furthermore, the Nucleus provides these processes 
 * with exception handling routines, low-level synchronization primitives, and a facility for “passing up” 
 * the handling of Program Trap, TLB exceptions and certain SYSCALL requests to the Support Level.
 * 
 * 
 * @details
 * In detail, the initial.c module accomplishes the following:
 * - Declare Level 3 global variables
 * - Populate Processor0 Pass-Up Vector
 * - Initalize Level 2 Data Structures - Active Semaphore List and Free PCB List
 * - Initialize all Nucleus maintained variables: Process Count (0), Soft-block Count (0), 
 *              Ready Queue (mkEmptyProcQ()), and Current Process (NULL), device semaphores (all set to zero)
 * - Configure System-wide Interval Timer 
 * - Create the first process
 * - Launch the first process and pass control to the Scheduler (scheduler.c)
 * 
 * 
 * @note Once main() completes, the system's primary execution control is handled by the Scheduler, 
 *       and the kernel is only re-entered due to exceptions or device interrupts.
 * 
 * 
 * @authors Nicolas & Tran
 * View version history and changes: https://github.com/AtypicalAsian/CS372-OS-Project
************************************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"

#include "/usr/include/umps3/umps/libumps.h"

/**************** METHOD DECLARATIONS***************************/ 
extern void test(); /*Function to help debug the Nucleus, defined in the test file for this module*/
extern void uTLB_RefillHandler(); /*this function is a placeholder function not implemented in Phase 2 and whose code is provided. This function implementation will be replaced when the support level is implemented*/


/*************GLOBAL VARIABLES DECLARATIONS*********************/ 
int procCnt; /*integer indicating the number of started, but not yet terminated processes.*/
int softBlockCnt; /*This integer is the number of started, but not terminated processes that in are the “blocked” state due to an I/O or timer request.*/
pcb_PTR ReadyQueue; /*Tail pointer to a queue of pcbs that are in the “ready” state.*/
pcb_PTR currProc; /*Pointer to the pcb that is in the “running” state, i.e. the current executing process.*/
int deviceSemaphores[MAXDEVICECNT]; /*Integer array of device semaphores that is associated with external (sub) devices, plus one semd for the Pseudo-clock*/
state_PTR savedExceptState; /*ptr to saved exception state*/
cpu_t time_of_day_start; /*current time from the system’s Time of Day (TOD) clock when a process starts running. Used to calculate difference between current time of day value and the start time when the process is interrupted*/



/***********************METHOD IMPLEMENTATIONS*********************************/

/**************************************************************************** 
 * This function is responsible for handling general exceptions. It determines 
 * the type of exception that occurred and delegates handling to the appropriate 
 * exception handler.
 * 
 * params: None
 * return: None

 *****************************************************************************/
 void gen_exception_handler(){

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
    
    state_t *saved_state; /* Pointer to the saved processor state at time of exception */  
    int exception_code; /* Stores the extracted exception type */  

    saved_state = (state_t *) BIOSDATAPAGE;  /* Retrieve the saved processor state from BIOS data page */
    exception_code = ((saved_state->s_cause) & GETEXCPCODE) >> CAUSESHIFT; /* Extract exception code from the cause register */

    if (exception_code == INTCONST) {  
        /* Case 1: Exception Code 0 - Device Interrupt */
        interruptsHandler();  /* call the Nucleus' device interrupt handler function */
    }  
    if (exception_code <= CONST3) {  
        /* Case 2: Exception Codes 1-3 - TLB Exceptions */
        tlbTrapHanlder();  /* call the Nucleus' TLB exception handler function */
    }  
    if (exception_code == SYSCONST) {  
        /* Case 3: Exception Code 8 - System Calls */
        sysTrapHandler();  /* call the Nucleus' SYSCALL exception handler function */
    }
    /* Case 4: All Other Exceptions - Program Traps */
    prgmTrapHandler(); /* calling the Nucleus' Program Trap exception handler function because the exception code is not 0-3 or 8*/
 }



/*Helper to init pass up vector*/
void populate_passUpVec(){
    proc0_passup_vec = (passupvector_t *) PASSUPVECTOR;                     /*Init processor 0 pass up vector pointer*/
    proc0_passup_vec->tlb_refll_handler = (memaddr) uTLB_RefillHandler;     /*Initialize address of the nucleus TLB-refill event handler*/
    proc0_passup_vec->tlb_refll_stackPtr = TOPSTKPAGE;                      /*Set stack pointer for the nucleus TLB-refill event handler to the top of the Nucleus stack page */
    proc0_passup_vec->execption_handler = (memaddr) gen_exception_handler;  /*Set the Nucleus exception handler address to the address of function that is to be the entry point for exception (and interrupt) handling*/
    proc0_passup_vec->exception_stackPtr = TOPSTKPAGE;                      /*Set the Stack pointer for the Nucleus exception handler to the top of the Nucleus stack page*/
}

/*Helper to init proccess state*/
void init_proc_state(pcb_PTR firstProc){
    dra = (devregarea_t *) RAMBASEADDR;     /*Set the base address of the device register area */
    topRAM = dra->rambase + dra->ramsize;   /*Calculate the top of RAM by adding the base address and total RAM size*/

    /*Initialize the process state*/
    firstProc->p_s.s_sp = topRAM; /*Stack pointer set to top of RAM*/
    firstProc->p_s.s_pc = (memaddr) test; /*Set PC to test()*/ 
    firstProc->p_s.s_t9 = (memaddr) test; /*Set t9 register to test(). For technical reasons, whenever one assigns a value to the PC one must also assign the same value to the general purpose register t9.*/
    firstProc->p_s.s_status = STATUS_ALL_OFF | STATUS_IE_ENABLE | STATUS_PLT_ON | STATUS_INT_ON; /*configure initial process state to run with interrupts, local timer enabled, kernel-mode on*/
}

/****************************************************************************
 * main()
 * 
 *  
 * @brief
 * This function serves as the entry point for Phase 2. It sets up core data structures
 * and subsystems that will enable the OS to handle exceptions, enforce time-sharing of the CPU,
 * and properly manage processes and their state transitions.
 * 
 * 
 * @protocol 
 * The following steps are performed:
 *  1. Initialize global variables
 *  2. Initialize Level 2 data structures
 *  3. Initialize Pass Up Vector fields for exceptions and TLB-refill events
 *      - Set the Nucleus TLB-Refill event handler address
 *      - Set Stack Pointer for Nucleus TLB-Refill event handler to top of Nucleus stack page
 *      - Set the Nucleus exception handler address to the address of general exception handler function
 *      - Set Stack Pointer for Nucleus exception handler to top of Nucleus stack page
 *  4. Configure system interval timer (100ms)
 *  5. Create the first process & initialize its process state
 *       - Allocates a new PCB
 *       - Initializes its stack pointer, program counter (PC), and status register (enables interrupts & kernel mode).
 *       - Inserts the process into the Ready Queue and increments procCnt.
 *  6. Call the scheduler to run the first process
 * 
 * 
 * @note  
 * - This function **only runs once** at system startup.  
 * - Once execution is transferred to the Scheduler, the system enters its normal 
 *   execution cycle and only re-enters the Nucleus upon **exceptions or interrupts**.  
 * 
 * 
 * @param None  
 * @return None  

 *****************************************************************************/
 int main(){
    /*Declare variables*/
    pcb_PTR first_proc; /* a pointer to the first process in the ready queue to be created so that the scheduler can begin execution */
    passupvector_t *proc0_passup_vec; /*Pointer to Processor 0 Pass-Up Vector */
    memaddr topRAM; /* the address of the last RAM frame */
    devregarea_t *dra; /* device register area that used to determine RAM size */

    /*Initialize device semaphores*/
    int i;
    for (i = 0; i < MAXDEVICECNT; i++) {
        deviceSemaphores[i] = INITDEVICESEM;
    }

    /*Initialize variables*/
    ReadyQueue = mkEmptyProcQ();  /*Initialize the Ready Queue*/
    currProc = NULL;  /*No process is running initially */
    procCnt = INITPROCCNT;  /*No active processes yet*/
    softBlockCnt = INITSBLOCKCNT;  /*No soft-blocked processes*/


    /*Initialize Level 2 data structures*/
    initPcbs(); /*Set up the Process Control Block (PCB) free list (pool of avaible pcbs)*/
    initASL(); /*Set up the Active Semaphore List (ASL)*/

    /*Initialize passUp vector fields*/
    populate_passUpVec();

    /*Load the system-wide Interval Timer with 100 milliseconds*/
    LDIT(INITTIMER);  /*Set interval timer to 100ms*/

    /*Create and Launch the First Process*/
    first_proc = allocPcb();    /*allocate a PCB from the PCB free list for the first process*/
    
    /*If there are avaible pcbs to be allocated*/
    if (first_proc != NULL){
        /*Initialize the process state for first process*/
        init_proc_state(first_proc);

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
