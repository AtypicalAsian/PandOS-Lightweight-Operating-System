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
 *       Program Trap exception handler. [Section 4.8] - vmSupport pass control here if page fault is a modification type (should not happen in pandOS)
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
#include "/usr/include/umps3/umps/libumps.h"

extern int devRegSem[MAXSHAREIODEVS+1];

/**************************************************************************************************
 * TO-DO 
 * terminate() is a wrapper for the kernel-mode restricted SYS2 service
 **************************************************************************************************/
void terminate()
{
    /* Make call to SYS2
    Ref: pandos section 4.7.1
    */
    nuke_til_it_glows(NULL);
}

/**************************************************************************************************
 * TO-DO 
 * Returns the number of microseconds since system boot
 * The method calls the hardware TOD clock and stores in register v0
 **************************************************************************************************/
void get_TOD(state_PTR excState)
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
void write_to_printer(support_t *currProcSupport, state_PTR exceptionState)
{

    /* 
    Ref: pandos section 4.7.3
    STEPS:
    1. Check if address we're writing from is outside of the uproc logical address space
    2. Check if length of string is within bounds (0-128)
    3. Find semaphore index corresponding with printer device (like SYS5, for printer device, its interrupt line is 6, device number ???)
    4. Perform SYS3 to gain mutex over printer device
    5. Use For Loop to iterating through each character and write one by one to printer: start at virtAddr, end at virtAddr + len
    (cond: printer device is not busy (ready))
       5.1. Retrieve current processor status before disabling all external interrupts
       5.2. Reset the status to 0x0 and disable all interrupts
       5.3. Pass character to printer device with d_data0 & d_command
       5.4. Request I/O to print the passed character
       5.5. Enable the interrupt again by restoring the previous processor status
       5.6 Increment transmitted character count
    (cond: printer device is busy)
       5.7 Return the negative of device status in v0
    6. Load the transmitted char count into v0 register
    7. Perform SYS4 to release printer device sempahore

    Ref: princOfOperations section 5.1, 5.6
    */

    int len = exceptionState->s_a2;
    int asid = currProcSupport->sup_asid-1;
    char* virtAddr = (char*) exceptionState->s_a1;
    
    /*Check if address we're writing from is outside of the uproc logical address space*/

    /*Check if length of string is within bounds (0-128)*/
    if (len < 0 || len > 128 || (int) virtAddr < KUSEG) /*DEFINE CONSTANTS FOR THESE*/
    {
        nuke_til_it_glows(NULL);
    }
    else{
        /*--------------Declare local variables---------------------*/
        int semIndex;
        int pid;
        int char_printed_count; /*tracks how many characters were printed*/
        char_printed_count = 0;
        /*----------------------------------------------------------*/
        /*semIndex = ((PRINTER_LINE_NUM - OFFSET) * DEVPERINT) + asid;*/
        /*devregarea_t *devRegArea = (devregarea_t *)RAMBASEADDR;*/ /* Pointer to the device register area */
        /*device_t *printerDevice = &(devRegArea->devreg[semIndex]);*/
        dtpreg_t* printerDevice = (dtpreg_t*) DEV_REG_ADDR(PRINTER_LINE_NUM,asid);

        SYSCALL(SYS3, (memaddr) &devRegSem[DEV_INDEX(PRINTER_LINE_NUM,asid,FALSE)], 0, 0);
        int idx = 0;
        int response = 1;
        while (idx < len){
            IEDISABLE;
            printerDevice->data0 = *virtAddr;
            printerDevice->command = TERMINAL_COMMAND_TRANSMITCHAR;
            response = SYSCALL(SYS5,PRINTER_LINE_NUM,asid,FALSE);
            IEENABLE;

            if ((response & 0x000000FF) == READY){
                virtAddr++;
                idx++;
            }
            else{
                idx = -(response & 0x000000FF);
                break;
            }
        }
        SYSCALL(SYS4,(memaddr) &devRegSem[DEV_INDEX(PRINTER_LINE_NUM,asid,FALSE)],0,0);
        exceptionState->s_v0 = idx;
    }
}
/* Do we need to ACK? */

void write_to_terminal(support_t *currProcSupport, state_PTR exceptionState) {
    /*Local variables*/
    int pid;
    int baseTerminalIndex;
    int semIndex;
    int len;

    pid = currProcSupport->sup_asid-1;
    char* virtAddr = (char*) exceptionState->s_a1;
    len = exceptionState->s_a2;

    if (len <= 0 || len >= 128 || (int) virtAddr < KUSEG) {
        nuke_til_it_glows(NULL);
    }

    /* STEPS: Similar to write_to_printer(),
    1. Find the semaphore index corresponding with terminal device (the interrupt line is 7)
    2. Modify the base semaphore index since it's terminal device and the operation is WRITE -> we need to increment by DEVPRINT (8)
    3. Use For loop to interate each character starting from virtAddr to (virtAddr + len)
    4. Before transmitting, disable interrupt by setSTATUS and clear all the bits
    5. Set the command for terminal device with bit shift (like the guide in Ref)
    6. Request I/O to pass the character to terminal device
    7. Check terminal transmitted status
    8. Issue ACK to let the device return to ready state after each iteration
    9. Save the character count (success) or device's status value (FAIL) to v0
    10. Unlock the semaphore by calling SYS4 & restore the device status 

    Ref: princOfOperations section 5.7
         pandos section 3.5.5, pg 27,28
    */
    else{
        baseTerminalIndex = ((TERMINAL_LINE_NUM - OFFSET) * DEVPERINT) + pid;
        semIndex = baseTerminalIndex + DEVPERINT;

        devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;
        device_t *terminalDevice = &(devRegArea->devreg[semIndex]);

        SYSCALL(SYS3,(memaddr) &devRegSem[DEV_INDEX(TERMINAL_LINE_NUM,pid,FALSE)], 0, 0);
        int idx = 0;
        int response = 1;

        while (idx < len){
            terminalDevice->d_data1 = (*virtAddr << 8) | 2;
            response = SYSCALL(SYS5,7,pid,FALSE);
            if ((response & TRANSM_MASK) == 5){
                virtAddr++;
                idx++;
            }
            else{
                idx = -(response & TRANSM_MASK);
                break;
            }
        }
        SYSCALL(SYS4,(memaddr) &devRegSem[DEV_INDEX(TERMINAL_LINE_NUM,pid,FALSE)],0,0);
        exceptionState->s_v0 = idx;
    }
}

void read_from_terminal(support_t *currProcSupport, state_PTR exceptionState) {
    /* 
    STEPS:
    1. Find the semaphore index corresponding with terminal device (the interrupt line is 7)
    2. Modify the base semaphore index since it's terminal device and the operation is READ -> we maintain the same base index
    3. Use While loop to interate each character received from the device until there is no more character to read
    4. Before saving the character into the buffer, disable interrupt by setSTATUS and clear all the bits
    5. Retrieve the received command for terminal device and add bit shift to get character value
        5.1. If getting char is not successful, save the negative device status to an existing defined variable and get out of the loop
    6. Perform SYS5 to request I/O
    7. Issue ACK to let the device return to ready state after each iteration
    8. Save the character count (success) or device's status value (FAIL) to v0
    9. Unlock the semaphore by calling SYS4 & restore the device status 
    Ref: pandos section 4.7.5, princOfOperations chapter 5.7 
    */
   int asid = currProcSupport->sup_asid-1;
   char* virtAddr = (char*) exceptionState->s_a1;
   int idx = 0, response;
   if ((int) virtAddr < KUSEG){
        nuke_til_it_glows(NULL);
    }
    else{
        /*Local variables*/
        int baseTerminalIndex;
        int semIndex;

        baseTerminalIndex = ((TERMINAL_LINE_NUM - OFFSET) * DEVPERINT) + asid;
        semIndex = baseTerminalIndex;

        devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;
        device_t *terminalDevice = &(devRegArea->devreg[semIndex]);
        SYSCALL(SYS3,(memaddr) &devRegSem[DEV_INDEX(TERMINAL_LINE_NUM,asid,TRUE)], 0, 0);
        while (TRUE){
            terminalDevice->d_command = 2;
            response = SYSCALL(SYS5,TERMINAL_LINE_NUM,asid,TRUE);
            if ((response & RECV_MASK) == 5){
                *virtAddr = (response & 0x0000FF00) >> 8;
                virtAddr++;
                idx++;
                /*if(((response & 0x0000FF00) >> 8) == 0x0a){
                    break;
                }*/
                if(((response & 0x0000FF00) >> 8) == '\n'){
                    break;
                }
            } else{
                idx = -(response & RECV_MASK);
                break;
            }
        }
        SYSCALL(SYS4,(memaddr) &devRegSem[DEV_INDEX(TERMINAL_LINE_NUM,asid,TRUE)],0,0);
        exceptionState->s_v0 = idx;
    }
}
/* Order of each step in WHILE LOOP */


/**************************************************************************************************
 * TO-DO 
 * Support Level Syscall Exception Handler
 * BIG PICTURE
 * Steps:
 *      1. Check if the syscall number falls within the range 9-13
 *            If invalid syscall number, handle as program trap
 *      2. Reads parameters in registers a1,a2,a3
 *      3. Manually imcrement PC+4 to avoid re-executing Syscall on return
 *      4. Execute appropriate syscall handler
 *      5. LDST to return to process that requested SYSCALL
 **************************************************************************************************/
void syscall_excp_handler(support_t *currProc_support_struct,unsigned int syscall_num_requested,state_t* exceptionState){
    /*--------------Declare local variables---------------------*/
    char* virtualAddr;    /*value stored in a1 - here it's the ptr to first char to be written/read*/
    int length;     /*value stored in a2 - here it's the length of the string to be written/read*/
    int index;
    int resp;
    char char_received;
    /*int param3;*/     /*value stored in a3*/
    /*----------------------------------------------------------*/

    switch(syscall_num_requested){
        case SYS9:
            terminate();
            break;
        case SYS10:
            get_TOD(exceptionState);
            break;
        case SYS11:
            write_to_printer(currProc_support_struct,exceptionState);
            break;
        case SYS12:
            write_to_terminal(currProc_support_struct,exceptionState);
            break;
        case SYS13:
            read_from_terminal(currProc_support_struct,exceptionState);
            break;
        default:
            nuke_til_it_glows(NULL);
            break;
    }
    LDST(exceptionState);
}

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
    support_t* currProc_supp_struct;    /*current process support structure*/
    int exception_code;                 /*exception code inside Cause Register*/
    int requested_syscall_num;          /*syscall number requested by calling process*/
    /*----------------------------------------------------------*/

    /*Step 1: Obtain current process support structure via syscall number 8*/
    currProc_supp_struct = (support_t*) SYSCALL(SYS8,0,0,0);
    state_t* exceptionState = (state_t*) &(currProc_supp_struct->sup_exceptState[GENERALEXCEPT]);
    requested_syscall_num = exceptionState->s_a0;

    /*Step 2: Examine Cause register in exceptState field of support structure and extract exception code*/
    exception_code = (((currProc_supp_struct->sup_exceptState[GENERALEXCEPT].s_cause) & GETEXCPCODE) >> CAUSESHIFT);

    /*Step 3: Pass control to appropriate handler based on exception code*/
    if (exception_code == SYSCONST){   /*If Cause.ExcCode is set to 8 -> call the syscall handler method [pandOS 3.7.1]*/
        syscall_excp_handler(currProc_supp_struct,requested_syscall_num,exceptionState);
    }
    else{
        nuke_til_it_glows(NULL);
    }
}


