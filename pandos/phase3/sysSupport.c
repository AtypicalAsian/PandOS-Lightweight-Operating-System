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
HIDDEN void get_TOD(state_t *excState);      /*SYS10 - retrieve the the number of microseconds since the system was last booted/reset to be placed*/
HIDDEN void write_to_printer(char *virtAddr, int len, support_t *currProcSupport); /*SYS11 - suspend requesting user proc until a line of output (string of characters) has been transmitted to the printer device associated with that U-proc*/
HIDDEN int write_to_terminal(char *virtAddr, int len, support_t *currProcSupport); /*SYS12 - suspend requesting user proc until a line of output (string of characters) has been transmitted to the terminal device associated with that U-proc*/
HIDDEN void read_from_terminal(); /*SYS13*/


/**************************************************************************************************
 * DONE
 * TO-DO 
 * Support Level General Exception Handler
 * BIG PICTURE
 *      1. Obtain current process' support structure
 *      2. Examine Cause register in exceptState field of support structure and extract exception code
 *      3. Pass control to either the support level syscall handler or the program trap handler
 **************************************************************************************************/
void gen_excp_handler(){
    /*--------------Declare local variables---------------------*/
    support_t* currProc_supp_struct;
    int exception_code;
    /*----------------------------------------------------------*/

    /*Step 1: Obtain current process support structure via syscall number 8*/
    currProc_supp_struct = (support_t*) SYSCALL(SYS8,0,0,0);

    /*Step 2: Examine Cause register in exceptState field of support structure and extract exception code*/
    exception_code = (((currProc_supp_struct->sup_exceptState[GENERALEXCEPT].s_cause) & GETEXCPCODE) >> CAUSESHIFT);

    /*Step 3: Pass control to appropriate handler based on exception code*/
    if (exception_code == SYSCONST){   /*If exception code is 8 -> call the syscall handler method*/
        syscall_excp_handler();
    }
    program_trap_handler(); /*Otherwise, treat the exception as a program trap*/
}


/**************************************************************************************************
 * TO-DO 
 * Support Level Program Trap Handler
 * BIG PICTURE
 **************************************************************************************************/
void program_trap_handler(){
    terminate();
}

/**************************************************************************************************
 * TO-DO 
 * terminate() is a wrapper for the kernel-mode restricted SYS2 service
 **************************************************************************************************/
void terminate()
{
    /* Make call to SYS2
    Ref: pandos section 4.7.1
    */
    SYSCALL(2, 0, 0, 0);
}

/**************************************************************************************************
 * TO-DO 
 * Returns the number of microseconds since system boot
 * The method calls the hardware TOD clock and stores in register v0
 **************************************************************************************************/
void get_TOD(state_t *excState)
{

    /* Get number of ticks per seconds from the last time the system was booted/reset

    Ref: pandos section 4.7.2
    */
    cpu_t currTime;
    STCK(currTime);

    excState->s_v0 = currTime;
}


/**************************************************************************************************
 * TO-DO 
 **************************************************************************************************/
void write_to_printer(char *virtAddr, int len, support_t *currProcSupport)
{
    /*
    Ref: pandos section 4.7.3
    */
    if (len < 0 || len > 128)
    {
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
    semIndex = ((PRINTER_LINE_NUM - OFFSET) * DEVPERINT) + (currProcSupport->sup_asid - 1);

    devregarea_t *devRegArea = (devregarea_t *)RAMBASEADDR; /* Pointer to the device register area */
    device_t *printerDevice = &(devRegArea->devreg[semIndex]);

    int i;
    for (i = 0; i < len; i++)
    {
        /*
        Ref: princOfOperations section 5.6, table 5.11
             princOfOperations section 2.3
        */
        if ((printerDevice->d_status & BUSY) == READY)
        {
            memaddr oldStatus = getSTATUS();

            /* Need to perform setSTATUS (disable interrupt) to ensure the atomicity */
            setSTATUS(STATUS_ALL_OFF);

            printerDevice->d_data0 = (memaddr) * (virtAddr + i);
            printerDevice->d_command = PRINTCHR;

            /* Need to perform waitForIO to "truly" request printing the character */
            SYSCALL(5, semIndex, 0, 0);

            /* Need to perform setSTATUS (enable interrupt again) to restore previous status & allow I/O request */
            setSTATUS(oldStatus);
        }
        else
        {
            break;
        }
    }

    /* Add SYSCALL 6 to unlock the semaphore */
    SYSCALL(6, 0, 0, 0);
}

int write_to_terminal(char *virtAddr, int len, support_t *currProcSupport) {

    if (len < 0 || len > 128) {
        SYSCALL(9, 0, 0, 0);
    }

    /* STEPS: Similar to write_to_printer(),
    1. Find the semaphore index corresponding with terminal device (the interrupt line is 7)
    2. Modify the base semaphore index since it's terminal device and the operation is WRITE -> we need to increment by DEVPRINT (8)
    3. Use For loop to interate each character starting from virtAddr to (virtAddr + len)
    4. Before transmitting, disable interrupt by setSTATUS and clear all the bits
    5. Set the command for terminal device with bit shift (like the guide in Ref)
    6. Request I/O to pass the character to terminal device
    7. Check terminal transmitted status
    8. Issue ACK to notify the interrupt
    9. Unlock the semaphore by calling SYS6 & restore the device status 

    Ref: princOfOperations section 5.7
         pandos section 3.5.5, pg 27,28
    */

    int baseTerminalIndex = ((TERMINAL_LINE_NUM - OFFSET) * DEVPERINT) + (currProcSupport->sup_asid - 1);
    int semIndex = baseTerminalIndex + DEVPERINT;

    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;
    termreg_t *terminalDevice = &(devRegArea->devreg[semIndex]);

    int transmittedChars;
    transmittedChars = 0;
    int i;

    for (i = 0; i < len; i ++) {
        memaddr transmitterStatus = (terminalDevice->transm_status & TERMINAL_STATUS_MASK);
        if (transmitterStatus == TERMINAL_STATUS_READY) {
            memaddr oldStatus = getSTATUS();
            setSTATUS(STATUS_ALL_OFF);

            char transmitChar = *(virtAddr + i);
            terminalDevice->transm_command = TERMINAL_COMMAND_TRANSMITCHAR | (transmitChar << TERMINAL_CHAR_SHIFT);

            SYSCALL(5, semIndex, 0, 0);
            memaddr newStatus = (terminalDevice->transm_status & TERMINAL_STATUS_MASK);

            terminalDevice->transm_command = ACK;

            setSTATUS(oldStatus);

            if (newStatus == TERMINAL_STATUS_TRANSMITTED) {
                transmittedChars ++;
            } else {
                SYSCALL(6, 0, 0, 0);
                return -newStatus;
            }
        } else if (transmitterStatus != READY) {
            SYSCALL(6, semIndex, 0, 0);
            return -transmitterStatus;
        }

    }

    SYSCALL(6, semIndex, 0, 0);
    return transmittedChars;
}