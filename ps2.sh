# Load the unstripped ELF before connecting, so breakpoints resolve correctly.
# Replace 192.168.0.10 with your PS2Link IP address.
set pagination off
set remotetimeout 10
file sample/testgdb.elf
target remote 192.168.0.10:12
