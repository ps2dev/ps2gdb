#include <stdio.h>
#include <stdint.h>
#include "preemptive/ee_thread_owner_observation.h"

static int failures;

static void check(int condition, const char *message)
{
    if (!condition) {
        ++failures;
        fprintf(stderr, "FAIL %s\n", message);
    }
}

int main(void)
{
    volatile struct ps2gdb_owner_observation main_thread = {0};
    volatile struct ps2gdb_owner_observation worker_thread = {0};
    const uint32_t begin = 0x00101118u;
    const uint32_t end = 0x00101200u;

    ps2gdb_owner_record(&main_thread, 10, 10, begin, begin, end);
    ps2gdb_owner_record(&main_thread, 10, 23, begin + 4, begin, end);
    ps2gdb_owner_record(&main_thread, 10, -1, begin + 8, begin, end);
    ps2gdb_owner_record(&main_thread, 10, 10, end, begin, end);
    check(main_thread.samples == 4 && main_thread.matches == 1 &&
          main_thread.other_threads == 1 && main_thread.unknown_ids == 1 &&
          main_thread.outside_range == 1,
          "observed identity is classified only for in-range application PCs");
    check(main_thread.first_pc == begin && main_thread.last_pc == end,
          "observed callback PC bounds retained without dereferencing");

    ps2gdb_owner_record(&worker_thread, 23, 10, begin + 4, begin, end);
    ps2gdb_owner_record(&worker_thread, 23, 23, begin + 8, begin, end);
    check(worker_thread.matches == 1 && worker_thread.other_threads == 1,
          "worker identity is classified independently of main thread");
    ps2gdb_owner_record(NULL, 23, 23, begin, begin, end);

    if (failures)
        return 1;
    puts("PASS read-only IRQ thread-owner observations are never proof of exclusivity");
    return 0;
}
