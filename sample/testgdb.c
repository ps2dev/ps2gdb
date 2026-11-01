#include <stdio.h>
#include <debug.h>
#include <kernel.h>

int gdb_stub_main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    volatile int i = 0;

    init_scr();
    if (gdb_stub_main(0, NULL) == 0) {
        scr_printf("i = %d\n", i += 2);
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
