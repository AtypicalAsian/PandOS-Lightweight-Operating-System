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
/*#include "/usr/include/umps3/umps/libumps.h"*/

extern int devRegSem[49];
extern pcb_PTR currentProcess;


void supexHandler(){
    support_t* except_supp = (support_t*) SYSCALL(SYS8, 0, 0, 0);
    state_t* exc_state = (state_t*) &(except_supp->sup_exceptState[GENERALEXCEPT]);
    if(CAUSE_GET_EXCCODE(exc_state->s_cause) == 8){
        sysHandler(except_supp,  exc_state, exc_state->s_a0);
    }
    else{
        killProc(NULL);
    }
}

void sysHandler(support_t* except_supp, state_t* exc_state, unsigned int sysNum){
    char* toRead;
    int index;
    int response;
    char recvChar;
    switch(sysNum){
        case SYS9:
            terminate();
            break;
        case SYS10:
            get_tod(exc_state);
            break;
        case SYS11:
            write_printer(except_supp, exc_state);
            break;
        case SYS12:
            write_terminal(except_supp, exc_state);
            break;
        case SYS13:
            read_terminal(except_supp, exc_state);
            break;
        default:
            killProc(NULL);
            break;
    }
    LDST(exc_state);
}


void terminate(){
    killProc(NULL);
}


void get_tod(state_t* exc_state){
    cpu_t tod;
    STCK(tod);
    exc_state->s_v0 = tod;
}


void write_printer(support_t* except_supp, state_t* exc_state){
    int asid = except_supp->sup_asid - 1;
    char* toPrint = (char*) exc_state->s_a1;
    int len = exc_state->s_a2;
    if((int)toPrint < KUSEG || len < 0 || len > 128)
        killProc(NULL);
    else{
        dtpreg_t* currDev = (dtpreg_t*) DEV_REG_ADDR(PRNTINT, asid);
        SYSCALL(SYS3, (memaddr) &devRegSem[DEV_INDEX(PRNTINT, asid, FALSE)], 0, 0);
        int index = 0;
        int response = 1;
        while(index < len){
            IEDISABLE;
            currDev->data0 = *toPrint;
            currDev->command = 2;
            response = SYSCALL(SYS5, PRNTINT, asid, FALSE);
            IEENABLE;
            if((response & 0x000000FF) == READY){
                index++;
                toPrint++;  
            }
            else{
                index = -(response & 0x000000FF);
                break;
            }
        }
        SYSCALL(SYS4, (memaddr) &devRegSem[DEV_INDEX(PRNTINT, asid, FALSE)], 0, 0);
        exc_state->s_v0 = index;
    }
} 

void write_terminal(support_t* except_supp, state_t* exc_state){
    int asid = except_supp->sup_asid - 1;
    char* toWrite = (char*) exc_state->s_a1;
    int len = exc_state->s_a2;
    if(len <= 0 || (int)toWrite < KUSEG || len >= 128){
        killProc(NULL);
    }
    else{
        devreg_t* currDev = (devreg_t*) DEV_REG_ADDR(TERMINT, asid);
        SYSCALL(SYS3, (memaddr) &devRegSem[DEV_INDEX(TERMINT, asid, FALSE)], 0, 0);
        int index = 0;
        int response = 1;
        while(index < len){
            currDev->term.transm_command =  (*toWrite<<8) | 2;
            response = SYSCALL(SYS5, 7, asid, FALSE);
            if((response & TRANSM_MASK) == 5){
                index++;
                toWrite++;
            }
            else{
                index = -(response & TRANSM_MASK);
                break;
            }
        }
        SYSCALL(SYS4, (memaddr) &devRegSem[DEV_INDEX(TERMINT, asid, FALSE)], 0, 0);
        exc_state->s_v0 = index;
    }
}

void read_terminal(support_t* except_supp, state_t* exc_state){
    int asid = except_supp->sup_asid - 1;
    char* toRead = (char*) exc_state->s_a1;
    int index = 0, response;
    if((int) toRead < KUSEG)
        killProc(NULL);
    else{
        devreg_t* currDev = (devreg_t*) DEV_REG_ADDR(TERMINT, asid);
        SYSCALL(SYS3, (memaddr) &devRegSem[DEV_INDEX(TERMINT, asid, TRUE)], 0, 0);
        while(TRUE){
            currDev->term.recv_command = 2;
            response = SYSCALL(SYS5, TERMINT, asid, TRUE);
            if((response & RECV_MASK) == 5){
                *toRead = (response & 0x0000FF00) >> 8;
                toRead++;
                index++;
                if(((response & 0x0000FF00) >> 8) == '\n'){
                    break;
                }
            } else {
                index = -(response & RECV_MASK);
                break;
            }
        }
        SYSCALL(SYS4, (memaddr) &devRegSem[DEV_INDEX(TERMINT, asid, TRUE)], 0, 0);
        exc_state->s_v0 = index;
    }
}
