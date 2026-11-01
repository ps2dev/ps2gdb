EE_BIN = ee_interrupt_context_probe.elf
EE_OBJS = ee_interrupt_context_probe.o
EE_CFLAGS += -g -Wall

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
