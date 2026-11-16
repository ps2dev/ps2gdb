#ifndef PS2GDB_PREEMPTIVE_STOP_STATE_H
#define PS2GDB_PREEMPTIVE_STOP_STATE_H

#include <stdint.h>
#include "ee_context_candidate.h"

/*
 * Pure decision model; deliberately NOT wired to EE interrupts, sockets,
 * comparators or exception handlers. All callers must externally serialize
 * access. No claim that an IRQ callback can identify its interrupted thread.
 */
enum ps2gdb_stop_phase {
    PS2GDB_STOP_IDLE = 0,
    PS2GDB_STOP_REQUESTED,
    PS2GDB_STOP_ARMED,
    PS2GDB_STOP_DISARM_PENDING,
    PS2GDB_STOP_CAPTURED
};

enum ps2gdb_stop_decision {
    PS2GDB_STOP_IGNORE = 0,
    PS2GDB_STOP_ARM_ONE_SHOT,
    PS2GDB_STOP_RETRY_AFTER_DISARM,
    PS2GDB_STOP_REPORT,
    PS2GDB_STOP_REFUSE_AND_DISARM
};

struct ps2gdb_stop_state {
    enum ps2gdb_stop_phase phase;
    uint32_t generation;
    int target_thread;
    int receiver_thread;
    uint32_t executable_begin;
    uint32_t executable_end;
    uint32_t armed_pc;
    unsigned int wrong_thread_hits;
    int retry_after_disarm;
};

/* One wrong-thread event may be retried only after a verified disarm
 * and a NEW trusted/exclusive owner observation. The second event
 * aborts this request instead of entering a retrap loop. This is
 * policy only: no EE provider can currently attest exclusivity. */
#define PS2GDB_STOP_MAX_WRONG_THREAD_RETRIES 1u
/* The EE BPC.BED bit, from PS2SDK ee_cop0_defs.h. An unobserved write
 * request is NOT an acknowledgment: callers must supply a live BPC
 * readback after disabling the hardware comparator. */
#define PS2GDB_STOP_BPC_BED (1u << 15)

static inline int ps2gdb_stop_begin(struct ps2gdb_stop_state *state,
                                   uint32_t generation, int target_thread,
                                   int receiver_thread, uint32_t begin,
                                   uint32_t end, int comparator_in_use)
{
    if (!state || state->phase != PS2GDB_STOP_IDLE || generation == 0 ||
        generation <= state->generation || target_thread <= 0 ||
        receiver_thread <= 0 || target_thread == receiver_thread ||
        begin < 0x00100000u || begin >= end || end > 0x02000000u ||
        (begin & 3u) || (end & 3u) || comparator_in_use)
        return 0;
    state->generation = generation;
    state->target_thread = target_thread;
    state->receiver_thread = receiver_thread;
    state->executable_begin = begin;
    state->executable_end = end;
    state->armed_pc = 0;
    state->wrong_thread_hits = 0;
    state->retry_after_disarm = 0;
    state->phase = PS2GDB_STOP_REQUESTED;
    return 1;
}

/* An interrupt observation is not proof of thread identity or scheduler
 * exclusivity. Even with a known target ID, the comparator is per-CPU, NOT
 * per-thread. An independent provider must prove the target will be the
 * ONLY thread able to execute the compared PC until the comparator fires
 * or is disarmed. No such EE provider is currently implemented. */
static inline enum ps2gdb_stop_decision ps2gdb_stop_observe(
    struct ps2gdb_stop_state *state, uint32_t generation,
    int observed_thread, uint32_t observed_pc, int trusted_owner,
    int exclusive_target, uint32_t comparator_mask)
{
    if (!state || state->phase != PS2GDB_STOP_REQUESTED ||
        state->generation != generation || !trusted_owner ||
        !exclusive_target || comparator_mask != UINT32_MAX ||
        observed_thread != state->target_thread ||
        observed_pc < state->executable_begin ||
        observed_pc >= state->executable_end || (observed_pc & 3u))
        return PS2GDB_STOP_IGNORE;
    state->phase = PS2GDB_STOP_ARMED;
    state->armed_pc = observed_pc;
    return PS2GDB_STOP_ARM_ONE_SHOT;
}

/* After a wrong-thread capture, the comparator MUST be disarmed and
 * independently acknowledged before any subsequent arm decision. Even a
 * complete target frame cannot be reported while BPC.BED is unverified.
 * Never rearm from the exception handler or report the wrong thread. */
static inline enum ps2gdb_stop_decision ps2gdb_stop_capture(
    struct ps2gdb_stop_state *state, uint32_t generation,
    int observed_thread, int trusted_owner,
    const ee_context_candidate *frame, uint32_t live_bpc)
{
    if (!state || state->phase != PS2GDB_STOP_ARMED ||
        state->generation != generation)
        return PS2GDB_STOP_IGNORE;
    if (!trusted_owner || observed_thread != state->target_thread) {
        state->phase = PS2GDB_STOP_DISARM_PENDING;
        if (state->wrong_thread_hits <= PS2GDB_STOP_MAX_WRONG_THREAD_RETRIES)
            ++state->wrong_thread_hits;
        state->retry_after_disarm =
            state->wrong_thread_hits <= PS2GDB_STOP_MAX_WRONG_THREAD_RETRIES;
        return state->retry_after_disarm ?
            PS2GDB_STOP_RETRY_AFTER_DISARM :
            PS2GDB_STOP_REFUSE_AND_DISARM;
    }
    if (!ee_context_is_complete(frame) ||
        frame->interrupted_thread_id != observed_thread ||
        frame->epc != state->armed_pc ||
        (live_bpc & PS2GDB_STOP_BPC_BED) == 0) {
        state->retry_after_disarm = 0;
        state->phase = PS2GDB_STOP_DISARM_PENDING;
        return PS2GDB_STOP_REFUSE_AND_DISARM;
    }
    state->phase = PS2GDB_STOP_CAPTURED;
    return PS2GDB_STOP_REPORT;
}

/* Only a BPC readback with the hardware-debug disable bit set may move
 * this state out of quarantine. Simply sending a BPC write is NOT proof
 * it took effect. A pending retry requires a fresh exclusive owner claim.
 * This policy does not itself read hardware or establish readback origin. */
static inline int ps2gdb_stop_disarm_ack(struct ps2gdb_stop_state *state,
                                        uint32_t generation,
                                        uint32_t live_bpc)
{
    if (!state || state->phase != PS2GDB_STOP_DISARM_PENDING ||
        state->generation != generation ||
        (live_bpc & PS2GDB_STOP_BPC_BED) == 0)
        return 0;
    state->phase = state->retry_after_disarm ?
                   PS2GDB_STOP_REQUESTED : PS2GDB_STOP_IDLE;
    state->armed_pc = 0;
    state->retry_after_disarm = 0;
    return 1;
}

static inline int ps2gdb_stop_cancel(struct ps2gdb_stop_state *state,
                                     uint32_t generation)
{
    int need_disarm;
    if (!state || state->phase == PS2GDB_STOP_IDLE ||
        state->generation != generation)
        return 0;
    need_disarm = state->phase == PS2GDB_STOP_ARMED ||
                  state->phase == PS2GDB_STOP_DISARM_PENDING;
    state->phase = need_disarm ?
                   PS2GDB_STOP_DISARM_PENDING : PS2GDB_STOP_IDLE;
    if (!need_disarm)
        state->armed_pc = 0;
    state->retry_after_disarm = 0;
    return need_disarm; /* caller MUST disarm and acknowledge if nonzero */
}

/* The R5900 IABM matching rule: a zero mask matches every instruction. */
static inline int ps2gdb_stop_address_matches(uint32_t pc, uint32_t addr,
                                               uint32_t mask)
{
    return (pc & mask) == (addr & mask);
}

#endif
