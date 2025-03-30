#ifndef TYPES
#define TYPES

/**************************************************************************** 
 *
 * This header file contains utility types definitions.
 * 
 ****************************************************************************/

#include "../h/const.h"


typedef signed int cpu_t;
typedef unsigned int memaddr;


/* Device Register */
typedef struct {
	unsigned int d_status;
	unsigned int d_command;
	unsigned int d_data0;
	unsigned int d_data1;
} device_t;

#define t_recv_status		d_status
#define t_recv_command		d_command
#define t_transm_status		d_data0
#define t_transm_command	d_data1

/* Device register type for disks, flash and printers */
typedef struct dtpreg {
	unsigned int status;
	unsigned int command;
	unsigned int data0;
	unsigned int data1;
} dtpreg_t;

/* Device register type for terminals */
typedef struct termreg {
	unsigned int recv_status;
	unsigned int recv_command;
	unsigned int transm_status;
	unsigned int transm_command;
} termreg_t;


typedef union devreg {
	dtpreg_t dtp;
	termreg_t term;
} devreg_t;


/* Bus Register Area */
typedef struct {
	unsigned int rambase;
	unsigned int ramsize;
	unsigned int execbase;
	unsigned int execsize;
	unsigned int bootbase;
	unsigned int bootsize;
	unsigned int todhi;
	unsigned int todlo;
	unsigned int intervaltimer;
	unsigned int timescale;
	unsigned int TLB_Floor_Addr;
	unsigned int inst_dev[DEVINTNUM];
	unsigned int interrupt_dev[DEVINTNUM];
	device_t	devreg[DEVINTNUM * DEVPERINT];
} devregarea_t;


/* Pass Up Vector */
typedef struct passupvector {
    unsigned int tlb_refll_handler;
    unsigned int tlb_refll_stackPtr;
    unsigned int execption_handler;
    unsigned int exception_stackPtr;
} passupvector_t;


/* single page table entry */
typedef struct pte_entry_t {
	unsigned int entryHI;
	unsigned int entryLO;
} pte_entry_t;

/* process context type */
typedef struct context_t {
	/* process context fields */
	unsigned int 	c_stackPtr,		/* stack pointer value */
					c_status,		/* status reg value */
					c_pc;			/* PC address */
} context_t;


#define STATEREGNUM	31
typedef struct state_t {
	unsigned int	s_entryHI;
	unsigned int	s_cause;
	unsigned int	s_status;
	unsigned int 	s_pc;
	int	 			s_reg[STATEREGNUM];

} state_t, *state_PTR;


/*define the swap pool struct*/
typedef struct swap_pool{
	int		asid; /*unique identifier*/
	pte_entry_t		*ownerEntry; /* a ptr to the matching PT entry in the PT of the owner process */
	int		pg_number; /*page number*/
} swap_pool_t;

typedef struct support_t {
	int				sup_asid;				/* process Id (asid) */
	state_t			sup_exceptState[2];		/* stored except states */
	context_t		sup_exceptContext[2];	/* pass up contexts */
	pte_entry_t		sup_privatePgTbl[32];	/* the user process's page table */
	int				sup_stackTLB[500];		/* the stack area for the process' TLB exception handler */
	int				sup_stackGen[500];		/* the stack area for the process' general exception handler */
} support_t;

/* Process Control Block (PCB) type */
typedef struct pcb_t {
    /* Process queue fields */
    struct pcb_t *p_next;  /* Pointer to next entry */
    struct pcb_t *p_prev;  /* Pointer to previous entry */

    /* Process tree fields */
    struct pcb_t *p_prnt;  /* Pointer to parent process */
    struct pcb_t *p_child; /* Pointer to first child process */
    struct pcb_t *p_sib;   /* Pointer to sibling process */
	struct pcb_t *p_lsib;  /* Pointer to left sibling (for doubly linked list)*/
	struct pcb_t *p_rsib;  /* Pointer to right sibling (for doubly linked list)*/
	

    /* Process status information */
    state_t p_s;           /* Processor state */
    cpu_t p_time;          /* CPU time used by the process */
    int *p_semAdd;         /* Pointer to semaphore on which the process is blocked */

    /* Support layer information */
    support_t *p_supportStruct; /* Pointer to support structure */
} pcb_t, *pcb_PTR;

typedef struct semd_t {
	struct	semd_t	*s_next;	/* next element on the ASL */
	int				*s_semAdd;	/* pointer to the semaphore*/
	pcb_t			*s_procQ;	/* process queue */
} semd_t, *semd_PTR;

#define	s_at	s_reg[0]
#define	s_v0	s_reg[1]
#define s_v1	s_reg[2]
#define s_a0	s_reg[3]
#define s_a1	s_reg[4]
#define s_a2	s_reg[5]
#define s_a3	s_reg[6]
#define s_t0	s_reg[7]
#define s_t1	s_reg[8]
#define s_t2	s_reg[9]
#define s_t3	s_reg[10]
#define s_t4	s_reg[11]
#define s_t5	s_reg[12]
#define s_t6	s_reg[13]
#define s_t7	s_reg[14]
#define s_s0	s_reg[15]
#define s_s1	s_reg[16]
#define s_s2	s_reg[17]
#define s_s3	s_reg[18]
#define s_s4	s_reg[19]
#define s_s5	s_reg[20]
#define s_s6	s_reg[21]
#define s_s7	s_reg[22]
#define s_t8	s_reg[23]
#define s_t9	s_reg[24]
#define s_gp	s_reg[25]
#define s_sp	s_reg[26]
#define s_fp	s_reg[27]
#define s_ra	s_reg[28]
#define s_HI	s_reg[29]
#define s_LO	s_reg[30]


#endif
