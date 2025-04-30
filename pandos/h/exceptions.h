#ifndef EXCEPTIONS
#define EXCEPTIONS
#include "../h/types.h"
/**************************************************************************** 
 * Nicolas & Tran
 * The externals declaration file for exceptions.c module
 * 
 * 
 ****************************************************************************/

void sysTrapHandler(unsigned int KUp); /*Syscall Handler*/
void tlbTrapHanlder(); /*TLB Trap Handler*/
void prgmTrapHandler(); /*Program Trap Handler*/
void gen_exception_handler(); /*General Exception Handler*/

/*SYSCALLS METHOD HANDLER*/
void createProcess(state_t *stateSYS, support_t *suppStruct); /*SYS1*/
void terminateProcess(); /*SYS2*/
void passeren(int *sem); /*SYS3*/
pcb_PTR verhogen(int *sem); /*SYS4*/
void waitForIO(int lineNum, int deviceNum, int readBool); /*SYS5*/
void getCPUTime(state_t *savedState); /*SYS6*/
void waitForClock(); /*SYS7*/
void getSupportData(support_t **resultAddress); /*SYS8*/
cpu_t get_elapsed_time(); /*helper method to calculate elapsed time since process quantum began*/
#define EXCODESHIFT   10

#endif