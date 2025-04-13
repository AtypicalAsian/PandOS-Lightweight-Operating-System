/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for sysSupport.c module
 * 
 ****************************************************************************/
#ifndef SYSUPPORT
#define SYSUPPORT
#include "../h/types.h"
#include "../h/const.h"

extern int support_device_sems[DEVICE_TYPES * DEVICE_INSTANCES];


void returnControlSup(support_t *support, int exc_code);
extern void sysSupportGenHandler();
extern void trapExcHandler(support_t *currentSupport);
void syscall_excp_handler(support_t *support_struct, int exceptionCode);
void terminate(support_t *support_struct);
void getTOD(state_PTR excState);
void writeToPrinter(char *virtualAddr, int len, support_t *support_struct);
void writeToTerminal(char *virtualAddr, int len, support_t *support_struct);
void readTerminal(char *virtualAddr, support_t *support_struct);

#endif