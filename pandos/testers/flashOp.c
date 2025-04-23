#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

/* Basic Flash Get/Put Test based on diskIO - will need to test more edge cases*/

#define FLASH_TEST_BLOCK 5
#define FLASH_UNIT 0

void main() {
    int fstatus;
    int *buffer;

    buffer = (int *)(SEG2 + (20 * PAGESIZE));  

    print(WRITETERMINAL, "Basic flashTest starts\n");

    /* Write value 123 to FLASH block 5 */
    *buffer = 123;
    fstatus = SYSCALL(FLASH_PUT, (int)buffer, FLASH_UNIT, FLASH_TEST_BLOCK);
    if (fstatus != READY) {
        print(WRITETERMINAL, "flashTest error: flash write failed\n");
    } else {
        print(WRITETERMINAL, "flashTest ok: flash write succeeded\n");
    }

    /* Clear buffer and read back from FLASH block 5 */
    *buffer = 0;
    fstatus = SYSCALL(FLASH_GET, (int)buffer, FLASH_UNIT, FLASH_TEST_BLOCK);
    if (fstatus != READY) {
        print(WRITETERMINAL, "flashTest error: flash read failed\n");
    } else if (*buffer != 123) {
        print(WRITETERMINAL, "flashTest error: flash read incorrect value\n");
    } else {
        print(WRITETERMINAL, "flashTest ok: flash read correct value\n");
    }

    print(WRITETERMINAL, "Basic flashTest completed\n");

    SYSCALL(TERMINATE, 0, 0, 0);
}

