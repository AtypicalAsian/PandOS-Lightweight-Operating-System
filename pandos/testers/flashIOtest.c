#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

#define FLASH_UNIT     0
#define BLOCK1         8    /*Valid flash block*/
#define BLOCK2         10   /*Another valid flash block*/

void main() {
    /* Ref: 
     * pandOS chapter 5
     * phase4/diskIOtest.c
     * princOfOperations pg 34,35,36 (see Block No., )
     */
    /* Declare a character buffer */
    char *buffer = (char *)(SEG2 + (30 * PAGESIZE));
    /* Initialize flash device status */
    int fstatus;

    print(WRITETERMINAL, "flashTest starts\n");

    strcpy(buffer, "hello world!");
    /* Perform SYSCALL 16 to write to flash device */
    fstatus = SYSCALL(FLASH_PUT, (int)buffer, FLASH_UNIT, BLOCK1);

    /* Check whether the flash device status is available to perform flash READ/WRITE operations */
    if (fstatus != READY) {
        print(WRITETERMINAL, "flashTest error: write to BLOCK1 failed\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }

    strcpy(buffer, "OS is fun!");
    fstatus = SYSCALL(FLASH_PUT, (int)buffer, FLASH_UNIT, BLOCK2);
    if (fstatus != READY) {
        print(WRITETERMINAL, "flashTest error: write to BLOCK2 failed\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }

    /* Overwrite character buffer to clear old value in BLOCK1 */
    strcpy(buffer, "overwrite before read");
    fstatus = SYSCALL(FLASH_GET, (int)buffer, FLASH_UNIT, BLOCK1);
    if (fstatus != READY) {
        print(WRITETERMINAL, "flashTest error: read from BLOCK1 failed\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }
    if (strcmp(buffer, "hello world!") == 0)
        print(WRITETERMINAL, "flashTest ok: BLOCK1 readback matched\n");
    else
        print(WRITETERMINAL, "flashTest error: BLOCK1 readback mismatch\n");

    /* Overwrite character buffer to clear old value in BLOCK2 */
    strcpy(buffer, "overwrite again");
    fstatus = SYSCALL(FLASH_GET, (int)buffer, FLASH_UNIT, BLOCK2);
    if (fstatus != READY) {
        print(WRITETERMINAL, "flashTest error: read from BLOCK2 failed\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }
    if (strcmp(buffer, "OS is fun!") == 0)
        print(WRITETERMINAL, "flashTest ok: BLOCK2 readback matched\n");
    else
        print(WRITETERMINAL, "flashTest error: BLOCK2 readback mismatch\n");

    print(WRITETERMINAL, "flashTest completed\n");
    SYSCALL(TERMINATE, 0, 0, 0);
}