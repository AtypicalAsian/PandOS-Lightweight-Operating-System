#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/const.h"


HIDDEN pcb_t pcbFree_table[MAXPROC];

HIDDEN pcb_PTR pcbFree_h;


void initPcbs() {
    pcbFree_h = &(pcbFree_table[0]); 

    int i;
    for (i = 0; i < MAXPROC - 1; ++i) {
        pcbFree_table[i].p_next = &(pcbFree_table[i + 1]);
    }

    pcbFree_table[MAXPROC - 1].p_next = NULL;

}


void freePcb(pcb_PTR p) {
   p->p_next = pcbFree_h;
   pcbFree_h = p; 
}


pcb_PTR allocPcb() {
    pcb_PTR head = NULL;
    if (pcbFree_h != NULL){
        head = pcbFree_h;
        pcbFree_h = head->p_next;
        
        head->p_next = NULL;
        head->p_prev = NULL;
        head->p_prnt = NULL;
        head->p_child = NULL;
        head->p_next_sib = NULL;
        head->p_prev_sib = NULL;
        head->p_semAdd = NULL;
        head->p_time = 0;
        
        head->p_s.s_cause = 0;
        head->p_s.s_entryHI = 0;
        int i;
        for (i = 0; i < STATEREGNUM; ++i) {
            head->p_s.s_reg[i] = 0;
        }
        head->p_s.s_entryHI = 0;
        head->p_s.s_pc = 0;
        head->p_s.s_status = 0;

        head->p_supportStruct = NULL;
    }
    return head;
}


pcb_PTR mkEmptyProcQ() {
    return NULL;
}


int emptyProcQ(pcb_PTR tp) {
    return tp == NULL;
}


pcb_PTR headProcQ(pcb_PTR tp){
    if (tp == NULL) return NULL;

    return tp->p_prev;
}


pcb_PTR outProcQ(pcb_PTR *tp, pcb_PTR p){
    if (*tp == NULL) return NULL;

    pcb_PTR toRemove = *tp;
    int found = FALSE;
    do {
        if (toRemove == p) {
            found = TRUE;
            break;
        }

        toRemove = toRemove->p_next;

    } while (toRemove != *tp);

    if (!found) return NULL;

    if (toRemove->p_next == toRemove) *tp = NULL;
    else {

        if (toRemove == *tp) *tp = (*tp)->p_next;

        toRemove->p_prev->p_next = toRemove->p_next;
        toRemove->p_next->p_prev = toRemove->p_prev;
    }

    toRemove->p_next = NULL;
    toRemove->p_prev = NULL;

    return toRemove;
}


void insertProcQ(pcb_PTR *tp, pcb_PTR p) {
    if (*tp == NULL) {
        *tp = p;
        p->p_prev = p;
        p->p_next = p;
        return;
    }

    p->p_next = *tp;
    p->p_prev = (*tp)->p_prev;
    p->p_prev->p_next = p;
    (*tp)->p_prev = p;
    *tp = p;
}


pcb_PTR removeProcQ(pcb_PTR *tp) {
    if(*tp == NULL) return NULL;

    pcb_PTR toRemove = (*tp)->p_prev;

    if (toRemove->p_prev == toRemove) *tp = NULL;
    else {
        (*tp)->p_prev = toRemove->p_prev;
        toRemove->p_prev->p_next = *tp;
    }

    toRemove->p_next = NULL;
    toRemove->p_prev = NULL;

    return toRemove;
}


int emptyChild(pcb_PTR p) {
    return p->p_child == NULL;
}


void insertChild(pcb_PTR prnt, pcb_PTR p) {

    if (prnt->p_child != NULL) {
        prnt->p_child->p_prev_sib = p;
    }

    p->p_prnt = prnt;
    p->p_next_sib = prnt->p_child;
    p->p_prev_sib = NULL;
    prnt->p_child = p;
}


pcb_PTR removeChild(pcb_PTR p) {
    if (p->p_child == NULL) {
        return NULL;
    }
    else {
        pcb_PTR child = p->p_child;
        p->p_child = child->p_next_sib;

        if (child->p_next_sib != NULL){
            child->p_next_sib->p_prev_sib = NULL;
        }

        child->p_prnt = NULL;
        child->p_next_sib = NULL;

        return child;
    }
}


pcb_PTR outChild(pcb_PTR p) {
    if (p->p_prnt == NULL) {
        return NULL;
    }

    if (p->p_prev_sib != NULL) {
        p->p_prev_sib->p_next_sib = p->p_next_sib;
    }
    
    if (p->p_next_sib != NULL) {
        p->p_next_sib->p_prev_sib = p->p_prev_sib;
    }

    if (p->p_prnt->p_child == p) {
        p->p_prnt->p_child = p->p_next_sib;
    }

    p->p_prev_sib = NULL;
    p->p_next_sib = NULL;
    p->p_prnt = NULL;

    return p;
}