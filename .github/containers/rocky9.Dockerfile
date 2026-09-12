# SPDX-FileCopyrightText: 2026 Ben Jarvis
#
# SPDX-License-Identifier: Unlicense

FROM rockylinux/rockylinux:9

RUN dnf install -y epel-release dnf-plugins-core && \
    dnf config-manager --set-enabled crb

# Keep the distro compiler and backend libraries aligned with libevpl CI.
RUN dnf -y update && \
    dnf -y --allowerasing install \
        gcc gcc-c++ libasan cmake ninja-build git flex bison pkgconfig \
        ca-certificates curl iproute python3 \
        openssl-devel openssl ncurses-devel uthash-devel libuuid-devel \
        liburing-devel libaio-devel rdma-core-devel numactl-devel libunwind-devel \
        libnghttp2-devel libcurl-devel protobuf-c-devel protobuf-c-compiler && \
    dnf clean all
