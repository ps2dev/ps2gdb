#ifndef PS2GDB_EE_HW_DEBUG_H
#define PS2GDB_EE_HW_DEBUG_H
#include "hw-breakpoint-state.h"

extern volatile unsigned int ps2gdb_hw_captured_epc;
extern volatile unsigned int ps2gdb_hw_captured_bpc;
extern volatile unsigned int ps2gdb_hw_captured_cause;
extern volatile unsigned int ps2gdb_hw_pending;
extern struct ps2gdb_hw_state ps2gdb_hw_breakpoints;
extern void ps2gdb_hw_trampoline(void);

int ps2gdb_hw_install(void);
void ps2gdb_hw_disarm(void);
void ps2gdb_hw_arm(void);
void ps2gdb_hw_close(void);

#endif
