#!/bin/sh
# Verify that consumers can install and compile against the public EE API.
set -eu
PREFIX=$(mktemp -d "${TMPDIR:-/tmp}/ps2gdb-install.XXXXXXXX")
cleanup() { rm -rf "$PREFIX"; }
trap cleanup EXIT HUP INT TERM

make -C ee install PS2GDB_INSTALL_PREFIX="$PREFIX"
test -s "$PREFIX/lib/libps2gdbStub.a"
test -s "$PREFIX/include/ps2gdb.h"

# A downstream C and C++ application can consume the staged public interface.
printf '%s\n' '#include <ps2gdb.h>' 'int test(void) { return gdb_stub_main(0, 0); }' |
    "${EE_CC:-mips64r5900el-ps2-elf-gcc}" -x c -fsyntax-only -I"$PREFIX/include" -
printf '%s\n' '#include <ps2gdb.h>' 'int test() { return gdb_stub_main(0, nullptr); }' |
    "${EE_CXX:-mips64r5900el-ps2-elf-g++}" -x c++ -fsyntax-only -I"$PREFIX/include" -

"${EE_NM:-mips64r5900el-ps2-elf-nm}" "$PREFIX/lib/libps2gdbStub.a" |
    grep -Eq ' [Tt] gdb_stub_main$'
echo 'PASS staged PS2SDK port installation and public C/C++ header'
