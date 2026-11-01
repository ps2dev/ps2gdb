#ifndef PS2GDB_HW_BREAKPOINT_STATE_H
#define PS2GDB_HW_BREAKPOINT_STATE_H

#include <stdint.h>
#include <string.h>
#include "r5900-rsp.h"

/*
 * GDB RSP hardware breakpoint/watchpoint bookkeeping. The EE has only one
 * instruction comparator and one shared data-address comparator. This pure
 * policy component deliberately does not arm hardware or claim RSP support
 * until a safe level-2 debug exception handoff has been validated.
 */
struct ps2gdb_hw_slot {
    uint32_t address;
    uint32_t size;
    unsigned int type;
};

struct ps2gdb_hw_state {
    struct ps2gdb_hw_slot instruction;
    struct ps2gdb_hw_slot data;
};

enum ps2gdb_hw_result {
    PS2GDB_HW_OK = 0,
    PS2GDB_HW_UNSUPPORTED,
    PS2GDB_HW_INVALID,
    PS2GDB_HW_EXHAUSTED,
    PS2GDB_HW_NOT_FOUND
};

static inline int ps2gdb_hw_parse_hex(const char **input, uint32_t *result)
{
    uint32_t value = 0;
    unsigned int count = 0;
    int hexval;
    while ((hexval = r5900_rsp_hex_digit((unsigned char)**input)) >= 0) {
        if (value > (UINT32_MAX >> 4)) return 0;
        value = (value << 4) | (unsigned int)hexval;
        ++*input;
        count++;
    }
    if (!count) return 0;
    *result = value;
    return 1;
}

/* EE direct-mapped RAM aliases also support uncached / uncached-accelerated
 * access. Keep the original virtual address in the comparator: only normalize
 * for range validation. Read hits depend on cache behavior on real hardware.
 */
static inline int ps2gdb_hw_valid_ram_range(uint32_t address, uint32_t length)
{
    uint32_t physical = address;
    if (address >= 0x20000000u && address < 0x22000000u)
        physical = address - 0x20000000u;
    else if (address >= 0x30000000u && address < 0x32000000u)
        physical = address - 0x30000000u;
    return r5900_rsp_valid_ram_range(physical, length);
}

/* Parse Ztype,addr,kind or ztype,addr,kind, without modifying any state. */
static inline enum ps2gdb_hw_result ps2gdb_hw_parse(const char *packet,
                                                     struct ps2gdb_hw_slot *slot,
                                                     int *insert)
{
    const char *cursor = packet;
    uint32_t type, address, length;
    if (*cursor == 'Z') *insert = 1;
    else if (*cursor == 'z') *insert = 0;
    else return PS2GDB_HW_INVALID;
    cursor++;
    if (!ps2gdb_hw_parse_hex(&cursor, &type) || *cursor != ',')
        return PS2GDB_HW_INVALID;
    cursor++;
    if (!ps2gdb_hw_parse_hex(&cursor, &address) || *cursor != ',')
        return PS2GDB_HW_INVALID;
    cursor++;
    if (!ps2gdb_hw_parse_hex(&cursor, &length) || *cursor)
        return PS2GDB_HW_INVALID;
    if (type < 1 || type > 4) return PS2GDB_HW_UNSUPPORTED;
    if (type == 1) {
        if (length != 4 || (address & 3)) return PS2GDB_HW_INVALID;
    } else if ((length != 1 && length != 2 && length != 4) ||
               (address & (length - 1)))
        return PS2GDB_HW_UNSUPPORTED;
    if (!ps2gdb_hw_valid_ram_range(address, length))
        return PS2GDB_HW_UNSUPPORTED;
    slot->type = type;
    slot->address = address;
    slot->size = length;
    return PS2GDB_HW_OK;
}

static inline enum ps2gdb_hw_result ps2gdb_hw_change(struct ps2gdb_hw_state *state,
                                                      const struct ps2gdb_hw_slot *slot,
                                                      int insert)
{
    struct ps2gdb_hw_slot *current = slot->type == 1 ?
                                      &state->instruction : &state->data;
    const int exact = current->type == slot->type &&
                      current->address == slot->address &&
                      current->size == slot->size;
    if (insert) {
        if (exact) return PS2GDB_HW_OK;
        if (current->type) return PS2GDB_HW_EXHAUSTED;
        *current = *slot;
        return PS2GDB_HW_OK;
    }
    if (!exact) return PS2GDB_HW_NOT_FOUND;
    memset(current, 0, sizeof(*current));
    return PS2GDB_HW_OK;
}

#endif
