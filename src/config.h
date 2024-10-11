#pragma once

#include <stdint.h>
#include <string.h>

enum flowbench_framework {
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
    FLOWBENCH_PROTO_INVALID = 0,
    FLOWBENCH_PROTO_TCP = 1,
    FLOWBENCH_PROTO_UDP = 2,
    FLOWBENCH_PROTO_RDMACM_RC = 3,
    FLOWBENCH_PROTO_RDMACM_UD = 4
};

enum flowbench_test {
    FLOWBENCH_TEST_INVALID    = 0,
    FLOWBENCH_TEST_PINGPONG   = 1,
    FLOWBENCH_TEST_THROUGHPUT = 2
};

struct flowbench_config {
    enum flowbench_framework    framework;
    enum flowbench_role         role;
    enum flowbench_mode         mode;
    enum flowbench_protocol     protocol;
    enum flowbench_test         test;
    const char                 *local;
    int                         local_port;
    const char                 *peer;
    int                         peer_port;
    uint64_t                    msg_size;
    uint64_t                    max_inflight_bytes;
    uint64_t                    max_inflight_msgs;
    uint64_t                    duration;
};

static enum flowbench_framework
map_framework(const char *name)
{
    if (strcmp(name,"evpl") == 0) {
        return FLOWBENCH_FRAMEWORK_EVPL;
    }

    return FLOWBENCH_FRAMEWORK_INVALID;
}
static enum flowbench_mode
map_mode(const char *name)
{
    if (strcmp(name,"stream") == 0) {
        return FLOWBENCH_MODE_STREAM;
    }

    if (strcmp(name,"msg") == 0) {
        return FLOWBENCH_MODE_MSG;
    }

    return FLOWBENCH_MODE_INVALID;
}

static enum flowbench_role
map_role(const char *name)
{
    if (strcmp(name,"server") == 0) {
        return FLOWBENCH_ROLE_SERVER;
    }

    if (strcmp(name,"client") == 0) {
        return FLOWBENCH_ROLE_CLIENT;
    }

    return FLOWBENCH_ROLE_INVALID;
}

static enum flowbench_protocol
map_protocol(const char *name)
{
    if (strcmp(name,"tcp") == 0) {
        return FLOWBENCH_PROTO_TCP;
    }

    if (strcmp(name,"udp") == 0) {
        return FLOWBENCH_PROTO_UDP;
    }

    if (strcmp(name,"rdmacm_ud") == 0) {
        return FLOWBENCH_PROTO_RDMACM_UD;
    }

    if (strcmp(name,"rdmacm_rc") == 0) {
        return FLOWBENCH_PROTO_RDMACM_RC;
    }

    return FLOWBENCH_PROTO_INVALID;

}

static enum flowbench_test
map_test(const char *name)
{
    if (strcmp(name,"pingpong") == 0) {
        return FLOWBENCH_TEST_PINGPONG;
    }

    if (strcmp(name,"throughput") == 0) {
        return FLOWBENCH_TEST_THROUGHPUT;
    }

    return FLOWBENCH_TEST_INVALID;
}
