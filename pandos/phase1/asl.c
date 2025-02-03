/**************************************************************************** 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

 This module manages the creation and release of semaphore descriptors  
 in two linked lists: the Active Semaphore List (ASL) and the semdFree list.  
 The ASL keeps track of semaphores that currently have at least one process  
 waiting in their associated queue, while the semdFree list stores available  
 semaphore descriptors that are not in use.  
 
 Both lists are implemented as NULL-terminated, singly linked lists.  
 Additionally, they function like a stack, where semaphores are added  
 and removed from the front of the list.  
****************************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/asl.h"
#include "../h/pcb.h"

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
 *  Initialize the semdFree list to contain all the elements of the array static semd_t semdTable[MAXPROC_SEM]
 *  Init ASL to have dummy head and tail nodes
 *  This method will be only called once during data structure initialization
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


    /* Init active semaphore list with dummy nodes */
    /* Init dummy nodes with smallest and largest memory address in 32-bit address to maintain sorted ASL */
    dummy_tail->s_next = NULL;
    dummy_tail->s_semAdd = (int*) LARGEST_ADDR; /*Largest possible address*/
    dummy_head->s_next = dummy_tail;
    dummy_head->s_semAdd = (int*) SMALLEST_ADDR; /*Smallest possible address*/

    /* Set head of active semaphore list (ASL) */
    semd_h = dummy_head;

}

/****************************************************************************  
 *  search_semp  
 *  Searches the ASL for the semaphore descriptor whose address directly  
 *  precedes where semAdd should belong.  
 *  
 *  params:  
 *      - int *semAdd: The memory address of the semaphore descriptor.  
 *  returns:  
 *      - Pointer to the preceding semaphore descriptor if found.  
 *      - If ASL is empty, returns NULL.  
 ****************************************************************************/  
 
semd_PTR search_semp(int *semAdd) {
    if (semd_h == NULL) return NULL;  /* ASL is empty */

    semd_PTR prev = NULL;
    semd_PTR curr = semd_h;  

    /* Traverse the ASL to find the correct position */
    while (curr != NULL && curr->s_semAdd < semAdd) {
        /* Stop at tail dummy node */
        if (curr->s_semAdd == (int*) LARGEST_ADDR) {
            return prev;
        }
        prev = curr;
        curr = curr->s_next;
    }

    return prev;  /* Return the node that precedes the target position */
}



/**************************************************************************** 
 *  insertBlocked
 *  Insert the pcb pointed to by p at the tail of the process queue associated 
 *  with the semaphore whose physical address is semAdd and set the 
 *  semaphore address of p to semAdd.
 *  If the semaphore is currently not active (i.e. there is no descriptor for it in the ASL), allocate
 *  a new descriptor from the semdFree list, insert it in the ASL (at the appropriate position), 
 *  initialize all of the fields
 * 
 *  params: pointer p to a pcb, memory address semAdd
 *  return: TRUE if a new semaphore descriptor needs to be allocated. Otherwise,
 *  FALSE in all other cases
 *****************************************************************************/
int insertBlocked(int *semAdd, pcb_PTR p) {
    if (p == NULL) return TRUE;  

    /* Find the position in the ASL */
    semd_PTR prev_ptr = search_semp(semAdd);
    semd_PTR curr_ptr;
    
    if (prev_ptr != NULL) {
        curr_ptr = prev_ptr->s_next;
    } else {
        curr_ptr = NULL;
    }

    /* If the semaphore exists, insert the process */
    if (curr_ptr != NULL && curr_ptr->s_semAdd == semAdd) {
        insertProcQ(&(curr_ptr->s_procQ), p);
        p->p_semAdd = semAdd;
        return FALSE;
    }

    /* If no free semaphores are available, return TRUE */
    if (semdFree_h == NULL) return TRUE;

    /* Allocate new semd from semdFree list */
    semd_PTR new_semd = semdFree_h;
    semdFree_h = semdFree_h->s_next;

    /* Initialize and insert the new semaphore descriptor */
    new_semd->s_semAdd = semAdd;
    new_semd->s_procQ = mkEmptyProcQ();
    insertProcQ(&(new_semd->s_procQ), p);
    p->p_semAdd = semAdd;

    /* Link new semd into the ASL */
    new_semd->s_next = curr_ptr;
    
    if (prev_ptr != NULL) {
        prev_ptr->s_next = new_semd;
    } else {
        semd_h = new_semd;  /* Update head if inserting at the start */
    }

    return FALSE;
}



/**************************************************************************** 
 *  removeBlocked
 *  Search the ASL for a descriptor of this semaphore. If none is found, return NULL; 
 *  otherwise, remove the first (i.e. head) pcb from the process queue of 
 *  the found semaphore descriptor, set that pcb’s address to NULL, and 
 *  return a pointer to it. If the process queue for this semaphore becomes 
 *  empty (emptyProcQ(s procq) is TRUE), remove the semaphore descriptor from 
 *  the ASL and return it to the semdFree list.
 * 
 *  params: memory address semAdd of a semaphore descriptor
 *  return: pointer to removed pcb. Otherwise, return NULL
 *****************************************************************************/
pcb_PTR removeBlocked(int *semAdd) {
    semd_PTR prev_ptr = search_semp(semAdd);
    semd_PTR curr_ptr;

    if (prev_ptr != NULL) {
        curr_ptr = prev_ptr->s_next;
    } else {
        curr_ptr = NULL;
    }

    /* If the semaphore is not found, return NULL */
    if (curr_ptr == NULL || curr_ptr->s_semAdd != semAdd) return NULL;

    /* Remove the first PCB from the queue */
    pcb_PTR removed_pcb = removeProcQ(&(curr_ptr->s_procQ));

    /* If no process was in the queue, return NULL */
    if (removed_pcb == NULL) return NULL;

    removed_pcb->p_semAdd = NULL;

    /* If queue is empty, remove the semaphore from ASL */
    if (emptyProcQ(curr_ptr->s_procQ)) {
        prev_ptr->s_next = curr_ptr->s_next;
        freeSemaphore(curr_ptr);
    }

    return removed_pcb;
}



/**************************************************************************** 
 *  outBlocked
 *  Remove the pcb pointed to by p from the process queue associated with p’s 
 *  semaphore (p→ p semAdd) on the ASL. If pcb pointed to by p does not appear in 
 *  the process queue associated with p’s semaphore, return NULL
 * 
 *  params: pointer p to a pcb
 *  return: pointer to the removed pcb. Otherwise, return NULL
 *****************************************************************************/
pcb_PTR outBlocked(pcb_PTR p) {
    if (p == NULL || p->p_semAdd == NULL) return NULL;

    int *semAdd = p->p_semAdd;

    semd_PTR prev_ptr = search_semp(semAdd);
    semd_PTR curr_ptr;

    if (prev_ptr != NULL) {
        curr_ptr = prev_ptr->s_next;
    } else {
        curr_ptr = NULL;
    }

    /* If the semaphore is not found, return NULL */
    if (curr_ptr == NULL || curr_ptr->s_semAdd != semAdd) return NULL;

    /* Remove the process from the semaphore's queue */
    pcb_PTR removed_pcb = outProcQ(&(curr_ptr->s_procQ), p);
    
    /* If process was not in the queue, return NULL */
    if (removed_pcb == NULL) return NULL;

    removed_pcb->p_semAdd = NULL;

    /* If the process queue becomes empty, remove semaphore from ASL */
    if (emptyProcQ(curr_ptr->s_procQ)) {
        prev_ptr->s_next = curr_ptr->s_next;
        freeSemaphore(curr_ptr);
    }

    return removed_pcb;
}


/**************************************************************************** 
 *  headBlocked
 *  Return a pointer to the pcb that is at the head of the process queue 
 *  associated with the semaphore semAdd. Return NULL if semAdd is 
 *  not found on the ASL or if the process queue associated with semAdd is empty.
 * 
 *  params: memory address semAdd of a semaphore descriptor
 *  return: pointer to pcb at the head of process queue associated with semaphore
 *          at semAdd. Otherwise, return NULL
 *****************************************************************************/
pcb_PTR headBlocked(int *semAdd) {
    if (semAdd == NULL) return NULL;

    semd_PTR curr_ptr = search_semp(semAdd);

    if (curr_ptr != NULL) {
        curr_ptr = curr_ptr->s_next;
    } else {
        curr_ptr = NULL;
    }

    /* If semaphore not found, return NULL */
    if (curr_ptr == NULL || curr_ptr->s_semAdd != semAdd) return NULL;

    /* Return the head of the process queue */
    return headProcQ(curr_ptr->s_procQ);
}


