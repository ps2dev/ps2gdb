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

static volatile unsigned int watched_word;
static volatile unsigned int debug_hits;
static volatile unsigned int last_debug_error_epc;
static volatile unsigned int last_debug_cause;
static volatile unsigned int last_debug_bpc;

/* Written only by the diagnostic assembly vector after comparators are off. */
volatile unsigned int ps2gdb_probe_debug_hits;
volatile unsigned int ps2gdb_probe_debug_error_epc;
volatile unsigned int ps2gdb_probe_debug_cause;

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
    unsigned int status_before = read_ee_status();
    printf("PS2GDB_HW_PROBE_BEGIN addr=%08x status=%08x DEV=%u\n",
           (unsigned int)&watched_word, status_before,
           !!(status_before & (1u << 23)));
    fflush(stdout);
    if (ee_dbg_install(2) == 0) {
        unsigned int armed_bpc, armed_dab, armed_dabm;
        printf("PS2GDB_HW_PROBE_VECTOR status=%08x DEV=%u\n",
               read_ee_status(), !!(read_ee_status() & (1u << 23)));
        fflush(stdout);
#ifdef PS2GDB_PROBE_TRIGGER
        ee_dbg_set_level2_handler(EE_EXC2_DBG, on_debug_exception);
#ifdef PS2GDB_PROBE_MINIMAL_VECTOR
        install_minimal_vector();
#endif
#endif
        ee_dbg_set_bpw((unsigned int)&watched_word, 0xffffffffu,
                       EE_BPC_DUE | EE_BPC_DSE | EE_BPC_DKE | EE_BPC_DXE);
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
        watched_word = 42;
#ifdef PS2GDB_PROBE_MINIMAL_VECTOR
        last_debug_bpc = ee_dbg_get_bpc();
        debug_hits = ps2gdb_probe_debug_hits;
        last_debug_error_epc = ps2gdb_probe_debug_error_epc;
        last_debug_cause = ps2gdb_probe_debug_cause;
        ok = watched_word == 42 && last_debug_bpc == 0 &&
             debug_hits == 1 &&
             M_EE_GET_CAUSE_EXC2(last_debug_cause) == EE_EXC2_DBG;
#elif defined(PS2GDB_PROBE_TRIGGER)
        ok = (debug_hits == 1 && watched_word == 42 &&
              M_EE_GET_CAUSE_EXC2(last_debug_cause) == EE_EXC2_DBG);
#else
        ok = (watched_word == 42);
#endif
        ee_dbg_clr_bps();
        ee_dbg_remove(2);
    }
    printf("PS2GDB_HW_PROBE_DONE status=%d hits=%u watched=%u errorepc=%08x cause=%08x bpc=%08x\n",
           ok ? 0 : 1, debug_hits, watched_word, last_debug_error_epc,
           last_debug_cause, last_debug_bpc);
    fflush(stdout);
    SleepThread();
    for (;;) SleepThread();
}
