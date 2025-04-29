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
#include "../h/types.h"
#include "../h/const.h"

#include "../h/asl.h"
#include "../h/pcb.h"

#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"

#include "/usr/include/umps3/umps/libumps.h"

/**************** METHOD DECLARATIONS***************************/
extern void test(); /*Function to help debug the Nucleus, defined in the test file for this module*/
extern void uTLB_RefillHandler(); /*support level TLB refill event handler*/


/*************GLOBAL VARIABLES DECLARATIONS*********************/
int procCnt; /*integer indicating the number of started, but not yet terminated processes.*/
int softBlockCnt; /*Integer representing the number of started, but not terminated processes that in are the “blocked” state due to an I/O or timer request.*/
pcb_PTR ReadyQueue; /*Tail pointer to a queue of pcbs that are in the “ready” state.*/
pcb_PTR currProc; /*Pointer to the pcb that is in the “running” state, i.e. the current executing process.*/
int deviceSemaphores[DEVICE_TYPES * DEV_UNITS]; /* semaphore integer array that represents each external (sub) device, plus one semd for the Pseudo-clock */
int semIntTimer; /* semaphore used by the interval timer (pseudo-clock) for timer-related blocking operations */

/***********************HELPER METHODS***************************************/

/****************************************************************************
 * debug_fxn()
 * 
 * 
 * @brief 
 * Helper function to debug the program as we set breakpoints during execution.
 * 
 * 
 * @note
 * The values for the four parameters to this debug fxn can be accessed in
 * registers a0,a1,a2,a3
 * 
 * 
 * @param i - can be line number or a unique identifier
 * @param p1 - first var to be configured
 * @param p2 - second var to be configured
 * @param p3 - third var to be configured
 * @return None
 * 

 *****************************************************************************/
void debug_fxn(int i, int p1, int p2, int p3){
    return;
}


/****************************************************************************
 * populate_passUpVec()
 * 
 *  
 * @brief
 * This function sets up the Pass-Up Vector, a data structure located in the  
 * BIOS Data Page, which defines where the system should transfer control  
 * when specific exceptions occur. Instead of the BIOS handling these exceptions  
 * directly, the Pass-Up Vector redirects them to the Nucleus exception handlers,  
 * allowing the kernel to manage them.  
 * 
 * 
 * @protocol 
 *  1. Retrieves the Pass-Up Vector from its memory location in the BIOS Data Page.  
 *  2. Assigns the TLB-Refill Exception Handler to handle TLB exceptions, which occur  
 *      when a virtual address translation is missing in the TLB.  
 *  3. Sets the stack pointer for the TLB-Refill Exception Handler
 *  4. Assigns the General Exception Handler to handle all other exceptions, including  
 *      system calls, program traps, and device interrupts.  
 *  6. Configures the stack pointer for the General Exception Handler
 * 
 * 
 * @note  
 * The Pass-Up Vector allows user-mode programs to trigger system calls and handle  
 * exceptional conditions efficiently.  
 * 
 * 
 * @param None
 * @return None
 * 

 *****************************************************************************/
void populate_passUpVec(){
    passupvector_t *proc0_passup_vec;                                       /*Pointer to Processor 0 Pass-Up Vector */
    proc0_passup_vec = (passupvector_t *) PASSUPVECTOR;                     /*Init processor 0 pass up vector pointer*/
    proc0_passup_vec->tlb_refill_handler = (memaddr) &uTLB_RefillHandler;     /*Initialize address of the nucleus TLB-refill event handler*/
    proc0_passup_vec->tlb_refill_stackPtr = TOPSTKPAGE;                      /*Set stack pointer for the nucleus TLB-refill event handler to the top of the Nucleus stack page */
    proc0_passup_vec->exception_handler = (memaddr) &gen_exception_handler;  /*Set the Nucleus exception handler address to the address of function that is to be the entry point for exception (and interrupt) handling*/
    proc0_passup_vec->exception_stackPtr = TOPSTKPAGE;                      /*Set the Stack pointer for the Nucleus exception handler to the top of the Nucleus stack page*/
}

/****************************************************************************
 * init_proc_state()
 * 
 * 
 * @brief 
 * This function sets up the initial state of the first process in the system  
 * by configuring its stack pointer, program counter, status register, and  
 * other key fields in the Process Control Block (PCB).  
 *  
 *
 * @protocol 
 * 1. Determines the top of RAM by accessing the Device Register Area (DRA),
 *    which contains the base RAM address and total RAM size.  
 * 2. Configures the Stack Pointer (SP) so that the process stack starts at 
 *    the highest valid RAM address.  
 * 3. Sets the Program Counter (PC) and t9 register to point to test()
 * 4. Initializes the Status Register to enable global and external interrupts,
 *    activate the Processor Local Timer (PLT), and ensure the process starts 
 *    in Kernel Mode.  
 * 
 * 
 * @note  
 * The t9 register is explicitly set to test() due to the MIPS convention that  
 * requires `t9` to match PC when jumping to a function.
 * 
 * 
 * @param pcb_PTR firstProc
 * @return None
 * 

 *****************************************************************************/
void init_proc_state(pcb_PTR firstProc){
	/*unsigned int ramBase;*/  /* Variable to store the base address of RAM */
	/*unsigned int ramSize;*/ /* Variable to store the total size of RAM */
	/*ramBase = *((int *)RAMBASEADDR);*/ /* Read the RAM base address */
	/*ramSize = *((int *)RAMBASESIZE);*/ /* Read the RAM size */
    memaddr topRAM;                         /* the address of the last RAM frame */
	/*topRAM = ramBase + ramSize;*/ /* Calculate top of RAM by adding the base address and the total RAM size */
	RAMTOP(topRAM);

    /*Initialize the process state*/
    firstProc->p_s.s_sp = topRAM;           /*Stack pointer set to top of RAM*/
    firstProc->p_s.s_pc = (memaddr) test;   /*Set PC to test()*/ 
    firstProc->p_s.s_t9 = (memaddr) test;   /*Set t9 register to test(). For technical reasons, whenever one assigns a value to the PC one must also assign the same value to the general purpose register t9.*/
    firstProc->p_s.s_status = IEPON | TEBITON | IMON; /*configure initial process state to run with interrupts, local timer enabled, kernel-mode on*/
}

/***********************MAIN METHOD***************************************/
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
 *  1. Declare variables
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
int main() {
	/*Declare variables*/
    pcb_PTR first_proc; /* a pointer to the first process in the ready queue to be created so that the scheduler can begin execution */

	passupvector_t *proc0_passup_vec;                                       /*Pointer to Processor 0 Pass-Up Vector */
    proc0_passup_vec = (passupvector_t *) PASSUPVECTOR;                     /*Init processor 0 pass up vector pointer*/
    proc0_passup_vec->tlb_refill_handler = (memaddr) &uTLB_RefillHandler;     /*Initialize address of the nucleus TLB-refill event handler*/
    proc0_passup_vec->tlb_refill_stackPtr = TOPSTKPAGE;                      /*Set stack pointer for the nucleus TLB-refill event handler to the top of the Nucleus stack page */
    proc0_passup_vec->exception_handler = (memaddr) &gen_exception_handler;  /*Set the Nucleus exception handler address to the address of function that is to be the entry point for exception (and interrupt) handling*/
    proc0_passup_vec->exception_stackPtr = TOPSTKPAGE;                      /*Set the Stack pointer for the Nucleus exception handler to the top of the Nucleus stack page*/

    /*Initialize variables*/
    ReadyQueue = mkEmptyProcQ();  /*Initialize the Ready Queue*/
    currProc = NULL;  /*No process is running initially */
    procCnt = INITPROCCNT;  /*No active processes yet*/
    softBlockCnt = INITSBLOCKCNT;  /*No soft-blocked processes*/

	/*Initialize Level 2 data structures*/
	initPcbs(); /*Set up the Process Control Block (PCB) free list (pool of avaible pcbs)*/
	initASL(); /*Set up the Active Semaphore List (ASL)*/


    /*populate_passUpVec();*/

	/*Load the system-wide Interval Timer with 100 milliseconds*/
	LDIT(INITTIMER); /*Set interval timer to 100ms*/

	/*Create and Launch the First Process*/
	first_proc = allocPcb(); /*allocate a PCB from the PCB free list for the first process*/
	procCnt++;
	first_proc->p_s.s_status = IEPON | TEBITON | IMON;
	first_proc->p_s.s_pc = (memaddr) &test;   /*Set PC to test()*/ 
    first_proc->p_s.s_t9 = (memaddr) &test;   /*Set t9 register to test()*/
	memaddr topRAM;
	RAMTOP(topRAM);
	first_proc->p_s.s_sp = topRAM; /*Set stack ptr to ram TOP*/
	insertProcQ(&ReadyQueue, first_proc); /*insert new proc into ready queue to be scheduled*/
	switchProcess(); /*call scheduler*/
	return 1;
}

