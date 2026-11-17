/*
 * Standalone EE scheduling experiment, not a debugger and not a GDB stop.
 * A higher-priority observer sleeps on the EE timer while a target burns
 * CPU without a yield or debugger polling. No handlers or comparators are
 * installed by this ELF. Never run on physical PS2 automatically.
 * The opt-in priority variants affect only this diagnostic, not ps2gdb.
 * Priority zero is an EE kernel priority; its use here is EMULATOR-ONLY
 * until an isolated hardware trial proves it does not harm the scheduler.
 */
#include <kernel.h>
#include <delaythread.h>
#include <stdio.h>

#define BUSY_COUNT_TICKS 75000000u
#define OBSERVER_PERIOD_US 10000u

#if defined(PS2GDB_SCHED_TARGET_PRIORITY) && \
    (PS2GDB_SCHED_TARGET_PRIORITY < 1 || PS2GDB_SCHED_TARGET_PRIORITY > 127)
#error PS2GDB_SCHED_TARGET_PRIORITY must be in the EE user priority range 1..127
#endif
#if defined(PS2GDB_SCHED_OBSERVER_PRIORITY) && \
    (PS2GDB_SCHED_OBSERVER_PRIORITY < 0 || PS2GDB_SCHED_OBSERVER_PRIORITY > 127)
#error PS2GDB_SCHED_OBSERVER_PRIORITY must be in range 0..127
#endif

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
    ee_thread_status_t observer_status = {0};
    int id, main_priority = -1, observer_priority = -1, started = 0;
    int observer_actual_priority = -1;
    int passed;
    unsigned int before = 0;
    int attempt;

#ifdef PS2GDB_SCHED_TARGET_PRIORITY
    /* Opt-in diagnostic only: changes this test target's priority. */
    ChangeThreadPriority(TH_SELF, PS2GDB_SCHED_TARGET_PRIORITY);
#elif defined(PS2GDB_SCHED_LOWER_TARGET_PRIORITY)
    /* Original diagnostic variant for comparison with earlier evidence. */
    ChangeThreadPriority(TH_SELF, 64);
#endif
    if (ReferThreadStatus(TH_SELF, &status) >= 0) {
        main_priority = status.current_priority;
#ifdef PS2GDB_SCHED_OBSERVER_PRIORITY
        /* Only a controlled, standalone probe may attempt priority zero. */
        observer_priority = PS2GDB_SCHED_OBSERVER_PRIORITY;
#else
        observer_priority = main_priority > 1 ? main_priority - 1 : 1;
#endif
        thread.func = observer;
        thread.stack = observer_stack;
        thread.stack_size = sizeof(observer_stack);
        thread.gp_reg = &_gp;
        thread.initial_priority = observer_priority;
        id = CreateThread(&thread);
        if (id >= 0 && StartThread(id, NULL) >= 0) {
            started = 1;
            DelayThread(50000);
            if (ReferThreadStatus(id, &observer_status) >= 0)
                observer_actual_priority = observer_status.current_priority;
            before = observer_ticks;
            unyielding_target();
            observer_shutdown = 1;
            for (attempt = 0; attempt < 500 && !observer_exited; ++attempt)
                DelayThread(1000);
        } else if (id >= 0) {
            DeleteThread(id);
        }
    }

    passed = started && observer_exited &&
             observer_actual_priority == observer_priority &&
             busy_iterations > 0 &&
             (observer_priority >= main_priority || observer_during_busy > 0);
    printf("PS2GDB_SCHED_PROBE_DONE status=%d started=%d main_priority=%d observer_priority=%d observer_actual_priority=%d ticks_before=%u ticks_total=%u during_busy=%u busy_iterations=%u observer_exited=%d\n",
           passed ? 0 : 1,
           started, main_priority, observer_priority,
           observer_actual_priority, before,
           observer_ticks, observer_during_busy, busy_iterations,
           observer_exited);
    fflush(stdout);
    /* Never exit to OSD while being launched by PS2Link. */
    SleepThread();
    for (;;)
        SleepThread();
}
