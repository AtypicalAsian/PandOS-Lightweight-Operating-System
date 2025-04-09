#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

void main() {
    int status;
    char buf[40];
    char temp;
    int start,end;

    print(WRITETERMINAL, "Enter your word: ");

    status = SYSCALL(READTERMINAL, (int)&buf[0], 0, 0);

    if (buf[status - 1] == '\n') {
        status --;
    }
    buf[status] = EOS;

    start = 0;
    end = status - 1;

    while (start < end) {
        temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }

    print(WRITETERMINAL, "\nReversed string of test result is: ");
    print(WRITETERMINAL, buf);
    print(WRITETERMINAL, "\n");

    SYSCALL(TERMINATE, 0, 0, 0);
}