# SPDX-FileCopyrightText: 2025 Ben Jarvis
#
# SPDX-License-Identifier: Unlicense

FROM ubuntu:24.04 AS build
ARG BUILD_TYPE=Release
ARG ENABLE_XLIO=1

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update && \
    apt-get -y --no-install-recommends upgrade && \
    apt-get -y --no-install-recommends install clang cmake ninja-build git lldb gdb less psmisc \
    uuid-dev libjansson-dev libclang-rt-18-dev llvm build-essential uthash-dev \
    autoconf automake make libtool pkg-config ca-certificates libssl-dev libnuma-dev  \
    libxxhash-dev liburcu-dev librdmacm-dev liburing-dev libunwind-dev flex bison libncurses-dev libcurl4-openssl-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*


RUN if [ "$ENABLE_XLIO" = "1" ] ; then \
    git clone https://github.com/Mellanox/libdpcp.git /libdpcp && \
    cd /libdpcp && \
    ./autogen.sh && \
    ./configure && \
    make -j8 && \
    make install && \
    git clone https://github.com/Mellanox/libxlio.git /libxlio && \
    cd /libxlio && \
    git checkout 9877e909310cde1112744206cfeb04524910c758 && \
    ./autogen.sh && \
    ./configure --with-dpcp=/usr/local && \
    make -j8 && \
    make install ; \
    fi

ADD / /flowbench

RUN mkdir /build && \
    cd /build && \
    cmake -G Ninja -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_C_COMPILER=clang -DDISABLE_TESTS=ON /flowbench && \
    ninja && \
    ninja install

FROM ubuntu:24.04
ARG BUILD_TYPE=Release

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update && \
    apt-get -y --no-install-recommends upgrade && \
    apt-get -y --no-install-recommends install libuuid1 librdmacm1 libjansson4 liburcu8t64 ibverbs-providers liburing2 libunwind8 \
    libncurses6 libssl3t64 libnuma1 && \
    if [ "${BUILD_TYPE}" = "Debug" ]; then \
    apt-get -y --no-install-recommends install llvm gdb ; \
    fi && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local/bin/flowbench /usr/local/bin/flowbench
COPY --from=build /usr/local/lib/* /usr/local/lib/
#COPY --from=build /usr/local/sbin/* /usr/local/sbin/
#COPY --from=build /usr/local/etc/* /usr/local/etc/

ENV LD_LIBRARY_PATH=/usr/local/lib

# Just so the dockerfile fails to build if we are missing libs or some such
RUN /usr/local/bin/flowbench -v

ENTRYPOINT ["/usr/local/bin/flowbench"]
