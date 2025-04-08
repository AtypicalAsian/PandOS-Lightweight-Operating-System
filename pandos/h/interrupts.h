#ifndef INTERRUPTS
#define INTERRUPTS

#define GETIP   0x0000FE00
#define IPSHIFT 8
#define TERMSTATUSMASK 0x000000FF

#define DEVREGADDR ((devregarea_t *)RAMBASEADDR)
void intExceptionHandler(state_t *exceptionState);

#endif