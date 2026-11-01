#
# Makefile template
#

EE_BIN = testgdb.elf
EE_OBJS = testgdb.o

EE_CFLAGS += -g -w
EE_INCS += -I../ee
EE_LIBS = -ldebug -lps2gdbStub -lps2ips
EE_LDFLAGS += -L../lib

all:
	cp -f $(PS2SDK)/iop/irx/ps2ips.irx ./
	$(MAKE) $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) ps2ips.irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
