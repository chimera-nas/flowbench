#!/bin/bash

# SPDX-FileCopyrightText: 2025 Ben Jarvis
#
# SPDX-License-Identifier: LGPL-2.1-only

FLOWBENCH=$1
SERVER_ARGS=$2
CLIENT_ARGS=$3

echo "flowbench: $FLOWBENCH"
echo "server_args: $SERVER_ARGS"
echo "client_args: $CLIENT_ARGS"
# Start server in background
$FLOWBENCH -r server $SERVER_ARGS &
server_pid=$!

# Give server time to initialize
sleep 5

# Start client
$FLOWBENCH -r client $CLIENT_ARGS
client_status=$?

echo Stopping server

kill -INT $server_pid
wait $server_pid
server_status=$?

rc=0

# Test passes only if both processes exit with 0
if [ $server_status -ne 0 ]; then
    echo "Server failed with status $server_status"
    rc=1
fi

if [ $client_status -ne 0 ]; then
    echo "Client failed with status $client_status"
    rc=1
fi
exit $rc
