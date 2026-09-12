#!/bin/bash

# SPDX-FileCopyrightText: 2025 - 2026 Ben Jarvis
#
# SPDX-License-Identifier: LGPL-2.1-only

FLOWBENCH=$1
SERVER_ARGS=$2
CLIENT_ARGS=$3

echo "flowbench: $FLOWBENCH"
echo "server_args: $SERVER_ARGS"
echo "client_args: $CLIENT_ARGS"


client_output=$(mktemp)
cleanup() {
    if [ -n "${client_pid:-}" ]; then
        kill -TERM "$client_pid" 2>/dev/null || true
        wait "$client_pid" 2>/dev/null || true
    fi
    if [ -n "${server_pid:-}" ]; then
        kill -INT "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
    rm -f "$client_output"
}
trap cleanup EXIT
trap 'exit 1' INT TERM

# Start server in background
"$FLOWBENCH" -r server $SERVER_ARGS &
server_pid=$!

# Give server time to initialize
sleep 2

# Start client
"$FLOWBENCH" -r client $CLIENT_ARGS > "$client_output" &
client_pid=$!

wait $client_pid
client_status=$?
client_pid=
cat "$client_output"

kill -INT $server_pid
wait $server_pid
server_status=$?
server_pid=

# A clean exit with no measured traffic is not a successful benchmark.
if ! grep -q "^Flow:" "$client_output" ||
   grep -q "Flow: Sent: 0.00 B .*Recv: 0.00 B " "$client_output"; then
    echo "No measured traffic" >&2
    exit 1
fi

# Test passes only if both processes exit with 0
if [ $server_status -eq 0 ] && [ $client_status -eq 0 ]; then
    exit 0
else
    exit 1
fi
