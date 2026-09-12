<!--
SPDX-FileCopyrightText: 2025 Ben Jarvis

SPDX-License-Identifier: LGPL-2.1-only
-->

# flowbench

## Purpose

flowbench is a network benchmarking tool, similar in function to tools like
iperf, sockperf, mellanox perftest suite, etc.  It has a few differentiated
features:

* It uses libevpl as a backend and therefore can support multiple different
  backend protocols.  For example, TCP/UDP and RDMA can be tested with the
  same tool.
* It supports a distributed model where multiple flows can be orchestrated
  across many machines to simulate larger scale network environments

## Status

flowbench (as well as libevpl on which it depends) are both in early stages
of development and are not usable yet.

## Building on Linux and macOS

Clone with `git clone --recurse-submodules`, or initialize an existing checkout
with `git submodule update --init --recursive`.

On macOS, install the Xcode command-line tools (`xcode-select --install`) and
Homebrew dependencies:

    brew install cmake ninja openssl@3 uthash

On Debian/Ubuntu:

    sudo apt-get install build-essential cmake ninja-build flex bison libncurses-dev libnuma-dev libssl-dev uthash-dev libcurl4-openssl-dev

Build and run Flowbench's tests:

    cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build/release
    ctest --test-dir build/release -R '^flowbench/' --output-on-failure

Use `-DCMAKE_BUILD_TYPE=Debug` for an AddressSanitizer build. The `make`
wrapper also supports `make release` and `make debug` and respects CMake's
compiler selection (including the CC environment variable).

macOS supports the TCP and UDP socket benchmarks through libevpl's kqueue
backend. RDMA and XLIO require their supported platforms and hardware.
Internal duration, latency, and bandwidth-window timing uses the same
header-only stopwatch dependency as libevpl.

CI covers Debug and Release builds on Ubuntu 24.04, Ubuntu 26.04, Rocky Linux
9/10, and macOS. The Ubuntu 26.04 and Rocky jobs use amd64 containers with
distro compilers and dependencies, then run the full Flowbench test suite in
isolated network namespaces. Their JUnit results are uploaded as CI artifacts.
