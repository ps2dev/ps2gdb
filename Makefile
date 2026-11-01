#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, Ray Donnelly ( rdonnelly@polygoons.com )
#  --------------------------------------------------------------------------


HOST_CC ?= cc
HOST_CFLAGS ?= -std=c99 -Wall -Wextra -Werror

all:
	$(MAKE) -C ee
	$(MAKE) -C sample

check: tests/r5900_rsp_test tests/registers_test tests/mock_rsp_server tests/rsp_protocol_test tests/hw_breakpoint_state_test tests/ctrlc_rsp_client
	./tests/r5900_rsp_test
	./tests/registers_test
	./tests/hw_breakpoint_state_test
	sh tests/check_protocol.sh

# Builds the EE hardware exception probe but never runs it automatically.
hardware-probe-build:
	$(MAKE) -C tests/hardware clean all

# Diagnostic macros compile only; the script restores a non-triggering ELF.
hardware-probe-variants-build:
	sh tests/hardware/check_variants.sh

check-install:
	$(MAKE) -C ee
	sh tests/check_install.sh

check-gdb: tests/mock_rsp_server
	$(MAKE) -C ee
	$(MAKE) -C sample
	sh tests/check_gdb.sh

tests/ctrlc_rsp_client: tests/ctrlc_rsp_client.c
	$(HOST_CC) $(HOST_CFLAGS) $< -o $@

# The mock server is used for host-to-host integration with PS2DEV GDB.
# Building it in CI also ensures its native C sources stay portable.
tests/r5900_rsp_test: tests/r5900_rsp_test.c ee/r5900-rsp.h
	$(HOST_CC) $(HOST_CFLAGS) $< -o $@

tests/registers_test: tests/registers_test.c ee/r5900-registers.h ee/r5900-rsp.h
	$(HOST_CC) $(HOST_CFLAGS) $< -o $@

tests/hw_breakpoint_state_test: tests/hw_breakpoint_state_test.c ee/hw-breakpoint-state.h ee/r5900-rsp.h
	$(HOST_CC) $(HOST_CFLAGS) $< -o $@

tests/rsp_protocol_test: tests/rsp_protocol_test.c ee/r5900-rsp.h
	$(HOST_CC) $(HOST_CFLAGS) $< -o $@

tests/mock_rsp_server: tests/mock_rsp_server.c ee/r5900-registers.h ee/r5900-rsp.h
	$(HOST_CC) $(HOST_CFLAGS) $< -o $@

debug:
	$(MAKE) -C ee debug
	$(MAKE) -C sample

install:
	$(MAKE) -C ee install

clean:
	$(MAKE) -C ee clean
	$(MAKE) -C sample clean
	rm -f tests/r5900_rsp_test tests/registers_test tests/mock_rsp_server tests/rsp_protocol_test tests/hw_breakpoint_state_test tests/ctrlc_rsp_client
