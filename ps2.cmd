set logging on ~/ps2dev/gdb.log
symbol-file ~/ps2dev/ps2gdbStub.elf
set endian little
set verbose 1
set language asm
set output-radix 16
target remote 192.168.0.10:12
break wait_a_while
c
