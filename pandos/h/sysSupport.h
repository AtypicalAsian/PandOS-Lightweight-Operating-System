/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for sysSupport.c module
 * 
 ****************************************************************************/
#ifndef SYSUPPORT
#define SYSUPPORT
#include "../h/types.h"
#include "../h/const.h"

extern int devSema4_support[DEVICE_TYPES * DEV_UNITS];


void returnControlSup(support_t *support, int exc_code);
extern void sysSupportGenHandler();
extern void syslvl_prgmTrap_handler(support_t *currentSupport);
void syscall_excp_handler(support_t *suppStruct, int syscall_num_requested);
void get_nuked(support_t *support_struct);
void getTOD(state_PTR excState);
void writeToPrinter(char *virtualAddr, int len, support_t *support_struct);
void writeToTerminal(char *virtualAddr, int len, support_t *support_struct);
void readTerminal(char *virtualAddr, support_t *support_struct);

#endif