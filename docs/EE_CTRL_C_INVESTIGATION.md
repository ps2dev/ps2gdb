<!-- markdownlint-configure-file { "MD043": false } -->
# EE Ctrl-C support — staged investigation

## Intended behavior

When GDB continues a target, it can send an unframed `0x03` byte to
request an interrupt. The current PS2GDB only reads its TCP socket while
stopped at an exception, so it cannot receive that byte while the EE
program is running.

This branch introduces an **opt-in cooperative safe point**, not yet a truly
preemptive Ctrl-C stop. An application periodically calls
`gdbstub_poll_interrupt()` from its own EE thread. The poll checks the socket
without blocking, and a received `0x03` invokes the existing level-1 BREAK
mechanism from the application thread. This lets PS2GDB preserve that
thread's registers and report `SIGINT` rather than a normal breakpoint stop.
No network operation runs inside an exception handler.

```sh
make clean
make PS2GDB_CTRL_C_COOPERATIVE=1 all
```

A game should call `gdbstub_poll_interrupt()` at a safe point, such as once
per frame. The API is a no-op in the ordinary build. It does **not** interrupt
arbitrary uninstrumented loops or other EE threads.

## Real PS2 test status

At `192.168.31.10`, the first opt-in build used an unthrottled busy loop
calling `recv(MSG_DONTWAIT)`. A native C RSP test client received the initial
`S05` stop but timed out waiting for its first Ctrl-C (`0x03`) to produce a
stop. PS2Link became unresponsive. Continuous EE-to-IOP network requests may
have overwhelmed PS2IPS; whether the nonblocking flag was also involved is
unproven.

The first revised build added a CP0 Count-based minimum polling interval,
a zero-timeout `select()` and a yielding sample. The initial Ctrl-C still
timed out, but PS2Link stayed responsive. Real-EE diagnostic counters showed
that the sample called the poller hundreds of times; `select()` returned
`-1` **every time**, so the poller never attempted to receive the Ctrl-C.

Replacing the failing `select()` with a rate-limited
`recv(MSG_DONTWAIT)` **passed on a real PS2**: the manual C RSP client
received two `T02` stop packets after two independent continue/Ctrl-C
cycles, detached successfully, and PS2Link remained responsive at
`192.168.31.10`. The poller's temporary diagnostic counters have been
removed. The interval limiter and a 10 ms sample yield prevent the earlier
unthrottled EE/IOP network calls.

The final non-diagnostic build was also tested on the real PS2, leaving
execution running for about 2.5 seconds before the first Ctrl-C. It
reported two `T02` stops, resumed successfully between them, detached,
and left PS2Link responsive.

**Actual GNU R5900 GDB 17 (MI2) was independently verified:**

- `-exec-continue` reported `^running` and `*running`.
- `-exec-interrupt` reported `^done` and
  `*stopped,reason="signal-received",signal-name="SIGINT"`.
- A second `-exec-continue` / `-exec-interrupt` cycle reported the same
  `SIGINT` stop and `-target-detach` succeeded.
- PS2Link remained responsive after the test, without an IOP reset.

For an interactive debugger, attach GNU GDB to port 12, use `continue`,
then press Ctrl-C once the game is running. With the opt-in build and
application safe-point calls, GDB should stop with `SIGINT` and allow
register inspection followed by another `continue`. It will not stop an
EE thread that never invokes the polling function.

`tests/ctrlc_rsp_client.c` is a C-only manual hardware test: it resumes the
sample, leaves it running for several seconds, sends raw Ctrl-C twice, checks
for two `T02`/`S02` stops, and detaches. CI compiles both ends but never
executes a hardware interrupt test. The above result validates a **cooperative
safe point** only; genuine interruption of arbitrary uninstrumented EE code
remains unimplemented.

## Subsequent preemptive milestone

To stop an arbitrary EE instruction without application polling would require
a separately validated way to capture the actual application's full
interrupted CPU context, including branch-delay state. That is intentionally
out of scope for this first branch. Do not patch an arbitrary suspended PC or
call the network stack from a kernel interrupt handler without independently
validating those operations on real hardware.
