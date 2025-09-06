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

