set logging on /cygdrive/c/gdb.log
symbol-file /usr/local/ps2dev/ps2gdbStub/ee/ps2gdbStub.elf
set endian little
set verbose 1
set language asm
set output-radix 16
target remote 192.168.1.52:12
break wait_a_while
c
