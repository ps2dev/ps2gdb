#include <stdint.h>
#include <stdio.h>
#include "preemptive/ee_stop_state.h"

static int failures;
static void expect(int condition, const char *description)
{
    if (!condition) {
        fprintf(stderr, "FAIL %s\n", description);
        ++failures;
    }
}

int main(void)
{
    struct ps2gdb_stop_state state = {0};
    ee_context_candidate frame = {0};

    expect(!ps2gdb_stop_begin(&state, 1, 16, 16,
                              0x00100000u, 0x00200000u, 0),
           "receiver cannot be target");
    expect(!ps2gdb_stop_begin(&state, 1, 0, 33,
                              0x00100000u, 0x00200000u, 0),
           "unknown target cannot arm");
    expect(!ps2gdb_stop_begin(&state, 1, 16, 33,
                              0x00100000u, 0x00200000u, 1),
           "occupied comparator cannot arm");
    expect(!ps2gdb_stop_begin(&state, 1, 16, 33,
                              0x80000000u, 0x80010000u, 0),
           "kernel address rejected");
    expect(ps2gdb_stop_begin(&state, 1, 16, 33,
                             0x00100000u, 0x00200000u, 0),
           "valid request queued");
    expect(!ps2gdb_stop_begin(&state, 2, 16, 33,
                              0x00100000u, 0x00200000u, 0),
           "duplicate request cannot replace active state");
    expect(ps2gdb_stop_observe(&state, 1, 16, 0x00101118u, 0,
                               1, UINT32_MAX) ==
               PS2GDB_STOP_IGNORE, "IRQ PC is not a trusted thread identity");
    expect(ps2gdb_stop_observe(&state, 1, 33, 0x00101118u, 1,
                               1, UINT32_MAX) ==
               PS2GDB_STOP_IGNORE, "receiver thread cannot be armed");
    expect(ps2gdb_stop_observe(&state, 2, 16, 0x00101118u, 1,
                               1, UINT32_MAX) ==
               PS2GDB_STOP_IGNORE, "stale or incorrect session");
    expect(ps2gdb_stop_observe(&state, 1, 16, 0x80000200u, 1,
                               1, UINT32_MAX) ==
               PS2GDB_STOP_IGNORE, "kernel PC cannot be armed");
    expect(ps2gdb_stop_observe(&state, 1, 16, 0x00101118u, 1,
                               0, UINT32_MAX) ==
               PS2GDB_STOP_IGNORE,
           "trusted owner alone cannot prove thread exclusivity");
    expect(ps2gdb_stop_observe(&state, 1, 16, 0x00101118u, 1,
                               1, 0) ==
               PS2GDB_STOP_IGNORE, "wildcard comparator never arms");
    expect(ps2gdb_stop_observe(&state, 1, 16, 0x00101118u, 1,
                               1, 0xffff0000u) ==
               PS2GDB_STOP_IGNORE, "region comparator is not thread selective");
    expect(ps2gdb_stop_observe(&state, 1, 16, 0x00101118u, 1,
                               1, UINT32_MAX) ==
               PS2GDB_STOP_ARM_ONE_SHOT, "trusted target observation");
    expect(state.armed_pc == 0x00101118u,
           "record exact compared instruction, not only executable region");
    expect(ps2gdb_stop_observe(&state, 1, 16, 0x00101118u, 1,
                               1, UINT32_MAX) ==
               PS2GDB_STOP_IGNORE, "cannot arm again while armed");
    expect(ps2gdb_stop_capture(&state, 1, 33, 1, &frame, PS2GDB_STOP_BPC_BED) ==
               PS2GDB_STOP_RETRY_AFTER_DISARM,
           "wrong-thread debug exception must retry");
    expect(state.phase == PS2GDB_STOP_DISARM_PENDING,
           "wrong-thread event quarantined until disarm acknowledgment");
    expect(ps2gdb_stop_observe(&state, 1, 16, 0x00101118u, 1,
                               1, UINT32_MAX) ==
               PS2GDB_STOP_IGNORE, "cannot rearm before disarm acknowledgment");
    expect(!ps2gdb_stop_disarm_ack(&state, 2, PS2GDB_STOP_BPC_BED),
           "wrong generation cannot acknowledge comparator disarm");
    expect(!ps2gdb_stop_disarm_ack(&state, 1, 0),
           "unverified comparator disarm cannot release quarantine");
    expect(!ps2gdb_stop_disarm_ack(&state, 1, 0x00007fffu),
           "BPC enabled or unknown bits without BED never confirm disarm");
    expect(ps2gdb_stop_disarm_ack(&state, 1, PS2GDB_STOP_BPC_BED) &&
               state.phase == PS2GDB_STOP_REQUESTED &&
               state.armed_pc == 0,
           "verified disarm permits a fresh exclusive-target observation");
    expect(ps2gdb_stop_observe(&state, 1, 16, 0x00101118u, 1,
                               1, UINT32_MAX) ==
               PS2GDB_STOP_ARM_ONE_SHOT, "rearm only after verified disarm");
    expect(ps2gdb_stop_capture(&state, 1, 16, 1, &frame, PS2GDB_STOP_BPC_BED) ==
               PS2GDB_STOP_REFUSE_AND_DISARM,
           "incomplete target state fails closed");
    expect(state.phase == PS2GDB_STOP_DISARM_PENDING,
           "incomplete frame remains quarantined until disarm");
    expect(ps2gdb_stop_disarm_ack(&state, 1, PS2GDB_STOP_BPC_BED) &&
               state.phase == PS2GDB_STOP_IDLE,
           "incomplete target frame is rejected after confirmed disarm");
    expect(ps2gdb_stop_observe(&state, 1, 16, 0x00101118u, 1,
                               1, UINT32_MAX) ==
               PS2GDB_STOP_IGNORE, "canceled request cannot rearm");

    expect(ps2gdb_stop_begin(&state, 2, 16, 33,
                             0x00100000u, 0x00200000u, 0),
           "newer request accepted");
    expect(ps2gdb_stop_observe(&state, 2, 16, 0x00101118u, 1,
                               1, UINT32_MAX) ==
               PS2GDB_STOP_ARM_ONE_SHOT, "second session armed");
    expect(!ps2gdb_stop_cancel(&state, 1) &&
               state.phase == PS2GDB_STOP_ARMED,
           "stale cancellation cannot disarm a newer session");
    expect(ps2gdb_stop_cancel(&state, 2) &&
               state.phase == PS2GDB_STOP_DISARM_PENDING,
           "canceling armed session must quarantine until disarm");
    expect(!ps2gdb_stop_disarm_ack(&state, 1, PS2GDB_STOP_BPC_BED) &&
               state.phase == PS2GDB_STOP_DISARM_PENDING,
           "stale acknowledgment cannot free a newer request");
    expect(ps2gdb_stop_disarm_ack(&state, 2, PS2GDB_STOP_BPC_BED) &&
               state.phase == PS2GDB_STOP_IDLE,
           "verified disarm completes canceled request");
    expect(!ps2gdb_stop_begin(&state, 1, 16, 33,
                              0x00100000u, 0x00200000u, 0),
           "older session generation never reused");

    expect(ps2gdb_stop_begin(&state, 3, 16, 33,
                             0x00100000u, 0x00200000u, 0),
           "new session after cancel");
    expect(ps2gdb_stop_observe(&state, 3, 16, 0x00101118u, 1,
                               1, UINT32_MAX) ==
               PS2GDB_STOP_ARM_ONE_SHOT, "fresh session armed");
    frame.gpr_valid_mask = UINT32_MAX;
    frame.state_valid_mask = EE_CTX_REQUIRED_STATE;
    frame.epc = 0x00101118u;
    frame.interrupted_thread_id = 16;
    expect(ps2gdb_stop_capture(&state, 3, 16, 1, &frame, PS2GDB_STOP_BPC_BED) ==
               PS2GDB_STOP_REPORT, "synthetic fully proven frame accepted");
    expect(state.phase == PS2GDB_STOP_CAPTURED, "captured state");
    expect(!ps2gdb_stop_cancel(&state, 3) &&
               state.phase == PS2GDB_STOP_IDLE,
           "captured session cleanup");
    expect(ps2gdb_stop_begin(&state, 4, 16, 33,
                             0x00100000u, 0x00200000u, 0),
           "new session after completed stop");
    expect(ps2gdb_stop_observe(&state, 4, 16, 0x00101118u, 1,
                               1, UINT32_MAX) ==
               PS2GDB_STOP_ARM_ONE_SHOT, "rearm for mismatched frame");
    frame.interrupted_thread_id = 99;
    expect(ps2gdb_stop_capture(&state, 4, 16, 1, &frame, PS2GDB_STOP_BPC_BED) ==
               PS2GDB_STOP_REFUSE_AND_DISARM,
           "frame owner must match independently observed owner");
    expect(ps2gdb_stop_disarm_ack(&state, 4, PS2GDB_STOP_BPC_BED),
           "owner mismatch rejected only after confirmed disarm");

    expect(ps2gdb_stop_begin(&state, 5, 16, 33,
                             0x00100000u, 0x00200000u, 0),
           "new request for bounded wrong-thread attempts");
    expect(ps2gdb_stop_observe(&state, 5, 16, 0x00101118u, 1,
                               1, UINT32_MAX) == PS2GDB_STOP_ARM_ONE_SHOT,
           "first exclusive arm");
    expect(ps2gdb_stop_capture(&state, 5, 33, 1, &frame, PS2GDB_STOP_BPC_BED) ==
               PS2GDB_STOP_RETRY_AFTER_DISARM,
           "first wrong-thread hit requests disarm and retry");
    expect(ps2gdb_stop_disarm_ack(&state, 5, PS2GDB_STOP_BPC_BED),
           "first wrong-thread hit disarmed");
    expect(ps2gdb_stop_observe(&state, 5, 16, 0x00101118u, 1,
                               1, UINT32_MAX) == PS2GDB_STOP_ARM_ONE_SHOT,
           "second exclusive arm after disarm");
    expect(ps2gdb_stop_capture(&state, 5, 33, 1, &frame, PS2GDB_STOP_BPC_BED) ==
               PS2GDB_STOP_REFUSE_AND_DISARM,
           "repeated wrong-thread trap refuses further automatic retries");
    expect(state.phase == PS2GDB_STOP_DISARM_PENDING,
           "second wrong-thread hit stays quarantined");
    expect(ps2gdb_stop_disarm_ack(&state, 5, PS2GDB_STOP_BPC_BED) &&
               state.phase == PS2GDB_STOP_IDLE,
           "retry budget exhausted: stop request canceled");
    expect(ps2gdb_stop_observe(&state, 5, 16, 0x00101118u, 1,
                               1, UINT32_MAX) == PS2GDB_STOP_IGNORE,
           "no indefinite wrong-thread rearming");

    expect(ps2gdb_stop_begin(&state, 6, 16, 33,
                             0x00100000u, 0x00200000u, 0),
           "new request for frame EPC validation");
    expect(ps2gdb_stop_observe(&state, 6, 16, 0x00101118u, 1,
                               1, UINT32_MAX) == PS2GDB_STOP_ARM_ONE_SHOT,
           "new request armed");
    expect(!ps2gdb_stop_disarm_ack(&state, 6, PS2GDB_STOP_BPC_BED),
           "cannot acknowledge disarm while the comparator remains armed");
    frame.interrupted_thread_id = 16;
    frame.epc = 0x80000100u;
    expect(ps2gdb_stop_capture(&state, 6, 16, 1, &frame, PS2GDB_STOP_BPC_BED) ==
               PS2GDB_STOP_REFUSE_AND_DISARM,
           "kernel frame EPC must not masquerade as target breakpoint");
    expect(ps2gdb_stop_cancel(&state, 6) &&
               state.phase == PS2GDB_STOP_DISARM_PENDING,
           "cancel during disarm pending cannot bypass hardware cleanup");
    expect(ps2gdb_stop_disarm_ack(&state, 6, PS2GDB_STOP_BPC_BED) &&
               state.phase == PS2GDB_STOP_IDLE,
           "quarantined cancel completes only after confirmed disarm");

    expect(ps2gdb_stop_begin(&state, 7, 16, 33,
                             0x00100000u, 0x00200000u, 0),
           "new request for wrong-PC frame rejection");
    expect(ps2gdb_stop_observe(&state, 7, 16, 0x00101118u, 1,
                               1, UINT32_MAX) == PS2GDB_STOP_ARM_ONE_SHOT,
           "known exact instruction comparator");
    frame.epc = 0x0010111cu;
    expect(ps2gdb_stop_capture(&state, 7, 16, 1, &frame, PS2GDB_STOP_BPC_BED) ==
               PS2GDB_STOP_REFUSE_AND_DISARM,
           "same-thread stop at a different instruction is not the expected hit");
    expect(!ps2gdb_stop_begin(&state, 8, 16, 33,
                              0x00100000u, 0x00200000u, 0),
           "cannot start next session before comparator disabled");
    expect(ps2gdb_stop_disarm_ack(&state, 7, PS2GDB_STOP_BPC_BED) &&
               state.phase == PS2GDB_STOP_IDLE &&
               state.armed_pc == 0,
           "mismatched EPC cannot be retried without a fresh request");

    expect(ps2gdb_stop_begin(&state, 8, 16, 33,
                             0x00100000u, 0x00200000u, 0),
           "new request after verified mismatched-PC cleanup");
    expect(ps2gdb_stop_observe(&state, 8, 16, 0x00101118u, 1,
                               1, UINT32_MAX) == PS2GDB_STOP_ARM_ONE_SHOT,
           "new exact comparator armed");
    frame.epc = 0x00101118u;
    expect(ps2gdb_stop_capture(&state, 8, 16, 0, &frame, PS2GDB_STOP_BPC_BED) ==
               PS2GDB_STOP_RETRY_AFTER_DISARM,
           "owner identity untrusted even when stop PC matches");
    expect(ps2gdb_stop_disarm_ack(&state, 8, PS2GDB_STOP_BPC_BED),
           "untrusted identity forces a new ownership observation");
    expect(ps2gdb_stop_observe(&state, 8, 16, 0x00101118u, 0,
                               1, UINT32_MAX) == PS2GDB_STOP_IGNORE,
           "untrusted ownership cannot rearm a previously trapped PC");
    expect(!ps2gdb_stop_cancel(&state, 8) &&
               state.phase == PS2GDB_STOP_IDLE,
           "cancel pending request without active comparator");

    expect(ps2gdb_stop_begin(&state, 9, 16, 33,
                             0x00100000u, 0x00200000u, 0),
           "new request for live BPC stop verification");
    expect(ps2gdb_stop_observe(&state, 9, 16, 0x00101118u, 1,
                               1, UINT32_MAX) == PS2GDB_STOP_ARM_ONE_SHOT,
           "target exact-PC comparator armed");
    frame.interrupted_thread_id = 16;
    frame.epc = 0x00101118u;
    expect(ps2gdb_stop_capture(&state, 9, 16, 1, &frame, 0) ==
               PS2GDB_STOP_REFUSE_AND_DISARM,
           "complete context cannot be published while BPC disable is unverified");
    expect(state.phase == PS2GDB_STOP_DISARM_PENDING,
           "unverified comparator cannot be reported as a GDB stop");
    expect(ps2gdb_stop_disarm_ack(&state, 9, PS2GDB_STOP_BPC_BED) &&
               state.phase == PS2GDB_STOP_IDLE,
           "verified comparator disable completes rejected stop");

    expect(ps2gdb_stop_address_matches(0x00101118u, 0, 0) &&
           ps2gdb_stop_address_matches(0x80000200u, 0, 0),
           "zero IABM dangerously matches all PCs");
    expect(ps2gdb_stop_address_matches(0x00101118u, 0x00100000u,
                                       0xffff0000u),
           "region IABM matches target");
    expect(!ps2gdb_stop_address_matches(0x00201118u, 0x00100000u,
                                        0xffff0000u),
           "region IABM excludes distinct region");

    if (failures)
        return 1;
    puts("PASS EE preemptive Ctrl-C fail-closed ownership and session model");
    return 0;
}
