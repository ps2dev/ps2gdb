EE_BIN = ee_interrupt_context_probe.elf
EE_OBJS = ee_interrupt_context_probe.o
BRANCH_BIN = ee_branch_delay_probe.elf
BRANCH_OBJS = ee_branch_delay_probe.o
CONTEXT_OBJ = ee_context_candidate.o
EE_CFLAGS += -g -Wall -Wextra -Werror

all: $(EE_BIN) $(BRANCH_BIN) $(CONTEXT_OBJ)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) $(BRANCH_BIN) $(BRANCH_OBJS) $(CONTEXT_OBJ)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

$(BRANCH_BIN): $(BRANCH_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) $(EXTRA_LDFLAGS) $(EE_LIBS)
