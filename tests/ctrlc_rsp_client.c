/*
 * Manual PS2 hardware RSP smoke test; never run against a random address.
 * Connect to a PS2GDB test ELF, resume, send the raw 0x03 GDB interrupt,
 * confirm SIGINT, repeat, and detach. Protocol bytes are deliberately C-only.
 * Usage: ./tests/ctrlc_rsp_client 192.168.31.10 [port]
 */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int sock;

static int send_all(const char *bytes, size_t n)
{
    while (n) {
        ssize_t sent = send(sock, bytes, n, 0);
        if (sent <= 0) return 0;
        bytes += sent;
        n -= (size_t)sent;
    }
    return 1;
}

static int getbyte(void)
{
    unsigned char c;
    ssize_t n = recv(sock, &c, 1, 0);
    return n == 1 ? c : -1;
}

static int send_packet(const char *text)
{
    unsigned char sum = 0;
    char trailer[3];
    const unsigned char *p;
    for (p = (const unsigned char *)text; *p; ++p) sum += *p;
    snprintf(trailer, sizeof(trailer), "%02x", sum);
    if (!send_all("$", 1) || !send_all(text, strlen(text)) ||
        !send_all("#", 1) || !send_all(trailer, 2)) return 0;
    /* First stop may emit a legacy '-' before the initial RSP ACK. */
    {
        int ack;
        do { ack = getbyte(); } while (ack == '-');
        return ack == '+';
    }
}

static int read_packet(char *out, size_t capacity)
{
    unsigned int sum = 0;
    size_t n = 0;
    int c, hi, lo;
    while ((c = getbyte()) >= 0 && c != '$') { }
    if (c < 0) return 0;
    while ((c = getbyte()) >= 0 && c != '#') {
        if (n + 1 >= capacity) return 0;
        out[n++] = (char)c;
        sum += (unsigned char)c;
    }
    if (c < 0) return 0;
    hi = getbyte(); lo = getbyte();
    if (hi < 0 || lo < 0) return 0;
    {   unsigned int check = 0;
        char digits[3] = {(char)hi, (char)lo, 0};
        if (sscanf(digits, "%2x", &check) != 1 || (sum & 255u) != check)
            return 0;
    }
    out[n] = 0;
    return send_all("+", 1);
}

int main(int argc, char **argv)
{
    struct sockaddr_in target;
    struct timeval timeout;
    char response[2048];
    int i, rc = 1;
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s PS2-IP [port]\n", argv[0]);
        return 2;
    }
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons((unsigned short)(argc == 3 ? atoi(argv[2]) : 12));
    if (inet_pton(AF_INET, argv[1], &target.sin_addr) != 1) return 2;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 1;
    timeout.tv_sec = 8; timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (connect(sock, (struct sockaddr *)&target, sizeof(target)) < 0) {
        perror("connect"); goto done;
    }
    /* Initial GDBStub stop sends '-' before its first framed packet. */
    if (!send_packet("?") || !read_packet(response, sizeof(response))) {
        fprintf(stderr, "initial stop packet failed\n"); goto done;
    }
    printf("INITIAL %s\n", response);
    for (i = 0; i < 2; i++) {
        if (!send_packet("c")) { fprintf(stderr, "continue failed\n"); goto done; }
        {
            const struct timespec pause_time = {0, 200000000};
            nanosleep(&pause_time, NULL);
        }
        if (!send_all("\003", 1) || !read_packet(response, sizeof(response))) {
            fprintf(stderr, "Ctrl-C #%d stop timed out\n", i + 1);
            goto done;
        }
        printf("CTRL_C_%d %s\n", i + 1, response);
        if (strncmp(response, "T02", 3) && strcmp(response, "S02")) {
            fprintf(stderr, "expected GDB SIGINT stop (02)\n"); goto done;
        }
    }
    if (!send_packet("D") || !read_packet(response, sizeof(response)) ||
        strcmp(response, "OK")) {
        fprintf(stderr, "detach failed\n"); goto done;
    }
    puts("PASS two cooperative Ctrl-C stops and detach");
    rc = 0;
done:
    close(sock);
    return rc;
}
