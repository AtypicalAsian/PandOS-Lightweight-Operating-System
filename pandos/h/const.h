#ifndef CONST
#define CONST
/**************************************************************************** 
 *
 * This header file contains utility constants & macro definitions.
 * 
 ****************************************************************************/
/* Hardware & software constants */
#define PAGESIZE		  4096			/* page size in bytes	*/
#define WORDLEN			  4				  /* word size in bytes	*/
#define MAXPROC 20 
#define MAXPAGES      32
#define MAXUPROCS 8
#define MAX_FREE_POOL 9
#define SWAP_POOL_CAP (MAXUPROCS * 2)
#define STACKSIZE 499


/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR		0x10000000
#define RAMBASESIZE		0x10000004
#define TODLOADDR		  0x1000001C
#define INTERVALTMR		0x10000020	
#define TIMESCALEADDR	0x10000024
#define TIMER_RESET_CONST 0xFFFFFFFF

/* utility constants */
#define	TRUE			    1
#define	FALSE			    0
#define HIDDEN			  static
#define NULL 			    ((void *)0xFFFFFFFF)
#define RESET			    0
#define ACK				    1
#define READY			    1
#define ON         1
#define OK         0
#define FREE     -1
#define INITPROCCNT 0
#define INITSBLOCKCNT 0
#define GETEXCPCODE 0x0000007C
#define MASTER_SEMA4_START 0
#define SWAP_SEMAPHORE_INIT 1
#define SUPP_SEMA4_INIT 1


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
#define PT_START 0x80000000
#define UPROCSTACKPG 0xBFFFF000
#define TOPSTKPAGE 0x20001000
#define PAGE31_ADDR 0xBFFFF000
#define PAGE_TABLE_MAX 31



/* device interrupts */
#define TIMERINT          1
#define INTERVALTMR_LINE  2   
#define DISKINT			  3
#define FLASHINT 		  4
#define NETWINT 		  5
#define PRNTINT 		  6
#define TERMINT			  7

#define DEVINTNUM		  5		  /* interrupt lines used by devices */
#define DEVPERINT		  8		  /* devices per interrupt line */
#define DEVREGLEN		  4		  /* device register field length in bytes, and regs per dev */	
#define DEVREGSIZE	  16 		/* device register size in bytes */

/* Constants for the different device numbers an interrupt may occur on */
#define	DEV0			0				/* constant representing device 0 */
#define	DEV1			1				/* constant representing device 1 */
#define	DEV2			2				/* constant representing device 2 */
#define	DEV3			3				/* constant representing device 3 */
#define	DEV4			4				/* constant representing device 4 */
#define	DEV5			5				/* constant representing device 5 */
#define	DEV6			6				/* constant representing device 6 */
#define	DEV7			7				/* constant representing device 7 */

/*Cause Register Mask to Isolate the correct corresponding to which line the interupt was generated from*/
#define	LINE1MASK		0x00000200		/* constant for setting all bits to 0 in the Cause register except for bit 9 -> line 1 interrupts*/
#define	LINE2MASK		0x00000400		/* constant for setting all bits to 0 in the Cause register except for bit 10 -> line 2 interrupts */
#define	LINE3MASK		0x00000800		/* constant for setting all bits to 0 in the Cause register except for bit 11 -> line 3 interrupts */
#define	LINE4MASK		0x00001000		/* constant for setting all bits to 0 in the Cause register except for bit 12 -> line 4 interrupts */
#define	LINE5MASK		0x00002000		/* constant for setting all bits to 0 in the Cause register except for bit 13 -> line 5 interrupts */
#define	LINE6MASK		0x00004000		/* constant for setting all bits to 0 in the Cause register except for bit 14 -> line 6 interrupts */
#define	LINE7MASK		0x00008000		/* constant for setting all bits to 0 in the Cause register except for bit 15 -> line 7 interrupts */


/* Constants for the different line numbers an interrupt may occur on */
#define	LINE1			1				/* constant representing line 1 */
#define	LINE2			2				/* constant representing line 2 */
#define	LINE3			3				/* constant representing line 3 */
#define	LINE4			4				/* constant representing line 4 */
#define	LINE5			5				/* constant representing line 5 */
#define	LINE6			6				/* constant representing line 6 */
#define	LINE7			7				/* constant representing line 7 */

<<<<<<< HEAD
/* Constant that represents when the first four bits in a terminal device's device register's status field are turned on */
#define	TERM_DEV_STATUSFIELD_ON		0x0F

/* device register field number for non-terminal devices */
#define STATUS			  0
#define COMMAND			  1
#define DATA0			  2
#define DATA1			  3

/* device register field number for terminal devices */
#define RECVSTATUS  	0
#define RECVCOMMAND 	1
#define TRANSTATUS  	2
#define TRANCOMMAND 	3

/* device common STATUS codes */
#define UNINSTALLED		0
#define READY			    1
#define BUSY			    3
#define PRINTCHR            2

/*Printer Device*/
#define PRINTER_LINE_NUM    6

/* device common COMMAND codes */
#define RESET			    0
#define ACK				    1
#define PRINTCHR            2 /* For printer device, specifically */

/* Printer device */
#define PRINTER_LINE_NUM    6
#define PRINTER_READY       1
#define PRINTER_BUSY        3
=======
/* Interupts constants */
#define GETIP   0x0000FE00
#define IPSHIFT 8
#define TRANS_CHAR 5
#define RECVD_CHAR 5
#define TERMSTATUSMASK 0x000000FF
>>>>>>> fafd3b43fb303831ee72b8e7787e119e265fbed2


/* syscall */
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

#define NO_INTS getSTATUS() & (~IECON)
#define YES_INTS getSTATUS() | IECON | IMON

#define GETEXECCODE    0x0000007C
#define LOCALTIMERINT  0x00000200
#define TIMERINTERRUPT 0x00000400
#define DISKINTERRUPT  0x00000800
#define FLASHINTERRUPT 0x00001000
#define NETWINTERRUPT  0x00002000
#define PRINTINTERRUPT 0x00004000
#define TERMINTERRUPT  0x00008000
#define CAUSESHIFT     2

#define SHIFT_VPN      12
#define SHIFT_ASID     6
#define IP_MASK     0x0000FF00     


/* Term and Dev Ops*/
#define OKCHARTRANS  5
#define TRANSMITCHAR 2
#define FLASHREAD  2
#define FLASHWRITE 3
#define DEV_UNITS   8 
#define DEVREGSIZE	    16
#define DEVICE_TYPES     6 
#define OFFSET 3 

/* Dev Semaphores*/
#define FLASHSEM 1
#define PRINTSEM 3
#define TERMSEM 4
#define TERMWRSEM 5

/* Time constants*/
#define TIMESLICE  5000     
#define SECOND     1000000
#define INITTIMER  100000
#define INTIMER  100000UL     
#define PLT_HIGHEST_VAL   0xFFFFFFFFUL

/* Phase 3 Constants*/
#define FLASHADDRSHIFT 8
#define VPN_MASK 0xFFFFF000
#define FRAMEADDRSHIFT 0x20020000
#define VALIDOFF 0xFFFFFDFF
#define EOS	 '\n'
#define PRINTCHR 2
#define TERMTRANSHIFT 8
#define EXCODE_NUM 20

#define TEXT_START    0x800000B0
#define SP_START      0xC0000000

#define MAX_SUPPORTS 9
#define POOLBASEADDR 0x20020000

#define VALIDBITOFF     0xFFFFFDFF
#define V_BIT_SET       0x00000200      /*Bit 9*/
#define D_BIT_SET       0x00000400      /*Bit 10*/


#define TERMINAL_STATUS_NOT_INSTALLED   0
#define TERMINAL_STATUS_READY   1
#define TERMINAL_STATUS_TRANSMITTED 5

#define TERMINAL_COMMAND_TRANSMITCHAR   2
#define TERMINAL_CHAR_SHIFT 8
#define TERMINAL_STATUS_MASK    0xFF

#define TERMINAL_STATUS_RECEIVED 5

#define P_BIT_MASK 0x80000000
#define BLOCK_SHIFT 8


<<<<<<< HEAD
/*Macro to get the index of the deviceSema4s array*/
/*Shift each line's devices into 8-slot region. If it's a terminal device (line_no 7), have to see whether it's transimission or receive*/
/*Otherwise, all other lines just use device 0-7*/
#define GET_DEV_INDEX(dev_num,line_no,isReceive)\
    (((line_no - 3) * 8) + ((line_no == 7) ? ((dev_num) * 2 + (isReceive)) : (dev_num)))



#define CONST1  1
#define CONST3  3

#define MODEXCEPTION 1


/* Phase 1: Additional constants */
#define MAXPROC 20
#define MAXPROC_SEM 22
#define SMALLEST_ADDR 0x00000000
#define LARGEST_ADDR  0x0FFFFFFF

/* Phase 2: Additional constants */

/* Related to the Pseudo-Clock: This is the max number of external (sub)devices in UMPS3, plus one additional semaphore */
#define MAXDEVICECNT	49

/*Address of top of the Nucleus stack page. Stack will start at this address and expand downward as data is pushed onto it*/
#define TOPSTKPAGE   0x20001000

/*interval timer intialized to 100ms in main() of initial.c*/
#define INITTIMER       100000

/*Pseudo-clock index in the device semaphores list*/
#define PSEUDOCLOCKIDX  (MAXDEVICECNT-1)

#define PSEUDOCLOCKSEM4INIT 0

/*Initial values for process count, softBlockCount, device semaphores, accumulated time for a process that has been created*/
#define INITSBLOCKCNT   0   /*initial soft block count*/
#define INITPROCCNT     0   /*initial process count*/
#define INITACCTIME     0   /*initial accumulated time for a process that has been created*/
#define INITDEVICESEM   0   /*initial value for device semaphore*/

#define SCHED_TIME_SLICE 5000  /*time slice for scheduler set to default 5ms*/
#define FIRSTDEVIDX 0

#define WORDSIZE     4


/*Phase 3 addresses*/
#define TEXT_START    0x800000B0
#define SP_START      0xC0000000

#define STACKSIZE   499
#define ID_SHIFT    6

/* Status Register Bitmask Constants (Processor State) */
#define STATUS_ALL_OFF   0x0  /* Clears all bits in the Status register (useful for initializing or bitwise-OR operations) */
#define STATUS_IE_ENABLE 0x00000004  /* Enables global interrupts (IEp, bit 2 = 1) after LDST */
#define STATUS_PLT_ON    0x08000000  /* Enables the Processor Local Timer (PLT) (TE, bit 27 = 1) */
#define STATUS_INT_ON    0x0000FF00  /* Enables all external interrupts by setting the Interrupt Mask bits */
#define STATUS_USERPON	 0x00000008	/* constant for setting the user-mode on after LDST (i.e., KUp (bit 3) = 1) */
#define	STATUS_IECOFF	 0xFFFFFFFE	/* constant for disabling the global interrupt bit (i.e., IEc (bit 0) = 0) */
#define STATUS_IECON	 0x00000001	/* constant for enabling the global interrupt bit (i.e., IEc (bit 0) = 1) */

#define INT_OFF     getSTATUS() & (STATUS_IECOFF)
#define INT_ON      getSTATUS() | STATUS_IECON | STATUS_INT_ON

#define	RESINSTRCODE	 0xFFFFFF28


#define STATUS_KUc_SHIFT  1  /* KUc bit is bit 1 in the Status register */
#define STATUS_KUc_MASK   0x1  /* Mask for extracting the bit (0000...0001) */
#define USER_MODE         0x1  /* Value when in user mode */
#define KERNEL_MODE       0x0  /* Value when in kernel mode */


#define SYS1              1
#define SYS2              2
#define SYS3              3
#define SYS4              4
#define SYS5              5
#define SYS6              6
#define SYS7              7
#define SYS8              8
#define SYS9              9
#define SYS10             10
#define SYS11             11
#define SYS12             12
#define SYS13             13

/* Constants for returning values in v0 to the caller */
#define ERRCONST		-1			/* constant denoting an error occurred in the caller's request */
#define SUCCESS	        0			/* constant denoting that the caller's request completed successfully */

/**/
#define LARGETIME        0xFFFFFFFF
#define INITIAL_TIME     0

#define NULL_PTR_ERROR   -1
#define NO_INTERRUPTS    -1
#define INTERRUPT_BITMASK_INITIAL  1

#define SEM4BLOCKED     0
#define OFFSET          3
#define MAXSHAREIODEVS  48

#define INDEXCLOCK      (MAXDEVICECNT - 1)

#define MAXUPROCESS     8 /*max number of user processes can be run at any point in time*/
#define MAXFRAMES       (MAXUPROCESS * 2)

#define VALIDBITOFF     0xFFFFFDFF
#define V_BIT_SET       0x00000200      /*Bit 9*/
#define D_BIT_SET       0x00000400      /*Bit 10*/

#define FREEFRAME       -1

#define LINENUM7        7

/*Cause Register Constansts (used in gen_exception_handler in initial.c)*/
#define CAUSESHIFT      2       /**/
#define GETEXCPCODE     0x0000007C /*set Cause register bits to 0 except for ExcCode field*/
#define INTCONST		0			/* exception code signaling an interrupt occurred */
#define TLBCONST		3			/* upper bound on the exception codes that signal a TLB exception occurred */
#define SYSCONST		8			/* exception code signaling a SYSCALL occurred */


/* operations */
#define	MIN(A,B)		((A) < (B) ? A : B)
#define MAX(A,B)		((A) < (B) ? B : A)
#define	ALIGNED(A)		(((unsigned)A & 0x3) == 0)

/* Terminal Device Codes */
#define TERMINAL_LINE_NUM   7

#define TERMINAL_STATUS_NOT_INSTALLED   0
#define TERMINAL_STATUS_READY   1
#define TERMINAL_STATUS_TRANSMITTED 5
#define TERMINAL_STATUS_RECEIVED    5

#define TERMINAL_COMMAND_TRANSMITCHAR   2

#define TERMINAL_CHAR_SHIFT 8
#define TERMINAL_STATUS_MASK    0xFF

/* Macro to load the Interval Timer */
=======
#define EXCSTATE ((state_t *) BIOSDATAPAGE)
>>>>>>> fafd3b43fb303831ee72b8e7787e119e265fbed2
#define LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 
#define STCK(T) ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))
#define IP(C) ((C & 0x0000FF00) >> 8)



#endif

