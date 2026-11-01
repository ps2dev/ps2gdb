#ifndef PS2GDB_R5900_REGISTERS_H
#define PS2GDB_R5900_REGISTERS_H

#include "gdb-stub.h"
#include "r5900-rsp.h"

/* The EE trap frame holds 128-bit integer registers.  GDB's MIPS register
 * schema exposes their low 64 bits.  Preserve the high 64 on writes. */
static inline uint64_t r5900_rsp_register_value(const gdb_regs_ps2 *frame,
                                          const struct gdb_regs *regs,
                                          unsigned int number)
{
    if (number < 32)
        return r5900_rsp_low64((const char *)&frame->reg0 + number * 16);
    /* GDB MIPS: r32=status, r33=LO, r34=HI, r35=BadVAddr. */
    if (number == 32) return regs->cp0_status;
    if (number == 33) return r5900_rsp_low64(&frame->lo);
    if (number == 34) return r5900_rsp_low64(&frame->hi);
    if (number == 35) return regs->cp0_badvaddr;
    if (number == 36) return regs->cp0_cause;
    if (number == 37) return regs->cp0_epc;
    if (number >= 38 && number < 70)
        return (uint32_t)*((const int *)&regs->fpr0 + number - 38);
    if (number >= 70 && number < 74)
        return (uint32_t)*((const int *)&regs->cp1_fsr + number - 70);
    if (number >= 74 && number < R5900_RSP_REG_COUNT)
        return (uint32_t)*((const int *)&regs->cp0_index + number - 74);
    return 0;
}

static inline void r5900_rsp_set_register(gdb_regs_ps2 *frame, struct gdb_regs *regs,
                                    unsigned int number, uint64_t value)
{
    uint32_t low = (uint32_t)value;
    if (number < 32) {
        if (number != 0) {
            r5900_rsp_set_low64((char *)&frame->reg0 + number * 16, value);
            *((unsigned int *)&regs->reg0 + number) = low;
        }
        return;
    }
    if (number == 33 || number == 34) {
        r5900_rsp_set_low64(number == 33 ? &frame->lo : &frame->hi, value);
        *(number == 33 ? &regs->lo : &regs->hi) = low;
    } else if (number == 32)
        regs->cp0_status = low;
    else if (number == 35)
        regs->cp0_badvaddr = low;
    else if (number == 36)
        regs->cp0_cause = low;
    else if (number == 37)
        regs->cp0_epc = low;
    else if (number >= 38 && number < 70)
        *((int *)&regs->fpr0 + number - 38) = low;
    else if (number >= 70 && number < 74)
        *((int *)&regs->cp1_fsr + number - 70) = low;
    else if (number >= 74 && number < R5900_RSP_REG_COUNT)
        *((int *)&regs->cp0_index + number - 74) = low;
}

static inline void r5900_rsp_encode_registers(char *output,
                                       const gdb_regs_ps2 *frame,
                                       const struct gdb_regs *regs)
{
    unsigned int i;
    for (i = 0; i < R5900_RSP_REG_COUNT; i++)
        r5900_rsp_encode_u64(output + i * R5900_RSP_REG_HEX,
                              r5900_rsp_register_value(frame, regs, i));
}

static inline int r5900_rsp_decode_registers(const char *data,
                                      gdb_regs_ps2 *frame,
                                      struct gdb_regs *regs)
{
    unsigned int i;
    uint64_t value;
    /* Validate the complete packet before mutating the saved machine state. */
    if (!r5900_rsp_valid_hex(data, R5900_RSP_ALL_HEX)) return 0;
    for (i = 0; i < R5900_RSP_REG_COUNT; i++) {
        if (!r5900_rsp_decode_u64(data + i * R5900_RSP_REG_HEX, &value))
            return 0;
        r5900_rsp_set_register(frame, regs, i, value);
    }
    return 1;
}

#endif
