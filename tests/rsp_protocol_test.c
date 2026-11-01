/* Native RSP session tests: checksums, splitting, registers and reconnect. */
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "../ee/r5900-rsp.h"

static const char hex[] = "0123456789abcdef";
static int failures;

static void check(int ok, const char *name)
{
    if (!ok) {
        fprintf(stderr, "FAIL %s\n", name);
        failures++;
    }
}

static int receive_byte(int fd)
{
    unsigned char byte;
    return recv(fd, &byte, 1, 0) == 1 ? byte : -1;
}

static int send_all(int fd, const char *buffer, size_t length, int fragmented)
{
    while (length) {
        size_t count = fragmented ? 1 : length;
        ssize_t n = send(fd, buffer, count, 0);
        if (n <= 0) return 0;
        buffer += n;
        length -= (size_t)n;
    }
    return 1;
}

static int connection(int port)
{
    int sock;
    struct sockaddr_in address;
    struct timeval timeout = {3, 0};
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons((unsigned short)port);
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (connect(sock, (const struct sockaddr *)&address, sizeof(address)) != 0) {
        close(sock);
        return -1;
    }
    return sock;
}

/* Receive the ACK and a checksum-validated reply, then acknowledge it. */
static int request(int fd, const char *command, char *response, size_t capacity,
                   int fragmented)
{
    char packet[4096];
    unsigned int checksum = 0, replysum = 0;
    size_t len = r5900_rsp_bounded_length(command, sizeof(packet) - 4);
    size_t i = 0;
    int c, first, second;
    if (len >= sizeof(packet) - 4 || capacity < 2) return 0;
    packet[0] = '$';
    memcpy(packet + 1, command, len);
    for (i = 0; i < len; i++) checksum = (checksum + (unsigned char)command[i]) & 255;
    packet[len + 1] = '#';
    packet[len + 2] = hex[checksum >> 4];
    packet[len + 3] = hex[checksum & 15];
    if (!send_all(fd, packet, len + 4, fragmented) || receive_byte(fd) != '+')
        return 0;
    if (receive_byte(fd) != '$') return 0;
    for (i = 0; (c = receive_byte(fd)) >= 0 && c != '#'; i++) {
        if (i >= capacity - 1) return 0;
        response[i] = (char)c;
        replysum = (replysum + c) & 255;
    }
    if (c < 0) return 0;
    response[i] = 0;
    first = receive_byte(fd);
    second = receive_byte(fd);
    if (first < 0 || second < 0 ||
        r5900_rsp_hex_digit(first) < 0 || r5900_rsp_hex_digit(second) < 0 ||
        (int)replysum != ((r5900_rsp_hex_digit(first) << 4) |
                          r5900_rsp_hex_digit(second))) return 0;
    return send_all(fd, "+", 1, 0);
}

static int is(int fd, const char *cmd, const char *expected, int fragmented)
{
    char response[4096];
    if (!request(fd, cmd, response, sizeof(response), fragmented)) return 0;
    return strcmp(response, expected) == 0;
}

int main(int argc, char **argv)
{
    char response[4096], packet[R5900_RSP_ALL_HEX + 2];
    uint64_t value;
    int port, fd;
    if (argc != 2) {
        fprintf(stderr, "Usage: rsp_protocol_test <port>\n");
        return 2;
    }
    port = atoi(argv[1]);
    fd = connection(port);
    if (fd < 0) {
        fprintf(stderr, "Could not connect to RSP fixture on port %d\n", port);
        return 3;
    }

    check(is(fd, "qSupported:multiprocess+", "PacketSize=7fc", 1),
          "fragmented qSupported handshake");
    check(is(fd, "?", "S05", 0), "stopped target status");
    check(is(fd, "qAttached", "1", 0), "attached state");
    check(is(fd, "qC", "QC1", 0), "thread identity");
    check(is(fd, "qfThreadInfo", "m1", 0), "thread enumeration");
    check(is(fd, "qsThreadInfo", "l", 0), "end thread enumeration");
    check(is(fd, "m100000,4", "E03", 0), "mock denies memory access");
    check(is(fd, "p1d", "0000ff0100000000", 1), "fragmented SP register read");
    check(is(fd, "p25", "e00a100000000000", 0), "PC is register 37");
    check(is(fd, "P1d=8877665544332211", "OK", 1), "write SP using P");
    check(is(fd, "p1d", "8877665544332211", 0), "read back written SP");
    check(is(fd, "P1d=invalid", "E01", 0), "reject invalid register hex");
    check(is(fd, "P5a=0000000000000000", "E01", 0), "reject register 90");

    check(request(fd, "g", response, sizeof(response), 0), "read full g packet");
    check(r5900_rsp_bounded_length(response, sizeof(response)) == R5900_RSP_ALL_HEX,
          "full register packet exactly 1440 bytes");
    check(r5900_rsp_decode_u64(response + 29 * 16, &value) &&
          value == 0x1122334455667788ULL, "g includes register update");
    memcpy(packet, "G", 1);
    memcpy(packet + 1, response, R5900_RSP_ALL_HEX);
    packet[R5900_RSP_ALL_HEX + 1] = 0;
    /* Modify the PC in the full G packet. */
    {
        char reg[17];
        r5900_rsp_encode_u64(reg, 0x0010a220);
        memcpy(packet + 1 + 37 * 16, reg, 16);
    }
    check(is(fd, packet, "OK", 0), "write full register packet");
    check(is(fd, "p25", "20a2100000000000", 0), "read back updated PC");
    packet[20] = '?';
    check(is(fd, packet, "E01", 0), "reject malformed full register packet");

    /* Deliberately corrupt the checksum; the mock must close the session. */
    check(send_all(fd, "$g#00", 5, 1), "send corrupted checksum");
    check(receive_byte(fd) < 0, "bad checksum closes mock session");
    close(fd);

    fd = connection(port);
    check(fd >= 0, "reconnect after malformed request");
    if (fd >= 0) {
        check(is(fd, "p25", "20a2100000000000", 1),
              "register state survives reconnect");
        check(is(fd, "D", "OK", 0), "clean detach");
        close(fd);
    }
    if (failures) return 1;
    puts("PASS RSP socket handshake, fragmentation, reads/writes, rejection and reconnect");
    return 0;
}
