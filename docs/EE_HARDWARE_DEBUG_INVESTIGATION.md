<!-- markdownlint-configure-file { "MD043": false } -->
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
- After explicitly clearing `EE_BPC_BED` with the unmodified PS2SDK
  level-2 handler, the probe hung. PS2Link stopped responding and host-side
  reset could not recover it.
- After a physical restart, a minimal level-2 assembly vector that simply
  disables BPC and uses `eret` successfully handled the watchpoint: the
  probe completed with BPC cleared and PS2Link remained responsive.
  The saved CPU state was not yet inspected in that first minimal test.
- The captured original `Status` was `0x70030c11` (`DEV=0`), so the CPU
  selected the normal level-2 vector at `0x80000100`, not the BIOS vector.
- A **temporary** copy of PS2SDK `libeedebug`, rebuilt outside the repo with
  its `$k0` emergency save redirected from `-0x20($zero)` to
  `-0x20($sp)`, still hung in the full level-2 handler. Therefore this
  suspected assembly problem alone is **not** proven to explain the hang.
  It would be premature to submit that specific change as an SDK fix.
  PS2Link again requires a physical restart for further hardware tests.
- Running the same ELF in PCSX2 produced no watchpoint and reported
  inconsistent values for BPC, DAB and DABM; that emulation result must
  **not** be treated as validation of actual EE debug exceptions.

The exact cause of the full-handler hang has **not** been established.
The diagnostic-only minimal assembly handler has since been extended to
record Cause and ErrorEPC after disabling comparators, without calling C;
**that extended handler has only been compiled, not yet retested on real
hardware**. Possible areas to
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

**Safety:** Only the default `make hardware-probe-build` is safe and it
**does not trigger** hardware exceptions. Enabling the trigger requires
explicitly compiling with `PS2GDB_PROBE_TRIGGER`; adding
`PS2GDB_PROBE_MINIMAL_VECTOR` switches to the narrow diagnostic handler.
The full PS2SDK exception handler has hung twice on the physical console;
never run that configuration automatically. Require an orderly recovery
plan and host-side PS2Link ping before any opt-in trigger test.

## Subsequent real-hardware validation (192.168.31.10, 2026-11-01)

The physical PS2 was restarted with PS2Link at `192.168.31.10`.
The debug vector's 32-byte entry now jumps to a separately linked assembly
handler; only the eight entry instructions are copied to `0x80000100`.
Copying the first 32 bytes of the entire handler (without a trampoline)
would truncate it as soon as additional register capture was added.

The assembly handler disables BPC before touching RAM, saves temporary
registers, captures `ErrorEPC` and `Cause`, restores the registers, then
returns via `eret`. Each test explicitly enabled `PS2GDB_PROBE_TRIGGER`
and `PS2GDB_PROBE_MINIMAL_VECTOR`, with PS2Link ping and host-side EE reset
between independent test executions. No IOP reset or EE program exit occurred.

| Experiment | Physical PS2 result | PS2Link after |
| --- | --- | --- |
| Cached write comparator | One debug exception, `ErrorEPC=0x001020a8`, `Cause=0x30038020`, observed value 42 | Responsive |
| Cached read, already hot in D-cache | **No exception**; read returned 42 | Responsive |
| Uncached read, using SDK `UNCACHED_SEG` RAM alias | One debug exception, read returned 42 | Responsive |
| Cached read after `FlushCache(0)` | One debug exception, read returned 42 | Responsive |
| Instruction comparator at the target function entry | One debug exception, `ErrorEPC` matched the function entry (`0x00101dbc`) | Responsive |
| Full 128-bit GPR snapshot through `sq` to aligned RAM | One debug exception, captured a valid aligned SP (`0x01ffecf0`); original store resumed correctly | Responsive |
| Minimal level-2 C callback on a separate 4 KiB stack, followed by GPR restoration | Callback executed once and the interrupted store resumed correctly | Responsive |

The initially failed instruction-comparator probe had a **test assertion
mistake**: it expected `watched_word=42`, although that probe does not
write the watched word. This was corrected; the retest reported `status=0`.

**Cache caveat:** EE read watchpoints are not reliably triggered by every
load into a watched cached address. The same load was detectable after a
cache flush or through an uncached alias. This is consistent with the EE
comparator observing only some memory transactions rather than every
cache-hit load. Consequently, advertised `Z3` (`rwatch`) functionality must
not promise to detect ordinary cached reads. Uncached aliases at
`0x20000000` and `0x30000000` are now recognized by the pure C address
policy, while preserving the requested virtual address for the comparator.
Only the `0x20000000` uncached alias has been physically tested so far.

**Context caveat:** These successful probes save the complete 128-bit GPR
bank but are not a complete EE CPU snapshot: HI/LO, FPU, SA, COP0 state,
software breakpoint ownership and thread identification still require
careful handling. The C callback's purpose was to verify a small function
can run with a dedicated exception stack after GPR capture, not to certify
networking or a full GDB context switch from level-2 debug state.

### Opt-in test build examples

The default CI command `make hardware-probe-build` **always cleans first**,
builds the **non-triggering** probe, and never executes it. Triggered
hardware tests are only for a controlled manually monitored physical PS2.
The commands below compile ELFs but do not run them:

```sh
# Physical hardware write watchpoint / assembly event capture
make -C tests/hardware clean
make -C tests/hardware EE_CFLAGS='-D_EE -G0 -g -Wall -DPS2GDB_PROBE_TRIGGER -DPS2GDB_PROBE_MINIMAL_VECTOR'

# Add -DPS2GDB_PROBE_EXEC for the instruction comparator
# Add -DPS2GDB_PROBE_READ for a hot-cache read (expected not to fire)
# Add -DPS2GDB_PROBE_READ -DPS2GDB_PROBE_READ_COLD for a flushed-cache read
# Add -DPS2GDB_PROBE_READ -DPS2GDB_PROBE_READ_UNCACHED for uncached read
# Add -DPS2GDB_PROBE_SAVE_GPRS for full 128-bit GPR capture
# Add -DPS2GDB_PROBE_SAVE_GPRS -DPS2GDB_PROBE_CALL_C to test the C callback
```

The next milestone is a safe **level-2-to-level-1 handoff** that preserves
the original `ErrorEPC`, lets GDB inspect the correct interrupted context
with interrupts enabled, and rearms the hardware comparator *after* the
interrupted instruction has executed. Do not enable production `Z1`–`Z4`
or mark PR #11 ready until this behavior is proven on hardware and under
real GNU GDB.
