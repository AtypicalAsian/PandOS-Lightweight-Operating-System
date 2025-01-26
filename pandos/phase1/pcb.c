#include "../h/pcb.h"
#include "../h/const.h"

static pcb_t pcbFree_table[MAXPROC];
static struct list_head pcbFree_h;

static pid_t curr_pid;
static struct list_head pid_list_h;


void initPcbs() {
    /*
    Initialize the pcbFree list to contain all the elements of the static array of MAXPROC pcbs
    Called once during data structure intialization
    */
    return NULL;
}

void freePcb(pcb_t *p){
    /**/
    return NULL;
}

pcb_t *allocPcb(){
    /**/
    return NULL;
}
