/**************************************************************************** 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

****************************************************************************/

#include "../h/pcb.h"
#include "../h/const.h"
#include "../h/types.h"

static pcb_PTR pcbFree_h; /* ptr for head of PCB Free List */

//================================================
//initPcbs
//Initialize the pcbFree as a linked list that contains all elements of the static array (pool) of MAXPROC pcbs
//PARAMETERS: None
//RETURN: none 
//================================================
void initPcbs() {
	static pcb_t pcb_pool[MAXPROC];         //Pool of PCBs from which processes can be allocated
	pcbFree_h = NULL;                       //List is initially empty
	for(int i = 0; i < MAXPROC; i++){
		//add each pcb to pcbFree list
		freePcb(&(pcb_pool[i]));
	}
}


//================================================
//freePcb
//Add the pcb pointed to by pointer p to the pcbFree list. 
//PARAMETERS: pointer to a pcb struct
//RETURN: none 
//================================================
void freePcb(pcb_PTR p){
    if (pcbFree_h == NULL){                //If pcbFree list is empty
		(*p).p_prev = NULL;
        (*p).p_next = NULL;
		pcbFree_h = p;
	}
	else{                                 //Otherwise, if there are items in pcbFree list
        (*p).p_prev = NULL;
		(*p).p_next = pcbFree_h;
		(*pcbFree_h).p_prev = p;
		pcbFree_h = p;
	}
}
//================================================
//allocPcb
//Remove an element from pcbFree linked list and intialize values for all pcb struct fields
//PARAMETERS: none
//RETURN: pointer to the pcb removed from the pcbFree linked list 
//================================================
pcb_PTR allocPcb(){
   if (pcbFree_h == NULL) return NULL;
   pcb_PTR freed_pcb_ptr = pcbFree_h;           //remove from head of linked list

   //if pcbFree only has one free pcb left
   if ((*freed_pcb_ptr).p_next == NULL) pcbFree_h = NULL;

   else{
    //detach next and prev pointers + update new head of freePcb linked list
    ((*freed_pcb_ptr).p_next)->p_prev = NULL;
    (*freed_pcb_ptr).p_prev = NULL;
    pcbFree_h = (*freed_pcb_ptr).p_next;
   }

   //intialize values for all pcb struct fields of newly freed pcb 
   
   //Process queue fields
   (*freed_pcb_ptr).p_prev = NULL;
   (*freed_pcb_ptr).p_next = NULL;

   //Process tree fields
   (*freed_pcb_ptr).p_child = NULL;
   (*freed_pcb_ptr).p_sib = NULL;
   (*freed_pcb_ptr).p_prnt = NULL;

   //Process status info
   for (int i = 0; i < STATEREGNUM; i++){
		freed_pcb_ptr->p_s.s_reg[i] = 0;
	}
   (*freed_pcb_ptr).p_time = 0;
   (*freed_pcb_ptr).p_semAdd = NULL;

   //Support layer info
   //p_supportStruct = NULL; ???? HAVE NOT DECLARED THIS AND UNSURE ABOUT INIT VALUE */

    return freed_pcb_ptr;
}
