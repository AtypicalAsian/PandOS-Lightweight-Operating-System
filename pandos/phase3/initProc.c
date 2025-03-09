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

/*HELPER METHODS*/
HIDDEN void init_userproc_processorState();

/*Initialize base processor state for user-process (define a function for this)*/
void init_userproc_processorState(){

}


/**************************************************************************************************
 * TO-DO  
 * Implement test() function
 *      1. Initialize I/O device semaphores + master semaphore
 *      2. Initialize Virtual Memory (swap pool table)
 *      3. Initialize base processor state for all user processes
 *      4. Create and launch max number of processes
 *      5. Optimization (perform P op on master sema4 MAXUPROCESS times)
 *      6. Terminate test()
 **************************************************************************************************/
void test(){
    /*Declare local variables*/
    int process_id; /*unique process id (asid) associated with each user process that's created (instantiated)*/
    static support_t supp_struct_array[MAXUPROCESS+1]; /*array of support structures*/

    /*Initialize I/O device semaphores to 1*/
    int i;
    for (i=0; i<MAXSHAREIODEVS; i++){
        deviceSema4s[i] = 1; /*DEFINE CONSTANT FOR 1*/
    }

    /*Initialize master semaphore to 0*/
    masterSema4 = 0; /*DEFINE CONSTANT FOR 0*/

    for (process_id=1; process_id < MAXUPROCESS + 1; process_id++){
        /*create and launch MAXUPROCESS user processes*/
        supp_struct_array[process_id].sup_asid = process_id;  /*set up user process' unique identifier ASID*/
        return;
    }

}