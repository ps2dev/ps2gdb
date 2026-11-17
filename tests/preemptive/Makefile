EE_BIN = ee_interrupt_context_probe.elf
EE_OBJS = ee_interrupt_context_probe.o
BRANCH_BIN = ee_branch_delay_probe.elf
BRANCH_OBJS = ee_branch_delay_probe.o
CONTEXT_OBJ = ee_context_candidate.o
STOP_POLICY_OBJ = ee_stop_policy_compile_only.o
OWNER_BIN = ee_thread_owner_probe.elf
OWNER_OBJS = ee_thread_owner_probe.o
SCHEDULE_BIN = ee_receiver_schedule_probe.elf
SCHEDULE_OBJS = ee_receiver_schedule_probe.o
EE_CFLAGS += -g -Wall -Wextra -Werror
ifeq ($(PS2GDB_SCHED_LOWER_TARGET_PRIORITY),1)
    EE_CFLAGS += -DPS2GDB_SCHED_LOWER_TARGET_PRIORITY
endif

all: $(EE_BIN) $(BRANCH_BIN) $(OWNER_BIN) $(SCHEDULE_BIN) $(CONTEXT_OBJ) $(STOP_POLICY_OBJ)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) $(BRANCH_BIN) $(BRANCH_OBJS) $(OWNER_BIN) $(OWNER_OBJS) $(SCHEDULE_BIN) $(SCHEDULE_OBJS) $(CONTEXT_OBJ) $(STOP_POLICY_OBJ)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

$(BRANCH_BIN): $(BRANCH_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) $(EXTRA_LDFLAGS) $(EE_LIBS)

$(OWNER_BIN): $(OWNER_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) $(EXTRA_LDFLAGS) $(EE_LIBS)

$(SCHEDULE_BIN): $(SCHEDULE_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) $(EXTRA_LDFLAGS) $(EE_LIBS)
