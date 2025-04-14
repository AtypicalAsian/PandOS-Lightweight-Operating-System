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

/*Support level device semaphores*/
int support_device_sems[DEVICE_TYPES * DEVICE_INSTANCES]; 



/**************************************************************************************************
 * DONE
 * This function is a wrapper to perform LDST
 * Can't use LDST directly in phase 3?
 **************************************************************************************************/
void returnControlSup(support_t *support, int exc_code)
{
    LDST(&(support->sup_exceptState[exc_code]));
}



/************************************************************************************************** 
 * @brief terminate() is a essentially a wrapper for the kernel-mode restricted SYS2 service
 * 
 * @details
 * 
 * @ref
 * pandOS - section 4.7.1
 **************************************************************************************************/
void terminate(support_t *support_struct)
{
    int dev_num = support_struct->sup_asid - 1;

    /*If the process is currently holding mutex of devices -> release all those locks*/
    int i;
    for (i = 0; i < DEVICE_TYPES; i++) {
        int index = i * DEVICE_INSTANCES + dev_num;
        if (support_device_sems[index] == 0){
            SYSCALL(SYS4, (memaddr)&support_device_sems[index],0,0);
        }
    }

    /*When u-proc terminates, mark all frames it occupy as free (no longer in use)*/
    for (i = 0; i < MAXPAGES; i++) {
        if(support_struct->sup_privatePgTbl[i].entryLO & VALIDON){
            setSTATUS(INTSOFF);
            support_struct->sup_privatePgTbl[i].entryLO &= ~VALIDON;
            update_tlb_handler(&(support_struct->sup_privatePgTbl[i]));
            setSTATUS(INTSON);
        }
    }
    SYSCALL(SYS4, (memaddr) &masterSema4, 0, 0);
    deallocate(support_struct);
    SYSCALL(SYS2, 0, 0, 0); /*Make the call to sys2 to terminate the uproc and its child processes*/
}


/**************************************************************************************************
 * @brief Return the number of microseconds since system boot
 * The method calls the hardware TOD clock and stores in register v0
 * 
 * @note Save current TOD to register v0
 * 
 * @ref
 * pandOS - section 4.7.2
 **************************************************************************************************/
void getTOD(state_PTR excState)
{
   cpu_t currTime;
   STCK(currTime);
   excState->s_v0 = currTime;
}


/**************************************************************************************************
 * @brief The method implements a syscall to perform WRITE operation to printer device
 * 
 * @details
 * 1. Check if address we're writing from is outside of the uproc logical address space
 * 2. Check if length of string is within bounds (0-128)
 * 3. Find semaphore index corresponding with printer device (like SYS5, for printer device, its interrupt line is 6, device number ???)
 * 4. Perform SYS3 to gain mutex over printer device
 * 5. Use For Loop to iterating through each character in the range [virtAddr, virtAddr + len], and write one by one to printer: 
 *  5.1 Given that the printer device is ready:
        - Retrieve current processor status before disabling all external interrupts
        - Reset the status to 0x0 and disable all interrupts
        - Pass character to printer device with d_data0 & d_command
        - Request I/O to print the passed character
        - Enable the interrupt again by restoring the previous processor status
        - Increment transmitted character count
    5.2 Given that the printer device is busy:
        - Return the negative of device status in v0
 * 6. Load the transmitted char count into register v0
 * 7. Perform SYS4 to release printer device sempahore
 * 
 * @ref
 * pandOS - section 4.7.3
 * princOfOperations - Chapter 4.2, Device register area starts from address 0x10000054
 * princOfOperations - Chapter 5.1, 5.6, Interrupt lines 3–7 are used for peripheral devices
 **************************************************************************************************/
void writeToPrinter(char *virtualAddr, int len, support_t *support_struct) {
    /*Check if address we're writing from is outside of the uproc logical address space*/

    /*Check if length of string is within bounds (0-128)*/
    /*if (len < 0 || len > 128 || (unsigned int) virtualAddr < KUSEG)
    {
        SYSCALL(SYS9, 0, 0, 0);
    }*/

    /*--------------Declare local variables---------------------*/
    int semIndex;
    int pid;
    int char_printed_count; /*tracks how many characters were printed*/
    char_printed_count = 0;
    /*----------------------------------------------------------*/

    pid = support_struct->sup_asid - 1;
    semIndex = ((PRNTINT - OFFSET) * DEVPERINT) + pid;
    char_printed_count = 0;

    SYSCALL(PASSEREN, (memaddr)&support_device_sems[semIndex], 0, 0);

    devregarea_t *busRegArea = (devregarea_t *)RAMBASEADDR; /* Pointer to the bus register area */
    device_t *printerDev = &(busRegArea->devreg[semIndex]);

    int i;
    for (i = 0; i < len; i++) {
        if(printerDev->d_status == READY) {
            setSTATUS(INTSOFF);
            printerDev->d_data0 = ((int) *(virtualAddr + i));
            printerDev->d_command = PRINTCHR;
            SYSCALL(WAITIO, PRNTINT, pid, 0);
            setSTATUS(INTSON);
            char_printed_count++;
        }
        else {
            char_printed_count = -(printerDev->d_status);
            i = len;
        }
    }
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = char_printed_count;
    SYSCALL(VERHOGEN, (memaddr) &support_device_sems[semIndex], 0, 0);
}


/**************************************************************************************************
 * @brief The method implements a syscall to perform WRITE operation to terminal device
 * 
 * @details
 * 1. Find the semaphore index corresponding with terminal device (the interrupt line is 7)
 * 2. Modify the base semaphore index since it's terminal device and the operation is WRITE -> we need to increment by DEVPRINT (8)
 * 3. Use For loop to interate each character starting from virtAddr to (virtAddr + len)
 * 4. Before transmitting, disable interrupt by setSTATUS and clear all the bits
 * 5. Set the command for terminal device with bit shift (like the guide in Ref)
 * 6. Request I/O to pass the character to terminal device
 * 7. Check terminal transmitted status
 * 8. Issue ACK to let the device return to ready state after each iteration (no need to do this)
 * 9. Save the character count (success) or device's status value (FAIL) to v0
 * 10. Unlock the semaphore by calling SYS4 & restore the device status 
 * 
 * @ref 
 * princOfOperations - section 5.7
 * pandOS - section 3.5.5
 **************************************************************************************************/
void writeToTerminal(char *virtualAddr, int len, support_t *support_struct) {
    if (len < 0 || len > 128 || (unsigned int) virtualAddr < KUSEG) {
        SYSCALL(SYS9, 0, 0, 0);
    }

    /*Local variables*/
    int pid;
    int baseTerminalIndex;
    int semIndex;
    int status;
    int transmittedChars;
    transmittedChars = 0;

    pid = support_struct->sup_asid-1;
    baseTerminalIndex = ((TERMINT - OFFSET) * DEVPERINT) + pid;
    semIndex = baseTerminalIndex + DEVPERINT;

    /*Calculate the offset for the terminal device row relative to disk*/
    unsigned int terminalOffset = (TERMINT - DISKINT) * (DEVICE_INSTANCES * DEVREGSIZE);

    /*Calculate the offset for the specific device instance (column) within that row*/
    unsigned int instanceOffset = pid * DEVREGSIZE;

    /*The total offset is the sum of the row offset and the instance offset*/
    unsigned int totalOffset = terminalOffset + instanceOffset;

    /*Add the total offset to the base address of the device registers*/
    device_t *terminalDevice = (device_t *)(DEVICEREGSTART + totalOffset);

    SYSCALL(SYS3,(memaddr) &support_device_sems[semIndex], 0, 0);

    int i;

    for (i = 0; i < len; i ++) {
        memaddr transmitterStatus = (terminalDevice->d_data0 & TERMINAL_STATUS_MASK);
        if (transmitterStatus == TERMINAL_STATUS_READY) {
            /* memaddr oldStatus = getSTATUS(); */
            setSTATUS(INTSOFF);
            char transmitChar = *(virtualAddr + i);
            terminalDevice->d_data1 = TERMINAL_COMMAND_TRANSMITCHAR | (transmitChar << TERMINAL_CHAR_SHIFT);
            /*memaddr newStatus = (terminalDevice->d_data0 & TERMINAL_STATUS_MASK);*/
            status = SYSCALL(SYS5, TERMINT, pid, 0);
            transmittedChars++;
            setSTATUS(INTSON);
        } else {
            transmittedChars = -(status);
            i = len;
        }
    }
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = transmittedChars;
    SYSCALL(SYS4,(memaddr) &support_device_sems[semIndex], 0, 0);
}

/**************************************************************************************************
 * @brief The method implements a syscall to perform READ operation from terminal device
 *
 * @details
 * 1. Find the semaphore index corresponding with terminal device (the interrupt line is 7)
 * 2. Modify the base semaphore index since it's terminal device and the operation is READ 
 * -> We maintain the same base index
 * 3. Use While loop to interate each character received from the device until there is no more character to read
 * 4. Before saving the character into the buffer, disable interrupt by setSTATUS and clear all the bits
 * 5. Retrieve the received command for terminal device and add bit shift to get character value
        - If getting char is not successful: 
            - Save the negative device status to an existing defined variable 
            - Get out of the loop
 * 6. Perform SYS5 to request I/O
 * 7. Issue ACK to let the device return to ready state after each iteration
 * 8. Save the character count (SUCCESS) or device's status value (FAIL) to register v0
 * 9. Unlock the semaphore by calling SYS4 & restore the device status
 * 
 * @ref
 * pandOS - section 4.7.5
 * princOfOperations - chapter 5.7 
 **************************************************************************************************/
void readTerminal(char *virtualAddr, support_t *support_struct){

    if (virtualAddr == NULL) {
        SYSCALL(SYS9, 0, 0, 0);
    }

    /*Local variables*/
    int pid;
    int baseTerminalIndex;
    int semIndex;

    pid = support_struct->sup_asid-1;
    baseTerminalIndex = ((TERMINT - OFFSET) * DEVPERINT) + pid;
    semIndex = baseTerminalIndex;
    /*Calculate the offset for the terminal device row relative to disk*/
    unsigned int terminalOffset = (TERMINT - DISKINT) * (DEVICE_INSTANCES * DEVREGSIZE);

    /*Calculate the offset for the specific device instance (column) within that row*/
    unsigned int instanceOffset = pid * DEVREGSIZE;

    /*The total offset is the sum of the row offset and the instance offset*/
    unsigned int totalOffset = terminalOffset + instanceOffset;

    /*Add the total offset to the base address of the device registers*/
    device_t *terminalDevice = (device_t *)(DEVICEREGSTART + totalOffset);

    SYSCALL(SYS3,(memaddr) &support_device_sems[semIndex], 0, 0);

    char currStr = ' ';
    int receivedChars;
    receivedChars = 0;
    int readStatus;

    while(((terminalDevice->d_status & TERMSTATUSMASK) == READY) && (currStr != EOS)) {
        setSTATUS(INTSOFF);
        terminalDevice->d_command = TRANSMITCHAR;
        readStatus = SYSCALL(WAITIO, TERMINT, pid, TRUE);
        setSTATUS(INTSON);
        if((readStatus & TERMSTATUSMASK) == OKCHARTRANS) {
            currStr = (readStatus >> DEVICE_INSTANCES);
            if(currStr != '\n') {
                *virtualAddr = currStr;
                virtualAddr++;
                receivedChars++;
            }
        }
        else {
            currStr = '\n';
        }
    }
    if((terminalDevice->d_status & TERMSTATUSMASK) != READY || (readStatus & TERMSTATUSMASK) != OKCHARTRANS) {
        receivedChars = -(readStatus);
    }

    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = receivedChars;
    SYSCALL(SYS4,(memaddr) &support_device_sems[semIndex], 0, 0);
    /*IMPLEMENTATION ENDS*/

}

/**************************************************************************************************
 * TO-DO 
 * Support Level Program Trap Handler
 * BIG PICTURE
 **************************************************************************************************/
void trapExcHandler(support_t *support_struct)
{
    terminate(support_struct);
}


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
void syscall_excp_handler(support_t *currProc_support_struct,int syscall_num_requested){
    /*--------------Declare local variables---------------------*/
    char* virtualAddr;    /*value stored in a1 - here it's the ptr to first char to be written/read*/
    int length;     /*value stored in a2 - here it's the length of the string to be written/read*/
    /*----------------------------------------------------------*/

    /* Validate syscall number */
    if (syscall_num_requested < 9 || syscall_num_requested > 13) {
        /* Invalid syscall number, treat as Program Trap */
        trapExcHandler(currProc_support_struct);
        return;
    }

    /*Step 2: Read values in registers a1-a3*/
    virtualAddr = (char *) currProc_support_struct->sup_exceptState[GENERALEXCEPT].s_a1;
    length = currProc_support_struct->sup_exceptState[GENERALEXCEPT].s_a2;
    /*param3 = currProc_support_struct->sup_exceptState[GENERALEXCEPT].s_a3;*/ /*no need for value in a3*/  

    /*Step 3: Increment PC+4 to execute next instruction on return*/
    currProc_support_struct->sup_exceptState[GENERALEXCEPT].s_pc += WORDLEN;

    /*Step 4: Execute appropriate syscall helper method based on requested syscall number*/
    switch(syscall_num_requested) {
        case SYS9:
            terminate(currProc_support_struct);
            break;

        case SYS10:
            getTOD(&currProc_support_struct->sup_exceptState[GENERALEXCEPT]);
            break;

        case SYS11:
            writeToPrinter(virtualAddr, length, currProc_support_struct);
            break;

        case SYS12:
            writeToTerminal(virtualAddr, length, currProc_support_struct);  
            break;

        case SYS13:
            readTerminal(virtualAddr, currProc_support_struct);
            break;

        default:
            trapExcHandler(currProc_support_struct);
            break;
    }
    returnControlSup(currProc_support_struct, GENERALEXCEPT);
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
void sysSupportGenHandler() {
    /*--------------Declare local variables---------------------*/
    support_t* currProc_supp_struct;    /*current process support structure*/
    int exception_code;                 /*exception code inside Cause Register*/
    int requested_syscall_num;          /*syscall number requested by calling process*/
    /*----------------------------------------------------------*/

    /*Step 1: Obtain current process support structure via syscall number 8*/
    currProc_supp_struct = (support_t*) SYSCALL(SYS8,0,0,0);

    /*Step 2: Examine Cause register in exceptState field of support structure and extract exception code*/
    exception_code = (((currProc_supp_struct->sup_exceptState[GENERALEXCEPT].s_cause) & GETEXCPCODE) >> CAUSESHIFT);

    /*Step 3: Pass control to appropriate handler based on exception code*/
    if (exception_code == 8){   /*If Cause.ExcCode is set to 8 -> call the syscall handler method [pandOS 3.7.1]*/
        requested_syscall_num = currProc_supp_struct->sup_exceptState[GENERALEXCEPT].s_a0; /*the syscall number is stored in register a0 [pandOS 3.7.1]*/
        syscall_excp_handler(currProc_supp_struct,requested_syscall_num); /*Pass to support level syscall handler*/
    }
    trapExcHandler(currProc_supp_struct); /*Otherwise, treat the exception as a program trap*/
}