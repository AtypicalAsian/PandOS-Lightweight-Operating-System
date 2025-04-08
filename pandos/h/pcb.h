#ifndef PCB
#define PCB

/************************* PROCQ.H *****************************
*
*  The externals declaration file for the Process Control Block
*    Module.
*
*  Written by Mikeyg
*/

#include "../h/const.h"
#include "../h/types.h"

void freePcb (pcb_PTR p);
pcb_PTR allocPcb ();
void initPcbs ();

pcb_PTR mkEmptyProcQ ();
int emptyProcQ (pcb_PTR tp);
void insertProcQ (pcb_PTR *tp, pcb_PTR p);
pcb_PTR removeProcQ (pcb_PTR *tp);
pcb_PTR outProcQ (pcb_PTR *tp, pcb_PTR p);
pcb_PTR headProcQ (pcb_PTR tp);

int emptyChild (pcb_PTR p);
void insertChild (pcb_PTR prnt, pcb_PTR p);
pcb_PTR removeChild (pcb_PTR p);
pcb_PTR outChild (pcb_PTR p);

#endif