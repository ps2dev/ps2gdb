/*
  _____     ___ ____
   ____|   |    ____|      PSX2 OpenSource Project
  |     ___|   |____       (C)2003, Ray Donnelly ( rdonnelly@polygoons.com )
  --------------------------------------------------------------------------
  gdb-low.S                PS2 REMOTE GDB STUB USING TCP
*/

#define __ASSEMBLY__

#include "r5900_regs.h"			// For nicer names.
#include "gdb-stub.h"			// For frame pointer offsets.

		.text
		.align	5
		.global	faked_ra
		.global	gdbstub_ps2_regs
		.global	trap_low
		.type	trap_low,@function
		.ent	trap_low

trap_low:
		.set	push
		.set	noreorder
		.set	noat

		la		k0,gdbstub_ps2_regs
		sq		v0,GDB_FR_REG2(k0)
		nop

		// Save the cop0, hi and lo registers.
		mfc0	v0,CP0_STATUS
		sw		v0,GDB_FR_STATUS(k0)
		mfc0	v0,CP0_CAUSE
		sw		v0,GDB_FR_CAUSE(k0)
		mfc0	v0,CP0_EPC
		sw		v0,GDB_FR_EPC(k0)
		mfc0	v0,CP0_BADVADDR				// Not restored.
		sw		v0,GDB_FR_BADVADDR(k0)
		pmfhi	v0
		sq		v0,GDB_FR_HI(k0)
		pmflo	v0
		sq		v0,GDB_FR_LO(k0)

		// Integer registers.
		sq		zero,GDB_FR_REG0(k0)		// Yeah, whatever!
		sq		at,GDB_FR_REG1(k0)
		sq		v1,GDB_FR_REG3(k0)
		sq		a0,GDB_FR_REG4(k0)
		sq		a1,GDB_FR_REG5(k0)
		sq		a2,GDB_FR_REG6(k0)
		sq		a3,GDB_FR_REG7(k0)
		sq		t0,GDB_FR_REG8(k0)
		sq		t1,GDB_FR_REG9(k0)
		sq		t2,GDB_FR_REG10(k0)
		sq		t3,GDB_FR_REG11(k0)
		sq		t4,GDB_FR_REG12(k0)
		sq		t5,GDB_FR_REG13(k0)
		sq		t6,GDB_FR_REG14(k0)
		sq		t7,GDB_FR_REG15(k0)
		sq		s0,GDB_FR_REG16(k0)
		sq		s1,GDB_FR_REG17(k0)
		sq		s2,GDB_FR_REG18(k0)
		sq		s3,GDB_FR_REG19(k0)
		sq		s4,GDB_FR_REG20(k0)
		sq		s5,GDB_FR_REG21(k0)
		sq		s6,GDB_FR_REG22(k0)
		sq		s7,GDB_FR_REG23(k0)
		sq		t8,GDB_FR_REG24(k0)
		sq		t9,GDB_FR_REG25(k0)
		// k0 already saved.
		// k1 already saved.
		sq		gp,GDB_FR_REG28(k0)
		sq		sp,GDB_FR_REG29(k0)
		sq		fp,GDB_FR_REG30(k0)
		sq		ra,GDB_FR_REG31(k0)

		// Floating point registers
		mfc0	v0,CP0_STATUS			// FPU enabled?
		srl		v0,v0,16
		andi	v0,v0,(ST0_CU1 >> 16)
		beqz	v0,2f					// Disabled, skip
		nop

		swc1	$0,GDB_FR_FPR0(k0)
		swc1	$1,GDB_FR_FPR1(k0)
		swc1	$2,GDB_FR_FPR2(k0)
		swc1	$3,GDB_FR_FPR3(k0)
		swc1	$4,GDB_FR_FPR4(k0)
		swc1	$5,GDB_FR_FPR5(k0)
		swc1	$6,GDB_FR_FPR6(k0)
		swc1	$7,GDB_FR_FPR7(k0)
		swc1	$8,GDB_FR_FPR8(k0)
		swc1	$9,GDB_FR_FPR9(k0)
		swc1	$10,GDB_FR_FPR10(k0)
		swc1	$11,GDB_FR_FPR11(k0)
		swc1	$12,GDB_FR_FPR12(k0)
		swc1	$13,GDB_FR_FPR13(k0)
		swc1	$14,GDB_FR_FPR14(k0)
		swc1	$15,GDB_FR_FPR15(k0)
		swc1	$16,GDB_FR_FPR16(k0)
		swc1	$17,GDB_FR_FPR17(k0)
		swc1	$18,GDB_FR_FPR18(k0)
		swc1	$19,GDB_FR_FPR19(k0)
		swc1	$20,GDB_FR_FPR20(k0)
		swc1	$21,GDB_FR_FPR21(k0)
		swc1	$22,GDB_FR_FPR22(k0)
		swc1	$23,GDB_FR_FPR23(k0)
		swc1	$24,GDB_FR_FPR24(k0)
		swc1	$25,GDB_FR_FPR25(k0)
		swc1	$26,GDB_FR_FPR26(k0)
		swc1	$27,GDB_FR_FPR27(k0)
		swc1	$28,GDB_FR_FPR28(k0)
		swc1	$29,GDB_FR_FPR29(k0)
		swc1	$30,GDB_FR_FPR30(k0)
		swc1	$31,GDB_FR_FPR31(k0)

		// FPU control registers
		mfc1	v0,CP1_STATUS
		sw		v0,GDB_FR_FSR(k0)
		mfc1	v0,CP1_REVISION
		sw		v0,GDB_FR_FIR(k0)

2:
		// Current stack frame ptr
		// Why? In case it differs from sp because of alloca, with our tool-chains, fp's used as another saved register (s8). As
		// such, I don't know which ABI we're using!
		sw		sp,GDB_FR_FRP(k0)

		// CP0 registers (R4000/R4400 unused registers skipped)
		mfc0	v0,CP0_INDEX
		sw		v0,GDB_FR_CP0_INDEX(k0)
		mfc0	v0,CP0_RANDOM
		sw		v0,GDB_FR_CP0_RANDOM(k0)
		mfc0	v0,CP0_ENTRYLO0
		sw		v0,GDB_FR_CP0_ENTRYLO0(k0)
		mfc0	v0,CP0_ENTRYLO1
		sw		v0,GDB_FR_CP0_ENTRYLO1(k0)
		mfc0	v0,CP0_CONTEXT
		sw		v0,GDB_FR_CP0_CONTEXT(k0)
		mfc0	v0,CP0_PAGEMASK
		sw		v0,GDB_FR_CP0_PAGEMASK(k0)
		mfc0	v0,CP0_WIRED
		sw		v0,GDB_FR_CP0_WIRED(k0)
		mfc0	v0,CP0_ENTRYHI
		sw		v0,GDB_FR_CP0_ENTRYHI(k0)
		mfc0	v0,CP0_PRID
		sw		v0,GDB_FR_CP0_PRID(k0)

		// Playstation 2 Specific registers
		mtc1	$0,fp0				// This saves acc.
		madd.s	fp0,fp0,fp0			// FPR[0] <- ACC + FPR[0] * FPR[0]
		mfc1	v0,fp0
		sw		v0,GDB_FR_CP1_ACC(k0)
		mfsa	v0
		sw		v0,GDB_FR_SHFT_AMNT(k0)
		mflo1	v0
		sw		v0,GDB_FR_LO1(k0)
		mfhi1	v0
		sw		v0,GDB_FR_HI1(k0)

		// Arg 0 for handle_exception is gdbstub_regs.
		move	a0,k0

		// Faked return from exception. Then jumps to handle_exception.
		la		at,faked_epc
		mtc0	at,CP0_EPC
		sync.p
		mfc0	at,CP0_STATUS
		li		v0,0xfffffffe
		and		at,v0
		mtc0	at,CP0_STATUS
		sync.p
		eret
		nop
		nop

faked_epc:
		nop
		nop
		jal		handle_exception
		nop
		nop
		nop


faked_ra:
		di

		li	$3, 0x64
		move	$4,$0
		syscall							// FlushCache(0) - Writeback data cache
		nop

		li	$3, 0x64
		li	$4, 2
		syscall							// FlushCache(2) - Flush inst cache
		nop

		nop
		nop
		la		k0,gdbstub_ps2_regs
		mfc0	at,CP0_STATUS			// Sony do this... I don't think I want to do this though. It's odd!
		li		k1,0xffffffe4			// last byte is 0b11100100, which sets IEc, KUc, KUp, IEo to 0. Not what you'd expect.
		and		at,at,k1
		mtc0	at,CP0_STATUS
		sync.p

		// Playstation 2 specific registers.
		lw		v0,GDB_FR_CP1_ACC(k0)
		mtc1	$0,fp0
		mtc1	v0,fp1
		adda.s	fp0,fp1
		lw		v0,GDB_FR_SHFT_AMNT(k0)
		mtsa	v0
		lw		v0,GDB_FR_LO1(k0)
		mtlo1	v0
		lw		v0,GDB_FR_HI1(k0)
		mthi1	v0
		lw		v0,GDB_FR_CP0_ENTRYHI(k0)
		lw		v1,GDB_FR_CP0_WIRED(k0)
		mtc0	v0,CP0_ENTRYHI
		mtc0	v1,CP0_WIRED
		lw		v0,GDB_FR_CP0_PAGEMASK(k0)
		lw		v1,GDB_FR_CP0_ENTRYLO1(k0)
		mtc0	v0,CP0_PAGEMASK
		mtc0	v1,CP0_ENTRYLO1
		lw		v0,GDB_FR_CP0_ENTRYLO0(k0)
		lw		v1,GDB_FR_CP0_INDEX(k0)
		mtc0	v0,CP0_ENTRYLO0
		mtc0	v1,CP0_INDEX
		lw		v0,GDB_FR_CP0_CONTEXT(k0)
		mtc0	v0,CP0_CONTEXT

		// Floating point registers
		mfc0	v0,CP0_STATUS		/* check if the FPU is enabled */
		srl		v0,v0,16
		andi	v0,v0,(ST0_CU1 >> 16)

		beqz	v0,3f			/* disabled, skip */
		nop

		lwc1	$31,GDB_FR_FPR31(k0)
		lwc1	$30,GDB_FR_FPR30(k0)
		lwc1	$29,GDB_FR_FPR29(k0)
		lwc1	$28,GDB_FR_FPR28(k0)
		lwc1	$27,GDB_FR_FPR27(k0)
		lwc1	$26,GDB_FR_FPR26(k0)
		lwc1	$25,GDB_FR_FPR25(k0)
		lwc1	$24,GDB_FR_FPR24(k0)
		lwc1	$23,GDB_FR_FPR23(k0)
		lwc1	$22,GDB_FR_FPR22(k0)
		lwc1	$21,GDB_FR_FPR21(k0)
		lwc1	$20,GDB_FR_FPR20(k0)
		lwc1	$19,GDB_FR_FPR19(k0)
		lwc1	$18,GDB_FR_FPR18(k0)
		lwc1	$17,GDB_FR_FPR17(k0)
		lwc1	$16,GDB_FR_FPR16(k0)
		lwc1	$15,GDB_FR_FPR15(k0)
		lwc1	$14,GDB_FR_FPR14(k0)
		lwc1	$13,GDB_FR_FPR13(k0)
		lwc1	$12,GDB_FR_FPR12(k0)
		lwc1	$11,GDB_FR_FPR11(k0)
		lwc1	$10,GDB_FR_FPR10(k0)
		lwc1	$9,GDB_FR_FPR9(k0)
		lwc1	$8,GDB_FR_FPR8(k0)
		lwc1	$7,GDB_FR_FPR7(k0)
		lwc1	$6,GDB_FR_FPR6(k0)
		lwc1	$5,GDB_FR_FPR5(k0)
		lwc1	$4,GDB_FR_FPR4(k0)
		lwc1	$3,GDB_FR_FPR3(k0)
		lwc1	$2,GDB_FR_FPR2(k0)
		lwc1	$1,GDB_FR_FPR1(k0)
		lwc1	$0,GDB_FR_FPR0(k0)


		// CP0 and integer registers
3:
		mfc0	t0,CP0_STATUS
		ori		t0,0x1f
		xori	t0,0x1f
		mtc0	t0,CP0_STATUS

		lw		v0,GDB_FR_STATUS(k0)
		lw		v1,GDB_FR_EPC(k0)

		mtc0	v0,CP0_STATUS
		sync.p
		mtc0	v1,CP0_EPC
		sync.p
		lq		v0,GDB_FR_HI(k0)
		lq		v1,GDB_FR_LO(k0)
		pmthi	v0
		pmtlo	v1
		lq		$31,GDB_FR_REG31(k0)
		lq		$30,GDB_FR_REG30(k0)
		lq		$29,GDB_FR_REG29(k0)
		lq		$28,GDB_FR_REG28(k0)
		lq		$25,GDB_FR_REG25(k0)
		lq		$24,GDB_FR_REG24(k0)
		lq		$23,GDB_FR_REG23(k0)
		lq		$22,GDB_FR_REG22(k0)
		lq		$21,GDB_FR_REG21(k0)
		lq		$20,GDB_FR_REG20(k0)
		lq		$19,GDB_FR_REG19(k0)
		lq		$18,GDB_FR_REG18(k0)
		lq		$17,GDB_FR_REG17(k0)
		lq		$16,GDB_FR_REG16(k0)
		lq		$15,GDB_FR_REG15(k0)
		lq		$14,GDB_FR_REG14(k0)
		lq		$13,GDB_FR_REG13(k0)
		lq		$12,GDB_FR_REG12(k0)
		lq		$11,GDB_FR_REG11(k0)
		lq		$10,GDB_FR_REG10(k0)
		lq		$9,GDB_FR_REG9(k0)
		lq		$8,GDB_FR_REG8(k0)
		lq		$7,GDB_FR_REG7(k0)
		lq		$6,GDB_FR_REG6(k0)
		lq		$5,GDB_FR_REG5(k0)
		lq		$4,GDB_FR_REG4(k0)
		lq		$3,GDB_FR_REG3(k0)
		lq		$2,GDB_FR_REG2(k0)
		lq		$1,GDB_FR_REG1(k0)

		mfc0	k0,CP0_CAUSE		// I added these
		ori		k0,k0,0x13			// I added these
		mtc0	k0,CP0_CAUSE		// I added these

		sync.p
		eret
		nop
		nop
		.set	pop
		.end	trap_low
