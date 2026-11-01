#!/bin/sh
# Cross-compile representative diagnostic variants; NEVER run them in CI.
set -eu
BASE='-D_EE -G0 -g -Wall -DPS2GDB_PROBE_TRIGGER -DPS2GDB_PROBE_MINIMAL_VECTOR'
restore_safe_probe() {
    make -C tests/hardware clean
    make -C tests/hardware
}
trap restore_safe_probe EXIT

make -C tests/hardware clean
make -C tests/hardware EE_CFLAGS="$BASE -DPS2GDB_PROBE_EXEC"
make -C tests/hardware clean
make -C tests/hardware EE_CFLAGS="$BASE -DPS2GDB_PROBE_READ -DPS2GDB_PROBE_READ_UNCACHED"
make -C tests/hardware clean
make -C tests/hardware EE_CFLAGS="$BASE -DPS2GDB_PROBE_READ -DPS2GDB_PROBE_READ_COLD"
make -C tests/hardware clean
make -C tests/hardware EE_CFLAGS="$BASE -DPS2GDB_PROBE_SAVE_GPRS -DPS2GDB_PROBE_CALL_C -DPS2GDB_PROBE_L1_HANDOFF"
make -C tests/hardware clean
make -C tests/hardware EE_CFLAGS="$BASE -DPS2GDB_PROBE_SAVE_GPRS -DPS2GDB_PROBE_L1_HANDOFF -DPS2GDB_PROBE_BD2"
echo 'PASS hardware diagnostic variants cross-compiled (never executed)'
