/**************************************************************************** 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

ACTIVE SEMAPHORE LIST IMPLEMENTATION
****************************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/asl.h"
#include "pcb.h"

HIDDEN semd_PTR semd_h;             /*ptr to head of active semaphore list (ASL)*/
HIDDEN semd_PTR semdFree_h;         /*ptr to head of free semaphore list*/


/**************************************************************************** 
 *  freeSemaphore
 *  Add a semaphore to the head of the free semaphore list
 *  params: ptr to a sempahore descriptor struct
 *  return: none 
 *****************************************************************************/
void freeSemaphore(semd_PTR sempahore){
    sempahore->s_next = semdFree_h;
    semdFree_h = sempahore;
}

/**************************************************************************** 
 *  initASL
 *  Initialize the semdFree list to contain all the elements of the array static semd_t semdTable[MAXPROC+2]
 *  Init ASL to have dummy head and tail nodes
 *  params: None
 *  return: none 
 *****************************************************************************/
void initASL(){
    static semd_t semdTable[MAXPROC_SEM];


    /************ Init Free Semaphore List ************/
    semdFree_h = NULL;

    int i;
    for (i=0;i<MAXPROC_SEM;i++){
        freeSemaphore(&semdTable[i]);
    }

    /************ Init Active Semaphore List ************/
    semd_PTR dummy_head = NULL;
    semd_PTR dummy_tail = NULL;

    /*remove first node from free semaphore list to make as dummy head node*/
    dummy_head = semdFree_h;
    semdFree_h = dummy_head->s_next;

    /*remove second node from free semaphore list to make dummy tail node*/
    dummy_tail = semdFree_h;
    semdFree_h = dummy_tail->s_next;


    /*Init active semaphore list with dummy nodes*/
    /*Init dummy nodes with smallest and largest memory address in 32-bit address to maintain sorted ASL*/
    dummy_tail->s_next = NULL;
    dummy_tail->s_semAdd = (int*) 0x0FFFFFFF; // Largest possible address
    dummy_head->s_next = dummy_tail;
    dummy_head->s_semAdd = (int*) 0x00000000; // Smallest possible address

    // Set head of active semaphore list (ASL)
    semd_h = dummy_head;
}

int insertBlocked(int *semAdd, pcb_PTR p){
    // find semAdd in active list first
    semd_PTR prev_ptr = semd_h;
    semd_PTR curr_ptr = semd_h->s_next;

    while ((curr_ptr != NULL) && (curr_ptr->s_semAdd < semAdd)) {
        prev_ptr = curr_ptr;
        curr_ptr = curr_ptr->s_next;
    }

    // if semAdd found, insert p into the corresponding procQ 
    if (curr_ptr->s_semAdd == semAdd) {
        insertProcQ(&(curr_ptr->s_procQ),p);
        p->p_semAdd = semAdd;
        return FALSE;
    }

    // check semdFree list
    if (semdFree_h == NULL) return TRUE;

    // create new sem_ptr from free list & insert p
    semd_PTR new_semd = semdFree_h;
    semdFree_h = semdFree_h->s_next;

    new_semd->s_semAdd = semAdd;
    new_semd->s_procQ = mkEmptyProcQ();
    insertProcQ(&(new_semd->s_procQ), p);
    p->p_semAdd = semAdd;
    
    prev_ptr->s_next = new_semd;
    new_semd->s_next = curr_ptr;
    return FALSE; 
}

pcb_PTR removeBlocked(int *semAdd){
    semd_PTR prev_ptr = semd_h;
    semd_PTR curr_ptr = semd_h->s_next;

    // traverse the ASL to find the semaphore
    while ((curr_ptr != NULL) && (curr_ptr->s_semAdd < semAdd)) {
        prev_ptr = curr_ptr;
        curr_ptr = curr_ptr->s_next;
    }

    if (curr_ptr == NULL || curr_ptr->s_semAdd != semAdd) return NULL;

     // remove the first PCB from the process queue
    pcb_PTR headProcQ = removeProcQ(&(curr_ptr->s_procQ));
    headProcQ->p_semAdd = NULL;

    // if the process queue becomes empty, remove semaphore from ASL
    if (emptyProcQ(headProcQ)) {
        prev_ptr->s_next = curr_ptr->s_next;
        curr_ptr->s_next = semdFree_h;
        semdFree_h = curr_ptr;
    }

    return headProcQ;
}

pcb_PTR outBlocked(pcb_PTR p){
    return p;
}


pcb_PTR headBlocked(int *semAdd){
    return semAdd;
}

