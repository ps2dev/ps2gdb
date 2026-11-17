#ifndef PS2GDB_EE_THREAD_OWNER_OBSERVATION_H
#define PS2GDB_EE_THREAD_OWNER_OBSERVATION_H

#include <stdint.h>

/*
 * Read-only statistics from an interrupt callback. Even a matching ID and
 * application PC are observations, NOT an attestation of thread exclusivity
 * or a valid register snapshot. Never feed this directly into stop_observe.
 */
struct ps2gdb_owner_observation {
    unsigned int samples;
    unsigned int matches;
    unsigned int other_threads;
    unsigned int unknown_ids;
    unsigned int outside_range;
    uint32_t first_pc;
    uint32_t last_pc;
};

static inline void ps2gdb_owner_record(
    volatile struct ps2gdb_owner_observation *observation,
    int expected_thread, int observed_thread, uint32_t pc,
    uint32_t loop_begin, uint32_t loop_end)
{
    if (!observation)
        return;
    if (!observation->samples)
        observation->first_pc = pc;
    observation->last_pc = pc;
    ++observation->samples;

    if (!pc || pc < loop_begin || pc >= loop_end) {
        ++observation->outside_range;
    } else if (observed_thread <= 0 || expected_thread <= 0) {
        ++observation->unknown_ids;
    } else if (observed_thread == expected_thread) {
        ++observation->matches;
    } else {
        ++observation->other_threads;
    }
}

#endif
