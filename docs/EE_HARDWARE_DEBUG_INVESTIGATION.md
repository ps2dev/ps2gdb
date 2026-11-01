<!-- markdownlint-configure-file { "MD043": false } -->
# EE hardware breakpoints and watchpoints — investigation

## Current result (physical EE, 2026-11-01)

**The opt-in `PS2GDB_HW_EXPERIMENTAL=1` GDB hardware-debug feature has passed
its previously blocking real-console regressions.** Standard builds do
not install the level-2 vector or enable `Z1`–`Z4`. This document retains
chronological investigation notes (including older failures) below.

- GDB `hbreak` at a `jal FlushCache` executed `stepi` successfully,
  stopping at `FlushCache` (`0x00117910`), then detached cleanly.
- With **both** EE hardware comparators active, real GDB stopped twice
  on the hardware function breakpoint and detected both watched stores:
  `0 → 11` and `11 → 22`. The latter store is in a JAL delay slot.
- After the second watchpoint, GDB `stepi` advanced from `0x00117910`
  to `0x00117914`, reporting a final watched value of 22.
- GDB removed both hardware comparators, inserted a different instruction
  comparator, reinserted the data comparator, and removed them again.
  Each `Z1/Z2/z1/z2` packet returned `OK`.
- Every final test detached normally and PS2Link remained responsive at
  `192.168.31.10`. No IOP reset or exit to OSD was used.

**Root cause of the reproduced stepping hang:** the debugger's cache
helper called `FlushCache()` while a temporary software-step breakpoint
still replaced the first instruction of that wrapper. This recursively
trapped the debugger. The experimental `flush_cache_all()` now performs
two direct EE cache syscalls, and both the isolated and combined GDB
retests succeeded. An opt-in CI disassembly check prevents reintroducing
the wrapper call.

Remaining limits: one instruction and one shared data comparator;
aligned 4-byte data requests only; cached reads can escape hardware
read-watchpoint detection; other exceptional control flows remain
unvalidated. **This is reviewable as an opt-in experimental feature,
not a claim of complete production-grade debugger coverage.**

## Historical investigation notes

Initially, the hardware GDB integration was not enabled. The sections
below document the incremental probes, failures, and fixes that led to
this opt-in implementation. Results described as "pending" were pending
at the time; the current outcome is recorded above.

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

The exact cause of the *PS2SDK full-handler* hang has **not** been
established. Subsequent tests successfully captured Cause and ErrorEPC
through a separate minimal assembly handler, as documented below. This
does not demonstrate a specific PS2SDK defect.

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

All experiments below left PS2Link responsive afterward:

- **Cached write:** one debug exception, `ErrorEPC=0x001020a8`,
  `Cause=0x30038020`; the write completed and the value became 42.
- **Hot cached read:** no debug exception; the read returned 42.
- **Uncached read:** one debug exception using the SDK's `UNCACHED_SEG`
  RAM alias; the read returned 42.
- **Cached read after `FlushCache(0)`:** one debug exception; the read
  returned 42.
- **Instruction comparator:** one debug exception at the target function
  entry (`ErrorEPC=0x00101dbc`).
- **128-bit GPR capture:** one debug exception; the `sq` snapshot preserved
  an aligned stack pointer (`0x01ffecf0`), and the original store resumed.
- **Minimal C callback:** invoked once on a separate 4 KiB stack inside
  level 2, then returned after restoring all 128-bit GPRs. The interrupted
  store resumed correctly.

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

The next milestone is to connect the diagnostic handoff below to the real
GDB `trap_low` and RSP stop loop, then rearm the comparator *after* the
interrupted instruction has executed. Do not enable production `Z1`–`Z4`
or mark PR #11 ready until that full behavior is proven under real GDB.

## Level-2-to-level-1 handoff validated (192.168.31.10)

One additional, **opt-in diagnostic-only** mode was implemented:
`PS2GDB_PROBE_L1_HANDOFF`. Before arming a hardware comparator, it installs
an independent level-1 handler for software `break` exceptions (cause 9).
On the hardware debug exception, the level-2 assembly routine records the
**original** `ErrorEPC`, disables BPC, and points `ErrorEPC` at a static
software `break` trampoline. `eret` then raises an ordinary level-1
exception. The level-1 handler records its `EPC` and `Cause`, restores the
original instruction address to CP0.EPC, and returns with `eret`. The
program's original store runs once with its hardware comparator disabled.
The earlier exception handler is restored on probe completion.

Real PS2 results, both with and without a C callback inside level 2:

- Exactly one level-2 exception (`Cause=0x30038020`).
- Exactly one level-1 software-break exception (`Cause=0x30038024`).
- The level-1 `EPC` matched the static break trampoline address.
- The original instruction resumed and wrote the expected value (42).
- The 128-bit GPR snapshot preserved the original stack pointer.
- Both tests completed with `status=0` and PS2Link still responsive.

This isolated diagnostic proves a **minimal** exception-state handoff
without TCP calls in level 2. The later opt-in GDB implementation uses
that technique with the existing `trap_low` and supports continue/step and
rearm in the real debugger (see below). It is not enabled in ordinary
builds; complex branch/delay-slot and concurrent comparator interactions
still require validation.

For a compile-only integration of all opt-in variants (never executing the
ELFs), run:

```sh
make hardware-probe-variants-build
```

The compilation script runs multiple builds with different explicit flags
and always restores the safe non-triggering default probe afterward.

## Experimental remote-GDB integration (opt-in)

The EE hardware-debug exception path can now be cross-built as an **opt-in**
GDB implementation, without changing the ordinary PS2GDB build:

```sh
make clean
make PS2GDB_HW_EXPERIMENTAL=1 all
```

A separately compiled `hw-exception.S` installs a 32-byte exception-vector
trampoline and captures the original `ErrorEPC` and BPC status before
redirecting to a software-break instruction. The existing level-1
`trap_low` saves the normal EE register context and moves into the
interruptible GDB packet loop. The experimental stub supports GDB `Z1`
through `Z4` and corresponding removals. Data comparator requests other
than aligned 4-byte words are not accepted by this experimental path;
cache-hot reads may not trigger the physical comparator.

The experimental sample links the SDK's `libeedebug` using the **public**
comparator APIs, rather than relying on SDK's full level-2 exception
handler (which previously hung in isolated probes). Projects that opt into
the experimental stub must link `-leedebug` in addition to the normal
PS2GDB/PS2IPS libraries. The `gdb_hw_test_word` global and
`gdb_hw_test_function` test function are compiled only with the flag.

On the physical PS2 at `192.168.31.10`, real GNU R5900 GDB confirmed:

- `hbreak *0x129600`: stopped at the requested instruction; `continue`
  reached a later software breakpoint at `0x129618`.
- `watch gdb_hw_test_word`: two consecutive writes were reported as
  **old 0 → new 11** and **old 11 → new 22**, respectively. The internal
  software single-step restored the original instruction, rearmed BPC,
  and did not create an extra user-visible stop.
- `hbreak gdb_hw_test_function`: the hardware breakpoint fired on both
  consecutive function invocations (`PC=0x104188` twice), confirming
  hardware instruction-comparator rearming.
- `stepi` after a hardware breakpoint reached the next instruction
  (`PC=0x10418c`), followed by another genuine hardware stop on the next
  function invocation.
- Native GDB `maintenance packet` requests confirmed duplicate insertion
  returns `OK`, a second distinct instruction breakpoint returns `E22`,
  data comparator exhaustion also returns `E22`, and removals return `OK`.
- GDB detach completed normally after each experiment; PS2Link was still
  responsive, without resetting the IOP or exiting the test ELF.

These are narrow but genuine end-to-end tests. The feature remains
**experimental**, pending cases not yet tested: branch-delay-slot
continuations, stepping across software-breakpoint collisions, concurrent
instruction/data comparisons, reconnection with active breakpoints, and
reliable cache-hit read watchpoints. Read watchpoints are fundamentally
cache-sensitive on this hardware. Ordinary builds intentionally do not
install or advertise this hardware exception handler.

## Unresolved interaction and deferred rearm fix (requires retest)

A real-GDB test with the instruction comparator on
`gdb_hw_test_function` and a simultaneous write watchpoint on
`gdb_hw_test_word` produced three expected stops: two function entries,
then the first write (`0 → 11`). A subsequent `continue` timed out and
PS2Link stopped responding at `192.168.31.10`. The PS2 needs a physical
restart before another hardware test. **Do not claim concurrent comparators
are validated yet.**

Disassembly of the affected test showed that GCC scheduled the **second**
watched store into the delay slot of a `jal FlushCache` instruction. The
EE's hardware-debug `Cause.BD2` flag indicates exceptions in delay slots,
and `ErrorEPC` points at the preceding branch instruction in that case
(see the [PS2 EE architecture documentation](https://psi-rockin.github.io/ps2tek/)).

The investigation also exposed a concrete self-breakpoint hazard in the
original automatic-rearm path: `single_step()` patched the branch target
with a temporary software `break` **during** the GDB packet loop. The
loop later called the public `FlushCache()` implementation at the same
address, which could trigger its own breakpoint before the debugged
program resumed. This is a strong explanation of the observed hang, but
has **not yet been experimentally proven** as its unique root cause.

The experimental implementation now stages the intended breakpoint
addresses in `ps2gdb_hw_deferred_step_addr0/1`, leaving the instructions
untouched throughout the C GDB loop. The final low-level return path in
`gdb-low.S` applies the temporary `break` opcodes **after the C loop
finishes**, then uses its existing direct kernel cache-flush syscalls to
synchronize instruction memory before restoring the EE registers and
executing `eret`. This prevents GDB's own C-side `FlushCache()` call from
running over a temporary breakpoint that is already installed. The
assembly build/disassembly has been checked, but real-PS2 validation is
still required. The experimental level-2 handler also now captures the
original CP0 `Cause` for checking `BD2` on the next controlled test.

## Retest after restart — combined stop still hangs

After the PS2 was restarted at `192.168.31.10`, the first GDB connection
stalled on the full-register `g` reply. RSP tracing confirmed that initial
handshakes and `q*` queries succeeded, but GDB timed out on the large reply.
Replacing register encoding with a synthetic 1,440-character reply did not
help, isolating the issue to transmission rather than register formatting.
Sending the identical response in **256-byte TCP writes** did work. The
experimental TCP sender was changed to use 256-byte chunks; GNU GDB
subsequently read the real PC and SP and detached cleanly on the console.
The standard non-experimental sender remains unchanged.

With that independent TCP issue corrected, the original combined test
was retried using the deferred temporary-breakpoint patch. GDB once again
reported two hardware stops at the watched function, then the first write
watchpoint (`0 → 11`), but timed out on the next `continue`. PS2Link became
unresponsive. Thus **deferred opcode patching alone does not resolve the
combined-comparator/branch-delay failure**. No further console operations
were attempted once the hang was confirmed.

For the next controlled physical-console run, a dedicated
`PS2GDB_PROBE_BD2` diagnostic mode has been cross-compiled. It places a
watched store **explicitly in the delay slot of `jal`**, checks that the
captured level-2 Cause has `EE_CAUSE_BD2` set, checks that `ErrorEPC` points
to the preceding branch instruction, and uses the previously validated
minimal level-2-to-level-1 handoff to resume execution. It neither invokes
GDB nor runs a network stack from an exception context. This opt-in probe
**has not been executed on real hardware**; the default CI build never
triggers it.

This diagnostic can distinguish whether the remaining failure is inherent
to the level-2 delay-slot handoff or specific to the real GDB stop/rearm
state machine. Only after the independent BD2 probe passes should the
combined GDB comparator test be attempted again. It is not safe to mark
this PR ready yet.

## Real BD2 probe and GDB `FlushCache` recursion root cause

The console was restarted at `192.168.31.10`. On the physical EE, the
isolated opt-in `PS2GDB_PROBE_BD2` probe **passed**:

- `ErrorEPC` matched the deliberate JAL at `0x0010254c`, with `Cause.BD2=1`.
- One level-2 debug exception and one level-1 software BREAK occurred.
- The watched word was 42 after resumption (`status=0`).
- PS2Link remained responsive.

A follow-up test captured the watched word **inside** the level-2 handler,
after disabling BPC but **before** returning to `ErrorEPC`. At that point
the value was still **0**; after replay it was **42**. This establishes
that this hardware watchpoint is *pre-store*: the delayed store must execute
after the trap. Simply jumping directly to the JAL target would skip the
watched write and is incorrect.

Next, GDB was tested with **only an instruction hardware breakpoint** at
the `jal FlushCache` instruction (`0x001298bc`). The initial hardware stop
worked, but `stepi` into the `FlushCache` target hung the console, **even
without any data watchpoint active**. This rules out simultaneous
hardware comparators as necessary for that particular failure.

Inspection of the existing C stub revealed a concrete recursion mechanism:

1. Software single-step puts a temporary `break` at the branch target,
   which in this case is the **first instruction of `FlushCache`**.
2. When the application reaches that temporary breakpoint, `trap_low`
   calls `handle_exception()` to service the stop.
3. **At the beginning of `handle_exception()`, before removing the step
   breakpoint**, the debugger invokes its own `flush_cache_all()` helper.
4. That helper used `FlushCache(0)` and `FlushCache(2)` through the same
   public wrapper whose first instruction was still a `break`. This can
   recursively re-enter the exception handler, causing a hang.

The opt-in implementation of `flush_cache_all()` now executes syscall
`0x64` directly for cache operations 0 and 2, matching the EE kernel ABI,
without invoking the public wrapper. The ordinary PS2GDB build retains
its existing implementation. The new function was cross-compiled and its
assembly inspected: it contains exactly two direct `syscall` instructions
and no call to `FlushCache`.

**Hardware retest of the direct-syscall fix is still pending.** PS2Link was
unresponsive following the isolated `stepi` failure. The next test should
start with `hbreak` at `jal FlushCache` and `stepi`, checking for a clean
stop at the target. Only after that passes should the complete combined
instruction/data breakpoint test run again. No IOP reset should be used.
