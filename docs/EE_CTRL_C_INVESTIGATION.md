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

The revised code adds a CP0 Count-based minimum polling interval, a
zero-timeout `select()` readiness check before `recv()`, and a
`DelayThread(10000)` yield in the sample. **This revision has not yet been
physically retested; a PS2Link restart is required first.**

`tests/ctrlc_rsp_client.c` is a C-only manual hardware test: it resumes the
sample, sends raw Ctrl-C twice, checks for two `T02`/`S02` stops, and
detaches. CI compiles both ends but never executes a hardware interrupt test.

## Subsequent preemptive milestone

To stop an arbitrary EE instruction without application polling would require
a separately validated way to capture the actual application's full
interrupted CPU context, including branch-delay state. That is intentionally
out of scope for this first branch. Do not patch an arbitrary suspended PC or
call the network stack from a kernel interrupt handler without independently
validating those operations on real hardware.
