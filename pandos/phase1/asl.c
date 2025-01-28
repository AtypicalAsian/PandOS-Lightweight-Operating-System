/**************************************************************************** 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

ACTIVE SEMAPHORE LIST IMPLEMENTATION
****************************************************************************/

#include "../h/const.h"
#include "../h/types.h"
int insertBlocked(int *semAdd, pcb_PTR p){
    return -1;
}
pcb_PTR removeBlocked(int *semAdd){
    return *semAdd;
}

pcb_PTR outBlocked(pcb_PTR p){
    return p;
}


pcb_PTR headBlocked(int *semAdd){
    return semAdd;
}


void initASL(){
    return;
}