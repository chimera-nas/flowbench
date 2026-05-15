# SPDX-FileCopyrightText: 2025 Ben Jarvis
#
# SPDX-License-Identifier: Unlicense

FROM ubuntu:26.04

ARG ENABLE_XLIO=1

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update && \
    apt-get -y --no-install-recommends upgrade && \
    apt-get -y --no-install-recommends install unminimize && \
    echo y | unminimize && \
    apt-get -y --no-install-recommends install gcc cmake ninja-build git gdb less psmisc uncrustify libnuma-dev curl \
    autoconf automake make libtool pkg-config vim docker.io sudo ncurses-bin libssl-dev openssl clangd reuse uthash-dev \
    net-tools tshark tcpdump uuid-dev iproute2 man-db manpages-dev ca-certificates ssh libjansson-dev libclang-rt-18-dev llvm \
    libxxhash-dev liburcu-dev librdmacm-dev liburing-dev libunwind-dev flex bison libncurses-dev libcurl4-openssl-dev build-essential ruby-full && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    curl -fsSL https://claude.ai/install.sh | bash && \
    echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc


RUN if [ "$ENABLE_XLIO" = "1" ] ; then \
    git clone https://github.com/benjarvis/libdpcp.git /libdpcp && \
    cd /libdpcp && \
    git checkout master-ubuntu26 && \
    ./autogen.sh && \
    ./configure && \
    make -j8 && \
    make install && \
    git clone https://github.com/Mellanox/libxlio.git /libxlio && \
    cd /libxlio && \
    git checkout 3.61.2 && \
    ./autogen.sh && \
    ./configure --with-dpcp=/usr/local && \
    make -j8 && \
    make install ; \
    fi
