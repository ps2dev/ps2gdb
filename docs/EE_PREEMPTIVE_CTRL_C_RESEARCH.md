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

```text
PS2GDB_PREEMPT_PROBE_IRQ hits=12 iterations=4709310 arg_first=001217ec arg_last=0010113c cp0_epc_first=00081fe0 cp0_epc_last=00081fe0
PS2GDB_PREEMPT_PROBE_APPPC busy_hits=11 busy_first=0010113c busy_last=0010113c busy_min=00101138 busy_max=0010115c busy_begin=00101114 busy_end=00101168
PS2GDB_PREEMPT_PROBE_DONE status=0
```

The first callback arrived before the busy-loop guard was set, hence
`arg_first` is outside the loop. The 11 callbacks counted *during* the loop
all reported aligned addresses within the labeled range. R5900 disassembly
of the actual ELF confirms `0x00101138` is `lw`, `0x0010113c` is `addiu`,
and `0x0010115c` is `bnez`, all inside the loop. These measurements support
using the callback argument as an interrupted PC observation; they do not
establish whether it is a delay-slot-adjusted EPC or expose any other saved
registers. No interrupt handler wrote kernel state or performed network I/O.

The existing `ee/gdb-low.S` normal-exception path saves 128-bit GPRs with
`sq`, plus special and FPU state, into its own debugger frame. It then
changes EPC and performs an `eret` into its debugger handoff. That frame
capture occurs on the debugger's exception path, **not** in the ordinary
PS2SDK VBlank callback. Reusing its frame layout does not by itself make
VBlank context capture safe. The next isolated test must establish ownership
of the interrupted thread, exception-entry register preservation and
branch-delay semantics before any interruption or restoration is attempted.

## Next read-only CP0 diagnostic

The probe now also records CP0 Cause and Status on its first and last VBlank
callbacks. Like the prior EPC sample, these are **handler-time values** and
must not be presented as a saved application exception frame. Captures are
plain `mfc0` reads; printing occurs after the busy loop, not from the IRQ.

This extension cross-compiled on 2026-11-02, but the subsequent connectivity
check reported `ps2link at 192.168.31.10 is not answering`, so the extended
diagnostic has **not been hardware-validated**. No reset or ELF launch was
attempted after the failed ping. The existing verified busy-loop result above
remains from the previous probe version.

PS2SDK declares `AddIntcHandler2` as a kernel syscall wrapper with the
third callback parameter named `addr`. Its public header does not define
whether that parameter is a raw EPC, a branch-delay-adjusted address, or a
pointer to other saved registers. The implementation of the EE kernel
interrupt dispatcher, rather than the public syscall wrapper, must establish
those semantics before register-frame access is attempted.

## Why this is not yet preemptive GDB

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
# When a prior EE test is suspended, host-side `ps2client reset` is allowed.
# NEVER reset the IOP and NEVER call Exit() from the probe.
ps2client -h 192.168.31.10 -t 18 -e PS2GDB_PREEMPT_PROBE_DONE \
  execee host:ee_interrupt_context_probe.elf
```

The probe ends in `SleepThread()`, preserving PS2Link. The CI build
**cross-compiles** the ELF only; it never triggers an EE interrupt.
