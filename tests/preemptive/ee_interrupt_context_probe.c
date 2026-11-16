/*
 * Isolated, non-mutating EE interrupt-context investigation.
 * Never interpret/dereference the opaque third callback argument.
 * No network, file I/O, or thread operations from the interrupt handler.
 * This is NOT a GDB stop implementation.
 */
#include <kernel.h>
#include <stdio.h>

static volatile unsigned int irq_hits;
static volatile unsigned int first_callback_addr;
static volatile unsigned int last_callback_addr;
static volatile unsigned int first_handler_epc;
static volatile unsigned int last_handler_epc;
static volatile unsigned int first_handler_cause;
static volatile unsigned int last_handler_cause;
static volatile unsigned int first_handler_status;
static volatile unsigned int last_handler_status;
static volatile unsigned int busy_iterations;
static volatile unsigned int busy_active;
static volatile unsigned int busy_irq_hits;
static volatile unsigned int busy_irq_first_pc;
static volatile unsigned int busy_irq_last_pc;
static volatile unsigned int busy_irq_min_pc = 0xffffffffu;
static volatile unsigned int busy_irq_max_pc;

extern unsigned char ps2gdb_probe_busy_begin[];
extern unsigned char ps2gdb_probe_busy_end[];

__attribute__((noinline)) static void run_unyielding_busy_loop(void)
{
    busy_active = 1;
    __asm__ volatile(".globl ps2gdb_probe_busy_begin\nps2gdb_probe_busy_begin:" ::: "memory");
    while (irq_hits < 12 && busy_iterations < 400000000u)
        busy_iterations++;
    /* Clear the observation guard before the return/epilogue so an IRQ
     * at the return instruction is not falsely classified as busy-loop PC. */
    busy_active = 0;
    __asm__ volatile(".globl ps2gdb_probe_busy_end\nps2gdb_probe_busy_end:\n nop" ::: "memory");
}

static int observe_vblank(int cause, void *user, void *addr)
{
    unsigned int epc, cp0_cause, cp0_status;
    (void)cause;
    (void)user;
    __asm__ volatile("mfc0 %0, $14" : "=r"(epc));
    __asm__ volatile("mfc0 %0, $13" : "=r"(cp0_cause));
    __asm__ volatile("mfc0 %0, $12" : "=r"(cp0_status));
    if (irq_hits == 0) {
        first_callback_addr = (unsigned int)addr;
        first_handler_epc = epc;
        first_handler_cause = cp0_cause;
        first_handler_status = cp0_status;
    }
    last_callback_addr = (unsigned int)addr;
    last_handler_epc = epc;
    last_handler_cause = cp0_cause;
    last_handler_status = cp0_status;
    if (busy_active) {
        unsigned int interrupted_pc = (unsigned int)addr;
        if (busy_irq_hits == 0)
            busy_irq_first_pc = interrupted_pc;
        busy_irq_last_pc = interrupted_pc;
        if (interrupted_pc < busy_irq_min_pc)
            busy_irq_min_pc = interrupted_pc;
        if (interrupted_pc > busy_irq_max_pc)
            busy_irq_max_pc = interrupted_pc;
        ++busy_irq_hits;
    }
    ++irq_hits;
    ExitHandler();
    return 0;
}

int main(void)
{
    int handler_id, old_intr;
    int ok;
    unsigned int hits;
    printf("PS2GDB_PREEMPT_PROBE_BEGIN thread=%d\n", GetThreadId());
    fflush(stdout);

    old_intr = DIntr();
    handler_id = AddIntcHandler2(INTC_VBLANK_S, observe_vblank, -1, NULL);
    if (handler_id >= 0)
        EnableIntc(INTC_VBLANK_S);
    if (old_intr) EIntr();
    if (handler_id < 0) {
        printf("PS2GDB_PREEMPT_PROBE_DONE status=1 add_handler=%d\n", handler_id);
        fflush(stdout);
        SleepThread();
        for (;;) SleepThread();
    }

    /* This loop does not poll any debugger flag or yield. It only observes
     * the IRQ counter once per iteration. INTC interrupts run asynchronously. */
    run_unyielding_busy_loop();

    old_intr = DIntr();
    /* Match PS2SDK's VBlank teardown: mask before removing the callback. */
    DisableIntc(INTC_VBLANK_S);
    RemoveIntcHandler(INTC_VBLANK_S, handler_id);
    if (old_intr) EIntr();
    hits = irq_hits;
    ok = hits >= 12 && first_callback_addr != 0 &&
         busy_irq_hits >= 8 &&
         busy_irq_min_pc >= (unsigned int)ps2gdb_probe_busy_begin &&
         busy_irq_max_pc < (unsigned int)ps2gdb_probe_busy_end;
    printf("PS2GDB_PREEMPT_PROBE_IRQ hits=%u iterations=%u arg_first=%08x arg_last=%08x cp0_epc_first=%08x cp0_epc_last=%08x\n",
           hits, busy_iterations, first_callback_addr, last_callback_addr,
           first_handler_epc, last_handler_epc);
    printf("PS2GDB_PREEMPT_PROBE_APPPC busy_hits=%u busy_first=%08x busy_last=%08x busy_min=%08x busy_max=%08x busy_begin=%08x busy_end=%08x\n",
           busy_irq_hits, busy_irq_first_pc, busy_irq_last_pc,
           busy_irq_min_pc, busy_irq_max_pc,
           (unsigned int)ps2gdb_probe_busy_begin,
           (unsigned int)ps2gdb_probe_busy_end);
    printf("PS2GDB_PREEMPT_PROBE_CP0 cause_first=%08x cause_last=%08x status_first=%08x status_last=%08x\n",
           first_handler_cause, last_handler_cause,
           first_handler_status, last_handler_status);
    printf("PS2GDB_PREEMPT_PROBE_DONE status=%d\n", ok ? 0 : 2);
    fflush(stdout);
    /* Do not return to the OSD. The PS2Link launcher retains this sleeping
     * user thread, so another execee requires a fresh launcher session. */
    SleepThread();
    for (;;) SleepThread();
}
