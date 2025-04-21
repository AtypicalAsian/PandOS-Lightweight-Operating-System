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
#include "../h/deviceSupportDMA.h"
// #include "/usr/include/umps3/umps/libumps.h"

/*Support level device semaphores*/
int devSema4_support[DEVICE_TYPES * DEV_UNITS]; 


/************************************************************************************************** 
 * @brief get_nuked() (or SYS9) is a essentially a wrapper for the kernel-mode restricted SYS2 service
 * 
 * @details
 * 1. Calculate the device index based on the uproc's proccess_id (ASID)
 * 2. Release all device semaphores the uproc is holding
 * 3. Invalidate all frames in the page table of the current uproc
 * 4. Decrement the master semaphore & de-allocate support_struct of U's proc (return back to free pool of suppStructs)
 * 5. Make SYSCALL 2 to terminate uproc and its children processes
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
        int index = i * DEV_UNITS + dev_num;
        if (devSema4_support[index] == 0){
            SYSCALL(SYS4, (memaddr)&devSema4_support[index],0,0);
        }
    }

    /*When u-proc terminates, mark all frames it occupy as free (no longer in use)*/
    for (i = 0; i < MAXPAGES; i++) {
        if(support_struct->sup_privatePgTbl[i].entryLO & VALIDON){
            setSTATUS(NO_INTS);
            support_struct->sup_privatePgTbl[i].entryLO &= ~VALIDON; /*invalidate the page*/
            update_tlb_handler(&(support_struct->sup_privatePgTbl[i])); /*update TLB to maintain consistency with page tables*/
            setSTATUS(YES_INTS);
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
 * 2. Find semaphore index corresponding with printer device
 * 3. Perform SYS3 to gain mutex over printer device
 * 4. Use For Loop to iterating through each character in the range [virtAddr, virtAddr + len], and write one by one to printer: 
 *  5.1 Given that the printer device is ready:
        - Retrieve current processor status
        - Disable interrupts
        - Pass character to printer device with d_data0 & d_command
        - Writing to d_command field will issue the I/O to print the passed character
        - Enable the interrupt again
        - Increment transmitted character count
    5.2 Given that the printer device is busy:
        - Return the negative of device status in v0
        - End transmission process
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
void write_to_printer(char *virtualAddr, int len, support_t *support_struct) {
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

    SYSCALL(SYS3, (memaddr)&devSema4_support[semIndex], 0, 0); /*Lock the printer device*/

    devregarea_t *busRegArea = (devregarea_t *)RAMBASEADDR; /* Pointer to the bus register area */
    device_t *printerDev = &(busRegArea->devreg[semIndex]); /*Pointer to printer device register*/

    int i;
    for (i = 0; i < len; i++) { /*Iterate over each character in the string to be printed*/
        if(printerDev->d_status == READY) {
            setSTATUS(NO_INTS); /*disable interrupts*/
            printerDev->d_data0 = ((int) *(virtualAddr + i)); /*Set data0 to the char to be transmitted to printer*/
            printerDev->d_command = PRINTCHR; /*Set command field to 2 to transmit the character in data0 to printer*/
            SYSCALL(SYS5, PRNTINT, pid, 0); /*block the process until I/O completes*/
            setSTATUS(YES_INTS); /*enable interrupts*/
            char_printed_count++; /*increment transmitted character count*/
        }
        else { /*If printer device is BUSY*/
            char_printed_count = -(printerDev->d_status); /*return negative of device's status value in v0*/
            i = len; /*force exit the loop*/
        }
    }
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = char_printed_count; /*return transmitted character count in v0 if successful print*/
    SYSCALL(SYS4, (memaddr) &devSema4_support[semIndex], 0, 0); /*unlock printer device*/
}


/**************************************************************************************************
 * @brief The method performs a WRITE operation to terminal device
 * 
 * @details
*  1. Computes the terminal's semaphore index and adjusts it for write operations (transmitters are behind receivers)
 *  2. Iterates over the characters from virtualAddr to virtualAddr+len.
 *  3. Disables interrupts, sets the device command with the current to-be-written character,
 *     and issues an I/O syscall to transmit it.
 *  4. Checks the device status and, if an error occurs, return negative of device's status value in v0
 *  5. Saves the total number of characters transmitted (or -status) in v0.
 *  6. Releases the terminal device semaphore
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
void write_to_terminal(char *virtualAddr, int len, support_t *support_struct) {
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
    unsigned int terminalOffset = (TERMINT - DISKINT) * (DEV_UNITS * DEVREGSIZE);

    /*Calculate the offset for the specific device unit within that row*/
    unsigned int instanceOffset = term_id * DEVREGSIZE;

    /*Sum row + instance offset*/
    unsigned int totalOffset = terminalOffset + instanceOffset;

    /*Add the total offset to the base address of the device registers*/
    device_t *terminalDevice = (device_t *)(DEVICEREGSTART + totalOffset);

    SYSCALL(SYS3,(memaddr) &devSema4_support[semIndex], 0, 0); /*Lock terminal device*/

    /*Iterate through each character in the string*/
    int i;
    for (i = 0; i < len; i ++) {
        memaddr transmitterStatus = (terminalDevice->d_data0 & TERMINAL_STATUS_MASK);
        if (transmitterStatus == TERMINAL_STATUS_READY) {
            setSTATUS(NO_INTS); /*disable interrupts*/
            char transmitChar = *(virtualAddr + i); 
            terminalDevice->t_transm_command = TERMINAL_COMMAND_TRANSMITCHAR | (transmitChar << TERMINAL_CHAR_SHIFT); /*Set data1 (t_transm_command) to the character to issue transmission*/
            status = SYSCALL(SYS5, TERMINT, term_id, 0); /*block the process until I/O completes*/
            transmittedChars++;
            setSTATUS(YES_INTS); /*enable interrupts*/
        } else {
            transmittedChars = -(status); /*return negative of device's status value in v0*/
            i = len;
        }
    }
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = transmittedChars; /*return transmitted character count in v0 if successful print*/
    SYSCALL(SYS4,(memaddr) &devSema4_support[semIndex], 0, 0); /*unlock terminal device*/
}

/**************************************************************************************************
 * @brief The method performs a READ operation from a specific terminal device
 *
 * @details
 * This function reads characters from a terminal device until either an end-of-string is 
 * encountered or the device is no longer ready. It:
 *   1. Determine the semaphore index for the terminal device (receiver)
 *   2. Calculate pointer to device_t that corresponds to the appropriate receiver terminal device (for the uproc asid)
 *   3. Lock the device semaphore
 *   4. Loop to fetch each character until the end-of-string is reached or the device status is not ready.
 *      5. Disable interrupts before each character is stored in the buffer.
 *      6. Issue transmit command + block uproc until IO finishes
 *      7. Retrieve the character via a bit-shifted value from the terminal's status; if unsuccessful,
 *      force exit loop and return negative of device's status value in v0.
 *      8. If successful, increment virtualAddr to read next character and increment transmitted character count
 *   9. Save the total number of characters received (or an error status) in v0.
 *   10. Release the device semaphore
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
void read_from_terminal(char *virtualAddr, support_t *support_struct){

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
    unsigned int terminalOffset = (TERMINT - DISKINT) * (DEV_UNITS * DEVREGSIZE);

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

    int continueReading = 1;
    
    /* Keep reading while the device is ready, no EOS seen, and no error flagged */
    while (continueReading && ((terminalDevice->d_status & TERMSTATUSMASK) == READY) && currChar != EOS) {
        setSTATUS(NO_INTS); /*disable interrupts*/
        terminalDevice->d_command = TRANSMITCHAR; /*write to d_command to issue read from terminal command*/
        readStatus = SYSCALL(SYS5, TERMINT, term_id, TRUE); /*block current process until IO finishes*/
        setSTATUS(YES_INTS); /*enable interrupts*/

        /*if character transfer is successful*/
        if ((readStatus & TERMSTATUSMASK) == OKCHARTRANS) {
            currChar = (readStatus >> DEV_UNITS);  /* Extract the received character */

            if (currChar != '\n') {
                *virtualAddr++ = currChar; /*store currChar into buffer and advance the pointer*/
                receivedChars++; /*increment received character count*/
            } else { /*encounter newline character -> stop reading*/
                continueReading = 0; /*force exit loop*/
            }
        } else { /*terminal status not ready -> stop reading*/
            continueReading = 0; /*force exit loop*/
        }
    }

    /*unsuccessful transmission*/
    if((terminalDevice->d_status & TERMSTATUSMASK) != READY || (readStatus & TERMSTATUSMASK) != OKCHARTRANS) {
        support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = -(readStatus); /*return negative of device's status value in v0*/
    }
    else{
        support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = receivedChars; /*return received character count in v0 if successful*/
    }
    SYSCALL(SYS4,(memaddr) &devSema4_support[semIndex], 0, 0); /*unlock terminal semaphore*/
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
    int a1_val; /*value stored in a1*/
    int a2_val; /*value stored in a2*/
    int a3_val; /*value stored in a3*/
    // char* virtualAddr;    /*value stored in a1 - here it's the ptr to first char to be written/read*/
    // int length;     /*value stored in a2 - here it's the length of the string to be written/read*/
    /*----------------------------------------------------------*/

    /* Validate syscall number */
    if (syscall_num_requested < SYS9 || syscall_num_requested > SYS18) { /*Will have to change for future phases*/
        /* Invalid syscall number, treat as Program Trap */
        syslvl_prgmTrap_handler(currProc_support_struct);
        return;
    }

    /*Step 2: Read values in registers a1-a3*/
    a1_val = currProc_support_struct->sup_exceptState[GENERALEXCEPT].s_a1;
    a2_val = currProc_support_struct->sup_exceptState[GENERALEXCEPT].s_a2;
    a3_val = currProc_support_struct->sup_exceptState[GENERALEXCEPT].s_a3;

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
            write_to_printer((char *) a1_val, a2_val, currProc_support_struct);
            break;

        case SYS12:
            write_to_terminal((char *) a1_val, a2_val, currProc_support_struct);  
            break;

        case SYS13:
            read_from_terminal((char *) a1_val, currProc_support_struct);
            break;
        
        case SYS14:
            disk_put(a1_val,a2_val,a3_val,currProc_support_struct);
            break;
        
        case SYS15:
            disk_get(a1_val,a2_val,a3_val,currProc_support_struct);
            break;
        
        case SYS16:
            flash_put(a1_val,a2_val,a3_val,currProc_support_struct);
            break;
        
        case SYS17:
            flash_get(a1_val,a2_val,a3_val,currProc_support_struct);
            break;

        default:
        syslvl_prgmTrap_handler(currProc_support_struct);
            break;
    }
    LDST(&(currProc_support_struct->sup_exceptState[GENERALEXCEPT]));
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