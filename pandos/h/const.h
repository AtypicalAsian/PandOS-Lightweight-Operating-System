#ifndef CONSTS
#define CONSTS

/**************************************************************************** 
 *
 * This header file contains utility constants & macro definitions.
 * 
 ****************************************************************************/

/* Hardware & software constants */
#define PAGESIZE		  4096			/* page size in bytes	*/
#define WORDLEN			  4				  /* word size in bytes	*/


/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR		0x10000000
#define RAMBASESIZE		0x10000004
#define TODLOADDR		  0x1000001C
#define INTERVALTMR		0x10000020	
#define TIMESCALEADDR	0x10000024


/* utility constants */
#define	TRUE			    1
#define	FALSE			    0
#define HIDDEN			  static
#define EOS				    '\0'

#define NULL 			    ((void *)0xFFFFFFFF)

/* device interrupts */
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

/* device common COMMAND codes */
#define RESET			    0
#define ACK				    1

/* Memory related constants */
#define KSEG0           0x00000000
#define KSEG1           0x20000000
#define KSEG2           0x40000000
#define KUSEG           0x80000000
#define RAMSTART        0x20000000
#define BIOSDATAPAGE    0x0FFFF000
#define	PASSUPVECTOR	  0x0FFFF900

/* Exceptions related constants */
#define	PGFAULTEXCEPT	  0
#define GENERALEXCEPT	  1

#define VPNSHIFT      12
#define VPNMASK         0xFFFFF000
#define POOLBASEADDR      0x20020000 /*Base address of the swap pool (used to calc frame address)*/
#define ENTRYLO_PFN_MASK  0x3FFFF000



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


/* Status Register Bitmask Constants (Processor State) */
#define STATUS_ALL_OFF   0x0  /* Clears all bits in the Status register (useful for initializing or bitwise-OR operations) */
#define STATUS_IE_ENABLE 0x00000004  /* Enables global interrupts (IEp, bit 2 = 1) after LDST */
#define STATUS_PLT_ON    0x08000000  /* Enables the Processor Local Timer (PLT) (TE, bit 27 = 1) */
#define STATUS_INT_ON    0x0000FF00  /* Enables all external interrupts by setting the Interrupt Mask bits */
#define STATUS_USERPON	 0x00000008	/* constant for setting the user-mode on after LDST (i.e., KUp (bit 3) = 1) */
#define	STATUS_IECOFF	 0xFFFFFFFE	/* constant for disabling the global interrupt bit (i.e., IEc (bit 0) = 0) */
#define STATUS_IECON	 0x00000001	/* constant for enabling the global interrupt bit (i.e., IEc (bit 0) = 1) */

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

/* Macro to load the Interval Timer */
#define LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 

/* Macro to read the TOD clock */
#define STCK(T) ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))

#endif