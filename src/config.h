#pragma once

#include <stdint.h>
#include <string.h>

enum flowbench_framework_id {
    FLOWBENCH_FRAMEWORK_INVALID = 0,
    FLOWBENCH_FRAMEWORK_EVPL    = 1,
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
    FLOWBENCH_PROTO_INVALID      = 0,
    FLOWBENCH_PROTO_TCP          = 1,
    FLOWBENCH_PROTO_UDP          = 2,
    FLOWBENCH_PROTO_RDMACM_RC    = 3,
    FLOWBENCH_PROTO_RDMACM_UD    = 4,
    FLOWBENCH_PROTO_XLIO_TCP     = 5,
    FLOWBENCH_PROTO_IO_URING_TCP = 6,
    FLOWBENCH_PROTO_TLS          = 7,

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
};

static enum flowbench_framework_id
map_framework(const char *name)
{
    if (strcmp(name, "evpl") == 0) {
        return FLOWBENCH_FRAMEWORK_EVPL;
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
