#ifndef PS2GDB_CTRLC_RECEIVER_STATE_H
#define PS2GDB_CTRLC_RECEIVER_STATE_H

/*
 * Host-testable socket-owner handshake; no EE kernel or socket operations.
 * Each transition must be serialized by the EE runtime (DIntr/EIntr).
 * Only one receiver can own a read, and only during a running session.
 */
struct ps2gdb_receiver_state {
    volatile unsigned int generation;
    volatile int running;
    volatile int in_read;
    volatile int interrupted;
    volatile int disconnected;
};

static inline int ps2gdb_receiver_run(struct ps2gdb_receiver_state *state,
                                      unsigned int generation)
{
    if (!state || !generation || generation < state->generation || state->in_read ||
        (state->running && state->generation == generation))
        return 0;
    state->generation = generation;
    state->running = 1;
    state->interrupted = 0;
    state->disconnected = 0;
    return 1;
}

static inline int ps2gdb_receiver_begin_read(struct ps2gdb_receiver_state *state,
                                             unsigned int *generation)
{
    if (!state || !generation || !state->running || state->in_read ||
        state->interrupted || state->disconnected)
        return 0;
    state->in_read = 1;
    *generation = state->generation;
    return 1;
}

/* result: 1=raw Ctrl-C, 0=EOF, -1=no byte, other byte=protocol
 * violation. Non-Ctrl-C bytes must never be silently lost. */
static inline void ps2gdb_receiver_finish_read(struct ps2gdb_receiver_state *state,
                                               unsigned int generation,
                                               int result)
{
    if (!state || !state->in_read)
        return;
    state->in_read = 0;
    if (!state->running || state->generation != generation)
        return;
    if (result == 1) {
        state->interrupted = 1;
        state->running = 0;
    } else if (result == 0 || result > 1) {
        /* A protocol violation or disconnect must not silently continue. */
        state->disconnected = 1;
        state->running = 0;
    }
}

/* The stopped RSP parser may only resume when in_read==0. */
static inline int ps2gdb_receiver_stop(struct ps2gdb_receiver_state *state)
{
    if (!state)
        return 0;
    state->running = 0;
    return !state->in_read;
}

static inline int ps2gdb_receiver_state_take_interrupt(struct ps2gdb_receiver_state *state)
{
    int result;
    if (!state || state->in_read)
        return 0;
    result = state->interrupted;
    state->interrupted = 0;
    return result;
}

static inline int ps2gdb_receiver_state_take_disconnect(struct ps2gdb_receiver_state *state)
{
    int result;
    if (!state || state->in_read)
        return 0;
    result = state->disconnected;
    state->disconnected = 0;
    return result;
}

#endif
