/*
 * Diagnostic-only level-2 handler. No C calls, networking or IOP operations.
 * Disable debug comparators BEFORE touching memory, record the exception,
 * restore scratch GPRs, then return to the original ErrorEPC using ERET.
 * Do not reuse this shortcut as a production debugger exception handler.
 */
.set push
.set noreorder
.set noat
.text
.balign 32
.globl ps2gdb_probe_minimal_vector
ps2gdb_probe_minimal_vector:
    mtbpc $zero
    sync.p
    sq $k0, -32($sp)
    sq $k1, -16($sp)

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

    lq $k1, -16($sp)
    lq $k0, -32($sp)
    eret
    nop
    nop
.set pop
