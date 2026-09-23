# SPDX-FileCopyrightText: 2026 Ben Jarvis
#
# SPDX-License-Identifier: Unlicense
#
# Flowbench built against an ubuntu 26.04 toolchain, whose liburing (2.14) is
# new enough for io_uring zero-copy receive. The 24.04 image in Dockerfile
# ships liburing 2.5 and silently compiles ZCRX out.
FROM ubuntu:26.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -y update && apt-get -y --no-install-recommends install \
    gcc cmake ninja-build git pkg-config build-essential \
    flex bison libncurses-dev libnuma-dev libssl-dev uthash-dev \
    libcurl4-openssl-dev uuid-dev libjansson-dev libxxhash-dev liburcu-dev \
    librdmacm-dev liburing-dev libunwind-dev ca-certificates && \
    apt-get clean && rm -rf /var/lib/apt/lists/*
ADD / /flowbench
RUN cmake -S /flowbench -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DDISABLE_TESTS=ON && \
    cmake --build /build --parallel && \
    cmake --install /build

FROM ubuntu:26.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -y update && apt-get -y --no-install-recommends install \
    libuuid1 librdmacm1 libjansson4 liburcu8t64 ibverbs-providers liburing2 \
    libunwind8 libncurses6 libssl3t64 libnuma1 libxxhash0 libcurl4t64 \
    ethtool iproute2 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*
COPY --from=build /usr/local/bin/flowbench /usr/local/bin/flowbench
COPY --from=build /usr/local/lib/ /usr/local/lib/
ENV LD_LIBRARY_PATH=/usr/local/lib
RUN /usr/local/bin/flowbench -v
ENTRYPOINT ["/usr/local/bin/flowbench"]
