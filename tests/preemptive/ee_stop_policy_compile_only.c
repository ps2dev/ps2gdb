/*
 * EE cross-compiler smoke test of the host-tested stop policy.
 * This object is deliberately NEVER linked into a running PS2 binary.
 * In particular, no interrupt or hardware comparator can be armed here.
 */
#include "ee_stop_state.h"
#include <ee_cop0_defs.h>

#if PS2GDB_STOP_BPC_BED != EE_BPC_BED
#error Stop-policy disable-bit contract differs from the installed PS2SDK
#endif

int ps2gdb_stop_policy_compile_only(void)
{
    struct ps2gdb_stop_state state = {0};
    const uint32_t start = 0x00101118u;
    if (!ps2gdb_stop_begin(&state, 1, 16, 33,
                           0x00100000u, 0x00200000u, 0))
        return -1;
    /* There is no EE owner/exclusivity observer yet. This must NEVER
     * yield a hardware arming decision. */
    return ps2gdb_stop_observe(&state, 1, 16, start, 0, 0,
                               UINT32_MAX) == PS2GDB_STOP_IGNORE ? 0 : -1;
}
