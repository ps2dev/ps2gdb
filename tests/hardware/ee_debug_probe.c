/*
 * Independent EE hardware data-watchpoint smoke probe.
 * The exception handler does not access networking, reset the IOP or exit.
 * Run via PS2Link host: and recover with host-side ps2client reset.
 */
#include <kernel.h>
#include <ee_debug.h>
#include <ee_cop0_defs.h>
#include <stdio.h>

static volatile unsigned int watched_word;
static volatile unsigned int debug_hits;
static volatile unsigned int last_debug_error_epc;
static volatile unsigned int last_debug_cause;
static volatile unsigned int last_debug_bpc;

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

int main(void)
{
    int ok = 0;
    printf("PS2GDB_HW_PROBE_BEGIN addr=%08x\n", (unsigned int)&watched_word);
    fflush(stdout);
    if (ee_dbg_install(2) == 0) {
        unsigned int armed_bpc, armed_dab, armed_dabm;
        ee_dbg_set_level2_handler(EE_EXC2_DBG, on_debug_exception);
        ee_dbg_set_bpw((unsigned int)&watched_word, 0xffffffffu,
                       EE_BPC_DUE | EE_BPC_DSE | EE_BPC_DKE | EE_BPC_DXE);
        armed_bpc = ee_dbg_get_bpc();
        armed_dab = ee_dbg_get_dab();
        armed_dabm = ee_dbg_get_dabm();
        printf("PS2GDB_HW_PROBE_ARMED bpc=%08x dab=%08x dabm=%08x\n",
               armed_bpc, armed_dab, armed_dabm);
        fflush(stdout);
        /* PS2SDK leaves BPC.BED set after ee_dbg_clr_bps at install. */
        ee_dbg_set_bpc(ee_dbg_get_bpc() & ~EE_BPC_BED);
        watched_word = 42;
        ee_dbg_clr_bps();
        ok = (debug_hits == 1 && watched_word == 42 &&
              M_EE_GET_CAUSE_EXC2(last_debug_cause) == EE_EXC2_DBG);
        ee_dbg_remove(2);
    }
    printf("PS2GDB_HW_PROBE_DONE status=%d hits=%u watched=%u errorepc=%08x cause=%08x bpc=%08x\n",
           ok ? 0 : 1, debug_hits, watched_word, last_debug_error_epc,
           last_debug_cause, last_debug_bpc);
    fflush(stdout);
    SleepThread();
    for (;;) SleepThread();
}
