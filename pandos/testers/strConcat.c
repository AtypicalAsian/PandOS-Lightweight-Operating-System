
/************************************************************************************************ 
 * Previous version of strConcat was somehow with the status variable used in the for loop
 * to copy chars from the 1st string. The loop is copying status-1 characters from the first string
 * into the concatenation buffer buf3, possibly anticipating a newline char -> error when joining 2
 * strings we were missing the last char from the first string
 * 
 * This version introduces copyLen var to correct for the 1 index offset
 ************************************************************************************************/
/* concatenates two strings together and prints them out */
#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"


void main() {
	int status, status2, i;
	int copyLen;
	char buf[20];
	char buf2[20];
	char buf3[40];
	
	print(WRITETERMINAL, "Strcat Test starts\n");
	print(WRITETERMINAL, "Enter a string: ");
		
	status = SYSCALL(READTERMINAL, (int)&buf[0], 0, 0);
	buf[status] = EOS;
	
	print(WRITETERMINAL, "\n");
	print(WRITETERMINAL, "Enter another string: ");

	status2 = SYSCALL(READTERMINAL, (int)&buf2[0], 0, 0);
	buf2[status2] = EOS;

	/* Check if the last character of the first string is a newline */
    if (status > 0 && buf[status - 1] == '\n') {
        copyLen = status - 1;   /* Exclude the newline */
    } else {
        copyLen = status;       /* Use the full length */
    }

	i = 0;
	for( i = 0; i < copyLen; i++ )
	{
		buf3[i] = buf[i];
	}

	for( i = 0; i < status2; i++ )
	{
		buf3[copyLen + i] = buf2[i];
	}

	buf3[copyLen + status2] = EOS;

	print(WRITETERMINAL, &buf3[0]);
	
	print(WRITETERMINAL, "\n\nStrcat concluded\n");

		
	/* Terminate normally */	
	SYSCALL(TERMINATE, 0, 0, 0);
}

