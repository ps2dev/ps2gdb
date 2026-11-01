# EE hardware breakpoints and watchpoints — investigation

This is experimental groundwork, **not** an enabled RSP feature. GDB's
`Z1`/`z1` (instruction comparator) and `Z2`–`Z4` (shared data comparator)
remain unsupported by the production `ps2gdbStub.c` until real-hardware
exception recovery works. In particular, do **not** advertise or acknowledge
hardware breakpoints before the hardware comparator is actually armed.

## Existing PS2SDK support

`libeedebug` already supports the EE hardware comparator registers through
`ee_dbg_set_bpx`, `ee_dbg_set_bpw`, `ee_dbg_set_bpr`, `ee_dbg_set_bpc`, and
related getters and clears. `ee_dbg_install(2)` installs the **level-2** EE
exception vector at `0x80000100`; a `EE_EXC2_DBG` handler receives the
saved `EE_RegFrame` with `ErrorEPC` (the level-2 resumption address).

The current PS2GDB exception handler is a **level-1** software `break`
handler. Calling the TCP/GDB packet loop from a level-2 exception would be
unsafe because the network stack requires normal interruptible execution.
Integrating the two requires a validated, context-preserving handoff from
level 2 to a level-1 breakpoint, followed by correct one-instruction
rearm/continue handling. The EE has **one** hardware instruction comparator
and **one shared** data-address comparator; exceeding those limits must be
reported, not silently accepted.

## Real hardware experiments (PS2Link, 192.168.1.10, 2026-11-01)

A small **standalone** probe under `tests/hardware/` uses `libeedebug` to
catch a write to a volatile EE-RAM word. It does not modify PS2GDB itself.
The only tested recovery action is the host-side `ps2client reset` command;
no code resets the IOP, exits to OSD, or attempts IOP synchronization.

- With the BPC state returned by `ee_dbg_set_bpw`, the program completed but
  the watchpoint did **not** trigger (`hits=0`, `watched=42`). The BPC snapshot
  was `0x203d8000`, including `EE_BPC_BED` (breakpoint exception disable).
- After explicitly clearing `EE_BPC_BED`, the probe reached the armed state
  but then hung before emitting the completion marker. PS2Link stopped
  responding; a host-side reset did not recover it. **Physical console
  recovery is required before any further real-hardware experiments.**
- Running the same ELF in PCSX2 produced no watchpoint and reported
  inconsistent values for BPC, DAB and DABM; that emulation result must
  **not** be treated as validation of actual EE debug exceptions.

The exact cause of the hang has **not** been established. Possible areas to
investigate include the level-2 handler's frame/return behavior, the BPC
re-enable sequence, and compiler-generated accesses during the exception.
Do not label the PS2SDK behavior a proven upstream defect based only on
these observations.

## Safe tests and next steps

`make check` runs **only native C tests** for packet parsing, memory ranges,
and one-instruction/one-data-comparator resource accounting. Native
`hw-breakpoint-state.h` deliberately never touches EE registers or installs
an exception handler. `make hardware-probe-build` cross-compiles the
standalone probe; neither target executes it on a console.

Before enabling production `Z1`–`Z4`:

1. Recover the physical PS2 and establish a reliable level-2 debug exception
   callback that restores execution without hanging or losing PS2Link.
2. Capture `ErrorEPC`, 128-bit GPR state, and BPC status without performing
   any network operation in the level-2 handler.
3. Validate a safe handoff to PS2GDB's ordinary interruptible GDB packet
   loop, and avoid recursive breakpoint traps.
4. On GDB continue, execute the interrupted instruction once with its
   comparator disarmed, then rearm it correctly. Handle delay slots and
   imprecise memory debug exceptions explicitly.
5. Add real-GDB tests of `hbreak`, `watch`, `rwatch`, and `awatch`, including
   removal, reconnect/detach cleanup, and resource-exhaustion reporting.

**Safety:** `tests/hardware/ee_debug_probe.elf` can hang a real console.
Do not run it automatically or repeat it without controlled recovery.
