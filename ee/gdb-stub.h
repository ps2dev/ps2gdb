/*
  _____     ___ ____
   ____|   |    ____|      PSX2 OpenSource Project
  |     ___|   |____       (C)2003, Ray Donnelly ( rdonnelly@polygoons.com )
  --------------------------------------------------------------------------
  gdb-stub.h               PS2 REMOTE GDB STUB USING TCP
*/

#ifndef __ASM_MIPS_GDB_STUB_H
#define __ASM_MIPS_GDB_STUB_H


/*
 * important register numbers - TODO :: MAKE SURE THESE ARE STILL CORRECT (IT WAS 8 YEARS AGO!)
 */

#define REG_EPC			37
#define REG_FP			72
#define REG_SP			29

/*
 * Stack layout for the GDB exception handler
 * Derived from the stack layout described in asm-mips/stackframe.h
 *
 * The first PTRSIZE*5 bytes are argument save space for C subroutines.
 */
#define NUMREGS				96
#define GPR_SIZE			16									// Saves 16 bytes per register regardless of actual size.
#define PTRSIZE				4

#define GDB_FR_REG0			(GPR_SIZE*5)						/* 0 */
#define GDB_FR_REG1			((GDB_FR_REG0) + GPR_SIZE)			/* 1 */
#define GDB_FR_REG2			((GDB_FR_REG1) + GPR_SIZE)			/* 2 */
#define GDB_FR_REG3			((GDB_FR_REG2) + GPR_SIZE)			/* 3 */
#define GDB_FR_REG4			((GDB_FR_REG3) + GPR_SIZE)			/* 4 */
#define GDB_FR_REG5			((GDB_FR_REG4) + GPR_SIZE)			/* 5 */
#define GDB_FR_REG6			((GDB_FR_REG5) + GPR_SIZE)			/* 6 */
#define GDB_FR_REG7			((GDB_FR_REG6) + GPR_SIZE)			/* 7 */
#define GDB_FR_REG8			((GDB_FR_REG7) + GPR_SIZE)			/* 8 */
#define GDB_FR_REG9	        ((GDB_FR_REG8) + GPR_SIZE)			/* 9 */
#define GDB_FR_REG10		((GDB_FR_REG9) + GPR_SIZE)			/* 10 */
#define GDB_FR_REG11		((GDB_FR_REG10) + GPR_SIZE)			/* 11 */
#define GDB_FR_REG12		((GDB_FR_REG11) + GPR_SIZE)			/* 12 */
#define GDB_FR_REG13		((GDB_FR_REG12) + GPR_SIZE)			/* 13 */
#define GDB_FR_REG14		((GDB_FR_REG13) + GPR_SIZE)			/* 14 */
#define GDB_FR_REG15		((GDB_FR_REG14) + GPR_SIZE)			/* 15 */
#define GDB_FR_REG16		((GDB_FR_REG15) + GPR_SIZE)			/* 16 */
#define GDB_FR_REG17		((GDB_FR_REG16) + GPR_SIZE)			/* 17 */
#define GDB_FR_REG18		((GDB_FR_REG17) + GPR_SIZE)			/* 18 */
#define GDB_FR_REG19		((GDB_FR_REG18) + GPR_SIZE)			/* 19 */
#define GDB_FR_REG20		((GDB_FR_REG19) + GPR_SIZE)			/* 20 */
#define GDB_FR_REG21		((GDB_FR_REG20) + GPR_SIZE)			/* 21 */
#define GDB_FR_REG22		((GDB_FR_REG21) + GPR_SIZE)			/* 22 */
#define GDB_FR_REG23		((GDB_FR_REG22) + GPR_SIZE)			/* 23 */
#define GDB_FR_REG24		((GDB_FR_REG23) + GPR_SIZE)			/* 24 */
#define GDB_FR_REG25		((GDB_FR_REG24) + GPR_SIZE)			/* 25 */
#define GDB_FR_REG26		((GDB_FR_REG25) + GPR_SIZE)			/* 26 */
#define GDB_FR_REG27		((GDB_FR_REG26) + GPR_SIZE)			/* 27 */
#define GDB_FR_REG28		((GDB_FR_REG27) + GPR_SIZE)			/* 28 */
#define GDB_FR_REG29		((GDB_FR_REG28) + GPR_SIZE)			/* 29 */
#define GDB_FR_REG30		((GDB_FR_REG29) + GPR_SIZE)			/* 30 */
#define GDB_FR_REG31		((GDB_FR_REG30) + GPR_SIZE)			/* 31 */

/*
 * Saved special registers... TODORMD :: Move all ps2 specific ones to the very end.
 */
#define GDB_FR_LO			((GDB_FR_REG31) + GPR_SIZE)			/* 32 */			// I've moved this...
#define GDB_FR_HI			((GDB_FR_LO) + GPR_SIZE)			/* 33 */			// ...and this...
#define GDB_FR_STATUS		((GDB_FR_HI) + GPR_SIZE)			/* 34 */
#define GDB_FR_BADVADDR		((GDB_FR_STATUS) + 4)				/* 35 */
#define GDB_FR_CAUSE		((GDB_FR_BADVADDR) + 4)				/* 36 */
#define GDB_FR_EPC			((GDB_FR_CAUSE) + 4)				/* 37 */

/*
 * Saved floating point registers
 */
#define GDB_FR_FPR0			((GDB_FR_EPC) + 4)					/* 38 */
#define GDB_FR_FPR1			((GDB_FR_FPR0) + 4)					/* 39 */
#define GDB_FR_FPR2			((GDB_FR_FPR1) + 4)					/* 40 */
#define GDB_FR_FPR3			((GDB_FR_FPR2) + 4)					/* 41 */
#define GDB_FR_FPR4			((GDB_FR_FPR3) + 4)					/* 42 */
#define GDB_FR_FPR5			((GDB_FR_FPR4) + 4)					/* 43 */
#define GDB_FR_FPR6			((GDB_FR_FPR5) + 4)					/* 44 */
#define GDB_FR_FPR7			((GDB_FR_FPR6) + 4)					/* 45 */
#define GDB_FR_FPR8			((GDB_FR_FPR7) + 4)					/* 46 */
#define GDB_FR_FPR9			((GDB_FR_FPR8) + 4)					/* 47 */
#define GDB_FR_FPR10		((GDB_FR_FPR9) + 4)					/* 48 */
#define GDB_FR_FPR11		((GDB_FR_FPR10) + 4)				/* 49 */
#define GDB_FR_FPR12		((GDB_FR_FPR11) + 4)				/* 50 */
#define GDB_FR_FPR13		((GDB_FR_FPR12) + 4)				/* 51 */
#define GDB_FR_FPR14		((GDB_FR_FPR13) + 4)				/* 52 */
#define GDB_FR_FPR15		((GDB_FR_FPR14) + 4)				/* 53 */
#define GDB_FR_FPR16		((GDB_FR_FPR15) + 4)				/* 54 */
#define GDB_FR_FPR17		((GDB_FR_FPR16) + 4)				/* 55 */
#define GDB_FR_FPR18		((GDB_FR_FPR17) + 4)				/* 56 */
#define GDB_FR_FPR19		((GDB_FR_FPR18) + 4)				/* 57 */
#define GDB_FR_FPR20		((GDB_FR_FPR19) + 4)				/* 58 */
#define GDB_FR_FPR21		((GDB_FR_FPR20) + 4)				/* 59 */
#define GDB_FR_FPR22		((GDB_FR_FPR21) + 4)				/* 60 */
#define GDB_FR_FPR23		((GDB_FR_FPR22) + 4)				/* 61 */
#define GDB_FR_FPR24		((GDB_FR_FPR23) + 4)				/* 62 */
#define GDB_FR_FPR25		((GDB_FR_FPR24) + 4)				/* 63 */
#define GDB_FR_FPR26		((GDB_FR_FPR25) + 4)				/* 64 */
#define GDB_FR_FPR27		((GDB_FR_FPR26) + 4)				/* 65 */
#define GDB_FR_FPR28		((GDB_FR_FPR27) + 4)				/* 66 */
#define GDB_FR_FPR29		((GDB_FR_FPR28) + 4)				/* 67 */
#define GDB_FR_FPR30		((GDB_FR_FPR29) + 4)				/* 68 */
#define GDB_FR_FPR31		((GDB_FR_FPR30) + 4)				/* 69 */

#define GDB_FR_FSR			((GDB_FR_FPR31) + 4)				/* 70 */
#define GDB_FR_FIR			((GDB_FR_FSR) + 4)					/* 71 */

#define GDB_FR_FRP			((GDB_FR_FIR) + 4)					/* 72 */
#define GDB_FR_DUMMY		((GDB_FR_FRP) + 4)					/* 73, unused ??? */

/*
 * Again, CP0 registers
 */
#define GDB_FR_CP0_INDEX	((GDB_FR_DUMMY) + 4)				/* 74 */
#define GDB_FR_CP0_RANDOM	((GDB_FR_CP0_INDEX) + 4)			/* 75 */
#define GDB_FR_CP0_ENTRYLO0	((GDB_FR_CP0_RANDOM) + 4)			/* 76 */
#define GDB_FR_CP0_ENTRYLO1	((GDB_FR_CP0_ENTRYLO0) + 4)			/* 77 */
#define GDB_FR_CP0_CONTEXT	((GDB_FR_CP0_ENTRYLO1) + 4)			/* 78 */
#define GDB_FR_CP0_PAGEMASK	((GDB_FR_CP0_CONTEXT) + 4)			/* 79 */
#define GDB_FR_CP0_WIRED	((GDB_FR_CP0_PAGEMASK) + 4)			/* 80 */
#define GDB_FR_CP0_REG7		((GDB_FR_CP0_WIRED) + 4)			/* 81 */
#define GDB_FR_CP0_REG8		((GDB_FR_CP0_REG7) + 4)				/* 82 */
#define GDB_FR_CP0_REG9		((GDB_FR_CP0_REG8) + 4)				/* 83 */
#define GDB_FR_CP0_ENTRYHI	((GDB_FR_CP0_REG9) + 4)				/* 84 */
#define GDB_FR_CP0_REG11	((GDB_FR_CP0_ENTRYHI) + 4)			/* 85 */
#define GDB_FR_CP0_REG12	((GDB_FR_CP0_REG11) + 4)			/* 86 */
#define GDB_FR_CP0_REG13	((GDB_FR_CP0_REG12) + 4)			/* 87 */
#define GDB_FR_CP0_REG14	((GDB_FR_CP0_REG13) + 4)			/* 88 */
#define GDB_FR_CP0_PRID		((GDB_FR_CP0_REG14) + 4)			/* 89 */

/*
 * Ps2 registers.
 */
#define GDB_FR_CP1_ACC		((GDB_FR_CP0_PRID) + 4)				/* 90 */
#define	GDB_FR_SHFT_AMNT	((GDB_FR_CP1_ACC) + 4)				/* 91 */
#define	GDB_FR_LO1			((GDB_FR_SHFT_AMNT) + 4)			/* 92 */
#define	GDB_FR_HI1			((GDB_FR_LO1) + 4)					/* 93 */
#define GDB_FR_CP0_DEPC		((GDB_FR_HI1) + 4)					/* 94 */
#define GDB_FR_CP0_PERFCNT	((GDB_FR_CP0_DEPC) + 4)				/* 95 */

//#define GDB_FR_SIZE			((((GDB_FR_CP0_PERFCNT) + 4) + (PTRSIZE-1)) & ~(PTRSIZE-1))
// With alignment.
#define GDB_FR_SIZE			((((GDB_FR_CP0_PERFCNT) + 12) + (PTRSIZE-1)) & ~(PTRSIZE-1))

#ifndef __ASSEMBLY__

/*
 * This is the same as above, but for the high-level
 * part of the GDB stub.
 */

typedef struct _gdb_regs_ps2 {
	/*
	 * Pad bytes for argument save space on the stack
	 * 20/40/80 Bytes for 32/64/128 bit code
	 */
	unsigned int pad0 __attribute__(( mode(TI) ));
	unsigned int pad1 __attribute__(( mode(TI) ));
	unsigned int pad2 __attribute__(( mode(TI) ));
	unsigned int pad3 __attribute__(( mode(TI) ));
	unsigned int pad4 __attribute__(( mode(TI) ));

	/*
	 * saved main processor registers
	 */
	unsigned int reg0 __attribute__(( mode(TI) ));
	unsigned int reg1 __attribute__(( mode(TI) ));
	unsigned int reg2 __attribute__(( mode(TI) ));
	unsigned int reg3 __attribute__(( mode(TI) ));
	unsigned int reg4 __attribute__(( mode(TI) ));
	unsigned int reg5 __attribute__(( mode(TI) ));
	unsigned int reg6 __attribute__(( mode(TI) ));
	unsigned int reg7 __attribute__(( mode(TI) ));
	unsigned int reg8 __attribute__(( mode(TI) ));
	unsigned int reg9 __attribute__(( mode(TI) ));
	unsigned int reg10 __attribute__(( mode(TI) ));
	unsigned int reg11 __attribute__(( mode(TI) ));
	unsigned int reg12 __attribute__(( mode(TI) ));
	unsigned int reg13 __attribute__(( mode(TI) ));
	unsigned int reg14 __attribute__(( mode(TI) ));
	unsigned int reg15 __attribute__(( mode(TI) ));
	unsigned int reg16 __attribute__(( mode(TI) ));
	unsigned int reg17 __attribute__(( mode(TI) ));
	unsigned int reg18 __attribute__(( mode(TI) ));
	unsigned int reg19 __attribute__(( mode(TI) ));
	unsigned int reg20 __attribute__(( mode(TI) ));
	unsigned int reg21 __attribute__(( mode(TI) ));
	unsigned int reg22 __attribute__(( mode(TI) ));
	unsigned int reg23 __attribute__(( mode(TI) ));
	unsigned int reg24 __attribute__(( mode(TI) ));
	unsigned int reg25 __attribute__(( mode(TI) ));
	unsigned int reg26 __attribute__(( mode(TI) ));
	unsigned int reg27 __attribute__(( mode(TI) ));
	unsigned int reg28 __attribute__(( mode(TI) ));
	unsigned int reg29 __attribute__(( mode(TI) ));
	unsigned int reg30 __attribute__(( mode(TI) ));
	unsigned int reg31 __attribute__(( mode(TI) ));					//	32

	/*
	 * Saved special registers - note, the ordering is different for ps2 against vanilla, this is because lo and hi are 128 bit
	 * and in order to keep alignment, I've re-ordered them to be before cp0_status, whilst the pc side expects them to be below.
	 * Note that in struct gdb_regs, the order is as expected.
	 */
	unsigned int lo __attribute__(( mode(TI) ));
	unsigned int hi __attribute__(( mode(TI) ));
	int cp0_status;
	int cp0_badvaddr;
	int cp0_cause;
	int cp0_epc;													//	6

	/*
	 * Saved floating point registers
	 */
	int fpr0,  fpr1,  fpr2,  fpr3,  fpr4,  fpr5,  fpr6,  fpr7;
	int fpr8,  fpr9,  fpr10, fpr11, fpr12, fpr13, fpr14, fpr15;
	int fpr16, fpr17, fpr18, fpr19, fpr20, fpr21, fpr22, fpr23;
	int fpr24, fpr25, fpr26, fpr27, fpr28, fpr29, fpr30, fpr31;		//	32

	int cp1_fsr;
	int cp1_fir;													//	2

	/*
	 * Frame pointer
	 */
	int frame_ptr;		// Eh? Isn't the frame pointer just reg30? (also called s8)
	int dummy;			/* unused */								//	2

	/*
	 * saved cp0 registers -> but not all are saved?
	 */
	int cp0_index;
	int cp0_random;
	int cp0_entrylo0;
	int cp0_entrylo1;
	int cp0_context;
	int cp0_pagemask;
	int cp0_wired;
	int cp0_reg7;				// NOT ACTUALLY SAVED... info
	int cp0_reg8;				// NOT ACTUALLY SAVED... badvaddr
	int cp0_reg9;				// NOT ACTUALLY SAVED... count
	int cp0_entryhi;
	int cp0_reg11;				// NOT ACTUALLY SAVED... compare
	int cp0_reg12;				// NOT ACTUALLY SAVED... status
	int cp0_reg13;				// NOT ACTUALLY SAVED... cause
	int cp0_reg14;				// NOT ACTUALLY SAVED... epc
	int cp0_prid;													//	16

	/*
	 * Ps2 specific registers - I've put these all at the end so the above is as similar as possible to vanilla mips.
	 */
	int cp1_acc;
	int shft_amnt;
	int lo1;
	int hi1;
	int cp0_depc;
	int cp0_perfcnt;												// 6
} gdb_regs_ps2;

// All this because the compiler won't allow any 128 bit arithmetic.
struct gdb_regs {
	/*
	 * Pad bytes for argument save space on the stack
	 * 20/40/80 Bytes for 32/64/128 bit code
	 */
	unsigned int pad0;
	unsigned int pad1;
	unsigned int pad2;
	unsigned int pad3;
	unsigned int pad4;

	/*
	 * saved main processor registers
	 */
	unsigned int reg0;
	unsigned int reg1;
	unsigned int reg2;
	unsigned int reg3;
	unsigned int reg4;
	unsigned int reg5;
	unsigned int reg6;
	unsigned int reg7;
	unsigned int reg8;
	unsigned int reg9;
	unsigned int reg10;
	unsigned int reg11;
	unsigned int reg12;
	unsigned int reg13;
	unsigned int reg14;
	unsigned int reg15;
	unsigned int reg16;
	unsigned int reg17;
	unsigned int reg18;
	unsigned int reg19;
	unsigned int reg20;
	unsigned int reg21;
	unsigned int reg22;
	unsigned int reg23;
	unsigned int reg24;
	unsigned int reg25;
	unsigned int reg26;
	unsigned int reg27;
	unsigned int reg28;		// gp
	unsigned int reg29;		// sp
	unsigned int reg30;		// fp
	unsigned int reg31;		// ra									//	32

	/*
	 * Saved special registers
	 */
	unsigned int cp0_status;
	unsigned int lo;
	unsigned int hi;
	unsigned int cp0_badvaddr;
	unsigned int cp0_cause;
	unsigned int cp0_epc;											//	6

	/*
	 * Saved floating point registers
	 */
	int fpr0,  fpr1,  fpr2,  fpr3,  fpr4,  fpr5,  fpr6,  fpr7;
	int fpr8,  fpr9,  fpr10, fpr11, fpr12, fpr13, fpr14, fpr15;
	int fpr16, fpr17, fpr18, fpr19, fpr20, fpr21, fpr22, fpr23;
	int fpr24, fpr25, fpr26, fpr27, fpr28, fpr29, fpr30, fpr31;		//	32

	int cp1_fsr;
	int cp1_fir;													//	2

	/*
	 * Frame pointer
	 */
	int frame_ptr;		// Eh? Isn't the frame pointer just a general purpose register (i.e. one of reg0...reg31???)
	int dummy;			/* unused */								//	2

	/*
	 * saved cp0 registers -> but not all are saved?
	 */
	int cp0_index;
	int cp0_random;
	int cp0_entrylo0;
	int cp0_entrylo1;
	int cp0_context;
	int cp0_pagemask;
	int cp0_wired;
	int cp0_reg7;				// NOT ACTUALLY SAVED... info
	int cp0_reg8;				// NOT ACTUALLY SAVED... badvaddr
	int cp0_reg9;				// NOT ACTUALLY SAVED... count
	int cp0_entryhi;
	int cp0_reg11;				// NOT ACTUALLY SAVED... compare
	int cp0_reg12;				// NOT ACTUALLY SAVED... status
	int cp0_reg13;				// NOT ACTUALLY SAVED... cause
	int cp0_reg14;				// NOT ACTUALLY SAVED... epc
	int cp0_prid;													//	16

	/*
	 * Ps2 specific registers - I've put these all at the end so the above is as similar as possible to vanilla mips.
	 */
	int cp1_acc;
	int shft_amnt;
	int lo1;
	int hi1;
	int cp0_depc;
	int cp0_perfcnt;												// 6
};

#define SIGHUP			1		// Hangup (POSIX).
#define SIGINT			2		// Interrupt (ANSI).
#define SIGQUIT			3		// Quit (POSIX).
#define SIGILL			4		// Illegal instruction (ANSI).
#define SIGTRAP			5		// Trace trap (POSIX).
#define SIGIOT			6		// IOT trap (4.2 BSD).
#define SIGABRT			SIGIOT	// Abort (ANSI).
#define SIGEMT			7
#define SIGFPE			8		// Floating-point exception (ANSI).
#define SIGKILL			9		// Kill, unblockable (POSIX).
#define SIGBUS			10		// BUS error (4.2 BSD).
#define SIGSEGV			11		// Segmentation violation (ANSI).
#define SIGSYS			12
#define SIGPIPE			13		// Broken pipe (POSIX).
#define SIGALRM			14		// Alarm clock (POSIX).
#define SIGTERM			15		// Termination (ANSI).
#define SIGUSR1			16		// User-defined signal 1 (POSIX).
#define SIGUSR2			17		// User-defined signal 2 (POSIX).
#define SIGCHLD			18		// Child status has changed (POSIX).
#define SIGCLD			SIGCHLD	// Same as SIGCHLD (System V).
#define SIGPWR			19		// Power failure restart (System V).
#define SIGWINCH		20		// Window size change (4.3 BSD, Sun).
#define SIGURG			21		// Urgent condition on socket (4.2 BSD).
#define SIGIO			22		// I/O now possible (4.2 BSD).
#define SIGPOLL			SIGIO	// Pollable event occurred (System V).
#define SIGSTOP			23		// Stop, unblockable (POSIX).
#define SIGTSTP			24		// Keyboard stop (POSIX).
#define SIGCONT			25		// Continue (POSIX).
#define SIGTTIN			26		// Background read from tty (POSIX).
#define SIGTTOU			27		// Background write to tty (POSIX).
#define SIGVTALRM		28		// Virtual alarm clock (4.2 BSD).
#define SIGPROF			29		// Profiling alarm clock (4.2 BSD).
#define SIGXCPU			30		// CPU limit exceeded (4.2 BSD).
#define SIGXFSZ			31		// File size limit exceeded (4.2 BSD).

// These should not be considered constants from userland.
#define SIGRTMIN		32
#define SIGRTMAX		(_NSIG-1)


// Status register bits available in all MIPS CPUs.
#define ST0_IM			0x0000ff00
#define STATUSB_IP0		8
#define STATUSF_IP0		(1   <<  8)
#define STATUSB_IP1		9
#define STATUSF_IP1		(1   <<  9)
#define STATUSB_IP2		10
#define STATUSF_IP2		(1   << 10)
#define STATUSB_IP3		11
#define STATUSF_IP3		(1   << 11)
#define STATUSB_IP4		12
#define STATUSF_IP4		(1   << 12)
#define STATUSB_IP5		13
#define STATUSF_IP5		(1   << 13)
#define STATUSB_IP6		14
#define STATUSF_IP6		(1   << 14)
#define STATUSB_IP7		15
#define STATUSF_IP7		(1   << 15)
#define ST0_DE			0x00010000
#define ST0_CE			0x00020000
#define ST0_CH			0x00040000
#define ST0_SR			0x00100000
#define ST0_BEV			0x00400000
#define ST0_RE			0x02000000
#define ST0_FR			0x04000000
#define ST0_CU			0xf0000000
#define ST0_CU0			0x10000000
#define ST0_CU1			0x20000000
#define ST0_CU2			0x40000000
#define ST0_CU3			0x80000000
#define ST0_XX			0x80000000	/* MIPS IV naming */


// Bitfields and bit numbers in the coprocessor 0 cause register.
// Refer to to your MIPS R4xx0 manual, chapter 5 for explanation.
#define CAUSEB_EXCCODE	2
#define CAUSEF_EXCCODE	(31  <<  2)
#define CAUSEB_IP		8
#define CAUSEF_IP		(255 <<  8)
#define CAUSEB_IP0		8
#define CAUSEF_IP0		(1   <<  8)
#define CAUSEB_IP1		9
#define CAUSEF_IP1		(1   <<  9)
#define CAUSEB_IP2		10
#define CAUSEF_IP2		(1   << 10)
#define CAUSEB_IP3		11
#define CAUSEF_IP3		(1   << 11)
#define CAUSEB_IP4		12
#define CAUSEF_IP4		(1   << 12)
#define CAUSEB_IP5		13
#define CAUSEF_IP5		(1   << 13)
#define CAUSEB_IP6		14
#define CAUSEF_IP6		(1   << 14)
#define CAUSEB_IP7		15
#define CAUSEF_IP7		(1   << 15)
#define CAUSEB_IV		23
#define CAUSEF_IV		(1   << 23)
#define CAUSEB_CE		28
#define CAUSEF_CE		(3   << 28)
#define CAUSEB_BD		31
#define CAUSEF_BD		(1   << 31)

#endif /* !__ASSEMBLY__ */
#endif /* __ASM_MIPS_GDB_STUB_H */
