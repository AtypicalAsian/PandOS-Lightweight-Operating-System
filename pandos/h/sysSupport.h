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


#endif