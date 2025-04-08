#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

void main() {
    int status1, status2;
    char buf1[20];
    char buf2[20];
    char resultBuf[20];
    int num1, num2, result;

    print(WRITETERMINAL, "Add two numbers Test starts\n");

    print(WRITETERMINAL, "Enter the first number: ");
    status1 = SYSCALL(READTERMINAL, (int)&buf1[0], 0, 0,);
    /* Set EOS for proper termination after finish reading a string */
    buf1[status1] = EOS;

    print(WRITETERMINAL, "\nEnter the second number: ");
    status2 = SYSCALL(READTERMINAL, (int)&buf2[0], 0, 0);
    /* Set EOS for proper termination after finish reading a string */
    buf2[status2] = EOS;

    /* Convert strings to integer values */
    num1 = atoi(buf1);
    num2 = atoi(buf2);

    result = num1 + num2;

    itoa(result, resultBuf);

    print(WRITETERMINAL, "\nResult is: ");
    print(WRITETERMINAL, &resultBuf[0]);

    print(WRITETERMINAL, "\n\Add two numbers Test concluded\n");

    SYSCALL(TERMINATE, 0, 0, 0);
}