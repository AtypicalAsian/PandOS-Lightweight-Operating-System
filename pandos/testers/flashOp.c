#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

/*Written based on diskIO test program to test flash read/write functionalities*/

#define FLASH_TEST_BLOCK_1 5
#define FLASH_TEST_BLOCK_2 10
#define FLASH_UNIT 0
#define MILLION 1000000

void main() {
    int i;
    int fstatus;
    int *buffer;

    buffer = (int *)(SEG2 + (20 * PAGESIZE)); // User space buffer

    print(WRITETERMINAL, "flashTest starts\n");

    // Write 123 to FLASH block 5
    *buffer = 123;
    fstatus = SYSCALL(FLASH_PUT, (int)buffer, FLASH_UNIT, FLASH_TEST_BLOCK_1);
    if (fstatus != READY)
        print(WRITETERMINAL, "flashTest error: flash i/o write result\n");
    else
        print(WRITETERMINAL, "flashTest ok: flash write result\n");

    // Overwrite buffer with 999 and write to FLASH block 10
    *buffer = 999;
    fstatus = SYSCALL(FLASH_PUT, (int)buffer, FLASH_UNIT, FLASH_TEST_BLOCK_2);

    // Read back from FLASH block 5
    fstatus = SYSCALL(FLASH_GET, (int)buffer, FLASH_UNIT, FLASH_TEST_BLOCK_1);
    if (*buffer != 123)
        print(WRITETERMINAL, "flashTest error: bad first flash block readback\n");
    else
        print(WRITETERMINAL, "flashTest ok: first flash block readback\n");

    // Read back from FLASH block 10
    fstatus = SYSCALL(FLASH_GET, (int)buffer, FLASH_UNIT, FLASH_TEST_BLOCK_2);
    if (*buffer != 999)
        print(WRITETERMINAL, "flashTest error: bad second flash block readback\n");
    else
        print(WRITETERMINAL, "flashTest ok: second flash block readback\n");

    // Capacity test: attempt to exceed flash capacity
    i = 0;
    fstatus = SYSCALL(FLASH_GET, (int)buffer, FLASH_UNIT, i);
    while ((fstatus == READY) && (i < MILLION)) {
        i++;
        fstatus = SYSCALL(FLASH_GET, (int)buffer, FLASH_UNIT, i);
    }

    if (i < MILLION)
        print(WRITETERMINAL, "flashTest ok: flash device capacity detection\n");
    else
        print(WRITETERMINAL, "flashTest error: flash capacity undetected\n");

    print(WRITETERMINAL, "flashTest: completed\n");

    // Try reading into protected RAM
    SYSCALL(FLASH_GET, SEG1, FLASH_UNIT, FLASH_TEST_BLOCK_1);
    print(WRITETERMINAL, "flashTest error: just read into segment 1\n");

    // Trigger traps for completeness
    i = i / 0;
    print(WRITETERMINAL, "flashTest error: divide by 0 did not terminate\n");

    LDST(buffer);
    print(WRITETERMINAL, "flashTest error: priv. instruction did not terminate\n");

    SYSCALL(1, (int) buffer, 0, 0);
    print(WRITETERMINAL, "flashTest error: sys1 did not terminate\n");

    SYSCALL(TERMINATE, 0, 0, 0);
}
