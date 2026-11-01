/*
 * Independent EE hardware data-watchpoint smoke probe.
 * The exception handler does not access networking, reset the IOP or exit.
 * Run via PS2Link host: and recover with host-side ps2client reset.
 */
#include <kernel.h>
#include <ee_debug.h>
#include <ee_cop0_defs.h>
#include <stdio.h>
#include <string.h>

/* Opt-in trigger variants only. All CI/default builds remain non-triggering. */
#if defined(PS2GDB_PROBE_MINIMAL_VECTOR) && !defined(PS2GDB_PROBE_TRIGGER)
#error PS2GDB_PROBE_MINIMAL_VECTOR requires PS2GDB_PROBE_TRIGGER
#endif
#if defined(PS2GDB_PROBE_CALL_C) && !defined(PS2GDB_PROBE_SAVE_GPRS)
#error PS2GDB_PROBE_CALL_C requires PS2GDB_PROBE_SAVE_GPRS
#endif
#if defined(PS2GDB_PROBE_CALL_C) && !defined(PS2GDB_PROBE_MINIMAL_VECTOR)
#error PS2GDB_PROBE_CALL_C requires PS2GDB_PROBE_MINIMAL_VECTOR
#endif
#if defined(PS2GDB_PROBE_READ_UNCACHED) && !defined(PS2GDB_PROBE_READ)
#error PS2GDB_PROBE_READ_UNCACHED requires PS2GDB_PROBE_READ
#endif
#if defined(PS2GDB_PROBE_L1_HANDOFF) && !defined(PS2GDB_PROBE_MINIMAL_VECTOR)
#error PS2GDB_PROBE_L1_HANDOFF requires PS2GDB_PROBE_MINIMAL_VECTOR
#endif
#if defined(PS2GDB_PROBE_BD2) && (!defined(PS2GDB_PROBE_L1_HANDOFF) || !defined(PS2GDB_PROBE_TRIGGER))
#error PS2GDB_PROBE_BD2 requires PS2GDB_PROBE_TRIGGER and PS2GDB_PROBE_L1_HANDOFF
#endif

#ifdef PS2GDB_PROBE_BD2
extern void ps2gdb_probe_bd2_write(volatile unsigned int *word);
extern unsigned char ps2gdb_probe_bd2_branch[];
#endif

static volatile unsigned int watched_word;
static volatile unsigned int observed_word;

#ifdef PS2GDB_PROBE_EXEC
__attribute__((noinline)) static void ps2gdb_probe_target(void)
{
    __asm__ volatile("nop" ::: "memory");
    observed_word = watched_word + 1;
}
#endif

static volatile unsigned int debug_hits;
static volatile unsigned int last_debug_error_epc;
static volatile unsigned int last_debug_cause;
static volatile unsigned int last_debug_bpc;

/* Written only by the diagnostic assembly vector after comparators are off. */
volatile unsigned int ps2gdb_probe_debug_hits;
volatile unsigned int ps2gdb_probe_debug_error_epc;
volatile unsigned int ps2gdb_probe_debug_cause;
#ifdef PS2GDB_PROBE_BD2
volatile unsigned int *ps2gdb_probe_watched_ptr = &watched_word;
volatile unsigned int ps2gdb_probe_value_at_exception;
#endif
#ifdef PS2GDB_PROBE_L1_HANDOFF
extern void ps2gdb_probe_l1_handler(void);
extern void ps2gdb_probe_l1_breakpoint(void);
volatile unsigned int ps2gdb_probe_l1_hits;
volatile unsigned int ps2gdb_probe_l1_epc;
volatile unsigned int ps2gdb_probe_l1_cause;
#endif
/* Optional diagnostic 128-bit GPR capture before the ERET return. */
u128 ps2gdb_probe_gpr_snapshot[32] __attribute__((aligned(16)));
#ifdef PS2GDB_PROBE_CALL_C
u128 ps2gdb_probe_exception_stack[256] __attribute__((aligned(16)));
volatile unsigned int ps2gdb_probe_c_hits;
__attribute__((noinline)) void ps2gdb_probe_c_callback(void)
{
    ps2gdb_probe_c_hits++;
}
#endif

#ifdef PS2GDB_PROBE_TRIGGER
static int on_debug_exception(EE_RegFrame *frame)
{
    debug_hits++;
    last_debug_error_epc = frame->errorepc;
    last_debug_cause = frame->cause;
    last_debug_bpc = frame->bpc;
    /* Do not rearm on return: the interrupted store must complete. */
    frame->bpc = EE_BPC_BED;
    return 0;
}

#endif

#if defined(PS2GDB_PROBE_TRIGGER) && defined(PS2GDB_PROBE_MINIMAL_VECTOR)
extern unsigned char ps2gdb_probe_minimal_vector[];
static void install_minimal_vector(void)
{
    int old_intr = DIntr();
    int old_mode = ee_set_opmode(0);
    memcpy((void *)0x80000100, ps2gdb_probe_minimal_vector, 32);
    FlushCache(0);
    FlushCache(2);
    ee_set_opmode(old_mode);
    if (old_intr) EIntr();
}
#endif

static unsigned int read_ee_status(void)
{
    unsigned int status;
    __asm__ volatile("mfc0 %0, $12" : "=r"(status));
    return status;
}

int main(void)
{
    int ok = 0;
    unsigned int saved_sp = 0;
    unsigned int status_before = read_ee_status();
    printf("PS2GDB_HW_PROBE_BEGIN addr=%08x status=%08x DEV=%u\n",
           (unsigned int)&watched_word, status_before,
           !!(status_before & (1u << 23)));
    fflush(stdout);
    if (ee_dbg_install(2) == 0) {
        unsigned int armed_bpc, armed_dab, armed_dabm;
#ifdef PS2GDB_PROBE_L1_HANDOFF
        void *old_break_handler = GetExceptionHandler(9);
        SetVCommonHandler(9, (void *)ps2gdb_probe_l1_handler);
#endif
        printf("PS2GDB_HW_PROBE_VECTOR status=%08x DEV=%u\n",
               read_ee_status(), !!(read_ee_status() & (1u << 23)));
        fflush(stdout);
#ifdef PS2GDB_PROBE_TRIGGER
        ee_dbg_set_level2_handler(EE_EXC2_DBG, on_debug_exception);
#ifdef PS2GDB_PROBE_MINIMAL_VECTOR
        install_minimal_vector();
#endif
#endif
#ifdef PS2GDB_PROBE_EXEC
        ee_dbg_set_bpx((unsigned int)ps2gdb_probe_target, 0xffffffffu,
                       EE_BPC_IUE | EE_BPC_ISE | EE_BPC_IKE | EE_BPC_IXE);
#elif defined(PS2GDB_PROBE_READ)
        watched_word = 42;
#if defined(PS2GDB_PROBE_READ_UNCACHED) || defined(PS2GDB_PROBE_READ_COLD)
        FlushCache(0); /* Flush cache before the read-comparator experiment. */
#endif
#ifdef PS2GDB_PROBE_READ_UNCACHED
        ee_dbg_set_bpr((unsigned int)UNCACHED_SEG(&watched_word), 0xffffffffu,
#else
        ee_dbg_set_bpr((unsigned int)&watched_word, 0xffffffffu,
#endif
                       EE_BPC_DUE | EE_BPC_DSE | EE_BPC_DKE | EE_BPC_DXE);
#else
        ee_dbg_set_bpw((unsigned int)&watched_word, 0xffffffffu,
                       EE_BPC_DUE | EE_BPC_DSE | EE_BPC_DKE | EE_BPC_DXE);
#endif
        armed_bpc = ee_dbg_get_bpc();
        armed_dab = ee_dbg_get_dab();
        armed_dabm = ee_dbg_get_dabm();
        printf("PS2GDB_HW_PROBE_ARMED bpc=%08x dab=%08x dabm=%08x\n",
               armed_bpc, armed_dab, armed_dabm);
        fflush(stdout);
        /* PS2SDK leaves BPC.BED set after ee_dbg_clr_bps at install. */
#ifdef PS2GDB_PROBE_TRIGGER
        ee_dbg_set_bpc(ee_dbg_get_bpc() & ~EE_BPC_BED);
#endif
#ifdef PS2GDB_PROBE_EXEC
        ps2gdb_probe_target();
#elif defined(PS2GDB_PROBE_READ)
#ifdef PS2GDB_PROBE_READ_UNCACHED
        observed_word = *(volatile unsigned int *)UNCACHED_SEG(&watched_word);
#else
        observed_word = watched_word;
#endif
#elif defined(PS2GDB_PROBE_BD2)
        ps2gdb_probe_bd2_write(&watched_word);
#else
        watched_word = 42;
#endif
#ifdef PS2GDB_PROBE_MINIMAL_VECTOR
        last_debug_bpc = ee_dbg_get_bpc();
        debug_hits = ps2gdb_probe_debug_hits;
        last_debug_error_epc = ps2gdb_probe_debug_error_epc;
        last_debug_cause = ps2gdb_probe_debug_cause;
#ifdef PS2GDB_PROBE_SAVE_GPRS
        memcpy(&saved_sp, &ps2gdb_probe_gpr_snapshot[29], sizeof(saved_sp));
#endif
        ok = last_debug_bpc == 0 &&
#ifdef PS2GDB_PROBE_SAVE_GPRS
             saved_sp != 0 && (saved_sp & 15) == 0 &&
#endif
#ifdef PS2GDB_PROBE_CALL_C
             ps2gdb_probe_c_hits == 1 &&
#endif
#ifdef PS2GDB_PROBE_EXEC
             watched_word == 0 && observed_word == 1 &&
             last_debug_error_epc == (unsigned int)ps2gdb_probe_target &&
#elif defined(PS2GDB_PROBE_READ_UNCACHED)
             watched_word == 42 && observed_word == 42 &&
#elif defined(PS2GDB_PROBE_READ)
             watched_word == 42 && observed_word == 42 &&
#elif defined(PS2GDB_PROBE_BD2)
             watched_word == 42 &&
             last_debug_error_epc == (unsigned int)ps2gdb_probe_bd2_branch &&
             (last_debug_cause & EE_CAUSE_BD2) != 0 &&
#else
             watched_word == 42 &&
#endif
             debug_hits == 1 &&
#ifdef PS2GDB_PROBE_L1_HANDOFF
             ps2gdb_probe_l1_hits == 1 &&
             ps2gdb_probe_l1_epc == (unsigned int)ps2gdb_probe_l1_breakpoint &&
             M_EE_GET_CAUSE_EXCODE(ps2gdb_probe_l1_cause) == 9 &&
#endif
             M_EE_GET_CAUSE_EXC2(last_debug_cause) == EE_EXC2_DBG;
#elif defined(PS2GDB_PROBE_TRIGGER)
        ok = (debug_hits == 1 && watched_word == 42 &&
              M_EE_GET_CAUSE_EXC2(last_debug_cause) == EE_EXC2_DBG);
#else
        ok = (watched_word == 42);
#endif
        ee_dbg_clr_bps();
        ee_dbg_remove(2);
#ifdef PS2GDB_PROBE_L1_HANDOFF
        if (old_break_handler)
            SetVCommonHandler(9, old_break_handler);
#endif
    }
#ifdef PS2GDB_PROBE_L1_HANDOFF
    printf("PS2GDB_HW_PROBE_L1 hits=%u epc=%08x cause=%08x\n",
           ps2gdb_probe_l1_hits, ps2gdb_probe_l1_epc,
           ps2gdb_probe_l1_cause);
#endif
#ifdef PS2GDB_PROBE_BD2
    printf("PS2GDB_HW_PROBE_BD2 expected_epc=%08x observed_epc=%08x bd2=%u value_at_exception=%u\n",
           (unsigned int)ps2gdb_probe_bd2_branch,
           last_debug_error_epc, !!(last_debug_cause & EE_CAUSE_BD2),
           ps2gdb_probe_value_at_exception);
#endif
    printf("PS2GDB_HW_PROBE_DONE status=%d hits=%u watched=%u observed=%u errorepc=%08x cause=%08x bpc=%08x savedsp=%08x\n",
           ok ? 0 : 1, debug_hits, watched_word, observed_word,
           last_debug_error_epc, last_debug_cause, last_debug_bpc,
           saved_sp);
    fflush(stdout);
    SleepThread();
    for (;;) SleepThread();
}
