# SPDX-FileCopyrightText: 2026 Ben Jarvis
#
# SPDX-License-Identifier: Unlicense

FROM ubuntu:26.04

ARG UBUNTU_MIRROR=""
ENV DEBIAN_FRONTEND=noninteractive

# Match libevpl's Azure mirror selection on GitHub-hosted runners.
RUN if [ "$UBUNTU_MIRROR" = azure ]; then \
        sed -i -e 's|//archive\.ubuntu\.com|//azure.archive.ubuntu.com|g' \
               -e 's|//security\.ubuntu\.com|//azure.archive.ubuntu.com|g' \
               -e 's|//ports\.ubuntu\.com|//azure.ports.ubuntu.com|g' \
            /etc/apt/sources.list.d/ubuntu.sources; \
    fi

# libevpl's distro dependencies plus ncurses for Flowbench's terminal UI.
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        gcc g++ cmake ninja-build git flex bison pkg-config ca-certificates \
        curl iproute2 python3 \
        libssl-dev openssl libncurses-dev uthash-dev uuid-dev \
        liburing-dev libaio-dev librdmacm-dev libnuma-dev libunwind-dev \
        libnghttp2-dev libcurl4-openssl-dev libprotobuf-c-dev protobuf-c-compiler && \
    apt-get clean && rm -rf /var/lib/apt/lists/*
