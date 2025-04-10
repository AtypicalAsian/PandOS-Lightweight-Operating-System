#ifndef CONST
#define CONST
/**************************************************************************** 
 *
 * This header file contains utility constants & macro definitions.
 * 
 ****************************************************************************/

/* Hardware & software constants */
#define PAGESIZE 4096 
#define WORDLEN  4 
#define MAXPROC 20 
#define MAXPAGES      32 
#define USERPGTBLSIZE MAXPAGES
#define UPROCMAX 8
#define POOLSIZE (UPROCMAX * 2)
#define STACKSIZE 499
#define DEVINTNUM 5
#define DEVPERINT 8

/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR   0x10000000
#define RAMBASESIZE   0x10000004
#define TODLOADDR     0x1000001C
#define INTERVALTMR   0x10000020
#define TIMESCALEADDR 0x10000024

/* device register addresses */
#define DEVICEREGSTART  0x10000054

/* Memory related constants */
#define KSEG1        0x20000000
#define KUSEG        0x80000000
#define ENDKUSEG    KUSEG + MAXPAGES
#define BIOSDATAPAGE 0x0FFFF000
#define PASSUPVECTOR 0x0FFFF900
#define UPROCSTARTADDR 0x800000B0
#define USERSTACKTOP   0xC0000000
#define STACKSTART    0x20001000
#define VPNBASE 0x80000000
#define UPROCSTACKPG 0xBFFFF000
#define TOPSTKPAGE 0x20001000

/* utility constants */
#define NULL 			    ((void *)0xFFFFFFFF)
#define TRUE 			    1
#define FALSE 			    0
#define HIDDEN 			    static
#define RESET			    0
#define ACK				    1
#define READY			    1
#define ON         1
#define OK         0
#define NOPROC     -1
#define INITPROCCNT 0
#define INITSBLOCKCNT 0
#define GETEXCPCODE 0x0000007C

/* device interrupts */
#define TIMERINT          1
#define INTERVALTMR_LINE  2   
#define DISKINT			  3
#define FLASHINT 		  4
#define NETWINT 		  5
#define PRNTINT 		  6
#define TERMINT			  7

/* Interupts constants */
#define GETIP   0x0000FE00
#define IPSHIFT 8
#define TRANS_CHAR 5
#define RECVD_CHAR 5
#define TERMSTATUSMASK 0x000000FF


/* syscall exceptions */
#define CREATEPROCESS 1
#define TERMINATEPROCESS   2
#define PASSEREN      3
#define VERHOGEN      4
#define WAITIO        5
#define GETTIME       6
#define CLOCKWAIT     7
#define GETSUPPORTPTR 8
#define TERMINATE     9
#define GET_TOD       10
#define WRITEPRINTER  11
#define WRITETERMINAL 12
#define READTERMINAL  13
#define DISK_PUT      14
#define DISK_GET      15
#define FLASH_PUT     16
#define FLASH_GET     17
#define DELAY         18

#define SYS1 1
#define SYS2 2
#define SYS3 3
#define SYS4 4
#define SYS5 5
#define SYS6 6
#define SYS7 7
#define SYS8 8
#define SYS9 9
#define SYS10 10
#define SYS11 11
#define SYS12 12
#define SYS13 13


#define TLBS              3
/* Exceptions related constants */
#define PGFAULTEXCEPT 0
#define GENERALEXCEPT 1

/* Status setting constants */
#define ALLOFF      0x00000000
#define USERPON     0x00000008
#define IEPON       0x00000004
#define IECON       0x00000001
#define IMON        0x0000FF00
#define TEBITON     0x08000000
#define DIRTYON  0x00000400
#define VALIDON  0x00000200

#define INTSOFF getSTATUS() & (~IECON)
#define INTSON getSTATUS() | IECON | IMON

#define GETEXECCODE    0x0000007C
#define LOCALTIMERINT  0x00000200
#define TIMERINTERRUPT 0x00000400
#define DISKINTERRUPT  0x00000800
#define FLASHINTERRUPT 0x00001000
#define NETWINTERRUPT  0x00002000
#define PRINTINTERRUPT 0x00004000
#define TERMINTERRUPT  0x00008000
#define CAUSESHIFT     2

#define VPNSHIFT      12
#define ASIDSHIFT     6


/*Cause Register Mask to Isolate the correct corresponding to which line the interupt was generated from*/
#define	LINE1MASK		0x00000200		/* constant for setting all bits to 0 in the Cause register except for bit 9 -> line 1 interrupts*/
#define	LINE2MASK		0x00000400		/* constant for setting all bits to 0 in the Cause register except for bit 10 -> line 2 interrupts */
#define	LINE3MASK		0x00000800		/* constant for setting all bits to 0 in the Cause register except for bit 11 -> line 3 interrupts */
#define	LINE4MASK		0x00001000		/* constant for setting all bits to 0 in the Cause register except for bit 12 -> line 4 interrupts */
#define	LINE5MASK		0x00002000		/* constant for setting all bits to 0 in the Cause register except for bit 13 -> line 5 interrupts */
#define	LINE6MASK		0x00004000		/* constant for setting all bits to 0 in the Cause register except for bit 14 -> line 6 interrupts */
#define	LINE7MASK		0x00008000		/* constant for setting all bits to 0 in the Cause register except for bit 15 -> line 7 interrupts */

/* Terminal and Device Operations*/
#define OKCHARTRANS  5
#define TRANSMITCHAR 2
#define FLASHREAD  2
#define FLASHWRITE 3
#define DEVICE_TYPES     6 
#define DEVICE_INSTANCES 8 
#define DEVREGSIZE	    16 

/* Device Sems*/
#define FLASHSEM 1
#define PRINTSEM 3
#define TERMSEM 4
#define TERMWRSEM 5

/* Time related constants*/
#define TIMESLICE  5000     
#define SECOND     1000000
#define INTIMER  100000UL     
#define PLT_HIGHEST_VAL   0xFFFFFFFFUL 

/* Phase 3 Constants*/
#define FLASHADDRSHIFT 8
#define MISSINGPAGESHIFT 0xFFFFF000
#define FRAMEADDRSHIFT 0x20020000
#define VALIDOFF 0xFFFFFDFF
#define EOS	 '\n'
#define PRINTCHR 2
#define TERMTRANSHIFT 8
#define EXCODE_NUM 20


#define NEVER 0xFFFFFFFF
#define DELAYASID 0

#define MAXPOINT ((void *)0xFFFFFFFF)
#define MINPOINT ((void *)0x00000000)

#define RAMTOP(T) ((T) = ((*((int *)RAMBASEADDR)) + (*((int *)RAMBASESIZE))))
#define EXCSTATE ((state_t *) BIOSDATAPAGE)
#define TIME_TO_TICKS(T) (T) * (*((cpu_t *)TIMESCALEADDR)) /*convert time value into hardware ticks*/
#define LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 
#define STCK(T) ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))
#define IP(C) ((C & 0x0000FF00) >> 8)
#define EXCCODE(C) ((C & 0x0000007C) >> 2)

/* Macro to read current kernel-mode user-mode control bit from Status register */
#define KUC(S) ((S & 0x00000002) >> 1)
#define KUP(S) (((S) & 0x00000008) >> 3)

#endif

