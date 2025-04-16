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


extern void sysSupportGenHandler();
extern void syslvl_prgmTrap_handler(support_t *currentSupport);
void get_nuked(support_t *support_struct);
void getTOD(state_PTR excState);
void write_to_printer(char *virtualAddr, int len, support_t *support_struct);
void write_to_terminal(char *virtualAddr, int len, support_t *support_struct);
void read_from_terminal(char *virtualAddr, support_t *support_struct);
void syscall_excp_handler(support_t *suppStruct, int syscall_num_requested);

#endif