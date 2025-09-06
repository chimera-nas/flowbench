#!/bin/bash

# SPDX-FileCopyrightText: 2025 Ben Jarvis
#
# SPDX-License-Identifier: LGPL-2.1-only

NETNS_NAME="flowbench_ns_$(date +%s%N)_$RANDOM"

cleanup() {
    if ip netns list | grep -q "^${NETNS_NAME}"; then
        ip netns delete "${NETNS_NAME}" 2>/dev/null || true
    fi
}

trap cleanup EXIT

echo "Running test in network namespace: ${NETNS_NAME}"

ip netns add "${NETNS_NAME}"

ip netns exec "${NETNS_NAME}" ip link set lo up

ip netns exec "${NETNS_NAME}" "$@"