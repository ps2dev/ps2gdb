#!/bin/sh
# Check the opt-in EE debugger cannot call the breakpointed FlushCache stub
# from its own cache-maintenance helper. Cross-build inspection only.
set -eu
OBJDUMP=${OBJDUMP:-mips64r5900el-ps2-elf-objdump}
ELF=${1:-sample/testgdb.elf}
DISASM=$(mktemp)
trap 'rm -f "$DISASM"' EXIT HUP INT TERM
"$OBJDUMP" -d "$ELF" | sed -n '/<flush_cache_all>:/,/^$/p' > "$DISASM"
test -s "$DISASM"
test "$(grep -c '[[:space:]]syscall' "$DISASM")" -eq 2
if grep -Eq '[[:space:]]jal[[:space:]]|<FlushCache>' "$DISASM"; then
    echo 'FAIL experimental cache helper calls FlushCache wrapper' >&2
    exit 1
fi
echo 'PASS experimental EE cache helper uses two direct syscalls'
