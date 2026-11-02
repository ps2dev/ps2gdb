<!-- markdownlint-configure-file { "MD043": false } -->
# Preemptive EE Ctrl-C — initial interrupt context investigation

## Scope

PR #12 integrated cooperative Ctrl-C: the EE application calls
`gdbstub_poll_interrupt()` on its own thread. The next goal is to stop an
**uninstrumented** EE application without any calls in the application loop.
This requires a separate transport/command receiver and a safe mechanism to
capture or reconstruct the interrupted application's register state.

The initial test below **only observes interrupts**. It never changes a saved
register, instruction, exception vector, or GDB connection.

## Verified in physical hardware

A standalone `AddIntcHandler2(INTC_VBLANK_S, ...)` callback was registered
on the real PS2 at `192.168.31.10`. The EE executed a bounded, CPU-bound
loop without a syscall, debugger poll, or scheduler yield inside it.
The first hardware test produced:

```text
PS2GDB_PREEMPT_PROBE_BEGIN thread=50
PS2GDB_PREEMPT_PROBE_IRQ hits=12 iterations=4962944
  arg_first=001216c0 arg_last=001215c4
  cp0_epc_first=00081fe0 cp0_epc_last=00081fe0
PS2GDB_PREEMPT_PROBE_DONE status=0
```

PS2Link remained responsive immediately afterward. Disassembly shows that
`0x001215c4` is an instruction in the application's `main` busy loop, while
`0x00081fe0` is in kernel address space. Thus the callback's **third
argument carries a useful interrupted application address**, whereas CP0
EPC sampled *from inside the callback* cannot be used as the application's
PC. We still cannot assert that it contains the entire exception context.

A second, tighter probe was executed successfully on physical hardware on
2026-11-02 after confirming PS2Link connectivity. It places externally
visible assembly labels around the no-yield busy loop and checks that the
observed interrupt addresses fall inside those markers:

<!-- markdownlint-disable MD013 -->
```text
PS2GDB_PREEMPT_PROBE_IRQ hits=12 iterations=4709310 arg_first=001217ec arg_last=0010113c cp0_epc_first=00081fe0 cp0_epc_last=00081fe0
PS2GDB_PREEMPT_PROBE_APPPC busy_hits=11 busy_first=0010113c busy_last=0010113c busy_min=00101138 busy_max=0010115c busy_begin=00101114 busy_end=00101168
PS2GDB_PREEMPT_PROBE_DONE status=0
```
<!-- markdownlint-enable MD013 -->

The first callback arrived before the busy-loop guard was set, hence
`arg_first` is outside the loop. The 11 callbacks counted *during* the loop
all reported aligned addresses within the labeled range. R5900 disassembly
of the actual ELF confirms `0x00101138` is `lw`, `0x0010113c` is `addiu`,
and `0x0010115c` is `bnez`, all inside the loop. These measurements support
using the callback argument as an interrupted PC observation; they do not
establish whether it is a delay-slot-adjusted EPC or expose any other saved
registers. No interrupt handler wrote kernel state or performed network I/O.

The existing `ee/gdb-low.S` normal-exception path saves most 128-bit GPRs
with `sq`, plus special and FPU state, into its own debugger frame. It then
changes EPC and performs an `eret` into its debugger handoff. That frame
capture occurs on the debugger's exception path, **not** in the ordinary
PS2SDK VBlank callback. Reusing its frame layout does not by itself make
VBlank context capture safe. The next isolated test must establish ownership
of the interrupted thread, exception-entry register preservation and
branch-delay semantics before any interruption or restoration is attempted.
In particular, `trap_low` does not explicitly save or restore the
`GDB_FR_REG26`/`GDB_FR_REG27` (k0/k1) slots; comments assume they are
already saved. We have not validated that assumption against the EE kernel's
vector-entry contract, so this is **not yet a proven complete register frame**.

## Next read-only CP0 diagnostic

The probe now also records CP0 Cause and Status on its first and last VBlank
callbacks. Like the prior EPC sample, these are **handler-time values** and
must not be presented as a saved application exception frame. Captures are
plain `mfc0` reads; printing occurs after the busy loop, not from the IRQ.

The extension ran on physical hardware on 2026-11-02 after a successful
PS2Link ping. The result was:

<!-- markdownlint-disable MD013 -->
```text
PS2GDB_PREEMPT_PROBE_BEGIN thread=16
PS2GDB_PREEMPT_PROBE_IRQ hits=12 iterations=4973473 arg_first=00121848 arg_last=00101140 cp0_epc_first=00081fe0 cp0_epc_last=00081fe0
PS2GDB_PREEMPT_PROBE_APPPC busy_hits=11 busy_first=00101148 busy_last=00101140 busy_min=00101140 busy_max=0010115c busy_begin=00101114 busy_end=00101168
PS2GDB_PREEMPT_PROBE_CP0 cause_first=70008000 cause_last=70008000 status_first=70030c10 status_last=70030c10
PS2GDB_PREEMPT_PROBE_DONE status=0
```
<!-- markdownlint-enable MD013 -->

The callback-side Cause value has BD clear (bit 31) and BD2 set (bit 30),
but no conclusion about the interrupted application's delay-slot state may
be drawn from that sample. Both Cause and Status remained unchanged between
the first and last callbacks even though the callback PCs differed. The
post-run PS2Link ping **failed**; the program's success marker does not
prove that the console remained reachable. No IOP reset was attempted.

PS2SDK declares `AddIntcHandler2` as a kernel syscall wrapper with the
third callback parameter named `addr`. Its public header does not define
whether that parameter is a raw EPC, a branch-delay-adjusted address, or a
pointer to other saved registers. The implementation of the EE kernel
interrupt dispatcher, rather than the public syscall wrapper, must establish
those semantics before register-frame access is attempted.

## Isolated branch-delay observation

`tests/preemptive/ee_branch_delay_probe.c` is a separate, read-only ELF. It
executes a fixed-count three-instruction loop with an explicit `bnez` and a
non-NOP `addiu` in the delay slot. The delay-slot operation counts executed
iterations, so the final count can be checked against the initial bound. An
`AddIntcHandler2` VBlank callback only counts addresses matching the labeled
decrement, branch and delay-slot instructions (plus other/out-of-range PCs).
The callback never performs a syscall, prints, patches code, or modifies a
register frame. The ELF returns to `SleepThread()` after printing results.

The cross-compiled R5900 disassembly was checked to verify the adjacent
instructions and labels:

```text
00101114 <ps2gdb_branch_loop_dec>:    addiu v0,v0,-1
00101118 <ps2gdb_branch_loop_branch>: bnez v0,0x00101114
0010111c <ps2gdb_branch_loop_delay>:  addiu v1,v1,1
00101120 <ps2gdb_branch_loop_end>:    jr ra
```

This probe **passed on physical PS2 on 2026-11-02** after a successful
PS2Link ping. Its exact captured output was:

<!-- markdownlint-disable MD013 -->
```text
PS2GDB_BRANCH_PROBE_BEGIN thread=16 iterations=120000000
PS2GDB_BRANCH_PROBE_IRQ hits=42 loop_hits=41 in_range=41 outside=0 first=00101114 last=00101118
PS2GDB_BRANCH_PROBE_PC dec=00101114 branch=00101118 delay=0010111c begin=00101110 end=00101120 dec_hits=19 branch_hits=22 delay_hits=0 other_hits=0
PS2GDB_BRANCH_PROBE_DONE status=0 slots=120000000
```
<!-- markdownlint-enable MD013 -->

All 41 callback addresses counted during the loop fell within its labels;
19 matched the decrement instruction, 22 matched the branch and none matched
the delay-slot instruction. The increment in the delay slot ran exactly
120,000,000 times. The test establishes that the **stock EE interrupt return**
preserved this loop's behavior; it does not establish complete context
capture or correct debugger-controlled instruction replay. In particular,
an EPC pointing to the branch may represent a branch-delay exception, and
the callback never records the *entry-time* BD bit.

The original post-run `ps2client ping` **failed**, as it did after the
extended CP0 probe. A later hardware experiment isolated an omitted VBlank
mask cleanup, documented below. The test's `DONE status=0` marker is not a
recovery test; post-run ping must be checked independently.

CI still only compiles the probe and checks ELF symbols; it never executes
the interrupt test on a runner.

## VBlank teardown and PS2Link lifecycle (verified)

Both diagnostics originally called `RemoveIntcHandler()` without disabling
`INTC_VBLANK_S`. PS2SDK's `graph_remove_vsync_handler()` demonstrates the
required cleanup order: `DisableIntc(INTC_VBLANK_S)` before
`RemoveIntcHandler(INTC_VBLANK_S, id)` while interrupts are suspended. The
tests now follow the same ordering. An enabled interrupt source with no
registered handler is a plausible cause of the former EE-side stall, though
the dispatcher internals have not been independently inspected.

On a physical PS2 at `192.168.31.10`, the updated branch-delay test returned:

<!-- markdownlint-disable MD013 -->
```text
PS2GDB_BRANCH_PROBE_IRQ hits=42 loop_hits=41 in_range=41 outside=0 first=00101114 last=00101114
PS2GDB_BRANCH_PROBE_PC dec=00101114 branch=00101118 delay=0010111c begin=00101110 end=00101120 dec_hits=21 branch_hits=20 delay_hits=0 other_hits=0
PS2GDB_BRANCH_PROBE_DONE status=0 slots=120000000
```
<!-- markdownlint-enable MD013 -->

Before this corrected test, PS2Link ping succeeded in 37 ms; after the test
it **also succeeded** in 47 ms, unlike earlier runs. A subsequent ping again
succeeded in 25 ms. This validates the source-mask cleanup under the tested
conditions without any IOP reset, console reboot or GDB-stub changes.

However, a subsequent attempt to `execee` the other diagnostic timed out
without an ELF launch. PS2Link still answered ping in 61 ms afterward. The
PS2Link EE loader keeps a nonzero `userThreadID` and refuses a second launch
while that slot is occupied. Our user thread intentionally ends in
`SleepThread()`, so the two outcomes are **distinct**: connectivity can be
healthy while new ELF launches remain blocked. Do not interpret a second
`execee` timeout as another lost PS2Link connection.

The PS2Link source's `pkoReset()` calls `SifIopReset(NULL, 0)`; therefore
`ps2client reset` **does reset the IOP** in this implementation and is not
permitted under the current hardware-safety requirements. Run one standalone
probe per externally restarted PS2Link session, or develop a separately
validated EE-only user-thread release mechanism before attempting repeated
launches. The probe does not return to the PS2 OSD.

To run it manually after checking PS2Link:

```sh
make preemptive-probe-build
cd tests/preemptive
ps2client -h 192.168.31.10 -t 3 ping
ps2client -h 192.168.31.10 -t 18 -e PS2GDB_BRANCH_PROBE_DONE \
  execee host:ee_branch_delay_probe.elf
```

Even successful loop accounting would demonstrate only that the *kernel's
existing interrupt return* preserved the loop, not that a custom debugger
could save and restore all state. A third callback address at the branch
instruction cannot by itself distinguish a branch interruption from an
exception in its delay slot, since architectural EPC may refer back to the
branch. Full-context capture therefore still requires a validated exception
entry with BD, EPC, GPR, HI/LO, SA, FPU and thread ownership preserved.

## Why this is not yet preemptive GDB

### Compile-only register-frame candidate

The next milestone now has an **unlinked object**, not a running exception
handler: `tests/preemptive/ee_context_candidate.S`. Its only entry assumes
`$k0` already contains a valid, 16-byte-aligned frame address. Under that
assumption it uses `sq` to record 30 complete 128-bit GPRs (0-25 and 28-31),
128-bit LO/HI, SA, LO1/HI1 and the currently readable CP0 EPC/Cause/Status.
It does not call the kernel, patch an instruction, install a vector, switch
stacks, or execute `eret`. **It is never linked into a hardware test ELF or
the production debugger**, and must not be called from an interrupt handler.

The missing GPRs are not merely an assembler oversight: using `$k0` as the
frame pointer has already destroyed the previous `$k0` value. The standard
EE kernel vector may also have changed `$k0` and `$k1` before dispatching a
registered handler. The publicly available PS2SDK syscall-wrapper source
does not establish a contract for recovering their original values. The
snapshot prototype therefore does **not** claim either register or preserve
an interrupted application's complete state. Its CP0 reads are also values
at **invocation time**, not proven exception-entry values; its FPU and
interrupted thread identity remain uncaptured.

`tests/preemptive/ee_context_candidate.h` defines a 720-byte aligned frame
with explicit per-register and provenance validity masks. The
`ee_context_is_complete()` gate rejects incomplete GPR masks, missing
entry-time CP0/branch-delay data, missing FPU/thread evidence, or unaligned
EPC. The prototype does not set any of those masks itself. This avoids
exposing an untrusted partial observation as a restorable GDB stop frame.

The native C test `tests/ee_context_candidate_test.c` checks the snapshot
offsets, the older `gdb_regs_ps2` ABI, preservation of all 16 bytes of each
GPR, and the refusal to resume incomplete/unaligned/unknown-thread frames.
The EE object is cross-compiled but never linked or run; CI additionally
checks its disassembly and its absence from the runnable probes.

The next step before **any** live preemptive handoff is to establish the
actual EE kernel interrupt-vector entry contract and a non-destructive
means of reading the interrupted thread's original saved context, including
`k0/k1` and entry-time BD/EPC. Only then can the candidate be extended to
capture and restore 128-bit GPR, FPU, and special-register state, with
deterministic exact-once instruction replay on `continue`.

The PS2SDK `ee_thread_status_t` returned by `ReferThreadStatus()` contains
thread metadata (stack, priority, status) but **no saved GPR/PC register
frame**. The `AddIntcHandler2()` callback provides an address-like third
argument, not a publicly documented full register snapshot. In particular,
we must not treat its address as a pointer to a mutable kernel register frame.

Before extending PS2GDB's asynchronous mode, validation must establish:

1. Which EE thread an asynchronous interrupt interrupted. Other network and
   system threads can be scheduled at the same time.
2. How to capture the full 128-bit GPRs, HI/LO, SA, CP0 EPC/Cause and
   relevant floating-point context *without overwriting kernel-owned state*.
3. How to distinguish branch instructions and delay-slot exceptions, so
   interrupted instructions execute exactly once after `continue`.
4. How a network reader requests interruption without performing TCP/RPC
   from an interrupt handler or causing a recursive debugger stop.
5. How to restore the original instruction and all exception handlers on
   stop, detach, disconnect, error and debugger shutdown.

Do not install an unconditional address-breakpoint comparator or patch a
running instruction based only on an unvalidated callback address. The
previous hardware-debug work already established that a debugger's own
cache-flush wrapper can recurse if it gets temporarily patched.

## Build and manually execute

```sh
make preemptive-probe-build
cd tests/preemptive
ps2client -h 192.168.31.10 -t 3 ping
# NEVER run `ps2client reset` here: PS2Link's handler resets the IOP.
# Never return to the PS2 OSD; this probe leaves its user thread asleep.
ps2client -h 192.168.31.10 -t 18 -e PS2GDB_PREEMPT_PROBE_DONE \
  execee host:ee_interrupt_context_probe.elf
```

The probe ends in `SleepThread()` rather than returning to the PS2 OSD.
This preserves the PS2Link environment but leaves its user-thread slot
occupied. Check connectivity with ping separately; a second `execee`
requires a new safe launch session. CI **cross-compiles** the ELF only;
it never triggers an EE interrupt.
