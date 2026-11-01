EE_BIN = ee_debug_probe.elf
EE_OBJS = ee_debug_probe.o ee_debug_minimal.o ee_debug_bd2.o
EE_CFLAGS += -g -Wall
EE_LIBS = -leedebug

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
