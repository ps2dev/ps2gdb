/* Native, safe hardware comparator/RSP bookkeeping regression tests. */
#include <stdio.h>
#include <string.h>
#include "../ee/hw-breakpoint-state.h"

static int failures;
static void check(int good, const char *message)
{
    if (!good) { fprintf(stderr, "FAIL %s\n", message); failures++; }
}

int main(void)
{
    struct ps2gdb_hw_state state = {0};
    struct ps2gdb_hw_slot slot = {0}, other = {0};
    int insert = -1;
    check(ps2gdb_hw_parse("Z1,101000,4", &slot, &insert) == PS2GDB_HW_OK && insert == 1,
          "hardware instruction breakpoint parse");
    check(ps2gdb_hw_change(&state, &slot, insert) == PS2GDB_HW_OK,
          "claim instruction comparator");
    check(ps2gdb_hw_change(&state, &slot, insert) == PS2GDB_HW_OK,
          "idempotent hardware breakpoint");
    check(ps2gdb_hw_parse("Z1,101004,4", &other, &insert) == PS2GDB_HW_OK &&
          ps2gdb_hw_change(&state, &other, insert) == PS2GDB_HW_EXHAUSTED,
          "reject second instruction comparator");
    check(ps2gdb_hw_parse("Z2,120000,4", &other, &insert) == PS2GDB_HW_OK &&
          ps2gdb_hw_change(&state, &other, insert) == PS2GDB_HW_OK,
          "data comparator independent of instruction comparator");
    check(ps2gdb_hw_parse("Z3,120000,4", &slot, &insert) == PS2GDB_HW_OK &&
          ps2gdb_hw_change(&state, &slot, insert) == PS2GDB_HW_EXHAUSTED,
          "read and write share one data comparator");
    check(ps2gdb_hw_parse("z2,120000,4", &other, &insert) == PS2GDB_HW_OK &&
          ps2gdb_hw_change(&state, &other, insert) == PS2GDB_HW_OK,
          "remove hardware watchpoint");
    check(ps2gdb_hw_change(&state, &other, insert) == PS2GDB_HW_NOT_FOUND,
          "reject mismatched remove");
    check(ps2gdb_hw_parse("Z4,120000,4", &slot, &insert) == PS2GDB_HW_OK &&
          ps2gdb_hw_change(&state, &slot, insert) == PS2GDB_HW_OK,
          "accept read/write watchpoint after freeing comparator");
    check(ps2gdb_hw_parse("Z2,01fffffe,4", &other, &insert) != PS2GDB_HW_OK,
          "reject outside memory range");
    check(ps2gdb_hw_parse("Z2,120003,4", &other, &insert) != PS2GDB_HW_OK,
          "reject unaligned data watchpoint");
    check(ps2gdb_hw_parse("Z1,101000,1", &other, &insert) != PS2GDB_HW_OK,
          "reject non-instruction-size hardware breakpoint");
    check(ps2gdb_hw_parse("Z2,120000,8", &other, &insert) == PS2GDB_HW_UNSUPPORTED,
          "reject unsupported comparator width");
    check(ps2gdb_hw_parse("Z2,120000,4junk", &other, &insert) == PS2GDB_HW_INVALID,
          "reject malformed suffix");
    check(ps2gdb_hw_parse("Z2,100000000,4", &other, &insert) == PS2GDB_HW_INVALID,
          "reject 32-bit address overflow");
    check(ps2gdb_hw_parse("Z3,20120000,4", &other, &insert) == PS2GDB_HW_OK &&
          other.address == 0x20120000u, "accept real-hardware uncached RAM read alias");
    check(ps2gdb_hw_parse("Z2,30120000,4", &other, &insert) == PS2GDB_HW_OK &&
          other.address == 0x30120000u, "preserve uncached accelerated RAM alias");
    check(ps2gdb_hw_parse("Z3,21fffffc,4", &other, &insert) == PS2GDB_HW_OK,
          "allow aligned last word in uncached RAM");
    check(ps2gdb_hw_parse("Z3,22000000,4", &other, &insert) == PS2GDB_HW_UNSUPPORTED,
          "reject addresses outside uncached RAM");
    check(ps2gdb_hw_parse("Z3,300bffff,1", &other, &insert) == PS2GDB_HW_UNSUPPORTED,
          "reject low memory outside application RAM alias");
    check(ps2gdb_hw_parse("Z", &other, &insert) == PS2GDB_HW_INVALID,
          "reject empty comparator packet");
    check(ps2gdb_hw_parse("Z2", &other, &insert) == PS2GDB_HW_INVALID,
          "reject truncated comparator packet");
    check(ps2gdb_hw_parse("Z2,", &other, &insert) == PS2GDB_HW_INVALID,
          "reject truncated comparator address");
    check(ps2gdb_hw_parse("Z2,12345", &other, &insert) == PS2GDB_HW_INVALID,
          "reject truncated comparator length");
    check(ps2gdb_hw_parse("Z2,12345,", &other, &insert) == PS2GDB_HW_INVALID,
          "reject empty comparator length");
    check(ps2gdb_hw_parse("Z0,101000,4", &other, &insert) == PS2GDB_HW_UNSUPPORTED,
          "software breakpoints stay separate from hardware resources");

    if (failures) return 1;
    puts("PASS GDB hardware-breakpoint packet parsing and comparator resource limits");
    return 0;
}
