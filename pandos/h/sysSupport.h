/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for sysSupport.c module
 * 
 ****************************************************************************/
#ifndef SYSSUPPORT
#define SYSSUPPORT
#include "../h/types.h"
#include "../h/const.h"

void gen_excp_handler();
void syscall_excp_handler(support_t *currProc_support_struct,unsigned int syscall_num_requested,state_t* exceptionState);

void terminate();
void get_TOD(state_PTR excState);
void write_to_printer(support_t *currProcSupport, state_PTR exceptionState);
void write_to_terminal(support_t *currProcSupport, state_PTR exceptionState);
void read_from_terminal(support_t *currProcSupport, state_PTR exceptionState);


#endif