#include <stdio.h>
#include <string.h>
#include "../ee/ctrlc-receiver-state.h"

static int failures;
static void check(int result, const char *message)
{
    if (!result) {
        fprintf(stderr, "FAIL %s\n", message);
        ++failures;
    }
}

int main(void)
{
    struct ps2gdb_receiver_state state = {0};
    unsigned int generation = 0;

    check(!ps2gdb_receiver_begin_read(&state, &generation),
          "receiver cannot read while stopped");
    check(!ps2gdb_receiver_run(&state, 0), "session zero invalid");
    check(ps2gdb_receiver_run(&state, 1), "begin first run");
    check(!ps2gdb_receiver_run(&state, 1), "reject duplicate arm");
    check(ps2gdb_receiver_begin_read(&state, &generation) && generation == 1,
          "receiver has sole ownership of current run");
    check(!ps2gdb_receiver_begin_read(&state, &generation),
          "only one concurrent socket read");
    check(!ps2gdb_receiver_stop(&state),
          "RSP parser blocked until in-flight read completes");
    ps2gdb_receiver_finish_read(&state, generation, 1);
    check(ps2gdb_receiver_stop(&state), "RSP parser can now own socket");
    check(!ps2gdb_receiver_state_take_interrupt(&state),
          "stale Ctrl-C from stopped session discarded");

    check(ps2gdb_receiver_run(&state, 2), "second run");
    check(ps2gdb_receiver_begin_read(&state, &generation), "second read");
    ps2gdb_receiver_finish_read(&state, generation, -1);
    check(!ps2gdb_receiver_state_take_interrupt(&state), "EAGAIN not Ctrl-C");
    check(ps2gdb_receiver_begin_read(&state, &generation), "read after EAGAIN");
    ps2gdb_receiver_finish_read(&state, generation, 1);
    check(ps2gdb_receiver_state_take_interrupt(&state), "Ctrl-C delivered once");
    check(!ps2gdb_receiver_state_take_interrupt(&state), "no duplicate Ctrl-C");
    check(!ps2gdb_receiver_begin_read(&state, &generation),
          "receiver releases socket after Ctrl-C");

    check(ps2gdb_receiver_run(&state, 3), "rearm after stop");
    check(ps2gdb_receiver_begin_read(&state, &generation), "read after rearm");
    ps2gdb_receiver_finish_read(&state, generation, 0);
    check(ps2gdb_receiver_state_take_disconnect(&state), "EOF delivered once");
    check(!ps2gdb_receiver_state_take_disconnect(&state), "EOF not duplicated");

    check(ps2gdb_receiver_run(&state, 4), "rearm after disconnect");
    check(ps2gdb_receiver_begin_read(&state, &generation), "read protocol byte");
    ps2gdb_receiver_finish_read(&state, generation, 2);
    check(ps2gdb_receiver_state_take_disconnect(&state),
          "unexpected RSP byte must stop, not be dropped silently");

    check(ps2gdb_receiver_run(&state, 5), "rearm after bad byte");
    check(ps2gdb_receiver_begin_read(&state, &generation), "read generation five");
    ps2gdb_receiver_finish_read(&state, generation - 1, 1);
    check(!ps2gdb_receiver_state_take_interrupt(&state), "wrong-generation Ctrl-C rejected");
    check(ps2gdb_receiver_stop(&state), "explicit stop after stale receive");
    check(!ps2gdb_receiver_run(&state, 4), "older session cannot reclaim socket");
    check(ps2gdb_receiver_run(&state, 5), "current session can resume");
    check(ps2gdb_receiver_stop(&state), "current session stops cleanly");
    check(!ps2gdb_receiver_run(NULL, 6), "null state rejected");
    check(!ps2gdb_receiver_stop(NULL), "null stop rejected");

    if (failures)
        return 1;
    puts("PASS independent Ctrl-C socket receiver ownership and generation model");
    return 0;
}
