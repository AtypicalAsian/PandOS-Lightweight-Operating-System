#ifndef SYSUPPORT_H
#define SYSUPPORT_H


#include "const.h"
#include "types.h"

extern int support_device_sems[DEVICE_TYPES * DEVICE_INSTANCES];

void returnControl();
void returnControlSup(support_t *support, int exc_code);
extern void sysSupportGenHandler();
extern void trapExcHandler(support_t *currentSupport);
void supportSyscallHandler(int exc_code, support_t *support_struct);
void terminate(support_t *support_struct);
void getTOD(support_t *support_struct);
void writeToPrinter(char *virtualAddr, int len, support_t *support_struct);
void writeToTerminal(char *virtualAddr, int len, support_t *support_struct);
void readTerminal(char *virtualAddr, support_t *support_struct);

#endif