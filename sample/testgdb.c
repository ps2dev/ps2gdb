#include <stdio.h>
#include <debug.h>
#include <kernel.h>
#include <ps2gdb.h>

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
    } else {
        printf("PS2GDB_SAMPLE_DONE status=1\n");
    }
    fflush(stdout);
    /* Keep PS2Link alive; return to it with ps2client reset. */
    SleepThread();
    for (;;) SleepThread();
}
