#ifndef ASL
#define ASL

/************************** ASL.H ******************************
*
*  The externals declaration file for the Active Semaphore List
*    Module.
*
*  Written by Mikeyg
*/

#include "../h/types.h"

int insertBlocked(int *semAdd, pcb_PTR p);
pcb_PTR removeBlocked(int *semAdd);
pcb_PTR outBlocked(pcb_PTR p);
pcb_PTR headBlocked(int *semAdd);
void initASL();

#endif