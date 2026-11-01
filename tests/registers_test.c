#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../ee/r5900-registers.h"

static int errors;
static void check(int ok, const char *name)
{
    if (!ok) {
        fprintf(stderr, "FAIL %s\n", name);
        errors++;
    }
}

int main(void)
{
    gdb_regs_ps2 frame;
    struct gdb_regs registers;
    char reply[R5900_RSP_ALL_HEX + 1];
    uint64_t value = 0;

    memset(&frame, 0, sizeof(frame));
    memset(&registers, 0, sizeof(registers));
    memset(reply, 0, sizeof(reply));
    registers.cp0_status = 0x12345678;
    registers.cp0_epc = 0x00101120;
    registers.reg29 = 0x11111111;
    r5900_rsp_set_low64(&frame.reg29, 0x123456789abcdef0ULL);
    /* Upper 64-bit half of GPR29 is not represented in GDB's wire schema. */
    memcpy((char *)&frame.reg29 + 8, "PRESERVE", 8);
    r5900_rsp_set_low64(&frame.lo, 0x5566778899aabbccULL);
    r5900_rsp_set_low64(&frame.hi, 0x1122334455667788ULL);

    r5900_rsp_encode_registers(reply, &frame, &registers);
    check(r5900_rsp_bounded_length(reply, sizeof(reply)) == R5900_RSP_ALL_HEX, "GDB 17 expects exactly 1440 hex chars");
    check(r5900_rsp_decode_u64(reply + 29*16, &value) &&
          value == 0x123456789abcdef0ULL, "GPR29 preserves 64-bit value");
    check(r5900_rsp_decode_u64(reply + 32*16, &value) &&
          value == 0x12345678, "register 32 is status, not LO");
    check(r5900_rsp_decode_u64(reply + 33*16, &value) &&
          value == 0x5566778899aabbccULL, "register 33 is LO");
    check(r5900_rsp_decode_u64(reply + 34*16, &value) &&
          value == 0x1122334455667788ULL, "register 34 is HI");
    check(r5900_rsp_decode_u64(reply + 37*16, &value) &&
          value == 0x101120, "register 37 is PC");
    {
        char reg_hex[R5900_RSP_REG_HEX + 1];
        r5900_rsp_encode_u64(reg_hex, 0x0123456789abcdefULL);
        memcpy(reply + 29*16, reg_hex, R5900_RSP_REG_HEX);
        r5900_rsp_encode_u64(reg_hex, 0x00103450);
        memcpy(reply + 37*16, reg_hex, R5900_RSP_REG_HEX);
    }
    check(r5900_rsp_decode_registers(reply, &frame, &registers), "valid G packet");
    check(r5900_rsp_low64(&frame.reg29) == 0x0123456789abcdefULL,
          "G packet writes low64 of register");
    check(memcmp((char *)&frame.reg29 + 8, "PRESERVE", 8) == 0,
          "G packet preserves upper64 of register");
    check(registers.reg29 == 0x89abcdefu && registers.cp0_epc == 0x103450u,
          "G packet updates saved registers and program counter");
    reply[100] = '?';
    check(!r5900_rsp_decode_registers(reply, &frame, &registers),
          "invalid G packet rejected");
    check(registers.cp0_epc == 0x103450u, "rejected packet does not mutate registers");

    if (errors) return 1;
    puts("PASS R5900 GDB 90-register layout, writes, and upper-bit preservation");
    return 0;
}
