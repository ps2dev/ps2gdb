/*
  _____     ___ ____
   ____|   |    ____|      PSX2 OpenSource Project
  |     ___|   |____       (C)2003, Ray Donnelly ( rdonnelly@polygoons.com )
                           Also, Tony Saveski ( t_saveski@yahoo.com )
  --------------------------------------------------------------------------
  gdb-low.S                PS2 REMOTE GDB STUB USING TCP
*/

#ifndef R5900_REGS_H
#define R5900_REGS_H

// MIPS CPU Registsers
#define zero	$0		// Always 0
#define at		$1		// Assembler temporary
#define v0		$2		// Function return
#define v1		$3		//
#define a0		$4		// Function arguments
#define a1		$5
#define a2		$6
#define a3		$7
#define t0		$8		// Temporaries. No need
#define t1		$9		// to preserve in your
#define t2		$10		// functions.
#define t3		$11
#define t4		$12
#define t5		$13
#define t6		$14
#define t7		$15
#define s0		$16		// Saved Temporaries.
#define s1		$17		// Make sure to restore
#define s2		$18		// to original value
#define s3		$19		// if your function
#define s4		$20		// changes their value.
#define s5		$21
#define s6		$22
#define s7		$23
#define t8		$24		// More Temporaries.
#define t9		$25
#define k0		$26		// Reserved for Kernel
#define k1		$27
#define gp		$28		// Global Pointer
#define sp		$29		// Stack Pointer
#define fp		$30		// Frame Pointer
#define ra		$31		// Function Return Address

// COP0
#define Index    $0  // Index into the TLB array 
#define Random   $1  // Randomly generated index into the TLB array 
#define EntryLo0 $2  // Low-order portion of the TLB entry for..
#define EntryLo1 $3  // Low-order portion of the TLB entry for
#define Context  $4  // Pointer to page table entry in memory
#define PageMask $5  // Control for variable page size in TLB entries 
#define Wired    $6  // Controls the number of fixed ("wired") TLB entries
#define BadVAddr $8  // Address for the most recent address-related exception
#define Count    $9  // Processor cycle count
#define EntryHi  $10 // High-order portion of the TLB entry
#define Compare  $11 // Timer interrupt control
#define Status   $12 // Processor status and control
#define Cause    $13 // Cause of last general exception
#define EPC      $14 // Program counter at last exception
#define PRId     $15 // Processor identification and revision
#define Config   $16 // Configuration register
#define LLAddr   $17 // Load linked address
#define WatchLo  $18 // Watchpoint address Section 6.25 on
#define WatchHi  $19 // Watchpoint control
#define Debug    $23 // EJTAG Debug register
#define DEPC     $24 // Program counter at last EJTAG debug exception
#define PerfCnt  $25 // Performance counter interface
#define ErrCtl   $26 // Parity/ECC error control and status
#define CacheErr $27 // Cache parity error control and status
#define TagLo    $28 // Low-order portion of cache tag interface
#define TagHi    $29 // High-order portion of cache tag interface
#define ErrorPC  $30 // Program counter at last error
#define DEASVE   $31 // EJTAG debug exception save register

/*
 * Coprocessor 0 register names
 */
#define CP0_INDEX $0
#define CP0_RANDOM $1
#define CP0_ENTRYLO0 $2
#define CP0_ENTRYLO1 $3
#define CP0_CONF $3
#define CP0_CONTEXT $4
#define CP0_PAGEMASK $5
#define CP0_WIRED $6
#define CP0_INFO $7
#define CP0_BADVADDR $8
#define CP0_COUNT $9
#define CP0_ENTRYHI $10
#define CP0_COMPARE $11
#define CP0_STATUS $12
#define CP0_CAUSE $13
#define CP0_EPC $14
#define CP0_PRID $15
#define CP0_CONFIG $16
#define CP0_LLADDR $17
#define CP0_WATCHLO $18
#define CP0_WATCHHI $19
#define CP0_XCONTEXT $20
#define CP0_FRAMEMASK $21
#define CP0_DIAGNOSTIC $22
#define CP0_DEBUG $23
#define CP0_DEPC $24
#define CP0_PERFORMANCE $25
#define CP0_ECC $26
#define CP0_CACHEERR $27
#define CP0_TAGLO $28
#define CP0_TAGHI $29
#define CP0_ERROREPC $30
#define CP0_DESAVE $31

/*
 * Coprocessor 1 (FPU) register names
 */
#define CP1_REVISION   $0
#define CP1_STATUS     $31

#define ST0_CH			0x00040000
#define ST0_SR			0x00100000
#define ST0_TS			0x00200000
#define ST0_BEV			0x00400000
#define ST0_RE			0x02000000
#define ST0_FR			0x04000000
#define ST0_CU			0xf0000000
#define ST0_CU0			0x10000000
#define ST0_CU1			0x20000000
#define ST0_CU2			0x40000000
#define ST0_CU3			0x80000000
#define ST0_XX			0x80000000	/* MIPS IV naming */

// Floating point (cop1) floating point general purpose registers.
#define fp0      $f0
#define fp1      $f1
#define fp2      $f2
#define fp3      $f3
#define fp4      $f4
#define fp5      $f5
#define fp6      $f6
#define fp7      $f7
#define fp8      $f8
#define fp9      $f9
#define fp10     $f10
#define fp11     $f11
#define fp12     $f12
#define fp13     $f13
#define fp14     $f14
#define fp15     $f15
#define fp16     $f16
#define fp17     $f17
#define fp18     $f18
#define fp19     $f19
#define fp20     $f20
#define fp21     $f21
#define fp22     $f22
#define fp23     $f23
#define fp24     $f24
#define fp25     $f25
#define fp26     $f26
#define fp27     $f27
#define fp28     $f28
#define fp29     $f29
#define fp30     $f30
#define fp31     $f31

#define fcr0     $0			// Floating point Implementation / Revision register.
#define fcr31    $31		// Floating point Control / Status register.
							// Floating point Accumulator (ACC) can't be peeked at except via madd.s.

#endif // R5900_REGS_H
