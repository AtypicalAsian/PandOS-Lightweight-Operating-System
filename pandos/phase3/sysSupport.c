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
    int device_instance = support_struct->sup_asid - 1;
    int charCount = 0;

    int semIndex = (PRINTSEM * DEVICE_INSTANCES) + device_instance;

    SYSCALL(PASSEREN, (memaddr) &support_device_sems[semIndex], 0, 0);

    device_t *device_int = (device_t *)(DEVICEREGSTART + ((PRNTINT - DISKINT) * (DEVICE_INSTANCES * DEVREGSIZE)) + (device_instance * DEVREGSIZE));
    
    int i;
    for (i = 0; i < len; i++) {
        if(device_int->d_status == READY) {

            setSTATUS(INTSOFF);
            device_int->d_data0 = ((int) *(virtualAddr + i));
            device_int->d_command = PRINTCHR;
            SYSCALL(WAITIO, PRNTINT, device_instance, 0);
            setSTATUS(INTSON);

            charCount++;
        }
        else {
            charCount = -(device_int->d_status);
            i = len;
        }
    }

    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = charCount;
    SYSCALL(VERHOGEN, (memaddr) &support_device_sems[semIndex], 0, 0);  

}

void writeToTerminal(char *virtualAddr, int len, support_t *support_struct) {
    int device_instance = support_struct->sup_asid - 1;
    unsigned int charCount = 0;

    int semIndex = (TERMWRSEM * DEVICE_INSTANCES) + device_instance;
    
    SYSCALL(PASSEREN, (memaddr) &support_device_sems[semIndex], 0, 0);

    device_t *device_int = (device_t *) (DEVICEREGSTART + ((TERMINT - DISKINT) * (DEVICE_INSTANCES * DEVREGSIZE)) + (device_instance * DEVREGSIZE));
    int status = OKCHARTRANS;


    int i;
    for (i = 0; i < len; i++) {
        if(((device_int->d_data0 & TERMSTATUSMASK) == READY) && ((status & TERMSTATUSMASK) == OKCHARTRANS)) {
            setSTATUS(INTSOFF);

            device_int->d_data1 = (((int) *(virtualAddr + i)) << TERMTRANSHIFT) | TRANSMITCHAR;
            status = SYSCALL(WAITIO, TERMINT, device_instance, 0);

            setSTATUS(INTSON);
            charCount++;
        }
        else{
            charCount = -(status);
            i = len;
        }
    }

    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = charCount;

    SYSCALL(VERHOGEN, (memaddr) &support_device_sems[semIndex], 0, 0);  

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