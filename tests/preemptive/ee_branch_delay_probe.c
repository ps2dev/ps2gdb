/*
 * Read-only VBlank observation around a fixed R5900 branch/delay-slot loop.
 * This does not install an exception vector or stop an application thread.
 */
#include <kernel.h>
#include <stdio.h>

#define BRANCH_ITERATIONS 120000000u

static volatile unsigned int irq_hits;
static volatile unsigned int loop_active;
static volatile unsigned int loop_hits;
static volatile unsigned int in_range_hits;
static volatile unsigned int outside_hits;
static volatile unsigned int decrement_hits;
static volatile unsigned int branch_hits;
static volatile unsigned int delay_hits;
static volatile unsigned int other_hits;
static volatile unsigned int first_pc;
static volatile unsigned int last_pc;

extern unsigned char ps2gdb_branch_loop_begin[];
extern unsigned char ps2gdb_branch_loop_dec[];
extern unsigned char ps2gdb_branch_loop_branch[];
extern unsigned char ps2gdb_branch_loop_delay[];
extern unsigned char ps2gdb_branch_loop_end[];

__attribute__((noinline)) static unsigned int run_branch_delay_loop(void)
{
    unsigned int remaining = BRANCH_ITERATIONS;
    unsigned int delay_slots;

    /* The delay slot increments exactly once for each loop iteration.
     * GCC allocates distinct registers; the branch and its slot stay adjacent. */
    __asm__ volatile(
        ".set push\n"
        ".set noreorder\n"
        ".globl ps2gdb_branch_loop_begin\n"
        "ps2gdb_branch_loop_begin:\n"
        "move %1, $zero\n"
        ".globl ps2gdb_branch_loop_dec\n"
        "ps2gdb_branch_loop_dec:\n"
        "addiu %0, %0, -1\n"
        ".globl ps2gdb_branch_loop_branch\n"
        "ps2gdb_branch_loop_branch:\n"
        "bnez %0, ps2gdb_branch_loop_dec\n"
        ".globl ps2gdb_branch_loop_delay\n"
        "ps2gdb_branch_loop_delay:\n"
        "addiu %1, %1, 1\n"
        ".globl ps2gdb_branch_loop_end\n"
        "ps2gdb_branch_loop_end:\n"
        ".set pop\n"
        : "+&r"(remaining), "=&r"(delay_slots)
        :
        : "memory");

    return remaining + delay_slots;
}

static int observe_vblank(int cause, void *user, void *addr)
{
    unsigned int pc = (unsigned int)addr;
    (void)cause;
    (void)user;

    if (loop_active) {
        if (loop_hits == 0)
            first_pc = pc;
        last_pc = pc;
        ++loop_hits;

        if (pc >= (unsigned int)ps2gdb_branch_loop_begin &&
            pc < (unsigned int)ps2gdb_branch_loop_end) {
            ++in_range_hits;
            if (pc == (unsigned int)ps2gdb_branch_loop_dec)
                ++decrement_hits;
            else if (pc == (unsigned int)ps2gdb_branch_loop_branch)
                ++branch_hits;
            else if (pc == (unsigned int)ps2gdb_branch_loop_delay)
                ++delay_hits;
            else
                ++other_hits;
        } else {
            ++outside_hits;
        }
    }

    ++irq_hits;
    ExitHandler();
    return 0;
}

int main(void)
{
    int handler_id, old_intr, ok;
    unsigned int slots;

    printf("PS2GDB_BRANCH_PROBE_BEGIN thread=%d iterations=%u\n",
           GetThreadId(), BRANCH_ITERATIONS);
    fflush(stdout);

    old_intr = DIntr();
    handler_id = AddIntcHandler2(INTC_VBLANK_S, observe_vblank, -1, NULL);
    if (handler_id >= 0)
        EnableIntc(INTC_VBLANK_S);
    if (old_intr)
        EIntr();

    if (handler_id < 0) {
        printf("PS2GDB_BRANCH_PROBE_DONE status=1 add_handler=%d\n", handler_id);
        fflush(stdout);
        SleepThread();
        for (;;)
            SleepThread();
    }

    loop_active = 1;
    slots = run_branch_delay_loop();
    loop_active = 0;

    old_intr = DIntr();
    /* Mask the source before unregistering its handler. Otherwise a later
     * VBlank could reach the kernel with no callback installed. */
    DisableIntc(INTC_VBLANK_S);
    RemoveIntcHandler(INTC_VBLANK_S, handler_id);
    if (old_intr)
        EIntr();

    ok = slots == BRANCH_ITERATIONS && in_range_hits >= 8;
    printf("PS2GDB_BRANCH_PROBE_IRQ hits=%u loop_hits=%u in_range=%u outside=%u first=%08x last=%08x\n",
           irq_hits, loop_hits, in_range_hits, outside_hits, first_pc, last_pc);
    printf("PS2GDB_BRANCH_PROBE_PC dec=%08x branch=%08x delay=%08x begin=%08x end=%08x dec_hits=%u branch_hits=%u delay_hits=%u other_hits=%u\n",
           (unsigned int)ps2gdb_branch_loop_dec,
           (unsigned int)ps2gdb_branch_loop_branch,
           (unsigned int)ps2gdb_branch_loop_delay,
           (unsigned int)ps2gdb_branch_loop_begin,
           (unsigned int)ps2gdb_branch_loop_end,
           decrement_hits, branch_hits, delay_hits, other_hits);
    printf("PS2GDB_BRANCH_PROBE_DONE status=%d slots=%u\n", ok ? 0 : 2, slots);
    fflush(stdout);
    SleepThread();
    for (;;)
        SleepThread();
}
