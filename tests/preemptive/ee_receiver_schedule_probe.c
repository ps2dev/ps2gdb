/*
 * Standalone EE scheduling experiment, not a debugger and not a GDB stop.
 * A higher-priority observer sleeps on the EE timer while a target burns
 * CPU without a yield or debugger polling. No handlers or comparators are
 * installed by this ELF. Never run on physical PS2 automatically.
 * The opt-in priority variant changes only this test target's scheduling.
 */
#include <kernel.h>
#include <delaythread.h>
#include <stdio.h>

#define BUSY_COUNT_TICKS 75000000u
#define OBSERVER_PERIOD_US 10000u

static volatile unsigned int busy_active;
static volatile unsigned int busy_iterations;
static volatile unsigned int observer_ticks;
static volatile unsigned int observer_during_busy;
static volatile int observer_shutdown;
static volatile int observer_exited;
static unsigned char observer_stack[8192] __attribute__((aligned(16)));

extern unsigned char ps2gdb_sched_busy_begin[];
extern unsigned char ps2gdb_sched_busy_end[];

static void observer(void *unused)
{
    (void)unused;
    while (!observer_shutdown) {
        ++observer_ticks;
        if (busy_active)
            ++observer_during_busy;
        DelayThread(OBSERVER_PERIOD_US);
    }
    observer_exited = 1;
    ExitThread();
}

__attribute__((noinline)) static void unyielding_target(void)
{
    unsigned int start, now;
    __asm__ volatile("mfc0 %0, $9" : "=r"(start));
    busy_active = 1;
    __asm__ volatile(".globl ps2gdb_sched_busy_begin\n"
                     "ps2gdb_sched_busy_begin:" ::: "memory");
    do {
        ++busy_iterations;
        __asm__ volatile("mfc0 %0, $9" : "=r"(now));
    } while ((unsigned int)(now - start) < BUSY_COUNT_TICKS);
    busy_active = 0;
    __asm__ volatile(".globl ps2gdb_sched_busy_end\n"
                     "ps2gdb_sched_busy_end:\n"
                     "nop" ::: "memory");
}

int main(void)
{
    ee_thread_t thread = {0};
    ee_thread_status_t status = {0};
    int id = -1, main_priority = -1, observer_priority = -1, started = 0;
    unsigned int before = 0;
    int attempt;

#ifdef PS2GDB_SCHED_LOWER_TARGET_PRIORITY
    /* Emulator-only control: changes the target's scheduling priority. */
    ChangeThreadPriority(TH_SELF, 64);
#endif
    if (ReferThreadStatus(TH_SELF, &status) >= 0) {
        main_priority = status.current_priority;
        observer_priority = main_priority > 1 ? main_priority - 1 : 1;
        thread.func = observer;
        thread.stack = observer_stack;
        thread.stack_size = sizeof(observer_stack);
        thread.gp_reg = &_gp;
        thread.initial_priority = observer_priority;
        id = CreateThread(&thread);
        if (id >= 0 && StartThread(id, NULL) >= 0) {
            started = 1;
            DelayThread(50000);
            before = observer_ticks;
            unyielding_target();
            observer_shutdown = 1;
            for (attempt = 0; attempt < 500 && !observer_exited; ++attempt)
                DelayThread(1000);
        } else if (id >= 0) {
            DeleteThread(id);
        }
    }

    printf("PS2GDB_SCHED_PROBE_DONE started=%d main_priority=%d observer_priority=%d ticks_before=%u ticks_total=%u during_busy=%u busy_iterations=%u observer_exited=%d\n",
           started, main_priority, observer_priority, before,
           observer_ticks, observer_during_busy, busy_iterations,
           observer_exited);
    fflush(stdout);
    /* Never exit to OSD while being launched by PS2Link. */
    SleepThread();
    for (;;)
        SleepThread();
}
