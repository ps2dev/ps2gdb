#!/bin/sh
# Host-only socket test: two sessions, including recovery from malformed RSP.
set -eu
PORT=${PS2GDB_PROTOCOL_TEST_PORT:-21601}
./tests/mock_rsp_server "$PORT" 2 &
SERVER=$!
cleanup() {
    kill "$SERVER" 2>/dev/null || true
    wait "$SERVER" 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM
# Server bind/listen will have completed before connecting on local CI runners.
sleep 1
./tests/rsp_protocol_test "$PORT"
wait "$SERVER"
