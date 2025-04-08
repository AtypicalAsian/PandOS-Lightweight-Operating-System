/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for sysSupport.c module
 * 
 ****************************************************************************/
#ifndef SYSSUPPORT
#define SYSSUPPORT
#include "../h/types.h"
#include "../h/const.h"

extern int deviceSema4s[MAXSHAREIODEVS];

extern void gen_excp_handler();
extern void program_trap_handler();
extern void init_deviceSema4s();
void syscall_excp_handler(support_t *currProc_support_struct,int syscall_num_requested);
extern void return_control(int exception_code, support_t *supportStruct);
void terminate();


#endif