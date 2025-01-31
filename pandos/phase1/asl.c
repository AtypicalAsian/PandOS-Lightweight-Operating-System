/**************************************************************************** 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

ACTIVE SEMAPHORE LIST IMPLEMENTATION
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


    /* Init active semaphore list with dummy nodes */
    /* Init dummy nodes with smallest and largest memory address in 32-bit address to maintain sorted ASL */
    dummy_tail->s_next = NULL;
    dummy_tail->s_semAdd = (int*) 0x0FFFFFFF; /*Largest possible address*/
    dummy_head->s_next = dummy_tail;
    dummy_head->s_semAdd = (int*) 0x00000000; /*Smallest possible address*/

    /* Set head of active semaphore list (ASL) */
    semd_h = dummy_head;

}


/**************************************************************************** 
 *  insertBlocked
 *  Insert the pcb pointed to by p at the tail of the process queue associated 
 *  with the semaphore whose physical address is semAdd and set the 
 *  semaphore address of p to semAdd.
 *  params: pointer p to a pcb, memory address semAdd
 *  return: TRUE if a new semaphore descriptor needs to be allocated. Otherwise,
 *  FALSE in all other cases
 *****************************************************************************/
int insertBlocked(int *semAdd, pcb_PTR p) {
    if (p == NULL) return TRUE;

    /* Find semAdd in active list */
    semd_PTR prev_ptr = semd_h;
    semd_PTR curr_ptr = semd_h->s_next;

    while ((curr_ptr != NULL) && (curr_ptr->s_semAdd < semAdd)) {
        prev_ptr = curr_ptr;
        curr_ptr = curr_ptr->s_next;
    }

    /* If semAdd is found, insert p into the corresponding procQ */
    if (curr_ptr != NULL && curr_ptr->s_semAdd == semAdd) {
        insertProcQ(&(curr_ptr->s_procQ), p);
        p->p_semAdd = semAdd;
        return FALSE;
    }

    /* Check if there are available free semaphore descriptors */
    if (semdFree_h == NULL) return TRUE;

    /* Allocate new semd from semdFree list */
    semd_PTR new_semd = semdFree_h;
    semdFree_h = semdFree_h->s_next;

    /* Initialize the new semd and insert p */
    new_semd->s_semAdd = semAdd;
    new_semd->s_procQ = mkEmptyProcQ();
    insertProcQ(&(new_semd->s_procQ), p);
    p->p_semAdd = semAdd;

    /* Link new semd into the active list */
    prev_ptr->s_next = new_semd;
    new_semd->s_next = curr_ptr;

    return FALSE; 
}


/**************************************************************************** 
 *  removeBlocked
 *  Remove the first (head) pcb from the process queue of the semaphore descriptor, set 
 * that pcb’s address to NULL
 *  params: memory address semAdd of a semaphore descriptor
 *  return: pointer to removed pcb. Otherwise, return NULL
 *****************************************************************************/
pcb_PTR removeBlocked(int *semAdd) {
    semd_PTR prev_ptr = semd_h;
    semd_PTR curr_ptr = semd_h->s_next;

    /* Traverse the ASL to find the semaphore */
    while ((curr_ptr != NULL) && (curr_ptr->s_semAdd < semAdd)) {
        prev_ptr = curr_ptr;
        curr_ptr = curr_ptr->s_next;
    }

    /* If semAdd is not found, return NULL */
    if (curr_ptr == NULL || curr_ptr->s_semAdd != semAdd) return NULL;

    /* Remove the first PCB from the process queue */
    pcb_PTR headProcQ = removeProcQ(&(curr_ptr->s_procQ));

    /* If no process was in the queue, return NULL */
    if (headProcQ == NULL) return NULL;

    headProcQ->p_semAdd = NULL;

    /* If the process queue becomes empty, remove semaphore from ASL */
    if (emptyProcQ(curr_ptr->s_procQ)) {  
        prev_ptr->s_next = curr_ptr->s_next; 
        freeSemaphore(curr_ptr); 
    }

    return headProcQ;
}


/**************************************************************************** 
 *  outBlocked
 *  Remove the pcb pointed to by p from the process queue associated with p’s 
 *  semaphore (p→ p semAdd) on the ASL.
 *  params: pointer p to a pcb
 *  return: pointer to the removed pcb. Otherwise, return NULL
 *****************************************************************************/
pcb_PTR outBlocked(pcb_PTR p) {
    /* Check if p is NULL */
    if (p == NULL || p->p_semAdd == NULL) return NULL;

    int *semAdd = p->p_semAdd;

    semd_PTR prev_ptr = semd_h;
    semd_PTR curr_ptr = semd_h->s_next;

    /* Traverse the ASL to find the semaphore */
    while ((curr_ptr != NULL) && (curr_ptr->s_semAdd < semAdd)) {
        prev_ptr = curr_ptr;
        curr_ptr = curr_ptr->s_next;
    }

    /* If semAdd is not found, return NULL */
    if (curr_ptr == NULL || curr_ptr->s_semAdd != semAdd) return NULL;

    /* Remove the process from the semaphore's queue */
    pcb_PTR removed_pcb = outProcQ(&(curr_ptr->s_procQ), p);
    
    /* If p was not found in the queue, return NULL */
    if (removed_pcb == NULL) return NULL;

    /* Clear the semaphore address in the PCB */
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
 *  params: memory address semAdd of a semaphore descriptor
 *  return: pointer to pcb at the head of process queue associated with semaphore
 *          at semAdd. Otherwise, return NULL
 *****************************************************************************/
pcb_PTR headBlocked(int *semAdd){
    if (semAdd == NULL) return NULL;

    semd_PTR curr = semd_h;             /*pointer to head of ASL*/
    while (curr != NULL && curr->s_semAdd < semAdd){
        curr = curr->s_next;
    }

    /*semaphore not found in current active semaphore list*/
    if (curr == NULL || curr->s_semAdd != semAdd){
        return NULL;
    }
    
    /*found semaphore (at semAdd), return head of process queue asscoiated with it*/
    else{
        return headProcQ(curr->s_procQ);
    }
}

