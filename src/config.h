// SPDX-FileCopyrightText: 2025 Ben Jarvis
//
// SPDX-License-Identifier: LGPL-2.1-only

#pragma once

#include <stdint.h>
#include <string.h>

enum flowbench_framework_id {
    FLOWBENCH_FRAMEWORK_INVALID   = 0,
    FLOWBENCH_FRAMEWORK_EVPL      = 1,
    FLOWBENCH_FRAMEWORK_EVPL_RPC2 = 2,
};

enum flowbench_role {
    FLOWBENCH_ROLE_INVALID = 0,
    FLOWBENCH_ROLE_SERVER  = 1,
    FLOWBENCH_ROLE_CLIENT  = 2
};

enum flowbench_mode {
    FLOWBENCH_MODE_INVALID = 0,
    FLOWBENCH_MODE_STREAM  = 1,
    FLOWBENCH_MODE_MSG     = 2
};

enum flowbench_protocol {
    FLOWBENCH_PROTO_INVALID           = 0,
    FLOWBENCH_PROTO_TCP               = 1,
    FLOWBENCH_PROTO_UDP               = 2,
    FLOWBENCH_PROTO_RDMACM_RC         = 3,
    FLOWBENCH_PROTO_RDMACM_UD         = 4,
    FLOWBENCH_PROTO_XLIO_TCP          = 5,
    FLOWBENCH_PROTO_IO_URING_TCP      = 6,
    FLOWBENCH_PROTO_TLS               = 7,
    FLOWBENCH_PROTO_LIBFABRIC_TCP     = 8,
    FLOWBENCH_PROTO_LIBFABRIC_VERBS   = 9,
    FLOWBENCH_PROTO_SPDK_TCP          = 10,
    FLOWBENCH_PROTO_SPDK_IO_URING_TCP = 11,

};

enum flowbench_test {
    FLOWBENCH_TEST_INVALID    = 0,
    FLOWBENCH_TEST_PINGPONG   = 1,
    FLOWBENCH_TEST_THROUGHPUT = 2
};

struct flowbench_config {
    enum flowbench_framework_id framework_id;
    enum flowbench_role role;
    enum flowbench_mode mode;
    enum flowbench_protocol protocol;
    enum flowbench_test test;
    int         interactive;
    int         bidirectional;
    int         reverse;
    int         huge_pages;
    const char *local;
    int         local_port;
    const char *peer;
    int         peer_port;
    int         num_threads;
    int         num_flows;
    uint64_t    msg_size;
    uint64_t    max_inflight;
    uint64_t    max_inflight_bytes;
    uint64_t    duration;
    /* io_uring zero-copy receive: NULL interface leaves ZCRX off. */
    const char *zcrx_interface;
    int         zcrx_rxq;
    int         zcrx_rxq_count;
    int         zcrx_buf_len;
    int         send_zc;
    int         send_zc_threshold; /* io_uring send-zc byte threshold; -1 = default */
    /* SPDK reactor CPU mask, e.g. "[8]"; only used by the spdk_* protocols. */
    const char *spdk_cpumask;
    int         poll_mode;   /* 1 = busy poll (default), 0 = event/wait mode */
    int         rdma_recv_depth; /* RDMA recv queue depth (SRQ/RQ); 0 = libevpl default */
    int         rdma_max_sge;    /* RDMA send max_sge (also zeros inline); 0 = libevpl default */
    int         rdma_flush_batch;  /* max RDMA sends posted per poll iteration; 0 = unbounded */
};

static enum flowbench_framework_id
map_framework(const char *name)
{
    if (strcmp(name, "evpl") == 0) {
        return FLOWBENCH_FRAMEWORK_EVPL;
    }

    if (strcmp(name, "evpl_rpc2") == 0) {
        return FLOWBENCH_FRAMEWORK_EVPL_RPC2;
    }

    return FLOWBENCH_FRAMEWORK_INVALID;
} /* map_framework */
static enum flowbench_mode
map_mode(const char *name)
{
    if (strcmp(name, "stream") == 0) {
        return FLOWBENCH_MODE_STREAM;
    }

    if (strcmp(name, "msg") == 0) {
        return FLOWBENCH_MODE_MSG;
    }

    return FLOWBENCH_MODE_INVALID;
} /* map_mode */

static enum flowbench_role
map_role(const char *name)
{
    if (strcmp(name, "server") == 0) {
        return FLOWBENCH_ROLE_SERVER;
    }

    if (strcmp(name, "client") == 0) {
        return FLOWBENCH_ROLE_CLIENT;
    }

    return FLOWBENCH_ROLE_INVALID;
} /* map_role */

static enum flowbench_protocol
map_protocol(const char *name)
{
    if (strcmp(name, "tcp") == 0) {
        return FLOWBENCH_PROTO_TCP;
    }

    if (strcmp(name, "udp") == 0) {
        return FLOWBENCH_PROTO_UDP;
    }

    if (strcmp(name, "rdmacm_ud") == 0) {
        return FLOWBENCH_PROTO_RDMACM_UD;
    }

    if (strcmp(name, "rdmacm_rc") == 0) {
        return FLOWBENCH_PROTO_RDMACM_RC;
    }

    if (strcmp(name, "xlio_tcp") == 0) {
        return FLOWBENCH_PROTO_XLIO_TCP;
    }

    if (strcmp(name, "io_uring_tcp") == 0) {
        return FLOWBENCH_PROTO_IO_URING_TCP;
    }

    if (strcmp(name, "tls") == 0) {
        return FLOWBENCH_PROTO_TLS;
    }

    if (strcmp(name, "libfabric_tcp") == 0) {
        return FLOWBENCH_PROTO_LIBFABRIC_TCP;
    }

    if (strcmp(name, "libfabric_verbs") == 0) {
        return FLOWBENCH_PROTO_LIBFABRIC_VERBS;
    }

    if (strcmp(name, "spdk_tcp") == 0) {
        return FLOWBENCH_PROTO_SPDK_TCP;
    }

    if (strcmp(name, "spdk_io_uring_tcp") == 0) {
        return FLOWBENCH_PROTO_SPDK_IO_URING_TCP;
    }

    return FLOWBENCH_PROTO_INVALID;

} /* map_protocol */

static enum flowbench_test
map_test(const char *name)
{
    if (strcmp(name, "pingpong") == 0) {
        return FLOWBENCH_TEST_PINGPONG;
    }

    if (strcmp(name, "throughput") == 0) {
        return FLOWBENCH_TEST_THROUGHPUT;
    }

    return FLOWBENCH_TEST_INVALID;
} /* map_test */
