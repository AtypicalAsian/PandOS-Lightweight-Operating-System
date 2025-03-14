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

/*SYSCALL 9-12 function declarations*/
HIDDEN void terminate();    /*SYS9 - terminates the executing user process. Essentially a user-mode wrapper for SYS2 (terminate running process)*/
HIDDEN void get_TOD();      /*SYS10 - retrieve the the number of microseconds since the system was last booted/reset to be placed*/
HIDDEN void write_to_printer(); /*SYS11 - suspend requesting user proc until a line of output (string of characters) has been transmitted to the printer device associated with that U-proc*/
HIDDEN void write_to_terminal(); /*SYS12 - suspend requesting user proc until a line of output (string of characters) has been transmitted to the terminal device associated with that U-proc*/
HIDDEN void read_from_terminal(); /*SYS13*/