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

### Read-only bootstrap-vector audit (not the live interrupt vector)

The EE distinguishes the normal RAM interrupt vector (`0x80000200`) from
the bootstrap ROM interrupt vector (`0xBFC00400`) when the corresponding
bootstrap-vector mode is enabled. See [ps2tek's EE exception-vector
reference](https://israpps.github.io/ps2tek/). The standard PS2SDK public
`AddIntcHandler2()` wrapper does not describe the internal save frame.

`tests/ee_bootstrap_vector_audit.c` is a native **C-only, read-only** audit
for an optional, locally owned raw EE BIOS image. It recognizes the
bootstrap-vector sequence that loads an address into `k0` and uses
`jalr k1,k0` to enter a shared save helper. On this path, **both original
`k0` and `k1` are overwritten before that helper can save them**. The
recognized helper uses 32 consecutive `sd` operations (8 bytes per slot),
with zero stored in slots 26 and 27 rather than the original `k0/k1` values.
Consequently, even the other registers are only captured as **64 bits**
on this particular bootstrap path, not complete 128-bit R5900 GPR values.

The tool's fixture tests verify all 32 stores, the entry-to-helper jump,
short-image rejection, and rejection of modified `k0` or jump instructions.
It matched two existing local PCSX2 BIOS references (V6 PAL and V12 PAL):

<!-- markdownlint-disable MD013 -->
```text
BOOTSTRAP_ONLY helper_offset=0x0680 gpr_slots=32 slot_width=64 k0_saved=0 k1_saved=0
```
<!-- markdownlint-enable MD013 -->

The raw BIOS images and any extracted disassembly **remain local**, outside
the repository; CI only executes synthetic tests. To inspect another local
reference without copying or uploading it:

```sh
make tests/ee_bootstrap_vector_audit
./tests/ee_bootstrap_vector_audit --self-test
./tests/ee_bootstrap_vector_audit --bios "/path/to/your/local/BIOS.bin"
```

**Crucial limitation:** this only proves the **bootstrap ROM** path on the
tested reference images. It does **not** establish the save logic of the
normal **live RAM** vector at `0x80000200`, the active console firmware,
or the behavior of any custom level-2 debug handler. It cannot be used as
evidence that a preemptive debugger can safely resume from a callback.

For an additional physical-PS2 cross-check, PS2Link's read-only `dumpmem`
returned 64 bytes from the application's known mapped address `0x00101114`,
and 1024 bytes from mapped low memory `0x00081f00`. In contrast, attempted
1024-byte reads from the physical vector page `0x00000000` and its KSEG0
alias `0x80000000` produced empty host files. PS2Link still responded to
ping after those attempts, and no IOP reset or kernel write occurred. An
empty dump is **not** a valid vector image, and no runtime ABI conclusions
are derived from it. The low-memory `0x00081f00` region is not equivalent
to an image of `0x80000200`.

The next research milestone must obtain the **actual installed RAM entry**
through an explicitly validated read-only mechanism (possibly PCSX2's
debugger or a documented kernel-mode copy operation), then compare its
first instructions and saved register locations against the bootstrap
reference. Until then the candidate frame's `k0/k1`, thread, FPU and
entry-time BD/EPC validity flags remain unset.

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

### Next isolated milestone: fail-closed stop policy

The unmerged preemption research was audited after PRs #13 and #14 were
integrated. This continuation imports only a pure, host-testable stop-request
state model and its compile-only EE check. No vector, hardware comparator,
network receiver, or production exception code is changed in this branch.
The model rejects stops unless an independently trusted target-thread
identity, exclusive target execution, an exact-PC comparator, and a
complete frame can all be proven. Wrong-thread events enter quarantine:
the comparator must be confirmed disabled using live BPC.BED readback
before the single permitted retry. A repeated wrong-thread event aborts.
The context gate additionally requires proven FPU accumulator and full-width
HI/LO provenance. The model's synthetic proof inputs do **not** constitute
physical-PS2 validation of any of these conditions.

An earlier isolated exact-PC level-2 diagnostic preserved original wide
`k0/k1` through a GDB stop and resume on one physical PS2 test. That result
does not establish thread-selective interruption, nested exception safety,
arbitrary ACC/FPU state, or correct BD2/ErrorEPC replay. The triggered
SDK-owned level-2 frame experiments also caused hangs; the SDK frame cannot
be treated as a complete restorable application context. Future live work
must validate target selection, full entry-time state and exact-once resume
separately before connecting the already-merged cooperative receiver to
preemptive hardware stops.

### Read-only interrupt-time thread identity observation

The separate EE owner-observation probe in tests/preemptive compares the
kernel's _iGetThreadId() result inside a VBlank callback with the known IDs
of two EE threads. Each runs a CPU-bound loop without GDB polling or
yielding. The callback also records the opaque application PC supplied by
the dispatcher without dereferencing it. It never installs a hardware
comparator, GDB stop, context patch or replacement exception vector.
The native C test checks matched, other, unknown and out-of-range cases.

The first isolated PCSX2 2.9.146 run (EE recompiler) produced:

<!-- markdownlint-disable MD013 -->
```text
PS2GDB_OWNER_PROBE_BEGIN main=1
PS2GDB_OWNER_PROBE_PHASE phase=1 expected=1 samples=12 matches=12 other=0 unknown=0 outside=0 first_pc=00101158 last_pc=00101158 iterations=3904373
PS2GDB_OWNER_PROBE_PHASE phase=2 expected=3 samples=12 matches=12 other=0 unknown=0 outside=0 first_pc=00101174 last_pc=00101174 iterations=3934068
PS2GDB_OWNER_PROBE_DONE status=0 worker=3
```
<!-- markdownlint-enable MD013 -->

The same probe also passed in the PCSX2 EE interpreter, using an isolated
game-configuration override with the EE recompiler disabled:

<!-- markdownlint-disable MD013 -->
```text
PS2GDB_OWNER_PROBE_BEGIN main=1
PS2GDB_OWNER_PROBE_PHASE phase=1 expected=1 samples=12 matches=12 other=0 unknown=0 outside=0 first_pc=00101170 last_pc=00101170 iterations=3578632
PS2GDB_OWNER_PROBE_PHASE phase=2 expected=3 samples=12 matches=12 other=0 unknown=0 outside=0 first_pc=00101170 last_pc=00101170 iterations=3605035
PS2GDB_OWNER_PROBE_DONE status=0 worker=3
```
<!-- markdownlint-enable MD013 -->

This supports that the interrupt-context ID tracks the executing thread
in this controlled emulator test. It does NOT establish a trustworthy
interrupt-entry frame, identify the thread from a level-2 debug exception,
prove exclusive execution after callback return, or make a CPU-wide
breakpoint safe. The loops run sequentially, not under concurrent network
load. Do not set trusted_owner or exclusive_target in the stop policy
based solely on these observations.

Build with make preemptive-probe-build; PCSX2 can boot the owner-observation
ELF using -elf and -bios with a locally configured BIOS. The probe ends in
SleepThread() and the emulator must be stopped manually afterward.

On November 17, 2026, PS2Link at 192.168.31.10 responded to host ping in
25 ms, and the same standalone owner-observation ELF completed on a real
PS2 with the following output:

<!-- markdownlint-disable MD013 -->
```text
PS2GDB_OWNER_PROBE_PHASE phase=1 expected=15 samples=12 matches=12 other=0 unknown=0 outside=0 first_pc=00101178 last_pc=0010117c iterations=5055121
PS2GDB_OWNER_PROBE_PHASE phase=2 expected=17 samples=12 matches=12 other=0 unknown=0 outside=0 first_pc=0010117c last_pc=0010115c iterations=5443616
PS2GDB_OWNER_PROBE_DONE status=0 worker=17
```
<!-- markdownlint-enable MD013 -->

PS2Link continued to answer host ping afterward (23 ms). The probe
disables and removes its VBlank callback before reporting completion,
then enters SleepThread() rather than returning to the OSD. No IOP reset,
kernel vector replacement, BPC comparator or GDB interruption was
performed. The PS2Link EE user-thread slot is occupied by this sleeping
probe until the operator prepares a fresh launcher session.

These 24 matching observations demonstrate that _iGetThreadId() agrees
with the known executing EE thread in these two controlled, sequential
busy loops on one physical console. They do not prove level-2 debug
exception ownership, a trustworthy register frame, uninterrupted
exclusivity after the callback, or correct context restoration.
The separate scheduling probe and its priority variants have NOT yet
been run on physical hardware.

### EE receiver scheduling under an unyielding target

The separate test in tests/preemptive/ee_receiver_schedule_probe.c runs a
bounded, CP0 Count-timed CPU loop with no yield or debugger polling. An EE
observer thread sleeps on a 10-ms kernel timer and records each time it
actually runs, including whether the target loop was active. The default
probe does not register interrupt handlers or touch debug registers.

PCSX2 2.9.146 ran this in both EE modes, with the following results:

| Scenario | EE recompiler | EE interpreter |
| --- | --- | --- |
| Default: target 1, observer 1 | 0 ticks | 0 ticks |
| Opt-in: target 64, observer 63 | 26 ticks | 24 ticks |
| Opt-in: target 2, observer 1 | 26 ticks | 24 ticks |
| Opt-in: target 1, observer 0 | 26 ticks | 24 ticks |

The priority-change variant is compiled only with the explicit
PS2GDB_SCHED_LOWER_TARGET_PRIORITY=1 Makefile option. That variant
changes only the standalone test target, not the debugger or any game.
The newer PS2GDB_SCHED_TARGET_PRIORITY=2 option provides an isolated
target-priority control; in the emulator it also permitted the
higher-priority observer to run while the target did not yield.
The isolated PS2GDB_SCHED_OBSERVER_PRIORITY=0 experiment left the
target at priority 1 and created an observer at priority 0.
ReferThreadStatus() confirmed the observer's effective priority was 0
in both PCSX2 modes; the observer still advanced during the busy loop
and exited. This is a potential way to service a Ctrl-C request without
altering the target's priority, but priority 0 is also used by PS2SDK's
kernel top thread. Interrupt scheduling, socket/RPC progress, coexistence
with kernel patches and PS2Link, and a physical-console recovery path
have NOT been established. Do not wire this setting into the real
Ctrl-C receiver or deploy this variant on PS2 hardware without a
separate, controlled safety test.
Both modes saw five observer ticks before the busy loop. A separate
emulator-only attempt to rotate the priority-1 ready queue from VBlank
reported 16 interrupt callbacks without enabling observer execution;
the unproductive and potentially unsafe queue-rotation code was removed.

The default-priority result is a critical limitation of the current
dedicated-EE-thread Ctrl-C receiver: the tested target starved an
equal-priority observer while it never yielded. The positive priority
variant shows an opt-in lower-priority debug target could make an EE
receiver schedulable, but such a change alters application scheduling and
is not an acceptable transparent default. An IOP-originating interrupt
notification is another candidate, but would need its own transport and
safe exception handoff. No approach here captures an arbitrary target's
register frame or proves safe preemptive GDB stop/resume.

The scheduling probe now reports an explicit status field: a successful
run requires a started and exited observer, a verified effective
priority, a completed busy loop and observer activity during the loop
when its priority is higher than the target's. It remains only an
observation of scheduler progress, not a valid GDB stop or resume.

### Target-specific kernel suspension (emulator only)

An additional standalone test, tests/preemptive/ee_thread_suspend_probe.c,
investigates whether a priority-0 observer can suspend and resume a
known priority-1 EE target while it runs without polling or yielding.
The observer calls SuspendThread(target_id), checks the target is
suspended using ReferThreadStatus(), compares the target's iteration
counter before and after a 50-ms sleep, then calls ResumeThread().
It never installs a debug exception vector or hardware comparator.

On PCSX2 2.9.146, both EE modes produced a successful result:

| Observation | Recompiler | Interpreter |
| --- | ---: | ---: |
| SuspendThread result | 1 | 1 |
| ResumeThread result | 1 | 1 |
| Iterations before/after suspension | 2680650 / 2680650 | 2383014 / 2383014 |
| Iterations after resuming | 12294383 | 10111954 |
| Target reported suspended | Yes | Yes |

This suggests that a specifically identified EE thread can be stopped
and resumed through kernel scheduling without a CPU-wide comparator.
It is NOT evidence that the kernel saves the full 128-bit EE GPRs,
k0/k1, HI/LO, FPU ACC, FCSR or an exact GDB-readable EPC. Nor does it
demonstrate safe coexistence with PS2Link or real-hardware behavior.
The priority-0 observer can conflict with the SDK kernel top thread.
The diagnostic is intentionally compile-only in CI and MUST NOT be
launched on a physical console without an explicit, controlled plan.
No such real-console test has been performed.

On November 17, 2026, an operator-authorized PS2Link reset restored the
EE launch slot on the physical console. The first immediate ping failed;
a bounded follow-up ping succeeded in 19 ms. The default, unmodified
priority-1 scheduling probe then completed on the same PS2:

<!-- markdownlint-disable MD013 -->
```text
PS2GDB_SCHED_PROBE_DONE started=1 main_priority=1 observer_priority=1 ticks_before=5 ticks_total=5 during_busy=0 busy_iterations=7499752 observer_exited=1
```
<!-- markdownlint-enable MD013 -->

PS2Link answered the post-test ping in 17 ms. No hardware debug
breakpoint, exception-vector replacement or additional reset was used.
The result confirms on one physical PS2 that an equal-priority observer
can be starved for the duration of this busy loop.

Later on November 17, 2026, after a single operator-authorized reset,
the priority-64 variant was loaded on the same PS2. No DONE marker was
received within the 30-second host timeout, so the console-output
completion check was inconclusive. PS2Link still answered ping. A pair
of read-only 32-byte EE memory dumps from the ELF's BSS at 0x00127090
matched byte for byte:

<!-- markdownlint-disable MD013 -->
```text
observer_exited=1 observer_shutdown=1 observer_during_busy=26 observer_ticks=31
busy_iterations=0x0072422a (7488042) busy_active=0
```
<!-- markdownlint-enable MD013 -->

The ELF was built with PS2GDB_SCHED_LOWER_TARGET_PRIORITY=1 and thus
requested target priority 64; the expected observer priority was 63.
The BSS dump proves that the observer executed during the target's
busy loop and exited, not that the requested thread priorities were
successfully applied (their actual values were printed only through
the missing console marker). The missing console output needs its
own investigation. PS2Link remained responsive, and neither the
memory dumps nor this probe modified an EE debug exception vector
or armed any hardware breakpoint.

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
