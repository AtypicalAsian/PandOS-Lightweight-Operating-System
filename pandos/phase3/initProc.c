/**************************************************************************************************  
 * @file initProc.c  
 *  
 * 
 * @brief  
 * This module implements test() and exports the Support Level’s global variables.
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
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
/*#include "/usr/include/umps3/umps/libumps.h"*/

/* GLOBAL VARIABLES DECLARATION */
int deviceSema4s[MAXSHAREIODEVS]; /*array of semaphores, each for a (potentially) shareable peripheral I/O device. These semaphores will be used for mutual exclusion*/
int masterSema4; /* A Support Level semaphore used to ensure that test() terminates gracefully by calling HALT() instead of PANIC() */