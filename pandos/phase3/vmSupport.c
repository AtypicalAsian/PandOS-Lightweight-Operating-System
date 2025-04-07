/**************************************************************************************************  
 * @file vmSupport.c  
 *  
 * 
 * @brief  
 * This module implements the TLB exception handler (The Pager). Additionally, the Swap Pool 
 * table and Swap Pool semaphore are local to this module. Instead of declaring them globally 
 * in initProc.c they are declared module-wide in vmSupport.c
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
 * 
 * 
 * TO-DO
 * This module implements the TLB exception handler (The Pager). Since reading and writing to each U-proc’s flash device is limited 
 * to supporting paging, this module should also contain the function(s) for 
 * reading and writing flash devices. Additionally, the Swap Pool table and Swap Pool semaphore are local to 
 * this module. Instead of declaring them globally in initProc.c they can be 
 * declared module-wide in vmSupport.c. The test function will now invoke a new “public” function initSwapStructs which will do the work 
 * of initializing both the Swap Pool table and accompanying semaphore.
 **************************************************************************************************/
#include "../h/types.h"
#include "../h/const.h"
#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/*Support Level Data Structures*/
HIDDEN swap_pool_t swapTable[POOLSIZE];

/*swap pool semaphore*/
HIDDEN int swapSem;

extern int devRegSem[49];
extern pcb_t* currProc;
extern int masterSem;


void updateTLB(pte_entry_t *updatedEntry){
    setENTRYHI(updatedEntry->entryHI);
    TLBP();
    if((getINDEX() & 0x80000000) == 0){
        setENTRYLO(updatedEntry->entryLO);
        TLBWI();
    } 
}


int findReplace(){

    static int currentReplacementIndex = 0;
    int i = 0;
    while((swapTable[(currentReplacementIndex + i) % 16].asid != -1) && (i < 16))
        ++i;
    i = (i == 16) ? 1 : i;
    
    return currentReplacementIndex = (currentReplacementIndex + i) % 16;
}

void init_swap(){
    int i;
    for(i= 0;i < 16; i++){
        swapTable[i].asid = -1; 
    }
    swapSem = 1;
}


void writeReadFlash(unsigned int RoW, unsigned int devNumber, unsigned int occupiedPageNumber, int swap_pool_index){
    unsigned int command = (occupiedPageNumber << 8) | RoW; 
    
    memaddr pfn = (swap_pool_index << 12) + POOLSTART;
    SYSCALL(SYS3, (memaddr) &devRegSem[DEV_INDEX(4, devNumber, FALSE)], 0, 0);
    dtpreg_t* flashaddr = (dtpreg_t*) DEV_REG_ADDR(4, devNumber);
    flashaddr->data0 = pfn;
    
    IEDISABLE;
    flashaddr->command = command;
    unsigned int deviceStatus = SYSCALL(SYS5, 4, devNumber, FALSE);
    IEENABLE;

    SYSCALL(SYS4, (memaddr) &devRegSem[DEV_INDEX(4, devNumber, FALSE)], 0, 0);

    if(deviceStatus != READY){
        killProc(&swapSem);
    }
}

void dirtyPage(unsigned int currASID, unsigned int occupiedPageNumber, unsigned int swap_pool_index){
    IEDISABLE;
    swapTable[swap_pool_index].ownerEntry->entryLO &= ~VALIDON;
    updateTLB(swapTable[swap_pool_index].ownerEntry);
    IEENABLE;
    writeReadFlash(WRITEFLASH, currASID-1, occupiedPageNumber, swap_pool_index); 
}


void clearSwap(int asid){
    int i;
    for(i = 0; i < POOLSIZE; i++){
        if(swapTable[i].asid == asid){
            swapTable[i].asid = -1;
        }
    }
}


void killProc(int *sem)
{
    clearSwap(currProc->p_supportStruct->sup_asid);
    if (sem != NULL)
    {
        SYSCALL(SYS4, (int) sem, 0, 0);
    }

    deallocate(currProc->p_supportStruct);
    
    SYSCALL(SYS4, (int) &masterSem, 0, 0);

    SYSCALL(SYS2, 0, 0, 0);    
}


void TLB_exceptionHandler(){    
    support_t* supp_p = (support_t*) SYSCALL(SYS8, 0, 0, 0);
    if(CAUSE_GET_EXCCODE(supp_p->sup_exceptState[PGFAULTEXCEPT].s_cause) == 1){
        killProc(NULL);
    }
    else{
        SYSCALL(SYS3, (memaddr) &swapSem, 0, 0);
        int missingPage = GETVPN(supp_p->sup_exceptState[PGFAULTEXCEPT].s_entryHI);
        int swap_pool_index = findReplace();

        if(swapTable[swap_pool_index].asid != -1){
            unsigned int occupiedASID = swapTable[swap_pool_index].asid;
            unsigned int occupiedPageNumber = swapTable[swap_pool_index].pg_number;            
            dirtyPage(occupiedASID, occupiedPageNumber, swap_pool_index);
        }

        writeReadFlash(READFLASH, supp_p->sup_asid-1, missingPage, swap_pool_index);


        IEDISABLE;
        swapTable[swap_pool_index].asid = supp_p->sup_asid;
        swapTable[swap_pool_index].pg_number = missingPage;
        swapTable[swap_pool_index].ownerEntry = &(supp_p->sup_privatePgTbl[missingPage]);

        supp_p->sup_privatePgTbl[missingPage].entryLO |= VALIDON;
        supp_p->sup_privatePgTbl[missingPage].entryLO = (supp_p->sup_privatePgTbl[missingPage].entryLO & ~ENTRYLO_PFN_MASK) | ((swap_pool_index << 12) + POOLSTART);
        
        updateTLB(&(supp_p->sup_privatePgTbl[missingPage]));
        IEENABLE;
        SYSCALL(SYS4, (memaddr) &swapSem, 0, 0);
        LDST(&(supp_p->sup_exceptState[PGFAULTEXCEPT]));
    }
}

pte_entry_t *findEntry(unsigned int pageNumber){
    return &(currProc->p_supportStruct->sup_privatePgTbl[pageNumber]);
}

void uTLB_refillHandler(){
    state_t* ex_state = (state_t*)BIOSDATAPAGE;
    int pg = GETVPN(ex_state->s_entryHI);
    setENTRYHI(currProc->p_supportStruct->sup_privatePgTbl[pg].entryHI);
    setENTRYLO(currProc->p_supportStruct->sup_privatePgTbl[pg].entryLO);
    TLBWR();        
    LDST(ex_state);
}       