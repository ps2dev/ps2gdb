#include <stdio.h>
#include <debug.h>
#include <kernel.h>
#include <delaythread.h>
#include <ps2gdb.h>

#ifdef PS2GDB_FCSR_DIAG
extern void breakpoint(void);
volatile unsigned int ps2gdb_fcsr_expected;
volatile unsigned int ps2gdb_fcsr_after;
volatile unsigned int ps2gdb_fcsr_cu1;

static void ps2gdb_test_fcsr(void)
{
    unsigned int status, original, value = 0x01000000u;

    __asm__ volatile("mfc0 %0,$12" : "=r"(status));
    ps2gdb_fcsr_cu1 = (status & 0x20000000u) != 0;
    if (!ps2gdb_fcsr_cu1) {
        printf("PS2GDB_FCSR_DIAG disabled: CU1 not enabled\n");
        fflush(stdout);
        return;
    }

    __asm__ volatile("cfc1 %0,$31" : "=r"(original));
    __asm__ volatile("ctc1 %0,$31\n\tcfc1 %0,$31" : "+r"(value) :: "memory");
    ps2gdb_fcsr_expected = value;
    /* Both stops are ordinary software BREAKs; no L2 debug comparator
     * or asynchronous receiver is installed by this diagnostic. */
    breakpoint();
    __asm__ volatile("cfc1 %0,$31" : "=r"(value) :: "memory");
    ps2gdb_fcsr_after = value;
    breakpoint();
    printf("PS2GDB_FCSR_DIAG status=%u before=%08x after=%08x\n",
           ps2gdb_fcsr_after == ps2gdb_fcsr_expected ? 0u : 1u,
           ps2gdb_fcsr_expected, ps2gdb_fcsr_after);
    fflush(stdout);
    __asm__ volatile("ctc1 %0,$31" :: "r"(original) : "memory");
}
#endif

#ifdef PS2GDB_HW_EXPERIMENTAL
/* Named hardware-watchpoint target for manual real-GDB diagnostics. */
volatile int gdb_hw_test_word;
volatile int gdb_hw_test_calls;
volatile int gdb_hw_test_read_result;

__attribute__((noinline)) void gdb_hw_test_function(void)
{
    gdb_hw_test_calls++;
    __asm__ volatile("" ::: "memory");
}
#endif

int main(int argc, char *argv[])
{
    volatile int i = 0;

    init_scr();
    if (gdb_stub_main(0, NULL) == 0) {
#ifdef PS2GDB_FCSR_DIAG
        ps2gdb_test_fcsr();
#endif
#ifdef PS2GDB_HW_EXPERIMENTAL
        gdb_hw_test_function();
        gdb_hw_test_function();
        gdb_hw_test_word = 11;
#endif
        scr_printf("i = %d\n", i += 2);
#ifdef PS2GDB_HW_EXPERIMENTAL
        gdb_hw_test_word = 22;
        FlushCache(0);
        gdb_hw_test_read_result =
            *(volatile int *)UNCACHED_SEG(&gdb_hw_test_word);
#endif
        scr_printf("i = %d\n", i += 10);
        printf("PS2GDB_SAMPLE_DONE status=0\n");
#ifdef PS2GDB_CTRL_C_COOPERATIVE
        /* Controlled real-hardware Ctrl-C experiment. The game is running,
         * but has a single safe point for the GDB raw interrupt request. */
        {
            volatile unsigned int counter = 0;
            while (1) {
                ++counter;
                gdbstub_poll_interrupt();
                /* Yield while the EE network RPC completes, and avoid
                 * overwhelming the IOP/PS2Link with a busy spin. */
                DelayThread(10000);
            }
        }
#endif
    } else {
        printf("PS2GDB_SAMPLE_DONE status=1\n");
    }
    fflush(stdout);
    /* Keep PS2Link alive. Do not use ps2client reset when preserving IOP
     * state: this PS2Link handler calls SifIopReset(). */
    SleepThread();
    for (;;) SleepThread();
}
