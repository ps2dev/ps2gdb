/*
 * ps2gdb EE remote debugger stub public interface.
 *
 * Link with -lps2gdbStub -lps2ips, then run the program under PS2Link
 * with the ps2ips.irx module accessible through host:.
 */
#ifndef PS2GDB_H
#define PS2GDB_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start the GDB TCP listener on port 12 and break on initial attach.
 * Returns 0 after the debugger resumes execution or -1 on setup failure.
 * argc/argv may be zero/NULL for the standard PS2Link use case.
 */
int gdb_stub_main(int argc, char *argv[]);

/* Close the remote stub's sockets when debugging is no longer required. */
void gdbstub_close(void);

/* Optional cooperative Ctrl-C safe point (PS2GDB_CTRL_C_COOPERATIVE=1).
 * Call on the single EE thread being debugged, e.g. once per game frame.
 * While running, checks the GDB TCP socket without blocking and enters the
 * normal register-preserving exception path if GDB sent its raw 0x03 byte.
 * No polling or TCP work occurs in an exception/interrupt handler.
 * This is NOT a preemptive stop of an arbitrary uninstrumented EE thread.
 */
void gdbstub_poll_interrupt(void);

#ifdef __cplusplus
}
#endif

#endif /* PS2GDB_H */
