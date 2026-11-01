/* Host-only integration fixture for the R5900 GDB register layout. */
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define PS2GDB_HOST_TEST 1
#include "../ee/r5900-registers.h"

static const char hex[] = "0123456789abcdef";

static int get_byte(int fd)
{
    unsigned char c;
    return recv(fd, &c, 1, 0) == 1 ? c : -1;
}

static int send_all(int fd, const char *data, size_t len)
{
    while (len) {
        ssize_t n = send(fd, data, len, 0);
        if (n <= 0) return -1;
        data += n;
        len -= (size_t)n;
    }
    return 0;
}

static int receive_packet(int fd, char *data, size_t size)
{
    int c, first, second;
    unsigned int checksum = 0;
    size_t length = 0;
    do {
        c = get_byte(fd);
        if (c < 0) return -1;
    } while (c != '$');
    while ((c = get_byte(fd)) >= 0 && c != '#') {
        if (length + 1 >= size) return -1;
        data[length++] = (char)c;
        checksum = (checksum + c) & 255;
    }
    if (c < 0) return -1;
    first = get_byte(fd);
    second = get_byte(fd);
    if (first < 0 || second < 0 ||
        r5900_rsp_hex_digit(first) < 0 || r5900_rsp_hex_digit(second) < 0 ||
        checksum != (unsigned int)((r5900_rsp_hex_digit(first) << 4) |
                                     r5900_rsp_hex_digit(second))) return -1;
    data[length] = 0;
    return send_all(fd, "+", 1);
}

static int reply_packet(int fd, const char *data)
{
    char packet[4096];
    unsigned int checksum = 0;
    size_t i, length = r5900_rsp_bounded_length(data, sizeof(packet) - 3);
    if (length + 5 > sizeof(packet)) return -1;
    packet[0] = '$';
    for (i = 0; i < length; i++) {
        packet[i + 1] = data[i];
        checksum = (checksum + (unsigned char)data[i]) & 255;
    }
    packet[length + 1] = '#';
    packet[length + 2] = hex[checksum >> 4];
    packet[length + 3] = hex[checksum & 15];
    return send_all(fd, packet, length + 4);
}

int main(int argc, char **argv)
{
    int listener, client, option = 1, session, session_count;
    struct sockaddr_in addr;
    gdb_regs_ps2 frame;
    struct gdb_regs regs;
    char request[4096], response[R5900_RSP_ALL_HEX + 1];
    uint64_t val;
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Usage: mock_rsp_server <port> [sessions]\n");
        return 2;
    }
    memset(&frame, 0, sizeof(frame));
    memset(&regs, 0, sizeof(regs));
    r5900_rsp_set_low64(&frame.reg29, 0x01ff0000);
    regs.reg29 = 0x01ff0000;
    regs.cp0_epc = 0x00100ae0;
    regs.cp0_status = 0x0000ff01;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return 3;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)atoi(argv[1]));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
        listen(listener, 1) < 0) return 4;
    session_count = argc == 3 ? atoi(argv[2]) : 1;
    if (session_count < 1 || session_count > 10) return 6;
    for (session = 0; session < session_count; session++) {
        client = accept(listener, NULL, NULL);
        if (client < 0) return 5;
        for (;;) {
            unsigned int index;
            if (receive_packet(client, request, sizeof(request)) < 0) break;
            response[0] = 0;
            if (!strncmp(request, "qSupported", 10)) strcpy(response, "PacketSize=7fc");
            else if (!strcmp(request, "?")) strcpy(response, "S05");
            else if (!strcmp(request, "qAttached")) strcpy(response, "1");
            else if (!strcmp(request, "qC")) strcpy(response, "QC1");
            else if (!strcmp(request, "qfThreadInfo")) strcpy(response, "m1");
            else if (!strcmp(request, "qsThreadInfo")) strcpy(response, "l");
            else if (!strncmp(request, "qOffsets", 8)) strcpy(response, "Text=0;Data=0;Bss=0");
            else if (!strcmp(request, "g"))
                r5900_rsp_encode_registers(response, &frame, &regs);
            else if (request[0] == 'G') {
                if (r5900_rsp_bounded_length(request + 1, sizeof(request) - 1) ==
                        R5900_RSP_ALL_HEX &&
                    r5900_rsp_decode_registers(request + 1, &frame, &regs))
                    strcpy(response, "OK");
                else
                    strcpy(response, "E01");
            } else if (request[0] == 'p' && r5900_rsp_parse_regno(request+1, &index) &&
                     index < R5900_RSP_REG_COUNT)
                r5900_rsp_encode_u64(response,
                    r5900_rsp_register_value(&frame, &regs, index));
            else if (request[0] == 'P') {
                char *equals = strchr(request, '=');
                if (equals && r5900_rsp_bounded_length(equals+1,
                    sizeof(request) - (equals + 1 - request)) == 16) {
                    *equals = 0;
                    if (r5900_rsp_parse_regno(request+1, &index) &&
                        index < R5900_RSP_REG_COUNT &&
                        r5900_rsp_decode_u64(equals+1, &val)) {
                        r5900_rsp_set_register(&frame, &regs, index, val);
                        strcpy(response, "OK");
                    } else strcpy(response, "E01");
                } else strcpy(response, "E01");
            } else if (request[0] == 'H' || request[0] == 'T') strcpy(response, "OK");
            else if (request[0] == 'm') strcpy(response, "E03");
            else if (request[0] == 'D') {
                reply_packet(client, "OK");
                break;
            }
            if (reply_packet(client, response) < 0) break;
    }
        close(client);
    }
    close(listener);
    return 0;
}
