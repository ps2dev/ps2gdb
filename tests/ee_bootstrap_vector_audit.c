/*
 * Read-only audit of an EE BIOS bootstrap interrupt vector.
 * Never install or execute a vector. No BIOS bytes are included in this repo.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BIOS_BASE 0xbfc00000u
#define IRQ_VECTOR 0x400u
#define MIN_IMAGE 0x800u
#define K0 26u
#define K1 27u

struct bootstrap_result {
    unsigned int helper_offset;
    unsigned int saved_registers;
    unsigned int zeroed_registers;
};

static uint32_t little_u32(const unsigned char *data, size_t offset)
{
    return (uint32_t)data[offset] | ((uint32_t)data[offset + 1] << 8) |
           ((uint32_t)data[offset + 2] << 16) |
           ((uint32_t)data[offset + 3] << 24);
}

static void put_u32(unsigned char *data, size_t offset, uint32_t value)
{
    data[offset] = (unsigned char)value;
    data[offset + 1] = (unsigned char)(value >> 8);
    data[offset + 2] = (unsigned char)(value >> 16);
    data[offset + 3] = (unsigned char)(value >> 24);
}

/* A match describes only the ROM bootstrap path, never the active RAM vector.
 * It proves that this path overwrites k0/k1 and stores low 64-bit GPR slots. */
static int inspect_bootstrap(const unsigned char *data, size_t size,
                             struct bootstrap_result *result)
{
    uint32_t lui, addiu, target;
    unsigned int helper, reg;

    if (!data || !result || size < MIN_IMAGE)
        return 0;

    lui = little_u32(data, IRQ_VECTOR);
    addiu = little_u32(data, IRQ_VECTOR + 4);
    if ((lui & 0xffff0000u) != 0x3c1a0000u ||
        (addiu & 0xffff0000u) != 0x275a0000u ||
        little_u32(data, IRQ_VECTOR + 8) != 0x0340d809u)
        return 0; /* lui k0; addiu k0,k0,imm; jalr k1,k0 */

    target = ((lui & 0xffffu) << 16) +
             (uint32_t)(int32_t)(int16_t)(addiu & 0xffffu);
    if (target < BIOS_BASE || target - BIOS_BASE > size - 8 - 32 * 4)
        return 0;
    helper = target - BIOS_BASE;
    if ((little_u32(data, helper) & 0xffff0000u) != 0x3c1a0000u ||
        (little_u32(data, helper + 4) & 0xffff0000u) != 0x375a0000u)
        return 0; /* helper immediately reassigns k0 as its save pointer */

    for (reg = 0; reg < 32; ++reg) {
        unsigned int source = reg == K0 || reg == K1 ? 0 : reg;
        uint32_t expected = 0xff400000u | (source << 16) | (reg * 8);
        if (little_u32(data, helper + 8 + reg * 4) != expected)
            return 0;
    }

    result->helper_offset = helper;
    result->saved_registers = 32;
    result->zeroed_registers = (1u << K0) | (1u << K1);
    return 1;
}

static int self_test(void)
{
    unsigned char data[MIN_IMAGE] = {0};
    struct bootstrap_result result = {0};
    unsigned int reg;

    put_u32(data, IRQ_VECTOR, 0x3c1abfc0u);
    put_u32(data, IRQ_VECTOR + 4, 0x275a0680u);
    put_u32(data, IRQ_VECTOR + 8, 0x0340d809u);
    put_u32(data, 0x680, 0x3c1abc1fu);
    put_u32(data, 0x684, 0x375af010u);
    for (reg = 0; reg < 32; ++reg) {
        unsigned int source = reg == K0 || reg == K1 ? 0 : reg;
        put_u32(data, 0x688 + reg * 4,
                0xff400000u | (source << 16) | (reg * 8));
    }

    if (!inspect_bootstrap(data, sizeof(data), &result) ||
        result.helper_offset != 0x680 || result.saved_registers != 32 ||
        result.zeroed_registers != ((1u << K0) | (1u << K1)))
        return 1;
    if (inspect_bootstrap(data, 0x400, &result))
        return 1; /* truncated ROM */

    put_u32(data, 0x688 + K0 * 4, 0xff5a00d0u);
    if (inspect_bootstrap(data, sizeof(data), &result))
        return 1; /* original k0 cannot be claimed as saved */
    put_u32(data, 0x688 + K0 * 4, 0xff4000d0u);
    put_u32(data, IRQ_VECTOR + 8, 0);
    if (inspect_bootstrap(data, sizeof(data), &result))
        return 1; /* vector no longer establishes k1 clobber */

    puts("PASS EE bootstrap vector audit and negative fixtures");
    return 0;
}

int main(int argc, char **argv)
{
    unsigned char data[MIN_IMAGE];
    struct bootstrap_result result;
    FILE *file;
    int matched;

    if (argc == 2 && strcmp(argv[1], "--self-test") == 0)
        return self_test();
    if (argc != 3 || strcmp(argv[1], "--bios") != 0) {
        fprintf(stderr, "Usage: %s --self-test | --bios /path/to/local/bios.bin\n",
                argv[0]);
        return 2;
    }
    file = fopen(argv[2], "rb");
    if (!file) {
        perror("Cannot open local BIOS reference");
        return 2;
    }
    matched = fread(data, 1, sizeof(data), file) == sizeof(data);
    if (ferror(file))
        matched = 0;
    fclose(file);

    if (!matched || !inspect_bootstrap(data, sizeof(data), &result)) {
        fprintf(stderr, "Unsupported or unrecognized bootstrap save sequence\n");
        return 1;
    }
    printf("BOOTSTRAP_ONLY helper_offset=0x%04x gpr_slots=%u "
           "slot_width=64 k0_saved=0 k1_saved=0\n",
           result.helper_offset, result.saved_registers);
    puts("Do not apply this result to the live 0x80000200 RAM vector.");
    return 0;
}
