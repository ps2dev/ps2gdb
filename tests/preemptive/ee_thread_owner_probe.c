/*
 * Read-only experiment for EE interrupt-time thread ID provenance.
 * The callback only samples the kernel's interrupt-context thread-ID syscall
 * and the opaque callback PC; it never mutates the thread, CPU exception
 * state, a comparator, a saved register frame, or a network socket.
 *
 * This is NOT a preemptive GDB stop and MUST be emulator-tested before
 * manual physical-console execution. No CI job executes this ELF.
 */
#include <kernel.h>
#include <stdio.h>
#include "ee_thread_owner_observation.h"

#define OWNER_HITS 12u
#define OWNER_MAX_ITERATIONS 450000000u
#define OWNER_MAIN_PHASE 1u
#define OWNER_WORKER_PHASE 2u

static volatile struct ps2gdb_owner_observation observations[3];
static volatile unsigned int iterations[3];
static volatile unsigned int active_phase;
static volatile int main_thread_id;
static volatile int worker_thread_id;
static volatile int worker_done;
static unsigned char worker_stack[8192] __attribute__((aligned(16)));

extern unsigned char ps2gdb_owner_busy_begin[];
extern unsigned char ps2gdb_owner_busy_end[];

__attribute__((noinline)) static void unyielding_loop(unsigned int phase)
{
    active_phase = phase;
    __asm__ volatile(".globl ps2gdb_owner_busy_begin\n"
                     "ps2gdb_owner_busy_begin:" ::: "memory");
    while (observations[phase].samples < OWNER_HITS &&
           iterations[phase] < OWNER_MAX_ITERATIONS)
        ++iterations[phase];
    active_phase = 0;
    __asm__ volatile(".globl ps2gdb_owner_busy_end\n"
                     "ps2gdb_owner_busy_end:\n"
                     "nop" ::: "memory");
}

static int observe_vblank(int cause, void *user, void *addr)
{
    unsigned int phase = active_phase;
    (void)cause;
    (void)user;

    if (phase == OWNER_MAIN_PHASE || phase == OWNER_WORKER_PHASE) {
        int observed_id;
        /*
         * The syscall result is an empirical observation, NOT a trusted
         * assertion that the saved application context belongs to this ID.
         */
        observed_id = _iGetThreadId();
        ps2gdb_owner_record(&observations[phase],
                           phase == OWNER_MAIN_PHASE ?
                               main_thread_id : worker_thread_id,
                           observed_id, (unsigned int)addr,
                           (unsigned int)ps2gdb_owner_busy_begin,
                           (unsigned int)ps2gdb_owner_busy_end);
    }
    ExitHandler();
    return 0;
}

static void worker(void *arg)
{
    (void)arg;
    worker_thread_id = GetThreadId();
    unyielding_loop(OWNER_WORKER_PHASE);
    worker_done = 1;
    WakeupThread(main_thread_id);
    ExitThread();
}

int main(void)
{
    ee_thread_t thread = {0};
    int handler_id, old_intr, worker_id = -1, ok;
    unsigned int phase;

    main_thread_id = GetThreadId();
    printf("PS2GDB_OWNER_PROBE_BEGIN main=%d\n", main_thread_id);
    fflush(stdout);

    old_intr = DIntr();
    handler_id = AddIntcHandler2(INTC_VBLANK_S, observe_vblank, -1, NULL);
    if (handler_id >= 0)
        EnableIntc(INTC_VBLANK_S);
    if (old_intr)
        EIntr();

    if (handler_id >= 0) {
        unyielding_loop(OWNER_MAIN_PHASE);
        thread.func = worker;
        thread.stack = worker_stack;
        thread.stack_size = sizeof(worker_stack);
        thread.gp_reg = &_gp;
        thread.initial_priority = 48;
        worker_id = CreateThread(&thread);
        if (worker_id >= 0) {
            if (StartThread(worker_id, NULL) >= 0)
                SleepThread();
            else {
                DeleteThread(worker_id);
                worker_id = -1;
            }
        }
    }

    old_intr = DIntr();
    if (handler_id >= 0) {
        DisableIntc(INTC_VBLANK_S);
        RemoveIntcHandler(INTC_VBLANK_S, handler_id);
    }
    if (old_intr)
        EIntr();

    ok = handler_id >= 0 && worker_id >= 0 && worker_done &&
         observations[OWNER_MAIN_PHASE].samples >= OWNER_HITS &&
         observations[OWNER_WORKER_PHASE].samples >= OWNER_HITS;
    for (phase = OWNER_MAIN_PHASE; phase <= OWNER_WORKER_PHASE; ++phase) {
        printf("PS2GDB_OWNER_PROBE_PHASE phase=%u expected=%d samples=%u matches=%u other=%u unknown=%u outside=%u first_pc=%08x last_pc=%08x iterations=%u\n",
               phase,
               phase == OWNER_MAIN_PHASE ? main_thread_id : worker_thread_id,
               observations[phase].samples, observations[phase].matches,
               observations[phase].other_threads,
               observations[phase].unknown_ids,
               observations[phase].outside_range,
               (unsigned int)observations[phase].first_pc,
               (unsigned int)observations[phase].last_pc, iterations[phase]);
    }
    printf("PS2GDB_OWNER_PROBE_DONE status=%d worker=%d\n",
           ok ? 0 : 1, worker_thread_id);
    fflush(stdout);

    /* Preserve PS2Link instead of returning to the OSD. */
    SleepThread();
    for (;;)
        SleepThread();
}
