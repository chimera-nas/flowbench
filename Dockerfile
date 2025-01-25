FROM ubuntu:24.04 AS build
ARG BUILD_TYPE=Release

RUN apt-get -y update && \
    apt-get -y --no-install-recommends upgrade && \
    apt-get -y --no-install-recommends install clang cmake ninja-build git lldb gdb less psmisc \
    uuid-dev libjansson-dev libclang-rt-18-dev llvm \
    libxxhash-dev liburcu-dev librdmacm-dev liburing-dev libunwind-dev flex bison libncurses-dev libcurl4-openssl-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

ADD / /flowbench

RUN mkdir /build && \
    cd /build && \
    cmake -G Ninja -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DDISABLE_TESTS=ON /flowbench && \
    ninja && \
    ninja install

FROM ubuntu:24.04

RUN apt-get -y update && \
    apt-get -y --no-install-recommends upgrade && \
    apt-get -y --no-install-recommends install libuuid1 librdmacm1 libjansson4 liburcu8t64 ibverbs-providers liburing2 libunwind8 \
    libncurses6 libssl3t64 && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local/bin/flowbench /usr/local/bin/flowbench
COPY --from=build /usr/local/lib/*.so /usr/local/lib/

# Just so the dockerfile fails to build if we are missing libs or some such
RUN /usr/local/bin/flowbench -v
