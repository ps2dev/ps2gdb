#ifndef PS2GDB_EE_CONTEXT_CANDIDATE_H
#define PS2GDB_EE_CONTEXT_CANDIDATE_H

/* Compile-only snapshot ABI. No live exception-entry contract. */
#define EE_CTX_GPR_STRIDE 16
#define EE_CTX_LO 512
#define EE_CTX_HI 528
#define EE_CTX_SA 544
#define EE_CTX_LO1 548
#define EE_CTX_HI1 552
#define EE_CTX_EPC 556
#define EE_CTX_CAUSE 560
#define EE_CTX_STATUS 564
#define EE_CTX_FPR 568
#define EE_CTX_FSR 696
#define EE_CTX_ACC 700
#define EE_CTX_THREAD 704
#define EE_CTX_GPR_MASK 708
#define EE_CTX_STATE 712
#define EE_CTX_SIZE 720

#define EE_CTX_GPRS_EXCEPT_K01 0xf3ffffffu
#define EE_CTX_VALID_LO_HI (1u << 0)
#define EE_CTX_VALID_SA_HILO1 (1u << 1)
#define EE_CTX_VALID_ENTRY_CP0 (1u << 2)
#define EE_CTX_VALID_FPU (1u << 3)
#define EE_CTX_VALID_THREAD (1u << 4)
#define EE_CTX_VALID_ENTRY_BD (1u << 5)
#define EE_CTX_REQUIRED_STATE 0x3fu

#ifndef __ASSEMBLY__
#include <stdint.h>
typedef struct __attribute__((aligned(16))) ee_context_candidate {
    uint8_t gpr[32][EE_CTX_GPR_STRIDE];
    uint8_t lo[16], hi[16];
    uint32_t sa, lo1, hi1, epc, cause, status;
    uint32_t fpr[32], fsr, acc;
    int32_t interrupted_thread_id;
    uint32_t gpr_valid_mask, state_valid_mask;
} ee_context_candidate;

static inline const uint8_t *ee_context_gpr_bytes(const ee_context_candidate *ctx,
                                                   unsigned int index)
{
    return ctx->gpr[index];
}

static inline const uint8_t *ee_context_lo_bytes(const ee_context_candidate *ctx)
{
    return ctx->lo;
}

static inline const uint8_t *ee_context_hi_bytes(const ee_context_candidate *ctx)
{
    return ctx->hi;
}

static inline uint32_t ee_context_sa_value(const ee_context_candidate *ctx)
{
    return ctx->sa;
}

static inline uint32_t ee_context_lo1_value(const ee_context_candidate *ctx)
{
    return ctx->lo1;
}

static inline uint32_t ee_context_hi1_value(const ee_context_candidate *ctx)
{
    return ctx->hi1;
}

static inline const uint32_t *ee_context_fpr_values(const ee_context_candidate *ctx)
{
    return ctx->fpr;
}

static inline uint32_t ee_context_fsr_value(const ee_context_candidate *ctx)
{
    return ctx->fsr;
}

static inline uint32_t ee_context_acc_value(const ee_context_candidate *ctx)
{
    return ctx->acc;
}

/* Raw observation only: these values are not verified entry-time CP0 state. */
static inline uint32_t ee_context_cause_value(const ee_context_candidate *ctx)
{
    return ctx->cause;
}

static inline uint32_t ee_context_status_value(const ee_context_candidate *ctx)
{
    return ctx->status;
}

/* A validated entry provider must set the masks; the prototype does not. */
static inline int ee_context_is_complete(const ee_context_candidate *ctx)
{
    return ctx != 0 && ctx->gpr_valid_mask == UINT32_MAX &&
           (ctx->state_valid_mask & EE_CTX_REQUIRED_STATE) == EE_CTX_REQUIRED_STATE &&
           ctx->interrupted_thread_id > 0 && (ctx->epc & 3u) == 0;
}
#endif
#endif
