/**************************************************************************************************  
 * @file deviceSupportDMA.c  
 * 
 * @ref
 * 
 * 
 * @authors  
 * Nicolas & Tran  
 * View version history and changes: https://github.com/AtypicalAsian/CS372-OS-Project
 **************************************************************************************************/
#include "../h/types.h"
#include "../h/const.h"
#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "../h/deviceSupportDMA.h"
#include "/usr/include/umps3/umps/libumps.h"


/*writes data from given memory address to specific disk device (diskNo)
* 5.2 pandos and 5.3 pops
*/
void disk_put(int *logicalAddr, int diskNo, int sectNo, support_t *support_struct) {

    /* Find the disk device register with given diskNo */
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
}


/*reads data from given location in disk
* 5.2 pandos and 5.3 pops
*/
void disk_get(int *logicalAddr, int diskNo, int sectNo, support_t *support_struct) {
    
    /* Find the disk device register with given diskNo */
    devregarea_t *busRegArea = (devregarea_t *) RAMBASEADDR;
    
}
