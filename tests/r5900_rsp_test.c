#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../ee/r5900-rsp.h"

static int failures;
static void check(int condition, const char *description)
{
    if (!condition) {
        fprintf(stderr, "FAIL %s\n", description);
        failures++;
    }
}

int main(void)
{
    char encoded[R5900_RSP_REG_HEX + 1];
    uint64_t value = 0, reg[2] = {0, 0x1122334455667788ULL};

    r5900_rsp_encode_u64(encoded, 0x0123456789abcdefULL);
    check(strcmp(encoded, "efcdab8967452301") == 0, "64-bit LE encode");
    check(r5900_rsp_decode_u64(encoded, &value) &&
          value == 0x0123456789abcdefULL, "64-bit LE decode");
    check(!r5900_rsp_decode_u64("efcdab89674523zz", &value),
          "reject malformed hex");
    r5900_rsp_set_low64(&reg[0], value);
    check(r5900_rsp_low64(&reg[0]) == 0x0123456789abcdefULL &&
          reg[1] == 0x1122334455667788ULL, "preserve upper 64-bit register");
    check(R5900_RSP_ALL_HEX == 1440, "GDB 17 register packet size");
    check(r5900_rsp_valid_ram_range(0x00100000, 4096), "allow user RAM");
    check(!r5900_rsp_valid_ram_range(0x01fffff0, 4096), "reject RAM overflow");
    check(!r5900_rsp_valid_ram_range(0x00000000, 64), "reject zero page");
    check(!r5900_rsp_valid_ram_range(0xffffffff, 128), "reject MMIO alias");
    if (failures) return 1;
    puts("PASS R5900 GDB RSP encoding, memory bounds, and 64-bit preservation");
    return 0;
}
