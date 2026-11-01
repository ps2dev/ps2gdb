/* Experimental EE hardware-debug wiring, off in ordinary PS2GDB builds. */
#include <tamtypes.h>
#include <kernel.h>
#include <ee_debug.h>
#include <string.h>
#include "hw-debug.h"

volatile unsigned int ps2gdb_hw_captured_epc;
volatile unsigned int ps2gdb_hw_captured_bpc;
volatile unsigned int ps2gdb_hw_captured_cause;
volatile unsigned int ps2gdb_hw_pending;

static unsigned int saved_vector[8] __attribute__((aligned(16)));
static int vector_installed;

extern unsigned char ps2gdb_hw_vector[];

struct ps2gdb_hw_state ps2gdb_hw_breakpoints;

int ps2gdb_hw_install(void)
{
    int old_intr;
    int old_mode;
    if (vector_installed) return 1;
    old_intr = DIntr();
    old_mode = ee_set_opmode(0);
    memcpy(saved_vector, (void *)0x80000100, sizeof(saved_vector));
    memcpy((void *)0x80000100, ps2gdb_hw_vector, sizeof(saved_vector));
    FlushCache(0);
    FlushCache(2);
    ee_set_opmode(old_mode);
    if (old_intr) EIntr();
    vector_installed = 1;
    return 1;
}

void ps2gdb_hw_disarm(void)
{
    ee_dbg_set_bpc(EE_BPC_BED);
}

void ps2gdb_hw_arm(void)
{
    const unsigned int imode = EE_BPC_IUE | EE_BPC_ISE | EE_BPC_IKE;
    const unsigned int dmode = EE_BPC_DUE | EE_BPC_DSE | EE_BPC_DKE;
    struct ps2gdb_hw_slot *ip = &ps2gdb_hw_breakpoints.instruction;
    struct ps2gdb_hw_slot *dp = &ps2gdb_hw_breakpoints.data;
    unsigned int control;
    ps2gdb_hw_disarm();
    ee_dbg_clr_bps();
    if (ip->type)
        ee_dbg_set_bpx(ip->address, 0xffffffffu, imode);
    if (dp->type == 2)
        ee_dbg_set_bpw(dp->address, 0xffffffffu, dmode);
    else if (dp->type == 3)
        ee_dbg_set_bpr(dp->address, 0xffffffffu, dmode);
    else if (dp->type == 4) {
        /* No private SDK API: enable both read and write comparison via
         * the public data-read setup and BPC data-write bit. */
        ee_dbg_set_bpr(dp->address, 0xffffffffu, dmode);
        ee_dbg_set_bpc(ee_dbg_get_bpc() | EE_BPC_DWE);
    }
    control = ee_dbg_get_bpc();
    if (ip->type || dp->type)
        ee_dbg_set_bpc(control & ~EE_BPC_BED);
}

void ps2gdb_hw_close(void)
{
    int old_intr, old_mode;
    ps2gdb_hw_disarm();
    memset(&ps2gdb_hw_breakpoints, 0, sizeof(ps2gdb_hw_breakpoints));
    ps2gdb_hw_pending = 0;
    if (!vector_installed) return;
    old_intr = DIntr();
    old_mode = ee_set_opmode(0);
    memcpy((void *)0x80000100, saved_vector, sizeof(saved_vector));
    FlushCache(0);
    FlushCache(2);
    ee_set_opmode(old_mode);
    if (old_intr) EIntr();
    vector_installed = 0;
}
