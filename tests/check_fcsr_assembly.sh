#!/bin/sh
# Compile-only audit: FCSR/FIR are COP1 controls, not floating data registers.
# This never installs an EE exception vector or executes an ELF.
set -eu

OBJECT=${1:-ee/gdb-low.o}
OBJDUMP=${OBJDUMP:-mips64r5900el-ps2-elf-objdump}
DISASM=$(mktemp)
trap 'rm -f "$DISASM"' EXIT HUP INT TERM

"$OBJDUMP" -dr "$OBJECT" > "$DISASM"
test "$(grep -c 'cfc1[[:space:]]*v0,c1_fcsr' "$DISASM")" -eq 1
test "$(grep -c 'cfc1[[:space:]]*v0,c1_fir' "$DISASM")" -eq 1
test "$(grep -c 'ctc1[[:space:]]*v0,c1_fcsr' "$DISASM")" -eq 1

# FCSR is restored after the final FPR load, before CP0 Status/EPC.
awk '
    /lwc1[[:space:]]+\$f0,/ { f0 = NR }
    /ctc1[[:space:]]+v0,c1_fcsr/ { fcsr = NR }
    /mtc0[[:space:]]+v0,c0_sr/ { status = NR }
    END { if (!f0 || !fcsr || !status || !(f0 < fcsr && fcsr < status)) exit 1 }
' "$DISASM"

echo 'PASS EE FCSR/FIR COP1 control save and post-FPR FCSR restore'
