/* Compile-only partial register snapshot. No vector installation. */
#define __ASSEMBLY__
#include "ee_context_candidate.h"
.set push
.set noreorder
.set noat
.text
.globl ee_context_candidate_only
.type ee_context_candidate_only, @function
ee_context_candidate_only:
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
    /* R26 and R27 were used by the entry mechanism; leave them invalid. */
    pmflo $k1
    sq $k1, EE_CTX_LO($k0)
    pmfhi $k1
    sq $k1, EE_CTX_HI($k0)
    mfsa $k1
    sw $k1, EE_CTX_SA($k0)
    mflo1 $k1
    sw $k1, EE_CTX_LO1($k0)
    mfhi1 $k1
    sw $k1, EE_CTX_HI1($k0)
    /* These CP0 values describe invocation, not validated exception entry. */
    mfc0 $k1, $14
    sw $k1, EE_CTX_EPC($k0)
    mfc0 $k1, $13
    sw $k1, EE_CTX_CAUSE($k0)
    mfc0 $k1, $12
    sw $k1, EE_CTX_STATUS($k0)
    /* No FPU or thread capture, no validity-mask updates. */
    jr $ra
    nop
.size ee_context_candidate_only, .-ee_context_candidate_only
.set pop
