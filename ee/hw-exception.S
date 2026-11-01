/*
 * Experimental EE debug level-2 -> software-break handoff.
 * The 32-byte vector jumps here. No C, network or IOP calls at level 2.
 * k0/k1 are reserved by the EE kernel and are clobbered by the vector.
 */
.set push
.set noreorder
.set noat
.text
.balign 32
.globl ps2gdb_hw_vector
ps2gdb_hw_vector:
    nop
    nop
    nop
    j ps2gdb_hw_exception
    nop
    nop
    nop
    nop

.balign 32
ps2gdb_hw_exception:
    mfbpc $k0
    mtbpc $zero
    sync.p
    la $k1, ps2gdb_hw_captured_bpc
    sw $k0, 0($k1)
    mfc0 $k0, $13
    la $k1, ps2gdb_hw_captured_cause
    sw $k0, 0($k1)
    mfc0 $k0, $30
    la $k1, ps2gdb_hw_captured_epc
    sw $k0, 0($k1)
    la $k0, ps2gdb_hw_pending
    li $k1, 1
    sw $k1, 0($k0)
    la $k0, ps2gdb_hw_trampoline
    mtc0 $k0, $30
    sync.p
    eret
    nop
    nop

.balign 32
.globl ps2gdb_hw_trampoline
ps2gdb_hw_trampoline:
    break
    nop
    nop
.set pop
