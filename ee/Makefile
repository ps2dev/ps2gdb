#
# Makefile template
#

PS2IP = $(PS2DEV)/ps2ip

EE_BIN  = ps2gdbStub.elf
EE_OBJS = ps2gdbStub.o gdb-low.o crt0.o

EE_CFLAGS +=	-g
EE_LDFLAGS +=	-Wl,-Map,ps2gdbStub.map -L. -L../lib -L$(PS2IP)/ee/lib

all: $(EE_BIN)
	ee-objdump --disassemble-all --disassemble-zeroes $(EE_BIN) > ps2gdbStub.S

clean:
	rm -f *.elf *.o *.map

include ../Makefile.pref
include ../Makefile.eeglobal
