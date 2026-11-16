#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../ee/gdb-stub.h"
#include "preemptive/ee_context_candidate.h"

static int failures;

static void check(int ok, const char *message)
{
    if (!ok) {
        fprintf(stderr, "FAIL %s\n", message);
        ++failures;
    }
}

int main(void)
{
    ee_context_candidate ctx = {0};
    gdb_regs_ps2 legacy = {0};
    unsigned int n, byte;

#define CHECK_OFFSET(field, offset) \
    check(offsetof(ee_context_candidate, field) == (offset), #field " offset")
    CHECK_OFFSET(gpr, 0);
    CHECK_OFFSET(lo, EE_CTX_LO);
    CHECK_OFFSET(hi, EE_CTX_HI);
    CHECK_OFFSET(sa, EE_CTX_SA);
    CHECK_OFFSET(lo1, EE_CTX_LO1);
    CHECK_OFFSET(hi1, EE_CTX_HI1);
    CHECK_OFFSET(epc, EE_CTX_EPC);
    CHECK_OFFSET(cause, EE_CTX_CAUSE);
    CHECK_OFFSET(status, EE_CTX_STATUS);
    CHECK_OFFSET(fpr, EE_CTX_FPR);
    CHECK_OFFSET(fsr, EE_CTX_FSR);
    CHECK_OFFSET(acc, EE_CTX_ACC);
    CHECK_OFFSET(interrupted_thread_id, EE_CTX_THREAD);
    CHECK_OFFSET(gpr_valid_mask, EE_CTX_GPR_MASK);
    CHECK_OFFSET(state_valid_mask, EE_CTX_STATE);
#undef CHECK_OFFSET
    check(sizeof(ctx) == EE_CTX_SIZE, "snapshot size");
    check(_Alignof(ee_context_candidate) == 16, "aligned frame");
    check(offsetof(gdb_regs_ps2, reg0) == GDB_FR_REG0, "GDB r0 ABI");
    check(offsetof(gdb_regs_ps2, reg26) == GDB_FR_REG26, "GDB k0 ABI");
    check(offsetof(gdb_regs_ps2, reg27) == GDB_FR_REG27, "GDB k1 ABI");
    check(offsetof(gdb_regs_ps2, reg31) == GDB_FR_REG31, "GDB r31 ABI");
    check(offsetof(gdb_regs_ps2, lo) == GDB_FR_LO, "GDB LO ABI");
    check(offsetof(gdb_regs_ps2, hi) == GDB_FR_HI, "GDB HI ABI");
    check(offsetof(gdb_regs_ps2, cp0_epc) == GDB_FR_EPC, "GDB EPC ABI");

    for (n = 0; n != 32; ++n)
        for (byte = 0; byte != 16; ++byte)
            ctx.gpr[n][byte] = (uint8_t)(n * 17u + byte);
    memcpy(&legacy.reg0, ctx.gpr, sizeof(ctx.gpr));
    check(memcmp(&legacy.reg0, ctx.gpr, sizeof(ctx.gpr)) == 0,
          "128-bit GPRs retain all bytes in legacy frame layout");
    check(ee_context_gpr_bytes(&ctx, 5)[15] == ctx.gpr[5][15],
          "GPR accessor retains all 16 bytes");
    ctx.lo[15] = 0xa5;
    ctx.hi[15] = 0x5a;
    ctx.sa = 31;
    ctx.lo1 = 0x78563412u;
    ctx.hi1 = 0xaabbccddu;
    ctx.fpr[31] = 0x11223344u;
    ctx.fsr = 0x003fffffu;
    ctx.acc = 0x87654321u;
    ctx.cause = 0x80008000u;
    ctx.status = 0x70030c10u;
    check(ee_context_lo_bytes(&ctx)[15] == 0xa5,
          "128-bit LO accessor");
    check(ee_context_hi_bytes(&ctx)[15] == 0x5a,
          "128-bit HI accessor");
    check(ee_context_sa_value(&ctx) == 31,
          "shift amount accessor");
    check(ee_context_lo1_value(&ctx) == 0x78563412u,
          "LO1 accessor");
    check(ee_context_hi1_value(&ctx) == 0xaabbccddu,
          "HI1 accessor");
    check(ee_context_fpr_values(&ctx)[31] == 0x11223344u,
          "FPU register accessor");
    check(ee_context_fsr_value(&ctx) == 0x003fffffu,
          "FPU status accessor");
    check(ee_context_acc_value(&ctx) == 0x87654321u,
          "FPU accumulator accessor");
    check(ee_context_cause_value(&ctx) == 0x80008000u,
          "raw Cause observation accessor");
    check(ee_context_status_value(&ctx) == 0x70030c10u,
          "raw Status observation accessor");

    /* A candidate cannot be used to stop/resume: k0/k1 and the original
     * exception/thread/FPU provenance have not been validated. */
    ctx.gpr_valid_mask = EE_CTX_GPRS_EXCEPT_K01;
    ctx.epc = 0x00101118u;
    ctx.interrupted_thread_id = 16;
    ctx.state_valid_mask = EE_CTX_REQUIRED_STATE;
    check(!ee_context_is_complete(&ctx), "missing k0/k1 refuses resume");
    ctx.gpr_valid_mask = UINT32_MAX;
    check(ee_context_is_complete(&ctx), "synthetic complete-frame contract");
    ctx.state_valid_mask &= ~EE_CTX_VALID_ENTRY_BD;
    check(!ee_context_is_complete(&ctx), "missing exception-entry BD");
    ctx.state_valid_mask = EE_CTX_REQUIRED_STATE;
    ctx.interrupted_thread_id = 0;
    check(!ee_context_is_complete(&ctx), "unknown interrupted thread");
    ctx.interrupted_thread_id = 16;
    ctx.epc |= 1u;
    check(!ee_context_is_complete(&ctx), "unaligned EPC");
    check(!ee_context_is_complete(NULL), "null frame");

    if (failures)
        return 1;
    puts("PASS EE candidate frame offsets, wide GPRs and incomplete-context rejection");
    return 0;
}
