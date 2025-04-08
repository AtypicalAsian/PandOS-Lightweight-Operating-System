#include <umps3/umps/libumps.h>
#include "../h/const.h"
#include "../h/types.h"

#include "../h/initProc.h"
#include "../h/sysSupport.h"
#include "../h/vmSupport.h"

int testSem;
support_t support_structs[UPROCMAX];
support_t *freeSupports[UPROCMAX+1];
int stackSupport;

void dealocate_sup(support_t *support){
    freeSupports[stackSupport] = support;
    stackSupport++;
}

support_t* allocate_sup() {
    support_t *tempSupport = NULL;
    
    if (stackSupport != 0){
        stackSupport--;
        tempSupport = freeSupports[stackSupport];
    }
    return tempSupport;
}

void initSupport() {
    stackSupport = 0;
    int i;
    for (i = 0; i < UPROCMAX; i++){
        dealocate_sup(&support_structs[i]);
    }
}


void test() {
    /* Initalise device reg semaphores */
    initSwapPool();
    initSupport();

    state_t initial_state;

    /* Set up initial processor state */
    initial_state.s_pc = UPROCSTARTADDR;
    initial_state.s_t9 = UPROCSTARTADDR;
    initial_state.s_sp = USERSTACKTOP;
    initial_state.s_status = IEPON | IMON | TEBITON | USERPON;

    support_t *supportStruct;
    int proc;
    for (proc= 1; proc <= UPROCMAX; proc++) {
        initial_state.s_entryHI = (proc << ASIDSHIFT);

        supportStruct = allocate_sup();
        
        /* Set up exception context */
        supportStruct->sup_privateSem = 0;

        supportStruct->sup_asid = proc;
        supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) &pager;
        supportStruct->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) &sysSupportGenHandler; 
        
        supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_status = IEPON | IMON | TEBITON;
        supportStruct->sup_exceptContext[GENERALEXCEPT].c_status = IEPON | IMON | TEBITON;

        supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) &(supportStruct->sup_stackTLB[STACKSIZE]);
        supportStruct->sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) &(supportStruct->sup_stackGen[STACKSIZE]);
        
        /* Initalise page table */
        int pgTblSize;
        for(pgTblSize = 0; pgTblSize<USERPGTBLSIZE-1; pgTblSize++) {
            supportStruct->sup_privatePgTbl[pgTblSize].pte_entryHI = VPNBASE + (pgTblSize << VPNSHIFT) + (proc << ASIDSHIFT);
            supportStruct->sup_privatePgTbl[pgTblSize].pte_entryLO = DIRTYON;
        }
        
        /* Set last entry in page table to the stack*/
        supportStruct->sup_privatePgTbl[USERPGTBLSIZE-1].pte_entryHI = UPROCSTACKPG + (proc << ASIDSHIFT);
        supportStruct->sup_privatePgTbl[USERPGTBLSIZE-1].pte_entryLO = DIRTYON;

         /* Create the process*/
        SYSCALL(CREATEPROCESS, (memaddr) &initial_state, (memaddr)supportStruct, 0);
    }
    
    int i;
    for (i = 0; i < UPROCMAX; i++){
        SYSCALL(PASSEREN, (memaddr) &testSem, 0, 0);
    }

    /* Terminate the current process */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0);
}
