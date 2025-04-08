#ifndef INITPROC
#define INITPROC


#include "types.h"

extern int testSem;
extern void test(void);

support_t* allocate_sup();
void initSupport();
void dealocate_sup(support_t *support);
 
#endif