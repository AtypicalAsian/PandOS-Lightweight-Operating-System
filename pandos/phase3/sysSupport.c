#include "/usr/include/umps3/umps/libumps.h"
#include "../h/initProc.h"
#include "../h/sysSupport.h"
#include "../h/vmSupport.h"


/* 2D Array of support level device semaphores */
int support_device_sems[DEVICE_TYPES][DEVICE_INSTANCES];



void returnControl()
{
    LDST(EXCSTATE);
}


void returnControlSup(support_t *support, int exc_code)
{
    LDST(&(support->sup_exceptState[exc_code]));
}


void trapExcHandler(support_t *support_struct)
{
    terminate(support_struct);
}


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
            getTOD(support_struct);
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

void terminate(support_t *support_struct)
{
    int dev_num = support_struct->sup_asid - 1;

    int i;
    for (i = 0; i < DEVICE_TYPES; i++) {
        if (support_device_sems[i][dev_num] == 0) {
            SYSCALL(VERHOGEN, (memaddr) &support_device_sems[i][dev_num], 0, 0);
        }
    }

    for (i = 0; i < MAXPAGES; i++) {
        if(support_struct->sup_privatePgTbl[i].entryLO & VALIDON){
            setSTATUS(INTSOFF);
            support_struct->sup_privatePgTbl[i].entryLO &= ~VALIDON;
            update_tlb_handler(&(support_struct->sup_privatePgTbl[i]));
            setSTATUS(INTSON);
        }
    }
    SYSCALL(VERHOGEN, (memaddr) &masterSema4, 0, 0);
    deallocate(support_struct);
    SYSCALL(TERMINATEPROCESS, 0, 0, 0);
}

void getTOD(support_t *support_struct)
{
    STCK(support_struct->sup_exceptState[GENERALEXCEPT].s_v0);
}

void writeToPrinter(char *virtualAddr, int len, support_t *support_struct) {
    int device_instance = support_struct->sup_asid - 1;
    int charCount = 0;

    SYSCALL(PASSEREN, (memaddr) &support_device_sems[PRINTSEM][device_instance], 0, 0);

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
    SYSCALL(VERHOGEN, (memaddr) &support_device_sems[PRINTSEM][device_instance], 0, 0);  

}

void writeToTerminal(char *virtualAddr, int len, support_t *support_struct) {
    int device_instance = support_struct->sup_asid - 1;
    unsigned int charCount = 0;
    
    SYSCALL(PASSEREN, (memaddr) &support_device_sems[TERMWRSEM][device_instance], 0, 0);

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

    SYSCALL(VERHOGEN, (memaddr) &support_device_sems[TERMWRSEM][device_instance], 0, 0);  

}

void readTerminal(char *virtualAddr, support_t *support_struct){
    int device_instance = support_struct->sup_asid - 1;
    int charCount = 0;
    int status;
    char string = ' ';

    SYSCALL(PASSEREN, (memaddr) &support_device_sems[TERMSEM][device_instance], 0, 0);

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
    SYSCALL(VERHOGEN, (memaddr) &support_device_sems[TERMSEM][device_instance], 0, 0);
    support_struct->sup_exceptState[GENERALEXCEPT].s_v0 = charCount;

}