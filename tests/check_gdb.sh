#!/bin/sh
# Exercise the real GNU R5900 debugger against the native C RSP fixture.
set -eu
GDB=${PS2GDB_GDB:-mips64r5900el-ps2-elf-gdb}
PORT=${PS2GDB_TEST_PORT:-21599}
LOG=$(mktemp)
./tests/mock_rsp_server "$PORT" &
SERVER=$!
cleanup() {
    kill "$SERVER" 2>/dev/null || true
    wait "$SERVER" 2>/dev/null || true
    rm -f "$LOG"
}
trap cleanup EXIT HUP INT TERM
sleep 1
"$GDB" -nx -batch -q \
    -ex 'set pagination off' -ex 'set confirm off' \
    -ex 'set remotetimeout 3' \
    -ex 'file sample/testgdb.elf' \
    -ex "target remote 127.0.0.1:$PORT" \
    -ex 'info registers pc sp sr lo hi' \
    -ex 'detach' > "$LOG" 2>&1 || { cat "$LOG"; exit 1; }
cat "$LOG"
grep -q 'pc: 0x100ae0' "$LOG"
grep -q 'sp: 0x1ff0000' "$LOG"
grep -q 'sr: 0xff01' "$LOG"
grep -q '\[Inferior 1 (Remote target) detached\]' "$LOG"
echo 'PASS real R5900 GDB versus native RSP fixture'
