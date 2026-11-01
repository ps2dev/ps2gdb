/*
 * Diagnostic-only level-2 exception handler; no C, TCP or IOP operations.
 * The 0x80000100 exception vector is exactly 32 bytes: it must jump to
 * the full handler rather than truncating a handler during installation.
 * This handler disables BPC before memory accesses and preserves k0/k1.
 * This is NOT a production exception-to-GDB handoff.
 */
.set push
.set noreorder
.set noat
.text
.balign 32
.globl ps2gdb_probe_minimal_vector
ps2gdb_probe_minimal_vector:
    nop
    nop
    nop
    j ps2gdb_probe_record_handler
    nop
    nop
    nop
    nop

.balign 32
.globl ps2gdb_probe_record_handler
ps2gdb_probe_record_handler:
    mtbpc $zero
    sync.p
    sq $k0, -32($sp)
    sq $k1, -16($sp)

#ifdef PS2GDB_PROBE_SAVE_GPRS
    /* Snapshot the real 128-bit EE GPRs before clobbering k1. */
    la $k0, ps2gdb_probe_gpr_snapshot
    sq $0, 0($k0)
    sq $1, 16($k0)
    sq $2, 32($k0)
    sq $3, 48($k0)
    sq $4, 64($k0)
    sq $5, 80($k0)
    sq $6, 96($k0)
    sq $7, 112($k0)
    sq $8, 128($k0)
    sq $9, 144($k0)
    sq $10, 160($k0)
    sq $11, 176($k0)
    sq $12, 192($k0)
    sq $13, 208($k0)
    sq $14, 224($k0)
    sq $15, 240($k0)
    sq $16, 256($k0)
    sq $17, 272($k0)
    sq $18, 288($k0)
    sq $19, 304($k0)
    sq $20, 320($k0)
    sq $21, 336($k0)
    sq $22, 352($k0)
    sq $23, 368($k0)
    sq $24, 384($k0)
    sq $25, 400($k0)
    sq $28, 448($k0)
    sq $29, 464($k0)
    sq $30, 480($k0)
    sq $31, 496($k0)
    /* Slot 27 contains original k1. Slot 26 contains original k0. */
    lq $k1, -32($sp)
    sq $k1, 416($k0)
#endif

    la $k1, ps2gdb_probe_debug_hits
    lw $k0, 0($k1)
    addiu $k0, $k0, 1
    sw $k0, 0($k1)

    mfc0 $k0, $30
    la $k1, ps2gdb_probe_debug_error_epc
    sw $k0, 0($k1)
    mfc0 $k0, $13
    la $k1, ps2gdb_probe_debug_cause
    sw $k0, 0($k1)

#ifdef PS2GDB_PROBE_L1_HANDOFF
    /* ERET now goes to a safe software BREAK trampoline, not the faulting SW. */
    la $k0, ps2gdb_probe_l1_breakpoint
    mtc0 $k0, $30
    sync.p
#endif

#ifdef PS2GDB_PROBE_CALL_C
    /* Switch to a dedicated aligned stack before any C call. */
    la $sp, ps2gdb_probe_exception_stack + 4096
    jal ps2gdb_probe_c_callback
    nop
    /* Restore the entire saved 128-bit integer context after the C call. */
    la $k0, ps2gdb_probe_gpr_snapshot
    lq $1, 16($k0)
    lq $2, 32($k0)
    lq $3, 48($k0)
    lq $4, 64($k0)
    lq $5, 80($k0)
    lq $6, 96($k0)
    lq $7, 112($k0)
    lq $8, 128($k0)
    lq $9, 144($k0)
    lq $10, 160($k0)
    lq $11, 176($k0)
    lq $12, 192($k0)
    lq $13, 208($k0)
    lq $14, 224($k0)
    lq $15, 240($k0)
    lq $16, 256($k0)
    lq $17, 272($k0)
    lq $18, 288($k0)
    lq $19, 304($k0)
    lq $20, 320($k0)
    lq $21, 336($k0)
    lq $22, 352($k0)
    lq $23, 368($k0)
    lq $24, 384($k0)
    lq $25, 400($k0)
    lq $28, 448($k0)
    lq $29, 464($k0)
    lq $30, 480($k0)
    lq $31, 496($k0)
    lq $27, 432($k0)
    lq $26, 416($k0)
#else
    lq $k1, -16($sp)
    lq $k0, -32($sp)
#endif
    eret
    nop
    nop

#ifdef PS2GDB_PROBE_L1_HANDOFF
.balign 32
.globl ps2gdb_probe_l1_breakpoint
ps2gdb_probe_l1_breakpoint:
    break
    nop
    nop

/* The kernel invokes this for level-1 exception code 9. No C/IOP/TCP. */
.balign 32
.globl ps2gdb_probe_l1_handler
ps2gdb_probe_l1_handler:
    sq $k0, -32($sp)
    sq $k1, -16($sp)
    la $k1, ps2gdb_probe_l1_hits
    lw $k0, 0($k1)
    addiu $k0, $k0, 1
    sw $k0, 0($k1)
    mfc0 $k0, $14
    la $k1, ps2gdb_probe_l1_epc
    sw $k0, 0($k1)
    mfc0 $k0, $13
    la $k1, ps2gdb_probe_l1_cause
    sw $k0, 0($k1)
    la $k1, ps2gdb_probe_debug_error_epc
    lw $k0, 0($k1)
    mtc0 $k0, $14
    sync.p
    lq $k1, -16($sp)
    lq $k0, -32($sp)
    eret
    nop
    nop
#endif
.set pop
