#
# Makefile template
#

EE_LIB  = ../lib/libps2gdbStub.a
EE_OBJS = ps2gdbStub.o gdb-low.o

EE_CFLAGS +=	-g
EE_LDFLAGS +=	-Wl,-Map,ps2gdbStub.map -L. -L../lib -L$(PS2SDK)/ee/lib

all: $(EE_LIB)
	 $(EE_AR) rcs $(EE_LIB) $(EE_OBJS)
clean:
	rm -f *.a *.o *.map

include ../Makefile.pref
include ../Makefile.eeglobal
