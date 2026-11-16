#ifndef PS2GDB_CTRLC_RECEIVER_H
#define PS2GDB_CTRLC_RECEIVER_H

/* Experimental EE-only transport used with cooperative safe points.
 * No exception handler, breakpoint comparator, or IOP reset is involved.
 * The receiver consumes only raw 0x03 while the application is running;
 * the stopped RSP parser owns the socket at all other times. */
int ps2gdb_receiver_init(void);
void ps2gdb_receiver_resume(int socket_fd, unsigned int generation);
void ps2gdb_receiver_pause(void);
int ps2gdb_receiver_take_interrupt(void);
int ps2gdb_receiver_take_disconnect(void);
void ps2gdb_receiver_close(void);

#endif
