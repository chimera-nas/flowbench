# SPDX-FileCopyrightText: 2025 Ben Jarvis
#
# SPDX-License-Identifier: Unlicense
#
# Ubuntu 26.04 so that liburing (2.14) is new enough for io_uring zero-copy
# receive (ZCRX). The 24.04 liburing (2.5) lacks io_uring_register_ifq and
# RECV_ZC, and libevpl silently compiles ZCRX out against it.

FROM ubuntu:26.04 AS build
ARG BUILD_TYPE=Release
ARG ENABLE_XLIO=1
ARG ENABLE_SPDK=0
ARG SPDK_VERSION=v25.09
ARG XLIO_REPO=https://github.com/Mellanox/libxlio.git
ARG XLIO_REF=3.80.1

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update && \
    apt-get -y --no-install-recommends upgrade && \
    apt-get -y --no-install-recommends install gcc cmake ninja-build git gdb less psmisc \
    uuid-dev libjansson-dev build-essential uthash-dev \
    autoconf automake make libtool pkg-config ca-certificates libssl-dev libnuma-dev  \
    libxxhash-dev liburcu-dev librdmacm-dev liburing-dev libunwind-dev flex bison libncurses-dev libcurl4-openssl-dev libnl-3-dev libnl-route-3-dev \
    libfabric-dev libfabric-bin libaio-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# NVIDIA XLIO (user-space TCP over ConnectX). XLIO_REF/XLIO_REPO are overridable
# for testing other versions.
RUN if [ "$ENABLE_XLIO" = "1" ] ; then \
    git clone --depth 1 https://github.com/Mellanox/libdpcp.git /libdpcp && \
    cd /libdpcp && \
    ./autogen.sh && \
    ./configure && \
    make -j8 && \
    make install && \
    git clone ${XLIO_REPO} /libxlio  && \
    cd /libxlio && \
    git checkout ${XLIO_REF} && \
    ./autogen.sh && \
    ./configure --with-dpcp=/usr/local && \
    make -j8 && \
    make install ; \
    fi

# Keep XLIO's stats reader for the runtime image (absent when XLIO is disabled).
RUN mkdir -p /xlio-bin && ( cp /usr/local/bin/xlio_stats /xlio-bin/ 2>/dev/null || true )

# SPDK (built from source, shared libs) so libevpl's EVPL_CORE_MECH_SPDK guest
# mode can link it via pkg-config. Guarded by ENABLE_SPDK so libfabric can be
# measured both with and without SPDK compiled in. Tests run without hugepages
# via spdk_env_opts.no_huge; DPDK uses a generic platform baseline.
RUN if [ "$ENABLE_SPDK" = "1" ] ; then \
    apt-get -y update && apt-get -y --no-install-recommends install \
        python3 python3-pip python3-pyelftools python3-setuptools \
        meson nasm patchelf help2man && \
    git clone --depth 1 --branch ${SPDK_VERSION} --recurse-submodules \
        --shallow-submodules https://github.com/spdk/spdk.git /tmp/spdk && \
    cd /tmp/spdk && \
    case "$(uname -m)" in \
        x86_64) spdk_arch=x86-64-v2 ;; \
        aarch64) spdk_arch=armv8-a ;; \
        *) echo "Unsupported SPDK architecture: $(uname -m)" >&2; exit 1 ;; \
    esac && \
    ./configure --prefix=/usr/local --with-shared \
        --target-arch="$spdk_arch" \
        --disable-tests --disable-unit-tests --disable-examples \
        --disable-apps --without-nvme-cuse --with-uring && \
    make -j$(nproc) DPDKBUILD_FLAGS=-Dplatform=generic && \
    make install && \
    sed -i "s|-L/tmp/spdk/dpdk/build/lib|-L/usr/local/lib|g" /usr/local/lib/pkgconfig/*.pc && \
    ldconfig && \
    cd / && rm -rf /tmp/spdk ; \
    fi

ADD / /flowbench

RUN mkdir /build && \
    cd /build && \
    cmake -G Ninja -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DDISABLE_TESTS=ON \
        -DLIBFABRIC_ENABLED=ON \
        -DSPDK_ENABLED=$([ "$ENABLE_SPDK" = "1" ] && echo YES || echo NO) /flowbench && \
    ninja && \
    ninja install

FROM ubuntu:26.04
ARG BUILD_TYPE=Release

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update && \
    apt-get -y --no-install-recommends upgrade && \
    apt-get -y --no-install-recommends install libuuid1 librdmacm1 libjansson4 liburcu8t64 ibverbs-providers liburing2 libunwind8 \
    libncurses6 libssl3t64 libnuma1 libxxhash0 libcurl4t64 libnl-3-200 libnl-route-3-200 libfabric1 libaio1t64 openssl ethtool iproute2 && \
    if [ "${BUILD_TYPE}" = "Debug" ]; then \
    apt-get -y --no-install-recommends install libasan8 gdb ; \
    fi && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local/bin/flowbench /usr/local/bin/flowbench
COPY --from=build /usr/local/lib/ /usr/local/lib/
COPY --from=build /xlio-bin/ /usr/local/bin/

ENV LD_LIBRARY_PATH=/usr/local/lib

# Just so the dockerfile fails to build if we are missing libs or some such
RUN /usr/local/bin/flowbench -v

ENTRYPOINT ["/usr/local/bin/flowbench"]
