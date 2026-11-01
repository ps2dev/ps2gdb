/*
  _____     ___ ____
   ____|   |    ____|      PSX2 OpenSource Project
  |     ___|   |____       (C)2003, Ray Donnelly ( rdonnelly@polygoons.com )
  --------------------------------------------------------------------------
  ps2gdbStub.c             PS2 REMOTE GDB STUB USING TCP
*/

#include <tamtypes.h>
#include <kernel.h>
#include <delaythread.h>

#include <stdio.h>
#include <string.h>
#include <debug.h>
#include <ps2ips.h>
#include <loadfile.h>
#include <tcpip.h>
#include "gdb-stub.h"
#include "r5900-registers.h"
#include "inst.h"

// This project no longer contains a main function. Instead, link it into your own project and call gdb_stub_main from your own
// main function.

// Unless you edit your gcc specs file, and change it from:-
// *subtarget_cc1_spec:
// ...to...
// *subtarget_cc1_spec:
// -fno-omit-frame-pointer
// ...then you'll need to comment this out. Also, you won't get stack back tracing working correctly either!
#define COMPILER_USES_30_AS_FP

// A few options until I've figured out which works best.
// At present, I use jal to get to handle_exception, so a normal c-style return is used to get back.
#define return_return() return

#define return_handle_exception return_return

// #define DEBUG

#define DEBUG_NONE       0
#define DEBUG_READS      (1 << 0)
#define DEBUG_WRITES     (1 << 1)
#define DEBUG_SINGLESTEP (1 << 2)
#define DEBUG_PRINTREGS  (1 << 3)
#define DEBUG_COMMS      (1 << 4)
#define DEBUG_COMMSINIT  (1 << 5)
#define DEBUG_OTHERS     (1 << 6)
#define DEBUG_EVERYTHING DEBUG_READS | DEBUG_WRITES | DEBUG_SINGLESTEP | DEBUG_PRINTREGS | DEBUG_COMMS | DEBUG_COMMSINIT | DEBUG_OTHERS
#define DEBUG_DODGYSTUFF DEBUG_READS | DEBUG_WRITES | DEBUG_SINGLESTEP | DEBUG_PRINTREGS | DEBUG_COMMS | DEBUG_OTHERS
#define DEBUG_NOTHING    0
#define DEBUG_MINIMAL    DEBUG_PRINTREGS | DEBUG_SINGLESTEP | DEBUG_WRITES | DEBUG_READS

#define DEBUG_PRINTREGS_SCREEN 0
#define DEBUG_PRINTREGS_PRINTF 1

#ifdef DEBUG
int debug_level_g = DEBUG_EVERYTHING;
#else
int debug_level_g = DEBUG_NONE;
#endif

// Comment this in if you want address errors etc (the usual ps2sdk suspects) to run through my handler. Some code I refered to
// was setting our exception handler for most types of trap. I don't know why!
// #define TRAP_ALL_EXCEPTIONS
#define GDB_PORT 12
#define BP_OPC   0x0000000d

// Low level support functions.
extern void trap_low(void);      // In "gdb-low.S".
extern int putDebugChar(char c); // Write a single character.
extern int getDebugChar(void);  // Read a single character.
static char *hex2mem(char *buf, char *mem, int count, int may_fault);

// Breakpoint function and actual address of break instruction.
extern void breakpoint(void);
extern void breakinst(void);

static int computeSignal(int tt);
static int hex(unsigned char ch);
static int hexToInt(char **ptr, int *intValue);
static unsigned char *mem2hex(char *mem, char *buf, int count, int may_fault);
void handle_exception(gdb_regs_ps2 *regs);
static int gdbstub_net_accept();

// These are the gdb buffers. For the tcpip comms, there are seperate buffers, which fill input_buffer and output_buffer when
// needed.
// BUFMAX defines the maximum number of characters in inbound/outbound buffers at least NUMREGBYTES*2 are needed for register
// packets.
#define BUFMAX 2048
static char input_buffer[BUFMAX];
static char output_buffer[BUFMAX];
static int gdbstub_initialised_g    = 0;
static int gdbstub_num_exceptions_g = 0;
static const char hexchars[]        = "0123456789abcdef";


#define SIZE_GDBSTUB_TCP_BUFFER 2048
char gdbstub_recv_buffer_g[SIZE_GDBSTUB_TCP_BUFFER];
char gdbstub_send_buffer_g[SIZE_GDBSTUB_TCP_BUFFER];

#define HOSTPATHIRX "host:"

// Comms structure for ps2sdk and this to share information. May phase this out if the two get integrated.
typedef struct _gdbstub_comms_data
{
    volatile int shutdown_should;
    volatile int shutdown_have;
    volatile int cmd_thread_id;
    volatile int gdb_thread_id;
    volatile int user_thread_id;
} gdbstub_comms_data;

char gdbstub_exception_stack[8 * 1024] __attribute__((aligned(16))); // This may be needed, I don't use it at present...

gdb_regs_ps2 gdbstub_ps2_regs __attribute__((aligned(16)));
struct gdb_regs gdbstub_regs;
int thread_id_g;


static const unsigned char *regName[NUMREGS] =
    {
        // GPRs (0-31)
        "zero",
        "at",
        "v0",
        "v1",
        "a0",
        "a1",
        "a2",
        "a3",
        "t0",
        "t1",
        "t2",
        "t3",
        "t4",
        "t5",
        "t6",
        "t7",
        "s0",
        "s1",
        "s2",
        "s3",
        "s4",
        "s5",
        "s6",
        "s7",
        "t8",
        "t9",
        "k0",
        "k1",
        "gp",
        "sp",
        "fp",
        "ra",

        // lo, hi, status, badvaddr, cause, epc. (32-37)
        "lo",
        "hi", // These are the last two 128 bit registers.
        "Stat",
        "BVAd",
        "Caus",
        "EPC",

        // FPRs (38-69)
        "fp0",
        "fp1",
        "fp2",
        "fp3",
        "fp4",
        "fp5",
        "fp6",
        "fp7",
        "fp8",
        "fp9",
        "fp10",
        "fp11",
        "fp12",
        "fp13",
        "fp14",
        "fp15",
        "fp16",
        "fp17",
        "fp18",
        "fp19",
        "fp20",
        "fp21",
        "fp22",
        "fp23",
        "fp24",
        "fp25",
        "fp26",
        "fp27",
        "fp28",
        "fp29",
        "fp30",
        "fp31",

        // FCR31 and FCR0. (70-71)
        "fsr",
        "fir", // fsr == fcr31, fir == fcr0

        "frp",
        "dummy", // "frp" is frame pointer, don't know what "dummy" is, seems dumb.

        // There's redundancy in these cp0 registers, reg12==status, 13==cause, 14==epc, 8==badvaddr. don't know why the stub does this?
        "cp0index",
        "cp0random",
        "entrylo0",
        "entrylo1",
        "context",
        "pagemask",
        "wired",
        "cp0reg7",
        "cp0reg8",
        "cp0reg9",
        "entryhi",
        "cp0reg11",
        "cp0reg12",
        "cp0reg13",
        "cp0reg14",
        "cp0prid",

        // Ps2 Specific saved registers. ACC, sa, hi1, lo1, DEPC (cp0reg24), PrfC (cp0reg25). Not handled on the remote GDB side yet.
        "acc",
        "sa",
        "hi1",
        "lo1",
        "DEPC",
        "PrfC",
};

static char *exception_names_g[13] =
    {
        "Interrupt", "TLB modification", "TLB load/inst fetch", "TLB store",
        "Address load/inst fetch", "Address store", "Bus error (instr)",
        "Bus error (data)", "Syscall", "Breakpoint", "Reserved instruction",
        "Coprocessor unusable", "Arithmetic overflow"};

// TODO :: Allow gdb to tell us when to kill the program; instead of this nonsense. It's in the protocol anyway.
volatile gdbstub_comms_data *comms_p_g;

// Gdb stub socket related global vars.
struct sockaddr_in gdb_local_addr_g;
struct sockaddr_in gdb_remote_addr_g;
fd_set comms_fd_g, temp_comms_fd_g;
int sh_g = -1;
int cs_g = -1;
static unsigned int gdbstub_connection_generation;
static int recv_buf_pos;
static int recv_buf_len;

void flush_cache_all()
{
    FlushCache(0);
    FlushCache(2);
}


// libc type functions.
// TODO :: Update to ps2sdk and remove these.
int my_atoi(char *txt)
{
    int is_neg = 0, val = 0;

    while (*txt == ' ')
        txt++;

    if (*txt == '-')
        is_neg = 1, txt++;

    while (*txt == ' ' || *txt == '\t')
        txt++;

    if (!*txt)
        return 0;

    while (*txt && *txt >= '0' && *txt <= '9') {
        val = 10 * val + (*txt - '0');
        txt++;
    }

    if (is_neg)
        val = -val;

    return val;
}


void wait_a_while(int time)
{
    int i, j;

    for (i = 0; i < 10000000; i++) {
        for (j = 0; j < time; j++) {
        }
    }
}


#ifdef DEBUG
int gdbstub_printf(int debug_mask, char *format, ...)
{
    va_list args;
    int ret;

    if ((debug_mask & debug_level_g) == 0)
        return 0;

    printf("GDBSTUB :: ");
    va_start(args, format);
    ret = vprintf(format, args);
    va_end(args);

    return ret;
}
#else
int gdbstub_printf(int debug_mask, char *format, ...)
{
    return 0;
}
#endif

int gdbstub_error(char *format, ...)
{
    va_list args;
    int ret;

    printf("GDBSTUBERROR :: ");
    va_start(args, format);
    ret = vprintf(format, args);
    va_end(args);
    return ret;
}

// Convert ch from a hex digit to an int
static int hex(unsigned char ch)
{
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

// getpacket depends on getDebugChar, but my communications happen in complete packets anyway, so this is a bit
// round the houses, though not particularly slow.
static void gdbstub_net_disconnect(void)
{
    if (cs_g >= 0) {
        disconnect(cs_g);
        cs_g = -1;
    }
    recv_buf_pos = 0;
    recv_buf_len = 0;
}

/* Return an unsigned byte, or -1 on a connection transition. */
int getDebugChar(void)
{
    int count;

    if (recv_buf_pos < recv_buf_len)
        return (unsigned char)gdbstub_recv_buffer_g[recv_buf_pos++];

    if (cs_g >= 0) {
        count = recv(cs_g, gdbstub_recv_buffer_g,
                     SIZE_GDBSTUB_TCP_BUFFER, 0);
        if (count > 0) {
            recv_buf_len = count;
            recv_buf_pos = 1;
            return (unsigned char)gdbstub_recv_buffer_g[0];
        }
        gdbstub_net_disconnect();
    }
    /* A GDB session can be restarted without rebooting the PS2. */
    while (gdbstub_net_accept() < 0)
        DelayThread(10000);
    return -1;
}

static int gdbstub_send_all(const char *data, int length)
{
    int sent = 0;
    while (sent < length) {
        int n = send(cs_g, data + sent, length - sent, 0);
        if (n <= 0) {
            gdbstub_net_disconnect();
            return 0;
        }
        sent += n;
    }
    return 1;
}

int putDebugChar(char c)
{
    return cs_g >= 0 && gdbstub_send_all(&c, 1);
}

// Scan for the sequence $<data>#<checksum>
// Although this uses getDebugChar, I've wrapped that function up so that it communicates in large packets, so there shouldn't
// be much of a speed issue with getpacket!
static void getpacket(char *buffer)
{
    int ch, i, hi, lo;
    unsigned int session;
    unsigned char checksum;
    int count, overflow;

    for (;;) {
        session = gdbstub_connection_generation;
        do {
            ch = getDebugChar();
            if (session != gdbstub_connection_generation)
                session = gdbstub_connection_generation;
        } while (ch != '$');

        checksum = 0;
        count = 0;
        overflow = 0;
        for (;;) {
            ch = getDebugChar();
            if (session != gdbstub_connection_generation) break;
            if (ch == '#') {
                hi = getDebugChar();
                lo = getDebugChar();
                if (session != gdbstub_connection_generation) break;
                hi = hex((unsigned char)hi);
                lo = hex((unsigned char)lo);
                if (hi < 0 || lo < 0 ||
                    checksum != (unsigned char)((hi << 4) | lo) || overflow) {
                    putDebugChar('-');
                    break;
                }
                buffer[count] = 0;
                putDebugChar('+');
                if (count > 2 && buffer[2] == ':') {
                    putDebugChar(buffer[0]);
                    putDebugChar(buffer[1]);
                    for (i = 3; i <= count; i++) buffer[i - 3] = buffer[i];
                }
                return;
            }
            checksum += (unsigned char)ch;
            if (count < BUFMAX - 1)
                buffer[count++] = (char)ch;
            else
                overflow = 1;
        }
    }
}

// Fast version. Whole packet sent at a time.
static void putpacket(char *buffer)
{
    unsigned char checksum = 0;
    int i, length, retry, ack;
    unsigned int generation;

    length = strlen(buffer);
    if (length > SIZE_GDBSTUB_TCP_BUFFER - 4) {
        gdbstub_error("oversized GDB reply (%d bytes)\n", length);
        return;
    }
    gdbstub_send_buffer_g[0] = '$';
    for (i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)buffer[i];
        gdbstub_send_buffer_g[i + 1] = ch;
        checksum += ch;
    }
    gdbstub_send_buffer_g[length + 1] = '#';
    gdbstub_send_buffer_g[length + 2] = hexchars[checksum >> 4];
    gdbstub_send_buffer_g[length + 3] = hexchars[checksum & 15];

    generation = gdbstub_connection_generation;
    for (retry = 0; retry < 3; retry++) {
        if (!gdbstub_send_all(gdbstub_send_buffer_g, length + 4)) return;
        do {
            ack = getDebugChar();
            if (generation != gdbstub_connection_generation)
                return; /* Let the new debugger start with a fresh packet. */
        } while (ack != '+' && ack != '-');
        if (ack == '+') return;
    }
    gdbstub_error("GDB did not acknowledge reply\n");
}

// Indicate to caller of mem2hex or hex2mem that there
// has been an error. WHAT'S THIS ABOUT THEN???
static volatile int mem_err = 0;

// Convert the memory pointed to by mem into hex, placing result in buf.
// Return a pointer to the last char put in buf (null), in case of mem fault, return 0.
// If MAY_FAULT is non-zero, then we will handle memory faults by returning a 0,
// else treat a fault like any other fault in the stub.
static unsigned char *mem2hex(char *mem, char *buf, int count, int may_fault)
{
    unsigned char ch;

    // set_mem_fault_trap(may_fault);

    flush_cache_all();

    gdbstub_printf(DEBUG_READS, "Read mem of %8x\n", mem);

    if ((int)mem < 0xc0000 || (int)mem > 0x2000000) {
        gdbstub_printf(DEBUG_READS, " Read faked due to location\n");
        // Prevent crash by pretending it's all zeroes.
        while (count-- > 0) {
            *buf++ = 0;
        }
        *buf = 0;

        return buf;
    }

    while (count-- > 0) {
        ch = *(mem++);
        if (mem_err)
            return 0;
        *buf++ = hexchars[ch >> 4];
        *buf++ = hexchars[ch & 0xf];
    }

    *buf = 0;

    // set_mem_fault_trap(0);

    return buf;
}

// Writes the binary of the hex array pointed to by into mem.
// Returns a pointer to the byte AFTER the last written.
static char *hex2mem(char *buf, char *mem, int count, int may_fault)
{
    int i;
    unsigned int old_val;
    unsigned char ch;
    char *mem2 = mem;

    switch (count) {
        case 1:
            old_val = *(char *)mem2;
            break;
        case 2:
            old_val = *(short *)mem2;
            break;
        case 4:
            old_val = *(int *)mem2;
            break;
        default:
            old_val = -1;
    }

    flush_cache_all();
    // set_mem_fault_trap(may_fault);

    for (i = 0; i < count; i++) {
        ch = hex(*buf++) << 4;
        ch |= hex(*buf++);
        *(mem++) = ch;
        if (mem_err)
            return 0;
    }

    switch (count) {
        case 1:
            gdbstub_printf(DEBUG_WRITES, "wrote byte:-\n\t\t%2x to [%8x], was %2x\n", *(unsigned char *)mem2, (int)mem2, old_val);
            break;
        case 2:
            gdbstub_printf(DEBUG_WRITES, "wrote short:-\n\t\t%4x to [%8x], was %4x\n", *(unsigned short *)mem2, (int)mem2, old_val);
            break;
        case 4:
            gdbstub_printf(DEBUG_WRITES, "wrote int:-\n\t\t%8x to [%8x], was %8x\n", *(unsigned int *)mem2, (int)mem2, old_val);
            break;
        default:
            gdbstub_printf(DEBUG_WRITES, "wrote to [%8x] bytes:-\n\t\t", (int)mem2);
            while (mem2 < mem) {
                gdbstub_printf(DEBUG_WRITES, "%2x ", *mem2++);
            }

            break;
    }

    // set_mem_fault_trap(0);
    flush_cache_all();

    return mem;
}


// Single-step using breakpoints. When an exception is handled, we need to restore the instructions hoisted
// when the breakpoints were set. This is where we save the original instructions.
static struct gdb_bp_save
{
    unsigned int addr;
    unsigned int val;
} step_bp[2];


// Sets breakpoint instructions for single stepping.
static void single_step(struct gdb_regs *regs)
{
    union mips_instruction insn;
    unsigned int targ;
    int is_branch, is_cond, i;

    targ      = regs->cp0_epc;
    insn.word = *(unsigned int *)targ;
    is_branch = is_cond = 0;

    switch (insn.i_format.opcode) {
        // jr and jalr are in r_format format.
        case spec_op:
            switch (insn.r_format.func) {
                case jalr_op:
                case jr_op:
                    targ      = *(&regs->reg0 + insn.r_format.rs);
                    is_branch = 1;
                    break;
            }
            break;

        // Group contains:-
        // bltz_op, bgez_op, bltzl_op, bgezl_op,
        // bltzal_op, bgezal_op, bltzall_op, bgezall_op.
        case bcond_op:
            is_branch = is_cond = 1;
            targ += 4 + (insn.i_format.simmediate << 2);
            break;

        // Unconditional and in j_format.
        case jal_op:
        case j_op:
            is_branch = 1;
            targ += 4;
            targ >>= 28;
            targ <<= 28;
            targ |= (insn.j_format.target << 2);
            break;

        // Conditional.
        case beq_op:
        case beql_op:
        case bne_op:
        case bnel_op:
        case blez_op:
        case blezl_op:
        case bgtz_op:
        case bgtzl_op:
        case cop0_op:
        case cop1_op:
        case cop2_op:
        case cop1x_op:
            is_branch = is_cond = 1;
            targ += 4 + (insn.i_format.simmediate << 2);
            break;
    }

    gdbstub_printf(DEBUG_SINGLESTEP, "Single step, epc [%8x], is_branch %d, is_cond %d, targ is %8x\n", regs->cp0_epc, is_branch, is_cond, targ);
    if (is_branch) {
        i = 0;
        if (is_cond && targ != (regs->cp0_epc + 8)) {
            step_bp[i].addr                  = regs->cp0_epc + 8;
            step_bp[i++].val                 = *(unsigned *)(regs->cp0_epc + 8);
            *(unsigned *)(regs->cp0_epc + 8) = BP_OPC;
            gdbstub_printf(DEBUG_SINGLESTEP, "hoisted %8x (epc + 8) with BP_OPC\n", regs->cp0_epc + 8);
        }
        step_bp[i].addr   = targ;
        step_bp[i].val    = *(unsigned *)targ;
        *(unsigned *)targ = BP_OPC;
        gdbstub_printf(DEBUG_SINGLESTEP, "hoisted %8x (targ) with BP_OPC\n", targ);
    } else {
        step_bp[0].addr                  = regs->cp0_epc + 4;
        step_bp[0].val                   = *(unsigned *)(regs->cp0_epc + 4);
        *(unsigned *)(regs->cp0_epc + 4) = BP_OPC;
        gdbstub_printf(DEBUG_SINGLESTEP, "hoisted %8x (epc + 4) with BP_OPC\n", regs->cp0_epc + 4);
    }
}


void gdbstub_ps2regs_to_vanilla(gdb_regs_ps2 *ps2_regs, struct gdb_regs *regs)
{
    regs->reg0         = *(int *)&ps2_regs->reg0;
    regs->reg1         = *(int *)&ps2_regs->reg1;
    regs->reg2         = *(int *)&ps2_regs->reg2;
    regs->reg3         = *(int *)&ps2_regs->reg3;
    regs->reg4         = *(int *)&ps2_regs->reg4;
    regs->reg5         = *(int *)&ps2_regs->reg5;
    regs->reg6         = *(int *)&ps2_regs->reg6;
    regs->reg7         = *(int *)&ps2_regs->reg7;
    regs->reg8         = *(int *)&ps2_regs->reg8;
    regs->reg9         = *(int *)&ps2_regs->reg9;
    regs->reg10        = *(int *)&ps2_regs->reg10;
    regs->reg11        = *(int *)&ps2_regs->reg11;
    regs->reg12        = *(int *)&ps2_regs->reg12;
    regs->reg13        = *(int *)&ps2_regs->reg13;
    regs->reg14        = *(int *)&ps2_regs->reg14;
    regs->reg15        = *(int *)&ps2_regs->reg15;
    regs->reg16        = *(int *)&ps2_regs->reg16;
    regs->reg17        = *(int *)&ps2_regs->reg17;
    regs->reg18        = *(int *)&ps2_regs->reg18;
    regs->reg19        = *(int *)&ps2_regs->reg19;
    regs->reg20        = *(int *)&ps2_regs->reg20;
    regs->reg21        = *(int *)&ps2_regs->reg21;
    regs->reg22        = *(int *)&ps2_regs->reg22;
    regs->reg23        = *(int *)&ps2_regs->reg23;
    regs->reg24        = *(int *)&ps2_regs->reg24;
    regs->reg25        = *(int *)&ps2_regs->reg25;
    regs->reg26        = *(int *)&ps2_regs->reg26;
    regs->reg27        = *(int *)&ps2_regs->reg27;
    regs->reg28        = *(int *)&ps2_regs->reg28;
    regs->reg29        = *(int *)&ps2_regs->reg29;
    regs->reg30        = *(int *)&ps2_regs->reg30;
    regs->reg31        = *(int *)&ps2_regs->reg31;
    regs->lo           = *(int *)&ps2_regs->lo;
    regs->hi           = *(int *)&ps2_regs->hi;
    regs->cp0_status   = ps2_regs->cp0_status;
    regs->cp0_badvaddr = ps2_regs->cp0_badvaddr;
    regs->cp0_cause    = ps2_regs->cp0_cause;
    regs->cp0_epc      = ps2_regs->cp0_epc;
    regs->fpr0         = ps2_regs->fpr0;
    regs->fpr1         = ps2_regs->fpr1;
    regs->fpr2         = ps2_regs->fpr2;
    regs->fpr3         = ps2_regs->fpr3;
    regs->fpr4         = ps2_regs->fpr4;
    regs->fpr5         = ps2_regs->fpr5;
    regs->fpr6         = ps2_regs->fpr6;
    regs->fpr7         = ps2_regs->fpr7;
    regs->fpr8         = ps2_regs->fpr8;
    regs->fpr9         = ps2_regs->fpr9;
    regs->fpr10        = ps2_regs->fpr10;
    regs->fpr11        = ps2_regs->fpr11;
    regs->fpr12        = ps2_regs->fpr12;
    regs->fpr13        = ps2_regs->fpr13;
    regs->fpr14        = ps2_regs->fpr14;
    regs->fpr15        = ps2_regs->fpr15;
    regs->fpr16        = ps2_regs->fpr16;
    regs->fpr17        = ps2_regs->fpr17;
    regs->fpr18        = ps2_regs->fpr18;
    regs->fpr19        = ps2_regs->fpr19;
    regs->fpr20        = ps2_regs->fpr20;
    regs->fpr21        = ps2_regs->fpr21;
    regs->fpr22        = ps2_regs->fpr22;
    regs->fpr23        = ps2_regs->fpr23;
    regs->fpr24        = ps2_regs->fpr24;
    regs->fpr25        = ps2_regs->fpr25;
    regs->fpr26        = ps2_regs->fpr26;
    regs->fpr27        = ps2_regs->fpr27;
    regs->fpr28        = ps2_regs->fpr28;
    regs->fpr29        = ps2_regs->fpr29;
    regs->fpr30        = ps2_regs->fpr30;
    regs->fpr31        = ps2_regs->fpr31;
    regs->cp1_fsr      = ps2_regs->cp1_fsr;
    regs->cp1_fir      = ps2_regs->cp1_fir;
    regs->frame_ptr    = ps2_regs->frame_ptr;
    regs->dummy        = ps2_regs->dummy;
    regs->cp0_index    = ps2_regs->cp0_index;
    regs->cp0_random   = ps2_regs->cp0_random;
    regs->cp0_entrylo0 = ps2_regs->cp0_entrylo0;
    regs->cp0_entrylo1 = ps2_regs->cp0_entrylo1;
    regs->cp0_context  = ps2_regs->cp0_context;
    regs->cp0_pagemask = ps2_regs->cp0_pagemask;
    regs->cp0_wired    = ps2_regs->cp0_wired;
    regs->cp0_reg7     = ps2_regs->cp0_reg7;
    regs->cp0_reg8     = ps2_regs->cp0_reg8;
    regs->cp0_reg9     = ps2_regs->cp0_reg9;
    regs->cp0_entryhi  = ps2_regs->cp0_entryhi;
    regs->cp0_reg11    = ps2_regs->cp0_reg11;
    regs->cp0_reg12    = ps2_regs->cp0_reg12;
    regs->cp0_reg13    = ps2_regs->cp0_reg13;
    regs->cp0_reg14    = ps2_regs->cp0_reg14;
    regs->cp0_prid     = ps2_regs->cp0_prid;
    regs->cp1_acc      = ps2_regs->cp1_acc;
    regs->shft_amnt    = ps2_regs->shft_amnt;
    regs->lo1          = ps2_regs->lo1;
    regs->hi1          = ps2_regs->hi1;
    regs->cp0_depc     = ps2_regs->cp0_depc;
    regs->cp0_perfcnt  = ps2_regs->cp0_perfcnt;
}


void gdbstub_vanilla_to_ps2regs(gdb_regs_ps2 *ps2_regs, struct gdb_regs *regs)
{
    *(int *)&ps2_regs->reg0  = (int)regs->reg0;
    *(int *)&ps2_regs->reg1  = (int)regs->reg1;
    *(int *)&ps2_regs->reg2  = (int)regs->reg2;
    *(int *)&ps2_regs->reg3  = (int)regs->reg3;
    *(int *)&ps2_regs->reg4  = (int)regs->reg4;
    *(int *)&ps2_regs->reg5  = (int)regs->reg5;
    *(int *)&ps2_regs->reg6  = (int)regs->reg6;
    *(int *)&ps2_regs->reg7  = (int)regs->reg7;
    *(int *)&ps2_regs->reg8  = (int)regs->reg8;
    *(int *)&ps2_regs->reg9  = (int)regs->reg9;
    *(int *)&ps2_regs->reg10 = (int)regs->reg10;
    *(int *)&ps2_regs->reg11 = (int)regs->reg11;
    *(int *)&ps2_regs->reg12 = (int)regs->reg12;
    *(int *)&ps2_regs->reg13 = (int)regs->reg13;
    *(int *)&ps2_regs->reg14 = (int)regs->reg14;
    *(int *)&ps2_regs->reg15 = (int)regs->reg15;
    *(int *)&ps2_regs->reg16 = (int)regs->reg16;
    *(int *)&ps2_regs->reg17 = (int)regs->reg17;
    *(int *)&ps2_regs->reg18 = (int)regs->reg18;
    *(int *)&ps2_regs->reg19 = (int)regs->reg19;
    *(int *)&ps2_regs->reg20 = (int)regs->reg20;
    *(int *)&ps2_regs->reg21 = (int)regs->reg21;
    *(int *)&ps2_regs->reg22 = (int)regs->reg22;
    *(int *)&ps2_regs->reg23 = (int)regs->reg23;
    *(int *)&ps2_regs->reg24 = (int)regs->reg24;
    *(int *)&ps2_regs->reg25 = (int)regs->reg25;
    *(int *)&ps2_regs->reg26 = (int)regs->reg26;
    *(int *)&ps2_regs->reg27 = (int)regs->reg27;
    *(int *)&ps2_regs->reg28 = (int)regs->reg28;
    *(int *)&ps2_regs->reg29 = (int)regs->reg29;
    *(int *)&ps2_regs->reg30 = (int)regs->reg30;
    *(int *)&ps2_regs->reg31 = (int)regs->reg31;
    *(int *)&ps2_regs->lo    = (int)regs->lo;
    *(int *)&ps2_regs->hi    = (int)regs->hi;
    ps2_regs->cp0_status     = regs->cp0_status;
    ps2_regs->cp0_badvaddr   = regs->cp0_badvaddr;
    ps2_regs->cp0_cause      = regs->cp0_cause;
    ps2_regs->cp0_epc        = regs->cp0_epc;
    ps2_regs->fpr0           = regs->fpr0;
    ps2_regs->fpr1           = regs->fpr1;
    ps2_regs->fpr2           = regs->fpr2;
    ps2_regs->fpr3           = regs->fpr3;
    ps2_regs->fpr4           = regs->fpr4;
    ps2_regs->fpr5           = regs->fpr5;
    ps2_regs->fpr6           = regs->fpr6;
    ps2_regs->fpr7           = regs->fpr7;
    ps2_regs->fpr8           = regs->fpr8;
    ps2_regs->fpr9           = regs->fpr9;
    ps2_regs->fpr10          = regs->fpr10;
    ps2_regs->fpr11          = regs->fpr11;
    ps2_regs->fpr12          = regs->fpr12;
    ps2_regs->fpr13          = regs->fpr13;
    ps2_regs->fpr14          = regs->fpr14;
    ps2_regs->fpr15          = regs->fpr15;
    ps2_regs->fpr16          = regs->fpr16;
    ps2_regs->fpr17          = regs->fpr17;
    ps2_regs->fpr18          = regs->fpr18;
    ps2_regs->fpr19          = regs->fpr19;
    ps2_regs->fpr20          = regs->fpr20;
    ps2_regs->fpr21          = regs->fpr21;
    ps2_regs->fpr22          = regs->fpr22;
    ps2_regs->fpr23          = regs->fpr23;
    ps2_regs->fpr24          = regs->fpr24;
    ps2_regs->fpr25          = regs->fpr25;
    ps2_regs->fpr26          = regs->fpr26;
    ps2_regs->fpr27          = regs->fpr27;
    ps2_regs->fpr28          = regs->fpr28;
    ps2_regs->fpr29          = regs->fpr29;
    ps2_regs->fpr30          = regs->fpr30;
    ps2_regs->fpr31          = regs->fpr31;
    ps2_regs->cp1_fsr        = regs->cp1_fsr;
    ps2_regs->cp1_fir        = regs->cp1_fir;
    ps2_regs->frame_ptr      = regs->frame_ptr;
    ps2_regs->dummy          = regs->dummy;
    ps2_regs->cp0_index      = regs->cp0_index;
    ps2_regs->cp0_random     = regs->cp0_random;
    ps2_regs->cp0_entrylo0   = regs->cp0_entrylo0;
    ps2_regs->cp0_entrylo1   = regs->cp0_entrylo1;
    ps2_regs->cp0_context    = regs->cp0_context;
    ps2_regs->cp0_pagemask   = regs->cp0_pagemask;
    ps2_regs->cp0_wired      = regs->cp0_wired;
    ps2_regs->cp0_reg7       = regs->cp0_reg7;
    ps2_regs->cp0_reg8       = regs->cp0_reg8;
    ps2_regs->cp0_reg9       = regs->cp0_reg9;
    ps2_regs->cp0_entryhi    = regs->cp0_entryhi;
    ps2_regs->cp0_reg11      = regs->cp0_reg11;
    ps2_regs->cp0_reg12      = regs->cp0_reg12;
    ps2_regs->cp0_reg13      = regs->cp0_reg13;
    ps2_regs->cp0_reg14      = regs->cp0_reg14;
    ps2_regs->cp0_prid       = regs->cp0_prid;
    ps2_regs->cp1_acc        = regs->cp1_acc;
    ps2_regs->shft_amnt      = regs->shft_amnt;
    ps2_regs->lo1            = regs->lo1;
    ps2_regs->hi1            = regs->hi1;
    ps2_regs->cp0_depc       = regs->cp0_depc;
    ps2_regs->cp0_perfcnt    = regs->cp0_perfcnt;
}


void gdbstub_show_ps2_regs(gdb_regs_ps2 *regs, int except_num, int to_screen, int clear_screen)
{
    static void (*printf_p)(const char *, ...);
    int i;
    int code, cause = regs->cp0_cause;

    code = cause & 0x7c;

    if (to_screen) {
        if (clear_screen)
            init_scr();
        printf_p = scr_printf;
    } else {
        printf_p = (void *)printf;
    }

    printf_p("\t\t\tGBDStub: %s exception #%d\n", exception_names_g[code >> 2], except_num);
    printf_p("\tStatus %08x  BadVAddr %08x  Cause %08x  EPC %08x\n\n", regs->cp0_status, regs->cp0_badvaddr, cause, regs->cp0_epc);

    // Print out the gprs.
    for (i = 0; i < 16; i++) {
        printf_p("%4s:%016lx%016lx %4s:%016lx%016lx\n",
                 regName[i], ((long *)&regs->reg0)[(i * 2) + 1], ((long *)&regs->reg0)[i * 2],
                 regName[i + 16], ((long *)&regs->reg0)[(i + 16) * 2 + 1], ((long *)&regs->reg0)[(i + 16) * 2]);
    }

    // Print out hi and lo. on ps2 these are 128 bit.
    printf_p("\n%4s:%016lx%016lx %4s:%016lx%016lx\n",
             regName[32], ((long *)&regs->lo)[1], ((long *)&regs->lo)[0],
             regName[33], ((long *)&regs->hi)[1], ((long *)&regs->hi)[0]);

    // Print out the floating point regs. There's cp1_acc as well, but I leave printing the ps2 registers at the end.
    if (!to_screen) {
        {
            int *pf = (int *)&regs->fpr0;

            for (i = 0; i < 8; i++) {
                printf_p("%4s:%08lX       %4s:%08lX       %4s:%08lX       %4s:%08lX\n", regName[38 + (i * 4)], pf[i * 4],
                         regName[38 + ((i * 4) + 1)], pf[(i * 4) + 1],
                         regName[38 + ((i * 4) + 2)], pf[(i * 4) + 2],
                         regName[38 + ((i * 4) + 3)], pf[(i * 4) + 3]);
            }
        }
    }
    printf_p("\n%4s:%08lX                           %4s:%08lX\n", regName[70], regs->cp1_fsr,
             regName[71], regs->cp1_fir);
}


// If asynchronously interrupted by gdb, then we need to set a breakpoint
// at the interrupted instruction so that we wind up stopped with a
// reasonable stack frame.
// It may be time to intergrate this code with ps2sdk.
static struct gdb_bp_save async_bp;

void set_async_breakpoint(unsigned int epc)
{
    async_bp.addr    = epc;
    async_bp.val     = *(unsigned *)epc;
    *(unsigned *)epc = BP_OPC;
    flush_cache_all();
}

// Normal gdb stubs execute in exception state. This one doesn't. This is because I need interrupts enabled in order to communicate
// with remote GDB via tcpip. It's probably really slow doing it this way and I should probably figure out a way of keeping it in
// exception state instead; serial communications??
void handle_exception(gdb_regs_ps2 *ps2_regs)
{
    int trap;
    int sigval;
    int addr;
    int length;
    char *ptr, *ptr2;
    struct gdb_regs *regs = &gdbstub_regs;

    // I probably flush the caches unnessisarily, in the process of 'bodging it until it works'.
    flush_cache_all();

    // Make the registers 'normal' - i.e. 32 bit. The remote GDB side never sees the 128 bit ps2_regs, only ever regs.
    // We put the changed values back just before exiting this function.
    gdbstub_ps2regs_to_vanilla(ps2_regs, regs);

    gdbstub_num_exceptions_g++;

    if (debug_level_g & DEBUG_PRINTREGS) {
#if (DEBUG_PRINTREGS_SCREEN == 1)
        gdbstub_show_ps2_regs(ps2_regs, gdbstub_num_exceptions_g, 1, 1);
#endif
#if (DEBUG_PRINTREGS_PRINTF == 1)
        gdbstub_show_ps2_regs(ps2_regs, gdbstub_num_exceptions_g, 0, 0);
#endif
    }

    // Turn cop1 on if it's not being used. Hmm.
    trap = (regs->cp0_cause & 0x7c) >> 2;
    if (trap == 11) {
        gdbstub_printf(DEBUG_OTHERS, "trap is 11 (cop unused)\n");
        if (((regs->cp0_cause >> CAUSEB_CE) & 3) == 1) {
            regs->cp0_status |= ST0_CU1;
            return;
        }
    }

    // If we're in breakpoint() increment the EPC beyond the breakpoint.
    // This doesn't apply for other breaks not in the breakpoint() function, remote GDB handles all that, although stub handles
    // single stepping breakpoints... Can they conflict? i.e. single stepping breakpoint overwriting a remote GDB known breakpoint.
    // I've used debuggers where this appears to have happened, resulting in break insns left floating in the code.
    if (trap == 9 && regs->cp0_epc == (unsigned long)breakinst) {
        gdbstub_printf(DEBUG_SINGLESTEP, "skipping breakinst\n");
        regs->cp0_epc += 4;
    }

    // If we were single_stepping, restore the opcodes hoisted for the breakpoint(s). Conditional branch instructions place a
    // breakpoint at both the branch target and epc+8.
    if (step_bp[0].addr) {
        gdbstub_printf(DEBUG_SINGLESTEP, "Restoring step_bp[0] %8x (was %8x) to %8x\n", step_bp[0].addr, *(unsigned *)step_bp[0].addr, step_bp[0].val);
        *(unsigned *)step_bp[0].addr = step_bp[0].val;
        step_bp[0].addr              = 0;

        if (step_bp[1].addr) {
            gdbstub_printf(DEBUG_SINGLESTEP, "Restoring step_bp[1] %8x (was %8x) to %8x\n", step_bp[1].addr, *(unsigned *)step_bp[1].addr, step_bp[1].val);
            *(unsigned *)step_bp[1].addr = step_bp[1].val;
            step_bp[1].addr              = 0;
        }
        flush_cache_all();
    }

    // If we were interrupted asynchronously by gdb, then a breakpoint was set at the EPC of the interrupt so
    // that we'd wind up here with an interesting stack frame.
    if (async_bp.addr) {
        *(unsigned *)async_bp.addr = async_bp.val;
        async_bp.addr              = 0;
    }

    // Reply to remote GDB that an exception has occurred.
    sigval = computeSignal(trap);
    ptr    = output_buffer;

    // Send trap type (converted to signal).
    *ptr++ = 'T';
    *ptr++ = hexchars[sigval >> 4];
    *ptr++ = hexchars[sigval & 0xf];

    // Send Error PC.
    *ptr++ = hexchars[REG_EPC >> 4];
    *ptr++ = hexchars[REG_EPC & 0xf];
    *ptr++ = ':';
    r5900_rsp_encode_u64(ptr, (uint32_t)regs->cp0_epc);
    ptr += R5900_RSP_REG_HEX;
    *ptr++ = ';';

#ifdef COMPILER_USES_30_AS_FP
    // Send frame pointer.
    *ptr++ = hexchars[REG_FP >> 4];
    *ptr++ = hexchars[REG_FP & 0xf];
    *ptr++ = ':';
    r5900_rsp_encode_u64(ptr, r5900_rsp_register_value(ps2_regs, regs, 30));
    ptr += R5900_RSP_REG_HEX;
    *ptr++ = ';';
#else
    // Send stack pointer as frame pointer instead, it's the best we can do?
    *ptr++ = hexchars[REG_FP >> 4];
    *ptr++ = hexchars[REG_FP & 0xf];
    *ptr++ = ':';
    r5900_rsp_encode_u64(ptr, r5900_rsp_register_value(ps2_regs, regs, 29));
    ptr += R5900_RSP_REG_HEX;
    *ptr++ = ';';
#endif

    // Send stack pointer.
    *ptr++ = hexchars[REG_SP >> 4];
    *ptr++ = hexchars[REG_SP & 0xf];
    *ptr++ = ':';
    r5900_rsp_encode_u64(ptr, r5900_rsp_register_value(ps2_regs, regs, 29));
    ptr += R5900_RSP_REG_HEX;
    *ptr++ = ';';

    // put the packet.
    *ptr++ = 0;
    gdbstub_initialised_g == 2 ? putpacket(output_buffer) : putDebugChar('-'), gdbstub_initialised_g = 2;

    // Wait for input from remote GDB.
    while (1) {
        output_buffer[0] = 0;
        getpacket(input_buffer);
        ptr = output_buffer;

        switch (input_buffer[0]) {
            case '?':
                // Return sigval.
                *ptr++ = 'S';
                *ptr++ = hexchars[sigval >> 4];
                *ptr++ = hexchars[sigval & 0xf];
                *ptr++ = 0;
                break;

            case 'd':
                // Toggle debug flag - TODO :: Use this for setting my debug flag perhaps?
                break;

            // Set thread for subsequent operations.
            case 'H':
                // Hct... c = 'c' for thread used in step and continue.
                //   t... can be -1 for all threads.
                //   c = 'g' for thread used in other operations.
                //   If zero, pick a thread, any thread.
                strcpy(output_buffer, "OK");
                break;

            // GNU GDB's mips:5900 wire layout has 90 64-bit registers.
            case 'g':
                r5900_rsp_encode_registers(output_buffer, ps2_regs, regs);
                break;

            case 'G':
                if (strlen(input_buffer + 1) != R5900_RSP_ALL_HEX ||
                    !r5900_rsp_decode_registers(input_buffer + 1, ps2_regs, regs))
                    strcpy(output_buffer, "E01");
                else
                    strcpy(output_buffer, "OK");
                break;

            case 'p': {
                unsigned int number;
                if (!r5900_rsp_parse_regno(input_buffer + 1, &number) ||
                    number >= R5900_RSP_REG_COUNT)
                    strcpy(output_buffer, "E01");
                else
                    r5900_rsp_encode_u64(output_buffer,
                        r5900_rsp_register_value(ps2_regs, regs, number));
                break;
            }

            case 'P': {
                unsigned int number;
                uint64_t value;
                char *equals = strchr(input_buffer + 1, '=');
                if (!equals || strlen(equals + 1) != R5900_RSP_REG_HEX) {
                    strcpy(output_buffer, "E01");
                    break;
                }
                *equals = 0;
                if (!r5900_rsp_parse_regno(input_buffer + 1, &number) ||
                    number >= R5900_RSP_REG_COUNT ||
                    !r5900_rsp_decode_u64(equals + 1, &value))
                    strcpy(output_buffer, "E01");
                else {
                    r5900_rsp_set_register(ps2_regs, regs, number, value);
                    strcpy(output_buffer, "OK");
                }
                break;
            }

            case 'q':
                if (strncmp(input_buffer, "qSupported", 10) == 0)
                    strcpy(output_buffer, "PacketSize=7fc");
                else if (strcmp(input_buffer, "qAttached") == 0)
                    strcpy(output_buffer, "1");
                else if (strcmp(input_buffer, "qC") == 0)
                    strcpy(output_buffer, "QC1");
                else if (strcmp(input_buffer, "qfThreadInfo") == 0)
                    strcpy(output_buffer, "m1");
                else if (strcmp(input_buffer, "qsThreadInfo") == 0)
                    strcpy(output_buffer, "l");
                else if (strncmp(input_buffer, "qOffsets", 8) == 0)
                    strcpy(output_buffer, "Text=0;Data=0;Bss=0");
                else if (strncmp(input_buffer, "qSymbol:", 8) == 0)
                    strcpy(output_buffer, "OK");
                break;

            case 'T':
                strcpy(output_buffer, "OK");
                break;

            // mAA..AA,LLLL: Read LLLL bytes at address AA..AA.
            case 'm':
                ptr2 = &input_buffer[1];

                if (hexToInt(&ptr2, &addr) && *ptr2++ == ',' &&
                    hexToInt(&ptr2, &length) && !*ptr2 && length >= 0 &&
                    length <= (SIZE_GDBSTUB_TCP_BUFFER - 4) / 2 &&
                    r5900_rsp_valid_ram_range((uint32_t)addr, (uint32_t)length)) {
                    if (mem2hex((char *)addr, output_buffer, length, 1))
                        break;
                    strcpy(output_buffer, "E03");
                } else
                    strcpy(output_buffer, "E01");
                break;

            // MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK.
            case 'M':
                ptr2 = &input_buffer[1];

                if (hexToInt(&ptr2, &addr) && *ptr2++ == ',' &&
                    hexToInt(&ptr2, &length) && length >= 0 &&
                    r5900_rsp_valid_ram_range((uint32_t)addr, (uint32_t)length) &&
                    *ptr2++ == ':' && strlen(ptr2) == (size_t)length * 2 &&
                    r5900_rsp_valid_hex(ptr2, length * 2)) {
                    if (hex2mem(ptr2, (char *)addr, length, 1))
                        strcpy(output_buffer, "OK");
                    else
                        strcpy(output_buffer, "E03");
                } else
                    strcpy(output_buffer, "E02");
                break;

            // cAA..AA: Continue at address AA..AA(optional).
            case 'c':
                // Try to read optional parameter, pc unchanged if no parm.
                ptr2 = &input_buffer[1];
                if (hexToInt(&ptr2, &addr))
                    regs->cp0_epc = addr;
                goto ret;
                break;

            // Detach without killing the EE program or resetting the IOP.
            case 'D':
                putpacket("OK");
                gdbstub_net_disconnect();
                goto ret;

            // Kill the program.
            case 'k':
                break;

            // Reset the whole machine.
            case 'r':
                break;

            // Single step.
            case 's':
                // There is no single step insn in the MIPS ISA, so uses break instead.
                single_step(regs);
                goto ret;

            // Set baud rate (bBB) - I've kept this here in case anyone wants to have a go at communicating some other way.
            case 'b':
                gdbstub_error("change baud rate not implemented\n");
                break;

        } // switch
        // reply to the request
        putpacket(output_buffer);
    } // while

ret:
    gdbstub_vanilla_to_ps2regs(ps2_regs, regs);
    flush_cache_all();
    return_handle_exception();
}


// Mappings between MIPS hardware trap types, and unix signals, which are what GDB understands.  It also indicates which hardware
// traps we need to commandeer when initializing the stub.
#ifdef TRAP_ALL_EXCEPTIONS
static struct hard_trap_info
{
    unsigned char tt;    // Trap type code for MIPS R3xxx and R4xxx
    unsigned char signo; // Signal that we map this trap into
} hard_trap_info[] = {
    {4, SIGBUS},  // address error (load).
    {5, SIGBUS},  // address error (store).
    {6, SIGBUS},  // instruction bus error.
    {7, SIGBUS},  // data bus error.
    {9, SIGTRAP}, // break.
    {10, SIGILL}, // reserved instruction.
    // {11, SIGILL},  // CPU unusable.
    {12, SIGFPE},  // overflow.
    {13, SIGTRAP}, // trap.
    {14, SIGSEGV}, // virtual instruction cache coherency.
    {15, SIGFPE},  // floating point exception.
    {23, SIGSEGV}, // watch.
    {31, SIGSEGV}, // virtual data cache coherency.
    {0, 0}         // Must be last.
};
#else
// Cut down to just the break instruction.
static struct hard_trap_info
{
    unsigned char tt;    // Trap type code for MIPS R3xxx and R4xxx.
    unsigned char signo; // Signal that we map this trap into.
} hard_trap_info[] = {
    {9, SIGTRAP}, // break.
    {0, 0}        // Must be last.
};
#endif


// Convert the MIPS hardware trap type code to a Unix signal number.
static int computeSignal(int tt)
{
    struct hard_trap_info *ht;

    for (ht = hard_trap_info; ht->tt && ht->signo; ht++)
        if (ht->tt == tt)
            return ht->signo;

    return SIGHUP; // Default for things we don't know about.
}


// While we find nice hex chars, build an int. Return number of chars processed.
static int hexToInt(char **ptr, int *intValue)
{
    int numChars = 0;
    int hexValue;

    unsigned int value = 0;

    while (**ptr) {
        hexValue = hex(**ptr);
        if (hexValue < 0) break;
        if (value > (UINT32_MAX >> 4)) return 0;
        value = (value << 4) | (unsigned int)hexValue;
        numChars++;
        (*ptr)++;
    }
    *intValue = (int)value;

    return (numChars);
}


void __attribute__((noinline)) breakpoint(void)
{
    if (!gdbstub_initialised_g) {
        gdbstub_error("breakpoint() called, but not initialised stub yet!\n");
        return;
    }
    __asm__ __volatile__("			\n"
                         "			.globl	breakinst	\n"
                         "			.set	noreorder	\n"
                         "			nop			\n"
                         "breakinst:		break			\n"
                         "			nop			\n"
                         "			.set	reorder		\n");
}

// -1 == failure
int gdbstub_net_open()
{
    int rc_bind, rc_listen;

    if (SifLoadModule(HOSTPATHIRX "ps2ips.irx", 0, NULL) < 0) {
        gdbstub_error("Load ps2ips.irx failed.\n");
        return -1;
    }

    if (ps2ip_init() < 0) {
        gdbstub_error("ps2ip_init failed.\n");
        return -1;
    }

    sh_g = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sh_g < 0) {
        gdbstub_error("Socket create failed.\n");
        return -1;
    }
    gdbstub_printf(DEBUG_COMMSINIT, "Socket returned %i.\n", sh_g);

    memset(&gdb_local_addr_g, 0, sizeof(gdb_local_addr_g));
    gdb_local_addr_g.sin_family      = AF_INET;
    gdb_local_addr_g.sin_addr.s_addr = htonl(INADDR_ANY); // any local address (a.k.a 0.0.0.0).
    gdb_local_addr_g.sin_port        = htons(GDB_PORT);

    // bind.
    rc_bind = bind(sh_g, (struct sockaddr *)&gdb_local_addr_g, sizeof(gdb_local_addr_g));
    if (rc_bind < 0) {
        gdbstub_error("Bind failed.\n");
        disconnect(sh_g); // Clean-up on failures.
        sh_g = -1;
        return -1;
    }
    gdbstub_printf(DEBUG_COMMSINIT, "Bind returned %i.\n", rc_bind);

    rc_listen = listen(sh_g, 2);
    if (rc_listen < 0) {
        gdbstub_error("Listen failed.\n");
        disconnect(sh_g); // Clean-up on failures.
        sh_g = -1;
        return -1;
    }
    gdbstub_printf(DEBUG_COMMSINIT, "Listen returned %i.\n", rc_listen);
    return 0;
}

static int gdbstub_net_accept()
{
    int remote_len;

    remote_len = sizeof(gdb_remote_addr_g);
    // Disconnect last connection if there was any.
    gdbstub_net_disconnect();
    cs_g = accept(sh_g, (struct sockaddr *)&gdb_remote_addr_g, &remote_len);

    if (cs_g < 0) {
        gdbstub_error("Accept failed.\n");
        return -1;
    }

    gdbstub_connection_generation++;
    FD_ZERO(&comms_fd_g);
    FD_SET(cs_g, &comms_fd_g);

    gdbstub_printf(DEBUG_COMMSINIT, "accept is %d\n", cs_g);

    gdbstub_printf(DEBUG_COMMSINIT, "netopen ok.\n");

    return 0;
}

void gdbstub_net_close()
{
    gdbstub_net_disconnect();
    if (sh_g >= 0) disconnect(sh_g);
    sh_g = -1;
    gdbstub_printf(DEBUG_COMMSINIT, "disconnected\n");

    return;
}


// -1 == failure
int gdbstub_init(int argc, char *argv[])
{
    struct hard_trap_info *ht;

    gdbstub_initialised_g    = 0;
    gdbstub_num_exceptions_g = 0;
    thread_id_g              = GetThreadId();

    if (gdbstub_net_open() == -1 || gdbstub_net_accept() == -1) {
        gdbstub_error("failed to open net connection.\n");
        return -1;
    }

    for (ht = hard_trap_info; ht->tt && ht->signo; ht++) {
        if (ht->tt < 4)
            SetVTLBRefillHandler(ht->tt, trap_low);
        else
            SetVCommonHandler(ht->tt, trap_low);
    }
    flush_cache_all();
    gdbstub_initialised_g = 1;
    gdbstub_printf(DEBUG_COMMS, "Waiting for remote GDB to connect\n\n\n");

    breakpoint();

    return 0;
}


void gdbstub_close()
{
    gdbstub_net_close();
    gdbstub_initialised_g = 0;

    return;
}


int gdb_stub_main(int argc, char *argv[])
{
    int i;

#if (DEBUG_PRINTREGS_SCREEN == 1)
    init_scr();
    scr_printf("GDB Stub Loaded\n\n");
#endif
    gdbstub_printf(DEBUG_COMMSINIT, "GDB Stub Loaded\n\n");

    for (i = 0; i < argc; i++) {
        gdbstub_printf(DEBUG_COMMSINIT, "arg %d is %s\n", i, argv[i]);
    }

    if (argc >= 2)
        comms_p_g = (gdbstub_comms_data *)my_atoi(argv[1]);
    else
        comms_p_g = NULL;

    if (gdbstub_init(argc, argv) == -1) {
        gdbstub_error("INIT FAILED\n");
        return -1;
    }

    return 0;
}
