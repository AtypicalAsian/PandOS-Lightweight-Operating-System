/**************************************************************************** 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

PROCESS CONTROL BLOCK QUEUE/TREE IMPLEMENTATION
****************************************************************************/

#include "../h/pcb.h"
#include "../h/const.h"
#include "../h/types.h"

HIDDEN pcb_PTR pcbFree_h;

/**************************************************************************** 
 *  initPcbs
 *  Initialize the pcbFree as a non-circular linked list that contains all elements of the static array (pool) of MAXPROC pcbs
 *    --> pcbFree act as a pool of free (unused) pcbs available for allocation when a new process is created
 *  params: None
 *  return: none 
 *****************************************************************************/
void initPcbs() {
	static pcb_t pcb_pool[MAXPROC];         /*Pool of PCBs from which processes can be allocated*/
	pcbFree_h = NULL;                       /*List is initially empty*/
    int i;
	for(i = 0; i < MAXPROC; i++){
		/*add each pcb to pcbFree list*/
		freePcb(&(pcb_pool[i]));
	}
}


/**************************************************************************** 
 *  freePcb
 *  Add the pcb pointed to by pointer p to the pcbFree list. 
 *  params: pointer to a pcb struct
 *  return: none 
 *****************************************************************************/
void freePcb(pcb_PTR p){
    if (pcbFree_h == NULL){                /*If pcbFree list is empty*/
		(*p).p_prev = NULL;
        (*p).p_next = NULL;
		pcbFree_h = p;
	}
	else{                                 /*Otherwise, if there are items in pcbFree list*/
        (*p).p_prev = NULL;
		(*p).p_next = pcbFree_h;
		(*pcbFree_h).p_prev = p;
		pcbFree_h = p;
	}
}

/**************************************************************************** 
 *  allocPcb
 *  Remove an element from pcbFree linked list and intialize values for all pcb struct fields
 *  params: none
 *  return: pointer to the pcb removed from the pcbFree linked list 
 *****************************************************************************/

pcb_PTR allocPcb(){
   if (pcbFree_h == NULL) return NULL;
   pcb_PTR freed_pcb_ptr = pcbFree_h;           /*remove from head of linked list*/

   /*if pcbFree only has one free pcb left*/
   if ((*freed_pcb_ptr).p_next == NULL) pcbFree_h = NULL;

   else{
    /*detach next and prev pointers + update new head of freePcb linked list*/
    ((*freed_pcb_ptr).p_next)->p_prev = NULL;
    (*freed_pcb_ptr).p_prev = NULL;
    pcbFree_h = (*freed_pcb_ptr).p_next;
   }

   /*intialize values for all pcb struct fields of newly freed pcb*/
   
   /*Process queue fields*/
   (*freed_pcb_ptr).p_prev = NULL;
   (*freed_pcb_ptr).p_next = NULL;

   /*Process tree fields*/
   (*freed_pcb_ptr).p_child = NULL;
   (*freed_pcb_ptr).p_sib = NULL;
   (*freed_pcb_ptr).p_prnt = NULL;

   /*Process status info*/
   int i;
   for (i = 0; i < STATEREGNUM; i++){
		freed_pcb_ptr->p_s.s_reg[i] = 0;
	}
   (*freed_pcb_ptr).p_time = 0;
   (*freed_pcb_ptr).p_semAdd = NULL;

   /*Support layer info*/
   /*p_supportStruct = NULL; ???? HAVE NOT DECLARED THIS AND UNSURE ABOUT INIT VALUE */

    return freed_pcb_ptr;
}



/**************************************************************************** 
                            PROCESS QUEUES
    double, circularly linked list with a tail pointer instead of a 
    head pointer. Used to organize PCBs based on their state or scheduling requirements.

****************************************************************************/


/**************************************************************************** 
 *  mkEmptyProcQ
 *  Initialize empty process queue with null pointer
 *  params: none
 *  return: tail pointer for new process queue
 *****************************************************************************/
pcb_PTR mkEmptyProcQ(){
    return NULL;
}

/**************************************************************************** 
 *  emptyProcQ
 *  check if process queue is currently empty
 *  params: tail pointer (tp) of process queue
 *  return: True if process queue is empty. False otherwise
 *****************************************************************************/
int emptyProcQ (pcb_PTR tp){
    return tp == NULL;
}


/**************************************************************************** 
 *  insertProcQ
 *  Insert the pcb pointed to by p into the process queue
 *  params: pointer to tail pointer of process queue, pcb pointer p
 *  return: none
 *****************************************************************************/
void insertProcQ (pcb_PTR *tp, pcb_PTR p){
    /*If process queue empty --> new pcb becomes single node in circular DLL*/
    if (emptyProcQ(*tp)){
        *tp = p;            /*p is new tail pointer*/
        p->p_next = p;      /*update next and prev to point to itself*/
        p->p_prev = p;
    }

    else{
        /*p becomes new tail, update current tail and associated next, prev pointers*/
        pcb_PTR currHead = (*tp)->p_next;       /*current head of circular DLL*/
        p->p_next = currHead;
        p->p_prev = *tp;
        currHead->p_prev = p;
        (*tp)->p_next = p;
        *tp = p;
    }
}



/**************************************************************************** 
 *  removeProcQ
 *  remove the head from the circular DLL (process queue)
 *  params: pointer to tail pointer of process queue
 *  return: pointer to removed element. Otherwise, NULL if list is empty
 *****************************************************************************/
pcb_PTR removeProcQ (pcb_PTR *tp){
    if (emptyProcQ(*tp)) return NULL;
    else{
        pcb_PTR removedHead = (*tp)->p_next;
        /*If queue only has 1 pcb*/
        if(removedHead == *tp) *tp = NULL;
        else{
            (*tp)->p_next = removedHead->p_next;
            removedHead->p_next->p_prev = *tp;
        }
        removedHead->p_next = NULL;
        removedHead->p_prev = NULL;
        return removedHead;
    }
}


/**************************************************************************** 
 *  outProcQ
 *  params: pointer to tail pointer of process queue, pcb pointer
 *  return: pointer to pcb p after it's removed from the queue. If not found, return NULL
 *****************************************************************************/
pcb_PTR outProcQ (pcb_PTR *tp, pcb_PTR p){
    if (emptyProcQ(*tp)) return NULL;

    pcb_PTR curr = (*tp)->p_next;           /*start at head*/
    pcb_PTR prev = *tp;                     /*trailing pointer starting from tail*/


    /*Loop through the list with curr,prev pointers until we find p, or until we circle back to the head (complete one loop)*/
    do {
        if (curr == p){
            /*Disconnect node from queue*/
            prev->p_next = curr->p_next;
            curr->p_next->p_prev = prev;

            /*if curr is the tail*/
            if (curr == *tp){
                if (curr == prev) {*tp = NULL;}     /*if only one node in process queue*/
                else {*tp = prev;}                  /*update tail to previous pcb node*/
            }

            curr->p_next = NULL;
            curr->p_prev = NULL;
            return curr;
        }
        prev = curr;
        curr = curr->p_next;
    } while (curr != (*tp)->p_next);
    return NULL;
}


/**************************************************************************** 
 *  headProcQ
 *  Return the head of the circular DLL process queue
 *  params: pointer to tail of process queue
 *  return: pointer to first pcb in process queue. Otherwise, NULL if queue is empty
 *****************************************************************************/
pcb_PTR headProcQ (pcb_PTR tp){
    if (emptyProcQ(tp)) return NULL;
    return tp->p_next;
}



/**************************************************************************** 
                            PROCESS TREES
        Represent pcbs organized as trees

****************************************************************************/


/**************************************************************************** 
 *  emptyChild
 *  params: pointer to a pcb in the tree
 *  return: TRUE if the pcb pointed to by p has no children. FALSE otherwise
 *****************************************************************************/
int emptyChild (pcb_PTR p){
    if (p->p_child == NULL) return 1;
    else return 0;
}

/**************************************************************************** 
 *  insertChild
 *  params: pointer to a child pcb and a parent pcb
 *  return: none. Make pcb pointed to by p a child of pcb pointed to by prnt
 *****************************************************************************/
void insertChild (pcb_PTR prnt, pcb_PTR p){
    /*Curr parent node has no children*/
    if (emptyChild(prnt)){
        prnt->p_child = p;                              /*Make p first node in childrent linked list of prnt*/
        p->p_rsib = NULL;
        p->p_lsib = NULL;
        p->p_prnt = prnt;
        /*p->p_sib = NULL;*/  /*If singly linked list*/
    }
    /*Curr parent already has children linked list populated*/
    else{
        /* Add new child to head of DLL */
        pcb_PTR currChildHead = prnt->p_child;
        prnt->p_child = p;
        p->p_lsib = NULL;
        p->p_rsib = currChildHead;
        p->p_prnt = prnt;
        currChildHead->p_lsib = p;
    }
    return;
}

/**************************************************************************** 
 *  removeChild
 *  params: pointer to a pcb in the tree
 *  return: remove first child of pcb pointed to by p and return pointer to removed pcb
 *****************************************************************************/
pcb_PTR removeChild (pcb_PTR p){
    /*If curr pcb has no children*/
    if (emptyChild(p)) return NULL;

    /*If only one child in children list */
    else if ((p->p_child)->p_rsib == NULL){
        pcb_PTR curr_child = p->p_child;
        curr_child->p_lsib = NULL;
        curr_child->p_rsib = NULL;
        curr_child->p_prnt = NULL;
        p->p_child = NULL;
        return curr_child;
    }

    /*If >1 child in children list */
    else{
        pcb_PTR curr_child = p->p_child;
        pcb_PTR curr_child_sibling = curr_child->p_rsib;
        p->p_child = curr_child_sibling;
        curr_child_sibling->p_lsib = NULL;
        curr_child->p_rsib = NULL;
        curr_child->p_prnt = NULL;
        return curr_child;
    }
}


/**************************************************************************** 
 *  outChild
 *  params: pointer to a pcb in the tree
 *  return: disconnect pcb pointed to by p from its parent and return the removed pcb
 *****************************************************************************/
pcb_PTR outChild (pcb_PTR p){
    if (p->p_prnt == NULL) return NULL;
    pcb_t *currentHead;
    currentHead = (p->p_prnt)->p_child;
    return p;
}