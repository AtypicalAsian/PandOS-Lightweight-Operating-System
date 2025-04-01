/**************************************************************************************************  
 * @file sysSupport.c  
 *  
 * 
 * @brief  
 * This module implements the Support Level’s:
 *      - general exception handler. [Section 4.6]
 *      - SYSCALL exception handler. [Section 4.7]
 *      - Program Trap exception handler. [Section 4.8]
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
 * TODO
 * This module implements
 *       general exception handler. [Section 4.6]
 *       SYSCALL exception handler. [Section 4.7]
 *       Program Trap exception handler. [Section 4.8] - vmSupport pass control here if page fault is a modification type
 * 
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

extern int deviceSema4s[MAXSHAREIODEVS];

/*SYSCALL 9-12 function declarations*/
HIDDEN void syscall_excp_handler(); /*Syscall exception handler*/
void gen_excp_handler(); /*General exception handler*/
void program_trap_handler(); /*Program Trap Handler*/
HIDDEN void terminate();    /*SYS9 - terminates the executing user process. Essentially a user-mode wrapper for SYS2 (terminate running process)*/
HIDDEN void get_TOD();      /*SYS10 - retrieve the the number of microseconds since the system was last booted/reset to be placed*/
HIDDEN void write_to_printer(); /*SYS11 - suspend requesting user proc until a line of output (string of characters) has been transmitted to the printer device associated with that U-proc*/
HIDDEN void write_to_terminal(); /*SYS12 - suspend requesting user proc until a line of output (string of characters) has been transmitted to the terminal device associated with that U-proc*/
HIDDEN void read_from_terminal(); /*SYS13*/

void program_trap_handler(){
    return;
}

void terminate() {

    /* Make call to SYS2

    Ref: pandos section 4.7.1 
    */
    SYSCALL(2, 0, 0, 0);

}

void get_TOD(state_t *excState) {

    /* Get number of ticks per seconds from the last time the system was booted/reset
    
    Ref: pandos section 4.7.2 
    */
    cpu_t currTime;
    STCK(currTime);

    excState->s_v0 = currTime;
}

void write_to_printer(char *virtAddr, int len, support_t *currProcSupport) {
    /* 
    Ref: pandos section 4.7.3 
    */
    if (len < 0 || len > 128) {
        SYSCALL(9, 0, 0, 0);
    }

    /* STEPS:
    1. Find semaphore index corresponding with printer device (like SYS5, for printer device, its interrupt line is 6, device number ???)
    2. After finding the semaphore, lock it before writing process
    3. Use For Loop to iterating through each character and write one by one to printer: start at virtAddr, end at virtAddr + len 
    (cond: the device register is ready to write)
       3.1. Retrieve current processor status before disabling all external interrupts
       3.2. Reset the status to 0x0 and disable all interrupts
       3.3. Pass character to printer device with d_data0 & d_command
       3.4. Request I/O to print the passed character 
       3.5. Enable the interrupt again by restoring the previous processor status
    
    Ref: princOfOperations section 5.1, 5.6
    */
   int semIndex;
   /*
   TO-DO: (Expand STEPS.1 & 2)
   1. PRINTER_LINE_NUM = 6
   2. Lock semaphore with semIndex (performing SYS5)
   
   */
   semIndex = ((PRINTER_LINE_NUM - OFFSET) * DEVPERINT) + (currProcSupport->sup_asid - 1);
   
   devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR; /* Pointer to the device register area */
   device_t *printerDevice = &(devRegArea->devreg[semIndex]);

   int i;
   for (i = 0; i < len; i ++) {
        /*
        Ref: princOfOperations section 5.6, table 5.11
             princOfOperations section 2.3
        */
        if ((printerDevice->d_status & PRINTER_BUSY) == PRINTER_READY) {
            unsigned int oldStatus = getSTATUS();

            /* Need to perform setSTATUS (disable interrupt) to ensure the atomicity */
            setSTATUS(STATUS_ALL_OFF);

            printerDevice->d_data0 = (memaddr) *(virtAddr + i);
            printerDevice->d_command = PRINTCHR;

            /* Need to perform waitForIO to "truly" request printing the character */
            SYSCALL(5, semIndex, 0, 0);

            /* Need to perform setSTATUS (enable interrupt again) to restore previous status & allow I/O request */
            setSTATUS(oldStatus);
        } else {
            break;
        }
    
   }

   /* Add SYSCALL 6 to unblock the semaphore */
   SYSCALL(6, 0, 0, 0);
}