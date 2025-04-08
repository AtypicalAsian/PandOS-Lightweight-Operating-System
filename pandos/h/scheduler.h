#ifndef SCHEDULER
#define SCHEDULER

extern volatile cpu_t timeSlice;   

void scheduler();

#endif