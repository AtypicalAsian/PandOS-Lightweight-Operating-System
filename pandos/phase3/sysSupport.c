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
#include "/usr/include/umps3/umps/libumps.h"

/*Support level device semaphores*/
int devSema4_support[DEVICE_TYPES * DEVICE_INSTANCES]; 



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
 * @brief get_nuked() (or SYS9) is a essentially a wrapper for the kernel-mode restricted SYS2 service
 * 
 * @details
 * 1. Calculate the device index based on the uproc's proccess_id (ASID)
 * 2. Release all device semaphores the uproc is holding
 * 3. Invalidate all frames in the page table of the current uproc
 * 4. Decrement the master semaphore & de-allocate support_struct of U's proc (return back to free pool of suppStructs)
 * 5. Make SYSCALL 2 to terminate U's proc and its children processes
 * 
 * @param: support_struct - pointer to the support structure of the current uproc. This makes it easier to access
 *                          fields like asid and the private page table
 * @return: None
 * 
 * @ref
 * pandOS - section 4.7.1
 * princOfOperations - section 4.6.2
 **************************************************************************************************/
void get_nuked(support_t *support_struct)
{
    int dev_num = support_struct->sup_asid - 1;

    /*If the process is currently holding mutex of devices -> release all those locks*/
    int i;
    for (i = 0; i < DEVICE_TYPES; i++) {
        int index = i * DEVICE_INSTANCES + dev_num;
        if (devSema4_support[index] == 0){
            SYSCALL(SYS4, (memaddr)&devSema4_support[index],0,0);
        }
    }

    /*When u-proc terminates, mark all frames it occupy as free (no longer in use)*/
    for (i = 0; i < MAXPAGES; i++) {
        if(support_struct->sup_privatePgTbl[i].entryLO & VALIDON){
            setSTATUS(INTSOFF);
            support_struct->sup_privatePgTbl[i].entryLO &= ~VALIDON; /*invalidate the page*/
            update_tlb_handler(&(support_struct->sup_privatePgTbl[i])); /*update TLB to maintain consistency with page tables*/
            setSTATUS(INTSON);
        }
    }
    SYSCALL(SYS4, (memaddr) &masterSema4, 0, 0);
    deallocate(support_struct); /*de-allocate the support structure*/
    SYSCALL(SYS2, 0, 0, 0); /*Make the call to sys2 to terminate the uproc and its child processes*/
}


/**************************************************************************************************
 * @brief Return the number of microseconds since system boot
 * The method calls the hardware TOD clock and stores the return value in register v0
 * 
 * @param: excState - pointer to saved exception state
 * @return: None
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
   excState->s_v0 = currTime; /*Place time in register v0*/
}


/**************************************************************************************************
 * @brief The method performs a WRITE operation to a specific printer device
 * 
 * @details
 * 1. Check if address we're writing from is outside of the uproc logical address space & if length
 *    of string to be written is within bounds (0-128)
 * 2. Find semaphore index corresponding with printer device (like SYS5, for printer device, its interrupt line is 6, device number ???)
 * 3. Perform SYS3 to gain mutex over printer device
 * 4. Use For Loop to iterating through each character in the range [virtAddr, virtAddr + len], and write one by one to printer: 
 *  5.1 Given that the printer device is ready:
        - Retrieve current processor status before disabling all external interrupts
        - Reset the status to 0x0 and disable all interrupts
        - Pass character to printer device with d_data0 & d_command
        - Request I/O to print the passed character
        - Enable the interrupt again by restoring the previous processor status
        - Increment transmitted character count
    5.2 Given that the printer device is busy:
        - Return the negative of device status in v0
 * 6. Load the transmitted char count into register v0 (or negative status value if print was unsuccessful)
 * 7. Perform SYS4 to release printer device sempahore
 * 
 * @param:
 *      1. virtualAddr - starting address of the first character in the string to be transmitted to printer
 *      2. len - length of string to be transmitted to printer
 *      3. support_struct - pointer to support struct of current uproc
 * 
 * @return: None
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
    int semIndex; /*Index to the device semaphore array*/
    int pid; /*printer id*/
    int char_printed_count; /*tracks how many characters were printed*/
    char_printed_count = 0;
    /*----------------------------------------------------------*/

    pid = support_struct->sup_asid - 1;
    semIndex = ((PRNTINT - OFFSET) * DEVPERINT) + pid;
    char_printed_count = 0;

    SYSCALL(PASSEREN, (memaddr)&devSema4_support[semIndex], 0, 0); /*Lock the printer device*/

    devregarea_t *busRegArea = (devregarea_t *)RAMBASEADDR; /* Pointer to the bus register area */
    device_t *printerDev = &(busRegArea->devreg[semIndex]); /*Pointer to printer device register*/

    int i;
    for (i = 0; i < len; i++) { /*Iterate over each character in the string to be printed*/
        if(printerDev->d_status == READY) {
            setSTATUS(INTSOFF); /*disable interrupts*/
            printerDev->d_data0 = ((int) *(virtualAddr + i)); /*Set data0 to the character to be transmitted to printer*/
            printerDev->d_command = PRINTCHR; /*Set command field to 2 to transmit the character in data0 to printer*/
            SYSCALL(WAITIO, PRNTINT, pid, 0); /*block the process until I/O completes*/
            setSTATUS(INTSON); /*enable interrupts*/
            char_printed_count++; /*increment transmitted character count*/
        }
        else { /*If printer device is BUSY*/
            char_printed_count = -(printerDev->d_status); /*return negative of device's status value in v0*/
            break;
        }
    }
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = char_printed_count; /*return transmitted character count in v0 if successful print*/
    SYSCALL(VERHOGEN, (memaddr) &devSema4_support[semIndex], 0, 0); /*unlock printer device*/
}


/**************************************************************************************************
 * @brief The method performs a WRITE operation to terminal device
 * 
 * @details
 * 1. Find the semaphore index corresponding with terminal device
 * 2. Modify the base semaphore index since it's terminal device and the operation is WRITE -> we need to increment by DEVPRINT (8)
 * 3. Use For loop to interate each character starting from virtAddr to (virtAddr + len)
 * 4. Before transmitting, disable interrupt by setSTATUS and clear all the bits
 * 5. Set the command for terminal device with bit shift (referenced @ref)
 * 6. Request I/O to pass the character to terminal device
 * 7. Check terminal transmitted status
 * 8. Issue ACK to let the device return to ready state after each iteration (no need to do this)
 * 9. Save the character count (success) or device's status value (FAIL) to v0
 * 10. Unlock the semaphore by calling SYS4 & restore the device status 
 * 
 * 
 * @param:
 *      1. virtualAddr - starting address of the first character in the string to be transmitted to printer
 *      2. len - length of string to be transmitted to printer
 *      3. support_struct - pointer to support struct of current uproc
 * 
 * @return: None
 * 
 * @ref 
 * princOfOperations - section 5.7
 * pandOS - section 3.5.5
 **************************************************************************************************/
void writeToTerminal(char *virtualAddr, int len, support_t *support_struct) {
    if (len < 0 || len > 128 || (unsigned int) virtualAddr < KUSEG) {
        SYSCALL(SYS9, 0, 0, 0);
    }

   /*--------------Declare local variables---------------------*/
    int term_id; /*terminal id*/
    int semIndex; /*Index to the device semaphore array*/
    int status;
    int transmittedChars; /*tracks how many characters were printed*/
    transmittedChars = 0; 
    /*----------------------------------------------------------*/


    term_id = support_struct->sup_asid-1;
    int baseTerminalIndex = ((TERMINT - OFFSET) * DEVPERINT) + term_id;
    semIndex = baseTerminalIndex + DEVPERINT; /*Transmission device semaphores are 8 bits behind reception for terminal devices*/

    /*Calculate the offset for the terminal device row relative to disk*/
    unsigned int terminalOffset = (TERMINT - DISKINT) * (DEVICE_INSTANCES * DEVREGSIZE);

    /*Calculate the offset for the specific device within that row*/
    unsigned int instanceOffset = term_id * DEVREGSIZE;

    /*The total offset is the sum of the row offset and the instance offset*/
    unsigned int totalOffset = terminalOffset + instanceOffset;

    /*Add the total offset to the base address of the device registers*/
    device_t *terminalDevice = (device_t *)(DEVICEREGSTART + totalOffset);

    SYSCALL(SYS3,(memaddr) &devSema4_support[semIndex], 0, 0); /*Lock terminal device*/

    /*Iterate through each character in the string*/
    int i;
    for (i = 0; i < len; i ++) {
        memaddr transmitterStatus = (terminalDevice->d_data0 & TERMINAL_STATUS_MASK);
        if (transmitterStatus == TERMINAL_STATUS_READY) {
            setSTATUS(INTSOFF); /*disable interrupts*/
            char transmitChar = *(virtualAddr + i); 
            terminalDevice->t_transm_command = TERMINAL_COMMAND_TRANSMITCHAR | (transmitChar << TERMINAL_CHAR_SHIFT); /*Set data1 (t_transm_command) to the character to issue transmission*/
            status = SYSCALL(SYS5, TERMINT, term_id, 0); /*block the process until I/O completes*/
            transmittedChars++;
            setSTATUS(INTSON); /*enable interrupts*/
        } else {
            transmittedChars = -(status); /*return negative of device's status value in v0*/
            break;
        }
    }
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = transmittedChars; /*return transmitted character count in v0 if successful print*/
    SYSCALL(SYS4,(memaddr) &devSema4_support[semIndex], 0, 0); /*unlock terminal device*/
}

/**************************************************************************************************
 * @brief The method performs a READ operation from a specific terminal device
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
 *  
 * @param:
 *      1. virtualAddr - starting address of the first character in the string to be transmitted to printer
 *      2. len - length of string to be transmitted to printer
 *      3. support_struct - pointer to support struct of current uproc
 * 
 * @return: None
 * 
 * @ref
 * pandOS - section 4.7.5
 * princOfOperations - chapter 5.7 
 **************************************************************************************************/
void readTerminal(char *virtualAddr, support_t *support_struct){

    if (virtualAddr == NULL) {
        SYSCALL(SYS9, 0, 0, 0);
    }

    /*--------------Declare local variables---------------------*/
    int term_id; /*terminal id*/
    int semIndex; /*Index to the device semaphore array*/
    /*----------------------------------------------------------*/

    term_id = support_struct->sup_asid-1;
    semIndex = ((TERMINT - OFFSET) * DEVPERINT) + term_id;
    
    /*Calculate the offset for the terminal device row relative to disk*/
    unsigned int terminalOffset = (TERMINT - DISKINT) * (DEVICE_INSTANCES * DEVREGSIZE);

    /*Calculate the offset for the specific device instance within that row*/
    unsigned int instanceOffset = term_id * DEVREGSIZE;

    /*Get total offeset to add to starting address of device regs*/
    unsigned int totalOffset = terminalOffset + instanceOffset;

    /*Add the total offset to the base address of the device registers*/
    device_t *terminalDevice = (device_t *)(DEVICEREGSTART + totalOffset);

    SYSCALL(SYS3,(memaddr) &devSema4_support[semIndex], 0, 0);

    char currChar = ' '; /*build the char being read in*/
    int receivedChars; /*tracks how many characters were read in*/
    receivedChars = 0;
    int readStatus;

    /*iterate through string until we reach end of character*/
    while(((terminalDevice->d_status & TERMSTATUSMASK) == READY) && (currChar != EOS)) {
        setSTATUS(INTSOFF); /*disable interrupts*/
        terminalDevice->d_command = TRANSMITCHAR; /*issue transmit command*/
        readStatus = SYSCALL(WAITIO, TERMINT, term_id, TRUE); /*block current proc until IO completes*/
        setSTATUS(INTSON); /*enable interrupts*/
        if((readStatus & TERMSTATUSMASK) == OKCHARTRANS) { /*if terminal status is OK, */
            currChar = (readStatus >> DEVICE_INSTANCES);
            if(currChar != '\n') {
                *virtualAddr = currChar; /*copy character to currChar*/
                virtualAddr++; /*move pointer to next char*/
                receivedChars++; /*increment count*/
            }
        }
        else {
            currChar = '\n'; /*set currChar to exit loop if character trasmission was not successful*/
        }
    }

    /*unsuccessful transmission*/
    if((terminalDevice->d_status & TERMSTATUSMASK) != READY || (readStatus & TERMSTATUSMASK) != OKCHARTRANS) {
        receivedChars = -(readStatus); /*return negative of device's status value in v0*/
    }

    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = receivedChars; /*return received character count in v0 if successful*/
    SYSCALL(SYS4,(memaddr) &devSema4_support[semIndex], 0, 0); /*unlock terminal semaphore*/
    /*IMPLEMENTATION ENDS*/

}

/**************************************************************************************************
 * @brief This method implements Support Level Program Trap Handler, which is used to handle program trap 
 * exception at support level by terminating the faulty U's proc
 * 
 * @note
 * Make call to terminate the U's proc by passing its support_struct
 *
 * @param: suppStruct - pointer to current uproc's support structure
 * @return: None
 * 
 * @ref 
 * pandOS - section 4.8
 **************************************************************************************************/
void syslvl_prgmTrap_handler(support_t *suppStruct)
{
    get_nuked(suppStruct);
}


/**************************************************************************************************
 * @brief The method implements Support Level Syscall Exception Handler, a user process-level handler 
 * that resolves the exceptions caused by system calls in the user mode 
 * 
 * @details
 * 1. Check if the syscall number falls within the range 9-13
 *    - If invalid syscall number, handle as program trap
 * 2. Reads parameters in registers a1,a2,a3
 * 3. Manually imcrement PC+4 to avoid re-executing Syscall on return
 * 4. Execute appropriate syscall handler
 * 5. LDST to return to process that requested SYSCALL
 *
 * @param:
 *      1. currProc_support_struct - pointer to current uproc's support structure
 *      2. syscall_num_requested - requested syscall value (to select appropriate handler)
 * @return: None
 * 
 * @ref
 * pandOS - section 4.7
 **************************************************************************************************/
void syscall_excp_handler(support_t *currProc_support_struct,int syscall_num_requested){
    /*--------------Declare local variables---------------------*/
    char* virtualAddr;    /*value stored in a1 - here it's the ptr to first char to be written/read*/
    int length;     /*value stored in a2 - here it's the length of the string to be written/read*/
    /*----------------------------------------------------------*/

    /* Validate syscall number */
    if (syscall_num_requested < SYS9 || syscall_num_requested > SYS13) { /*Will have to change for future phases*/
        /* Invalid syscall number, treat as Program Trap */
        syslvl_prgmTrap_handler(currProc_support_struct);
        return;
    }

    /*Step 2: Read values in registers a1-a3*/
    virtualAddr = (char *) currProc_support_struct->sup_exceptState[GENERALEXCEPT].s_a1;
    length = currProc_support_struct->sup_exceptState[GENERALEXCEPT].s_a2;
    /*param3 = currProc_support_struct->sup_exceptState[GENERALEXCEPT].s_a3;*/ /*no need for value in a3 yet*/  

    /*Step 3: Increment PC+4 to execute next instruction on return*/
    currProc_support_struct->sup_exceptState[GENERALEXCEPT].s_pc += WORDLEN;

    /*Step 4: Execute appropriate syscall helper method based on requested syscall number*/
    switch(syscall_num_requested) {
        case SYS9:
            get_nuked(currProc_support_struct);
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
        syslvl_prgmTrap_handler(currProc_support_struct);
            break;
    }
    returnControlSup(currProc_support_struct, GENERALEXCEPT);
}

/**************************************************************************************************
 * @brief This method implements Support Level General Exception Handler
 * 
 * @details
 * 1. Obtain current process' support structure
 * 2. Examine Cause register in exceptState field of support structure and extract exception code
 * 3. Pass control to either the support level syscall handler or the program trap handler
 * 
 * @param: None
 * @return: None
 * 
 * @ref
 * pandOS - section 4.6
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
    syslvl_prgmTrap_handler(currProc_supp_struct); /*Otherwise, treat the exception as a program trap*/
}