#ifndef SYSCALLS
#define SYSCALLS

#include "types.h"

void createProc(state_t * statep, support_t * supportp);
void terminateProc();
void passeren(int *semAdd);
pcb_PTR verhogen(int *semAdd);
void waitIO(int intlNo, int dNum, bool waitForTermRea);
void cpuTime(cpu_t * resultAddress);
void waitClk();
void getSupportData(support_t ** resultAddress);

#endif