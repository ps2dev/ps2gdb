/*
 * Emulator-only scheduling research; NEVER auto-run on physical PS2.
 *
 * A priority-zero helper attempts to suspend one identified priority-one
 * target while that target is in a timed, unyielding loop. This does not
 * involve GDB, debug comparators, exception vectors or network requests.
 * Even a successful suspend/resume does not provide register provenance.
 */
#include <kernel.h>
#include <delaythread.h>
#include <stdio.h>

#define TARGET_BUSY_TICKS 150000000u
#define OBSERVER_DELAY_US 100000u

static volatile unsigned int target_busy;
static volatile unsigned int target_iterations;
static volatile unsigned int before_suspend;
static volatile unsigned int after_suspend;
static volatile int observed_running;
static volatile int suspend_result = -1;
static volatile int resume_result = -1;
static volatile int observer_done;
static int target_thread_id;
static unsigned char observer_stack[8192] __attribute__((aligned(16)));

extern unsigned char ps2gdb_suspend_loop_begin[];
extern unsigned char ps2gdb_suspend_loop_end[];

static void observer(void *unused)
{
    ee_thread_status_t thread_status = {0};
    int i;
    (void)unused;

    DelayThread(OBSERVER_DELAY_US);
    for (i = 0; i < 100 && !target_busy; ++i)
        DelayThread(1000);

    if (target_busy) {
        observed_running = 1;
        before_suspend = target_iterations;
        suspend_result = SuspendThread(target_thread_id);
        if (suspend_result >= 0) {
            /* Observe the thread state, but do not read internal frames. */
            if (ReferThreadStatus(target_thread_id, &thread_status) >= 0 &&
                thread_status.status != THS_SUSPEND)
                observed_running = 0;
            DelayThread(50000);
            after_suspend = target_iterations;
            resume_result = ResumeThread(target_thread_id);
        }
    }
    observer_done = 1;
    ExitThread();
}

__attribute__((noinline)) static void unyielding_target(void)
{
    unsigned int start, now;
    __asm__ volatile("mfc0 %0, $9" : "=r"(start));
    target_busy = 1;
    __asm__ volatile(".globl ps2gdb_suspend_loop_begin\n"
                     "ps2gdb_suspend_loop_begin:" ::: "memory");
    do {
        ++target_iterations;
        __asm__ volatile("mfc0 %0, $9" : "=r"(now));
    } while ((unsigned int)(now - start) < TARGET_BUSY_TICKS);
    target_busy = 0;
    __asm__ volatile(".globl ps2gdb_suspend_loop_end\n"
                     "ps2gdb_suspend_loop_end:\n"
                     "nop" ::: "memory");
}

int main(void)
{
    ee_thread_t thread = {0};
    ee_thread_status_t self = {0};
    ee_thread_status_t observer_status = {0};
    int started = 0, main_priority = -1;
    int observer_actual_priority = -1;
    int passed;

    target_thread_id = GetThreadId();
    if (ReferThreadStatus(TH_SELF, &self) >= 0)
        main_priority = self.current_priority;

    if (main_priority == 1) {
        int observer_id;
        thread.func = observer;
        thread.stack = observer_stack;
        thread.stack_size = sizeof(observer_stack);
        thread.gp_reg = &_gp;
        thread.initial_priority = 0;
        observer_id = CreateThread(&thread);
        if (observer_id >= 0) {
            if (StartThread(observer_id, NULL) >= 0) {
                started = 1;
                if (ReferThreadStatus(observer_id, &observer_status) >= 0)
                    observer_actual_priority = observer_status.current_priority;
            } else {
                DeleteThread(observer_id);
            }
        }
    }
    if (started) {
        int i;
        unyielding_target();
        for (i = 0; i < 100 && !observer_done; ++i)
            DelayThread(1000);
    }

    passed = started && observer_actual_priority == 0 &&
             observer_done && observed_running &&
             suspend_result >= 0 && resume_result >= 0 &&
             before_suspend == after_suspend &&
             target_iterations > after_suspend;
    printf("PS2GDB_SUSPEND_PROBE_DONE status=%d main_priority=%d observer_priority=%d started=%d observer_done=%d saw_suspended=%d suspend_rc=%d resume_rc=%d before=%u after=%u final=%u\n",
           passed ? 0 : 1, main_priority, observer_actual_priority,
           started, observer_done,
           observed_running, suspend_result, resume_result,
           before_suspend, after_suspend, target_iterations);
    fflush(stdout);

    /* Returning to OSD after a PS2Link launch would lose the session. */
    SleepThread();
    for (;;)
        SleepThread();
}
