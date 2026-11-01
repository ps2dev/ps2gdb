#ifndef PS2GDB_R5900_RSP_H
#define PS2GDB_R5900_RSP_H

#include <stdint.h>
#include <string.h>

/* GDB mips:5900 exposes 90 remote registers of 8 bytes each. */
#define R5900_RSP_REG_COUNT 90
#define R5900_RSP_REG_BYTES 8
#define R5900_RSP_REG_HEX (R5900_RSP_REG_BYTES * 2)
#define R5900_RSP_ALL_HEX (R5900_RSP_REG_COUNT * R5900_RSP_REG_HEX)

static inline int r5900_rsp_hex_digit(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static inline void r5900_rsp_encode_u64(char dest[R5900_RSP_REG_HEX + 1], uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    unsigned int i;
    for (i = 0; i < R5900_RSP_REG_BYTES; i++) {
        unsigned int byte = (unsigned int)(value >> (i * 8)) & 0xff;
        dest[i * 2] = digits[byte >> 4];
        dest[i * 2 + 1] = digits[byte & 15];
    }
    dest[R5900_RSP_REG_HEX] = 0;
}

static inline int r5900_rsp_decode_u64(const char *source, uint64_t *value)
{
    unsigned int i;
    uint64_t result = 0;
    for (i = 0; i < R5900_RSP_REG_HEX; i += 2) {
        int high = r5900_rsp_hex_digit((unsigned char)source[i]);
        int low = r5900_rsp_hex_digit((unsigned char)source[i + 1]);
        if (high < 0 || low < 0) return 0;
        result |= (uint64_t)((high << 4) | low) << (i * 4);
    }
    *value = result;
    return 1;
}

static inline int r5900_rsp_valid_hex(const char *data, unsigned int characters)
{
    unsigned int i;
    for (i = 0; i < characters; i++)
        if (r5900_rsp_hex_digit((unsigned char)data[i]) < 0) return 0;
    return 1;
}

static inline int r5900_rsp_parse_regno(const char *text, unsigned int *number)
{
    unsigned int value = 0;
    if (!*text) return 0;
    while (*text) {
        int digit = r5900_rsp_hex_digit((unsigned char)*text++);
        if (digit < 0 || value > (UINT32_MAX >> 4)) return 0;
        value = (value << 4) | (unsigned int)digit;
    }
    *number = value;
    return 1;
}

/* The EE stores a 128-bit GPR; GDB sees the low 64 bits only. */
static inline uint64_t r5900_rsp_low64(const void *reg)
{
    uint64_t value;
    memcpy(&value, reg, sizeof(value));
    return value;
}

static inline void r5900_rsp_set_low64(void *reg, uint64_t value)
{
    memcpy(reg, &value, sizeof(value));
}

static inline int r5900_rsp_valid_ram_range(uint32_t addr, uint32_t length)
{
    return addr >= 0x000c0000u && addr < 0x02000000u &&
           length <= 0x02000000u - addr;
}

#endif
