#ifndef SCHEDULER
#define SCHEDULER

extern volatile cpu_t timeSlice;   

void switchProcess();

#endif