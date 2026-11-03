#include <tamtypes.h>
#include <kernel.h>
#include <delaythread.h>
#include <ps2ips.h>
#include <string.h>

#include "ctrlc-receiver.h"
#include "ctrlc-receiver-state.h"

/* This code is intentionally off unless PS2GDB_CTRL_C_RECEIVER=1.
 * The experimental thread runs outside every EE exception context. */
#define RECEIVER_INTERVAL_US 15000
#define RECEIVER_STACK_SIZE 8192

static unsigned char receiver_stack[RECEIVER_STACK_SIZE]
    __attribute__((aligned(16)));
static struct ps2gdb_receiver_state receiver_state;
static int receiver_fd = -1;
static int receiver_thread_id = -1;
static volatile int receiver_shutdown;
static volatile int receiver_exited;

static int enter_critical(void)
{
    return DIntr();
}

static void leave_critical(int was_enabled)
{
    if (was_enabled)
        EIntr();
}

static void receiver_thread(void *arg)
{
    (void)arg;
    while (!receiver_shutdown) {
        unsigned int generation = 0;
        unsigned char ch = 0;
        int read_active, socket_fd, n, outcome;
        int old = enter_critical();
        read_active = ps2gdb_receiver_begin_read(&receiver_state,
                                                  &generation);
        socket_fd = receiver_fd;
        leave_critical(old);

        if (read_active && socket_fd < 0) {
            old = enter_critical();
            ps2gdb_receiver_finish_read(&receiver_state, generation, 0);
            leave_critical(old);
        }
        if (!read_active || socket_fd < 0) {
            DelayThread(RECEIVER_INTERVAL_US);
            continue;
        }

        /* Single-byte, nonblocking read: no TCP/RPC ever occurs in IRQ. */
        n = recv(socket_fd, &ch, 1, MSG_DONTWAIT);
        outcome = n == 1 ? (ch == 0x03 ? 1 : 2) : (n == 0 ? 0 : -1);

        old = enter_critical();
        ps2gdb_receiver_finish_read(&receiver_state, generation, outcome);
        leave_critical(old);
        DelayThread(RECEIVER_INTERVAL_US);
    }
    receiver_exited = 1;
    ExitThread();
}

int ps2gdb_receiver_init(void)
{
    ee_thread_t thread;
    int id;
    if (receiver_thread_id >= 0)
        return 0;
    memset(&receiver_state, 0, sizeof(receiver_state));
    memset(&thread, 0, sizeof(thread));
    receiver_fd = -1;
    receiver_shutdown = 0;
    receiver_exited = 0;
    thread.func = (void *)receiver_thread;
    thread.stack = receiver_stack;
    thread.stack_size = sizeof(receiver_stack);
    thread.gp_reg = &_gp;
    thread.initial_priority = 48;
    id = CreateThread(&thread);
    if (id < 0)
        return -1;
    receiver_thread_id = id;
    if (StartThread(id, NULL) < 0) {
        DeleteThread(id);
        receiver_thread_id = -1;
        return -1;
    }
    return 0;
}

void ps2gdb_receiver_resume(int socket_fd, unsigned int generation)
{
    int old = enter_critical();
    if (!receiver_shutdown && socket_fd >= 0 &&
        ps2gdb_receiver_run(&receiver_state, generation))
        receiver_fd = socket_fd;
    leave_critical(old);
}

void ps2gdb_receiver_pause(void)
{
    int in_read;
    do {
        int old = enter_critical();
        in_read = !ps2gdb_receiver_stop(&receiver_state);
        leave_critical(old);
        if (in_read)
            DelayThread(1000);
    } while (in_read);
}

int ps2gdb_receiver_take_interrupt(void)
{
    int old = enter_critical();
    int pending = ps2gdb_receiver_state_take_interrupt(&receiver_state);
    leave_critical(old);
    return pending;
}

int ps2gdb_receiver_take_disconnect(void)
{
    int old = enter_critical();
    int pending = ps2gdb_receiver_state_take_disconnect(&receiver_state);
    leave_critical(old);
    return pending;
}

void ps2gdb_receiver_close(void)
{
    int attempts;
    if (receiver_thread_id < 0)
        return;
    ps2gdb_receiver_pause();
    receiver_shutdown = 1;
    for (attempts = 0; attempts < 100 && !receiver_exited; ++attempts)
        DelayThread(1000);
    if (!receiver_exited)
        TerminateThread(receiver_thread_id);
    DeleteThread(receiver_thread_id);
    receiver_thread_id = -1;
    receiver_fd = -1;
}
