/*
 * Opt-in MIPS branch-delay-slot data-watchpoint test.
 * The watched store is explicitly in the delay slot of a JAL, so the
 * level-2 exception must set Cause.BD2 and ErrorEPC to this JAL address.
 * The probe uses the existing minimal L2 -> L1 handoff; it never uses TCP.
 */
.set push
.set noreorder
.set noat
.text
.balign 32
.globl ps2gdb_probe_bd2_write
.globl ps2gdb_probe_bd2_branch
ps2gdb_probe_bd2_write:
    addiu $sp, $sp, -16
    sq $ra, 0($sp)
    li $t1, 42
ps2gdb_probe_bd2_branch:
    jal ps2gdb_probe_bd2_target
    sw $t1, 0($a0)
    lq $ra, 0($sp)
    jr $ra
    addiu $sp, $sp, 16
ps2gdb_probe_bd2_target:
    jr $ra
    nop
.set pop
