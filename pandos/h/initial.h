#ifndef INITIAL
#define INITIAL

extern unsigned int process_count;
extern unsigned int soft_block_count;
extern pcb_PTR ready_tp;
extern pcb_PTR current_proc;
extern int device_sems[DEVICE_TYPES][DEVICE_INSTANCES];
extern int IntervalTimerSem;
int main(void);

#endif