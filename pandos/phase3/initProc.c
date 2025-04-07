/**************************************************************************************************  
 * @file initProc.c  
 *  
 * 
 * @brief  
 * This module implements test() and exports the Support Level's global variables.
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

/*pool of sup structs to allocate*/
static support_t supPool[MAXUPROCESS];
int masterSem;

/*sup_struct free list*/
support_t* suppFree;

/*sup level device semaphores*/
int devRegSem[48];

extern void TLB_exceptionHandler();
extern void exceptionHandler();
extern void supexHandler();


void deallocate(support_t*  toDeallocate){
    support_t *tmp;
    tmp = suppFree;
    if(tmp == NULL){
        suppFree = toDeallocate;
        suppFree->next = NULL;
    }
    else{
        while(tmp->next != NULL)    tmp = tmp->next;
        tmp->next = toDeallocate;
        tmp = tmp->next;
        tmp->next = NULL;
    }    
}

support_t* allocate(){
    support_t* tmp;
    tmp = suppFree;
    if(tmp == NULL)
        return NULL;
    else{
        suppFree = suppFree->next;
        tmp->next = NULL;
        return tmp;
    }    
}

void init_supLevSem(){
    int i;
    for (i = 0; i < 49; i += 1)
        devRegSem[i] = 1;
}


void createProc(int id){
    memaddr ramTOP;
    RAMTOP(ramTOP);
    memaddr topStack = ramTOP - (2*id*PAGESIZE);

   /*init process state*/
    state_t newState;
    newState.s_entryHI = id <<6;
    newState.s_pc = newState.s_t9 = 0x800000B0;
    newState.s_sp = 0xC0000000;
    newState.s_status = 0x0000FF00 | 0x00000004 | 0x08000000 | 0x00000008;

    /*get one supp struct from free list*/
    support_t* supStruct = allocate();
    if(supStruct != NULL){
        supStruct->sup_asid = id;
        
        /*init general exception context*/
        supStruct->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) supexHandler;
        supStruct->sup_exceptContext[GENERALEXCEPT].c_status = 0x0000FF00 | 0x00000004 | 0x08000000;
        supStruct->sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) topStack;

        /*init page fault exception context*/
        supStruct->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) TLB_exceptionHandler;
        supStruct->sup_exceptContext[PGFAULTEXCEPT].c_status = 0x0000FF00 | 0x00000004 | 0x08000000;
        supStruct->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) (topStack + PAGESIZE);

        /*init TLB entries*/
        int i;
        for (i = 0; i < 31; i++)
        {
            supStruct->sup_privatePgTbl[i].entryHI = 0x80000000 + (i << VPNSHIFT) + (id << 6);
            supStruct->sup_privatePgTbl[i].entryLO = 0x00000400;
        }
        supStruct->sup_privatePgTbl[31].entryHI = 0xBFFFF000 + (id << 6);
        supStruct->sup_privatePgTbl[31].entryLO = 0x00000400;
        
        int status = SYSCALL(SYS1, (memaddr) &newState, (memaddr) supStruct, 0);

        if (status != 1)
        {
            SYSCALL(SYS2, 0, 0, 0);
        }
    }
}

void InstantiatorProcess(){
    init_swap();
    init_supLevSem();    
    masterSem = 0;
    suppFree = NULL;

    int i;
    for(i=0; i < 8; i++){
        deallocate(&supPool[i]);
        
    }
    int id;
    for(id=0; id < 8; id++)
    {
		createProc(id+1);
	}

    /*do P op on master semaphore for each process*/
    int j;
    for(j=0; j < 8; j++)
    {
		SYSCALL(SYS3, (int) &masterSem, 0, 0);
	}
    SYSCALL(SYS2, 0, 0, 0);
}

