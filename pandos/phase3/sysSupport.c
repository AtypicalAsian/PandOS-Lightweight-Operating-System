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

int support_device_sems[DEVICE_TYPES * DEVICE_INSTANCES]; /*Support level device semaphores*/



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
 * TO-DO 
 * terminate() is a essentially a wrapper for the kernel-mode restricted SYS2 service
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
 * TO-DO 
 * Returns the number of microseconds since system boot
 * The method calls the hardware TOD clock and stores in register v0
 **************************************************************************************************/
void getTOD(state_PTR excState)
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
void writeToPrinter(char *virtualAddr, int len, support_t *support_struct) {
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
    
    /*Check if address we're writing from is outside of the uproc logical address space*/

    /*Check if length of string is within bounds (0-128)*/
    if (len < 0 || len > 128 || (unsigned int) virtualAddr < KUSEG) /*DEFINE CONSTANTS FOR THESE*/
    {
        SYSCALL(SYS9, 0, 0, 0);
    }

    /*--------------Declare local variables---------------------*/
    int semIndex;
    int pid;
    int char_printed_count; /*tracks how many characters were printed*/
    char_printed_count = 0;
    /*----------------------------------------------------------*/
    pid = support_struct->sup_asid-1;
    semIndex = ((PRNTINT - OFFSET) * DEVPERINT) + pid;

    devregarea_t *devRegArea = (devregarea_t *)RAMBASEADDR; /* Pointer to the device register area */
    device_t *printerDevice = &(devRegArea->devreg[semIndex]);

    SYSCALL(SYS3, (memaddr) &support_device_sems[semIndex], 0, 0);

    int i;
    for (i = 0; i < len; i++)
    {
        /*
        Ref: princOfOperations section 5.6, table 5.11
             princOfOperations section 2.3
        */
        if (printerDevice->d_status == READY) /*no need to & with BUSY?*/
        {

            /* Need to perform setSTATUS (disable interrupt) to ensure the atomicity */
            setSTATUS(INTSOFF);

            printerDevice->d_data0 = (memaddr) * (virtualAddr + i);
            printerDevice->d_command = PRINTCHR;
            char_printed_count ++;

            /* Need to perform waitForIO to "truly" request printing the character */
            SYSCALL(SYS5, semIndex, pid, 0);

            /* Need to perform setSTATUS (enable interrupt again) to restore previous status & allow I/O request */
            setSTATUS(INTSOFF);
            
        }
        else
        {
            /*If printer device status code is not READY -> have to return negative of device status*/
            support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = -(printerDevice->d_status);
            break;
        }
    }

    /* Add SYSCALL 4 to unlock the semaphore */
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = char_printed_count;
    SYSCALL(SYS4,(memaddr) &support_device_sems[semIndex], 0, 0);
}


/**************************************************************************************************
 * TO-DO 
 **************************************************************************************************/
void writeToTerminal(char *virtualAddr, int len, support_t *support_struct) {
    if (len < 0 || len > 128 || (unsigned int) virtualAddr < KUSEG) {
        SYSCALL(SYS9, 0, 0, 0);
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

    /*Local variables*/
    int pid;
    int baseTerminalIndex;
    int semIndex;

    pid = support_struct->sup_asid-1;
    baseTerminalIndex = ((TERMINT - OFFSET) * DEVPERINT) + pid;
    semIndex = baseTerminalIndex + DEVPERINT;

    devregarea_t *devRegArea = (devregarea_t *) RAMBASEADDR;
    device_t *terminalDevice = &(devRegArea->devreg[semIndex]);
    SYSCALL(SYS3,(memaddr) &support_device_sems[semIndex], 0, 0);

    int transmittedChars;
    transmittedChars = 0;
    int i;

    for (i = 0; i < len; i ++) {
        memaddr transmitterStatus = (terminalDevice->d_data0 & TERMINAL_STATUS_MASK);
        if (transmitterStatus == TERMINAL_STATUS_READY) {
            /* memaddr oldStatus = getSTATUS(); */
            setSTATUS(INTSOFF);

            char transmitChar = *(virtualAddr + i);
            terminalDevice->d_data1 = TERMINAL_COMMAND_TRANSMITCHAR | (transmitChar << TERMINAL_CHAR_SHIFT);
            memaddr newStatus = (terminalDevice->d_data0 & TERMINAL_STATUS_MASK);

            SYSCALL(SYS5, semIndex, pid, 0);

            setSTATUS(INTSON);

            if (newStatus == TERMINAL_STATUS_TRANSMITTED) {
                transmittedChars ++;
            } else {
                transmittedChars = -(newStatus);
                break;
            }
        } else if (transmitterStatus != READY) {
            transmittedChars = -(transmitterStatus);
            break;
        }
    }
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = transmittedChars;
    SYSCALL(SYS4,(memaddr) &support_device_sems[semIndex], 0, 0);
}

void readTerminal(char *virtualAddr, support_t *support_struct){
    int device_instance = support_struct->sup_asid - 1;
    int charCount = 0;
    int status;
    char string = ' ';

    int semIndex = (TERMSEM * DEVICE_INSTANCES) + device_instance;

    SYSCALL(PASSEREN, (memaddr) &support_device_sems[semIndex], 0, 0);

    device_t* device_int = (device_t *)(DEVICEREGSTART + ((TERMINT - DISKINT) * (DEVICE_INSTANCES * DEVREGSIZE)) + (device_instance * DEVREGSIZE));

    while(((device_int->d_status & TERMSTATUSMASK) == READY) && (string != EOS)) {
        
        setSTATUS(INTSOFF);

        device_int->d_command = TRANSMITCHAR;

        status = SYSCALL(WAITIO, TERMINT, device_instance, TRUE);

        setSTATUS(INTSON);

        if((status & TERMSTATUSMASK) == OKCHARTRANS) {
            
            string = (status >> DEVICE_INSTANCES);

            if(string != EOS) {
                *virtualAddr = string;
                virtualAddr++;
                charCount++;
            }
        }
        else {
            string = EOS;
        }
    }

    if((device_int->d_status & TERMSTATUSMASK) != READY || (status & TERMSTATUSMASK) != OKCHARTRANS) {
        charCount = -(status);
    }
    SYSCALL(VERHOGEN, (memaddr) &support_device_sems[semIndex], 0, 0);
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = charCount;

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
 * DONE
 * TO-DO 
 * Support Level General Exception Handler
 * BIG PICTURE
 *      1. Obtain current process' support structure
 *      2. Examine Cause register in exceptState field of support structure and extract exception code
 *      3. Pass control to either the support level syscall handler or the program trap handler
 **************************************************************************************************/
void sysSupportGenHandler() {

    support_t *support_struct = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    int cause = support_struct->sup_exceptState[GENERALEXCEPT].s_cause;
    int exc_code = EXCCODE(cause);
    if (exc_code == 8) {
        int syscall_num = support_struct->sup_exceptState[GENERALEXCEPT].s_a0;
        supportSyscallHandler(syscall_num, support_struct);
    }
    else {
        trapExcHandler(support_struct);
    }
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
void supportSyscallHandler(int exc_code, support_t *support_struct)
{
    if (exc_code < TERMINATE || exc_code > DELAY) {
        trapExcHandler(support_struct);
        return;
    }

    int arg1 = support_struct->sup_exceptState[GENERALEXCEPT].s_a1;
    int arg2 = support_struct->sup_exceptState[GENERALEXCEPT].s_a2;

    switch(exc_code) {
        case TERMINATE:
            terminate(support_struct);
            break;

        case GET_TOD:
            getTOD(&support_struct->sup_exceptState[GENERALEXCEPT]);
            break;

        case WRITEPRINTER:
            writeToPrinter((char *) arg1, arg2, support_struct);
            break;

        case WRITETERMINAL:
            writeToTerminal((char *) arg1, arg2, support_struct);  
            break;

        case READTERMINAL:
            readTerminal((char *) arg1, support_struct);
            break;
        default:
            trapExcHandler(support_struct);
            break;
    }


    support_struct->sup_exceptState[GENERALEXCEPT].s_pc += WORDLEN;
    returnControlSup(support_struct, GENERALEXCEPT);

}