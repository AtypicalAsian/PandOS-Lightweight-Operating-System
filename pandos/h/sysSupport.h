/**************************************************************************** 
 * Nicolas & Tran
 * Declaration File for sysSupport.c module
 * 
 ****************************************************************************/
#ifndef SYSSUPPORT
#define SYSSUPPORT
#include "../h/types.h"
#include "../h/const.h"

void supexHandler();
void sysHandler(support_t* except_supp, state_t* exc_state, unsigned int sysNum);

void terminate();
void get_tod(state_t* exc_state);
void write_printer(support_t* except_supp, state_t* exc_state);
void write_terminal(support_t* except_supp, state_t* exc_state);
void read_terminal(support_t* except_supp, state_t* exc_state);


#endif