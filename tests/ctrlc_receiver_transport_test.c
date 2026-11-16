/*
 * POSIX-only, real socketpair handshake test for the pure receiver model.
 * This does not run EE code or emulate the PS2IPS implementation.
 */
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "../ee/ctrlc-receiver-state.h"

struct test_receiver {
    pthread_mutex_t lock;
    struct ps2gdb_receiver_state state;
    int fd;
    int shutdown;
};

static void sleep_millis(long ms)
{
    const struct timespec duration = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&duration, NULL);
}

static void *reader(void *argument)
{
    struct test_receiver *receiver = argument;
    for (;;) {
        unsigned int generation = 0;
        unsigned char byte;
        int active, shutdown, result;
        ssize_t count;

        pthread_mutex_lock(&receiver->lock);
        shutdown = receiver->shutdown;
        active = ps2gdb_receiver_begin_read(&receiver->state, &generation);
        pthread_mutex_unlock(&receiver->lock);
        if (shutdown)
            break;
        if (!active) {
            sleep_millis(1);
            continue;
        }
        count = recv(receiver->fd, &byte, 1, 0);
        result = count == 1 ? (byte == 0x03 ? 1 : 2) :
                 (count == 0 ? 0 : -1);
        pthread_mutex_lock(&receiver->lock);
        ps2gdb_receiver_finish_read(&receiver->state, generation, result);
        pthread_mutex_unlock(&receiver->lock);
        sleep_millis(1);
    }
    return NULL;
}

static int await_interrupt(struct test_receiver *receiver)
{
    int i;
    for (i = 0; i < 2000; ++i) {
        int pending;
        pthread_mutex_lock(&receiver->lock);
        pending = ps2gdb_receiver_state_take_interrupt(&receiver->state);
        pthread_mutex_unlock(&receiver->lock);
        if (pending)
            return 1;
        sleep_millis(1);
    }
    return 0;
}

int main(void)
{
    int fds[2] = {-1, -1}, failures = 0, round;
    pthread_t worker;
    struct test_receiver receiver;
    memset(&receiver, 0, sizeof(receiver));
    if (pthread_mutex_init(&receiver.lock, NULL) ||
        socketpair(AF_UNIX, SOCK_STREAM, 0, fds)) {
        fprintf(stderr, "FAIL test socket or mutex creation\n");
        return 1;
    }
    receiver.fd = fds[0];
    if (fcntl(fds[0], F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr, "FAIL setting test socket nonblocking\n");
        close(fds[0]); close(fds[1]);
        pthread_mutex_destroy(&receiver.lock);
        return 1;
    }
    if (pthread_create(&worker, NULL, reader, &receiver)) {
        fprintf(stderr, "FAIL test reader thread creation\n");
        close(fds[0]); close(fds[1]);
        pthread_mutex_destroy(&receiver.lock);
        return 1;
    }

    for (round = 0; round < 12; ++round) {
        unsigned char prefix = 0;
        int started, stopped, attempt;

        pthread_mutex_lock(&receiver.lock);
        started = ps2gdb_receiver_run(&receiver.state, (unsigned int)round + 1);
        pthread_mutex_unlock(&receiver.lock);
        if (!started || send(fds[1], "\003", 1, 0) != 1 ||
            !await_interrupt(&receiver)) {
            fprintf(stderr, "FAIL Ctrl-C receive in run %d\n", round);
            ++failures;
            break;
        }

        /* Once stopped, the RSP parser, not the receiver, must own bytes. */
        pthread_mutex_lock(&receiver.lock);
        stopped = ps2gdb_receiver_stop(&receiver.state);
        pthread_mutex_unlock(&receiver.lock);
        if (!stopped || send(fds[1], "$", 1, 0) != 1) {
            fprintf(stderr, "FAIL RSP handoff in run %d\n", round);
            ++failures;
            break;
        }
        for (attempt = 0; attempt < 200 && prefix != '$'; ++attempt) {
            if (recv(fds[0], &prefix, 1, 0) == 1)
                break;
            sleep_millis(1);
        }
        if (prefix != '$') {
            fprintf(stderr, "FAIL stopped RSP byte ownership in run %d\n", round);
            ++failures;
            break;
        }
    }

    pthread_mutex_lock(&receiver.lock);
    receiver.shutdown = 1;
    ps2gdb_receiver_stop(&receiver.state);
    pthread_mutex_unlock(&receiver.lock);
    pthread_join(worker, NULL);
    close(fds[0]);
    close(fds[1]);
    pthread_mutex_destroy(&receiver.lock);
    if (failures)
        return 1;
    puts("PASS 12 socketpair Ctrl-C handoffs with exclusive stopped RSP ownership");
    return 0;
}
