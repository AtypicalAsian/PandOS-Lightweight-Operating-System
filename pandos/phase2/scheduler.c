/**************************************************************************** 
CS372 - Operating Systems
Dr. Mikey Goldweber
Written by: Nicolas & Tran

This module contains the implementation of the deadlock detector and the 
scheduler.

To view version history and changes:
    - Remote GitHub Repo: https://github.com/AtypicalAsian/CS372-OS-Project
****************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "/usr/include/umps3/umps/libumps.h"

