// SPDX-FileCopyrightText: 2025 Ben Jarvis
//
// SPDX-License-Identifier: LGPL-2.1-only

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#include "evpl/evpl.h"
#include "utlist.h"

#include "framework.h"
#include "config.h"
#include "common.h"
#include "stats.h"

#include "flowbench_xdr.h"

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif /* unlikely */

struct flowbench_evpl_flow {
    struct evpl_rpc2_conn       *conn;
    struct flowbench_flow        stats;
    struct evpl_deferral         dispatch;
    struct flowbench_evpl_state *state;

    struct flowbench_evpl_flow  *prev;
    struct flowbench_evpl_flow  *next;

    uint64_t                     inflight_msgs;
    uint64_t                     inflight_bytes;
    uint64_t                     inflight_pings;
    struct timespec             *ping_times;
    int                         *ping_slots;
    int                          ping_head;
    int                          ping_tail;
    int                          ping_ring_mask;
    int                          connected;
};

struct flowbench_evpl_state {
    struct flowbench_evpl_shared *shared;
    struct flowbench_evpl_flow   *flows;
    struct evpl_rpc2_thread      *rpc2_thread;
    struct flowbench_stats       *stats;
    struct evpl_thread           *thread;
    struct evpl_listener_binding *listen_binding;
    struct evpl                  *evpl;
    struct evpl_iovec             iovec;
};

struct flowbench_evpl_shared {
    struct flowbench_config     *config;
    struct FLOWBENCH_V1          flowbench_program;
    struct evpl_rpc2_server     *server;
    int                          client;
    int                          stopped;
    enum evpl_protocol_id protocol;
    struct flowbench_evpl_state *states;
    struct evpl_endpoint        *local;
    struct evpl_endpoint        *remote;
};

void
rpc2_close_flow(struct flowbench_evpl_flow *flow)
{
    if (flow->ping_times) {
        free(flow->ping_times);
    }
    if (flow->ping_slots) {
        free(flow->ping_slots);
    }
    free(flow);
} /* close_flow */

static inline int
rpc2_can_send(struct flowbench_evpl_flow *flow)
{
    struct flowbench_evpl_state   *state  = flow->state;
    struct flowbench_evpl_shared  *shared = state->shared;
    const struct flowbench_config *config = shared->config;

    switch (config->test) {
        case FLOWBENCH_TEST_THROUGHPUT:
            if ((config->role == FLOWBENCH_ROLE_CLIENT && !config->reverse) ||
                (config->role == FLOWBENCH_ROLE_SERVER && config->reverse) ||
                config->bidirectional) {
                return (flow->inflight_msgs < config->max_inflight);
            } else {
                return 0;
            }
        case FLOWBENCH_TEST_PINGPONG:
            if (config->role == FLOWBENCH_ROLE_SERVER) {
                return 0;
            } else {
                return (flow->connected && flow->inflight_pings < config->max_inflight);
            }
        default:
            return 0;
    } /* switch */
} /* can_send */

static void
rpc2_recv_call_pingpong_callback(
    struct evpl           *evpl,
    struct evpl_rpc2_conn *conn,
    struct Ping           *ping,
    struct evpl_rpc2_msg  *msg,
    void                  *private_data)
{
    struct flowbench_evpl_flow   *flow   = private_data;
    struct flowbench_evpl_state  *state  = flow->state;
    struct flowbench_evpl_shared *shared = state->shared;
    struct Pong                   pong;
    int                           rc;

    pong.data.iov    = &state->iovec;
    pong.data.niov   = 1;
    pong.data.length = state->iovec.length;
    evpl_iovec_addref(&state->iovec);

    rc = shared->flowbench_program.send_reply_pingpong(state->evpl,
                                                       &pong,
                                                       msg);

    if (unlikely(rc)) {
        fprintf(stderr, "Failed to send reply for pingpong: %d\n", rc);
        exit(1);
    }

} /* rpc2_recv_call_pingpong_callback */

static void
rpc2_recv_call_datagram_callback(
    struct evpl           *evpl,
    struct evpl_rpc2_conn *conn,
    struct Datagram       *request,
    struct evpl_rpc2_msg  *msg,
    void                  *private_data)
{
    struct flowbench_evpl_flow   *flow   = private_data;
    struct flowbench_evpl_state  *state  = flow->state;
    struct flowbench_evpl_shared *shared = state->shared;
    int                           rc;
    struct timespec               now;

    evpl_get_hf_monotonic_time(evpl, &now);

    flowbench_flow_add_recv_bytes(&flow->stats, &now, request->data.length);
    flowbench_flow_add_recv_msgs(&flow->stats, &now, 1);

    rc = shared->flowbench_program.send_reply_datagram(state->evpl,
                                                       msg);

    if (unlikely(rc)) {
        fprintf(stderr, "Failed to send reply for datagram: %d\n", rc);
        exit(1);
    }

} /* rpc2_recv_call_datagram_callback */

static void
rpc2_recv_reply_pingpong_callback(
    struct evpl *evpl,
    struct Pong *reply,
    int          status,
    void        *private_data)
{
    struct flowbench_evpl_flow  *flow  = private_data;
    struct flowbench_evpl_state *state = flow->state;
    struct timespec              now;

    evpl_get_hf_monotonic_time(evpl, &now);

    if (unlikely(status)) {
        fprintf(stderr, "Received error procedure reply for pingpong: %d\n", status);
        return;
    }

    flowbench_flow_add_sent_bytes(&flow->stats, &now, reply->data.length);
    flowbench_flow_add_sent_msgs(&flow->stats, &now, 1);
    flowbench_flow_add_recv_bytes(&flow->stats, &now, reply->data.length);
    flowbench_flow_add_recv_msgs(&flow->stats, &now, 1);
    flow->inflight_bytes -= reply->data.length;
    flow->inflight_msgs  -= 1;
    flow->inflight_pings--;

    if (flow->ping_slots[flow->ping_tail]) {
        flowbench_flow_add_latency(&flow->stats,
                                   ts_interval(&now, &flow->ping_times[flow->ping_tail]));
        flow->ping_slots[flow->ping_tail] = 0;
        flow->ping_tail                   = (flow->ping_tail + 1) & flow->ping_ring_mask;
    }

    evpl_defer(state->evpl, &flow->dispatch);


} /* rpc2_recv_callback */

static void
rpc2_recv_reply_datagram_callback(
    struct evpl *evpl,
    int          status,
    void        *private_data)
{
    struct flowbench_evpl_flow    *flow   = private_data;
    struct flowbench_evpl_state   *state  = flow->state;
    struct flowbench_evpl_shared  *shared = state->shared;
    const struct flowbench_config *config = shared->config;
    struct timespec                now;

    evpl_get_hf_monotonic_time(evpl, &now);

    if (unlikely(status)) {
        fprintf(stderr, "Received error procedure reply for datagram: %d\n", status);
        return;
    }

    flowbench_flow_add_sent_bytes(&flow->stats, &now, config->msg_size);
    flowbench_flow_add_sent_msgs(&flow->stats, &now, 1);
    flow->inflight_bytes -= config->msg_size;
    flow->inflight_msgs  -= 1;
    evpl_defer(evpl, &flow->dispatch);
} /* rpc2_recv_reply_datagram_callback */

static void
rpc2_flow_dispatch_callback(
    struct evpl *evpl,
    void        *private_data)
{
    struct flowbench_evpl_flow    *flow   = private_data;
    struct flowbench_evpl_state   *state  = flow->state;
    struct flowbench_evpl_shared  *shared = state->shared;
    const struct flowbench_config *config = shared->config;
    struct Ping                    ping;
    struct Datagram                datagram;
    int                            ddp             = 0;
    int                            max_write_chunk = 0;

    if (config->msg_size > 4096) {
        ddp             = 1;
        max_write_chunk = config->msg_size;
    }

    if (unlikely(!flow->conn)) {
        return;
    }

    while (rpc2_can_send(flow)) {
        if (config->test == FLOWBENCH_TEST_PINGPONG) {
            evpl_get_hf_monotonic_time(evpl, &flow->ping_times[flow->ping_head]);
            flow->ping_slots[flow->ping_head] = 1;
            flow->ping_head                   = (flow->ping_head + 1) & flow->ping_ring_mask;
            flow->inflight_pings++;

            ping.data.iov    = &state->iovec;
            ping.data.niov   = 1;
            ping.data.length = config->msg_size;
            evpl_iovec_addref(&state->iovec);

            shared->flowbench_program.send_call_pingpong(&shared->flowbench_program.rpc2,
                                                         evpl,
                                                         flow->conn,
                                                         &ping,
                                                         ddp,
                                                         max_write_chunk,
                                                         0,
                                                         rpc2_recv_reply_pingpong_callback,
                                                         flow);

        } else {

            datagram.data.iov    = &state->iovec;
            datagram.data.niov   = 1;
            datagram.data.length = config->msg_size;

            evpl_iovec_addref(&state->iovec);

            shared->flowbench_program.send_call_datagram(&shared->flowbench_program.rpc2,
                                                         evpl,
                                                         flow->conn,
                                                         &datagram,
                                                         1,
                                                         0,
                                                         0,
                                                         rpc2_recv_reply_datagram_callback,
                                                         flow);

        }
        flow->inflight_bytes += config->msg_size;
        flow->inflight_msgs++;
    }
} /* flow_dispatch_callback */


static struct flowbench_evpl_flow *
rpc2_create_flow(
    struct flowbench_evpl_state *state,
    const char                  *srcaddr,
    int                          srcport,
    const char                  *dstaddr,
    int                          dstport)
{
    struct flowbench_evpl_flow   *flow;
    struct flowbench_evpl_shared *shared = state->shared;
    struct flowbench_config      *config = shared->config;
    int                           ping_ring_size;

    flow = calloc(1, sizeof(*flow));

    snprintf(flow->stats.src, sizeof(flow->stats.src), "%s:%d", srcaddr, srcport);
    snprintf(flow->stats.dst, sizeof(flow->stats.dst), "%s:%d", dstaddr, dstport);

    flow->state = state;

    DL_APPEND(state->flows, flow);

    if (config->test == FLOWBENCH_TEST_PINGPONG) {
        ping_ring_size = 8;

        while (ping_ring_size < config->max_inflight) {
            ping_ring_size <<= 1;
        }

        flow->ping_times     = calloc(ping_ring_size, sizeof(struct timespec));
        flow->ping_slots     = calloc(ping_ring_size, sizeof(int));
        flow->ping_ring_mask = ping_ring_size - 1;
        flow->ping_head      = 0;
        flow->ping_tail      = 0;
    }

    flowbench_add_flow(state->stats, &flow->stats);

    return flow;

} /* create_flow */

static void
rpc2_connect_flow(
    struct flowbench_evpl_state *state,
    struct flowbench_evpl_flow  *flow,
    struct evpl_rpc2_conn       *conn)
{

    flow->conn = conn;

    evpl_deferral_init(&flow->dispatch, rpc2_flow_dispatch_callback, flow);

    evpl_defer(state->evpl, &flow->dispatch);
} /* rpc2_connect_flow */

static void
rpc2_notify_callback(
    struct evpl_rpc2_thread *thread,
    struct evpl_rpc2_conn   *conn,
    struct evpl_rpc2_notify *notify,
    void                    *private_data)
{
    struct flowbench_evpl_state  *state  = private_data;
    struct flowbench_evpl_shared *shared = state->shared;
    struct flowbench_config      *config = shared->config;
    struct flowbench_evpl_flow   *flow;

    switch (notify->notify_type) {
        case EVPL_RPC2_NOTIFY_ACCEPTED:
            flow = rpc2_create_flow(state, config->local, config->
                                    local_port, config->peer, config->
                                    peer_port);

            conn->server_private_data = flow;

            rpc2_connect_flow(state, flow, conn);

            break;
        case EVPL_RPC2_NOTIFY_CONNECTED:
            flow            = conn->server_private_data;
            flow->connected = 1;
            evpl_defer(state->evpl, &flow->dispatch);
            break;
        case EVPL_RPC2_NOTIFY_DISCONNECTED:
            flow = conn->server_private_data;
            flowbench_remove_flow(state->stats, &flow->stats);

            DL_DELETE(state->flows, flow);
            free(flow->ping_times);
            free(flow->ping_slots);
            free(flow);
            break;
        default:
            fprintf(stderr, "Unhandled evpl rpc2 notification type %u\n", notify->notify_type);
    } /* switch */
} /* rpc2_notify_callback */

static void *
flowbench_evpl_rpc2_thread_init(
    struct evpl *evpl,
    void        *private_data)
{
    struct flowbench_evpl_state   *state  = private_data;
    struct flowbench_evpl_shared  *shared = state->shared;
    const struct flowbench_config *config = shared->config;
    struct evpl_rpc2_conn         *conn;
    struct flowbench_evpl_flow    *flow;
    struct evpl_rpc2_program      *programs[1];

    state->evpl = evpl;

    evpl_iovec_alloc(state->evpl, config->msg_size, 4096, 1, &state->iovec);

    programs[0] = &shared->flowbench_program.rpc2;

    state->rpc2_thread = evpl_rpc2_thread_init(evpl, programs, 1, rpc2_notify_callback, state);

    switch (config->role) {
        case FLOWBENCH_ROLE_SERVER:

            evpl_rpc2_server_attach(state->rpc2_thread, shared->server, state);

            break;
        case FLOWBENCH_ROLE_CLIENT:


            flow = rpc2_create_flow(state, config->local, config->
                                    local_port, config->peer, config->
                                    peer_port);

            conn = evpl_rpc2_client_connect(state->rpc2_thread,
                                            shared->protocol,
                                            state->shared->remote,
                                            programs, 1, flow);

            rpc2_connect_flow(state, flow, conn);

            break;
        default:
            abort();
    } /* switch */

    return state;
} /* flowbench_evpl_thread_init */

static void
flowbench_evpl_rpc2_thread_destroy(
    struct evpl *evpl,
    void        *private_data)
{
    struct flowbench_evpl_state  *state  = private_data;
    struct flowbench_evpl_shared *shared = state->shared;
    struct flowbench_config      *config = shared->config;


    if (config->role == FLOWBENCH_ROLE_SERVER) {
        evpl_rpc2_server_detach(state->rpc2_thread, shared->server);
    }

    evpl_rpc2_thread_destroy(state->rpc2_thread);

    evpl_iovec_release(&state->iovec);


} /* flowbench_evpl_thread_destroy */

void *
flowbench_evpl_rpc2_init(
    struct flowbench_config *config,
    struct flowbench_stats  *stats)
{
    struct flowbench_evpl_shared *shared;
    struct flowbench_evpl_state  *state;
    struct evpl_rpc2_program     *programs[1];
    int                           i;
    struct evpl_global_config    *evpl_config;

    shared = calloc(1, sizeof(*shared));

    evpl_config = evpl_global_config_init();

    if (config->huge_pages) {
        evpl_global_config_set_huge_pages(evpl_config, 1);
    }

    evpl_global_config_set_rdmacm_tos(evpl_config, 104);
    evpl_global_config_set_rdmacm_srq_prefill(evpl_config, 1);
    evpl_global_config_set_max_datagram_size(evpl_config, 4096 + 256);
    evpl_global_config_set_tls_verify_peer(evpl_config, 0);

    evpl_init(evpl_config);

    shared->config = config;
    shared->states = calloc(config->num_threads, sizeof(*state));

    for (i = 0; i < config->num_threads; ++i) {

        state = &shared->states[i];

        state->stats  = stats;
        state->shared = shared;
    }

    FLOWBENCH_V1_init(&shared->flowbench_program);

    shared->flowbench_program.recv_call_pingpong = rpc2_recv_call_pingpong_callback;
    shared->flowbench_program.recv_call_datagram = rpc2_recv_call_datagram_callback;


    if (config->role == FLOWBENCH_ROLE_SERVER) {
        programs[0]    = &shared->flowbench_program.rpc2;
        shared->server = evpl_rpc2_server_init(programs, 1);
    }

    return shared;
} /* flowbench_evpl_init */

void
flowbench_evpl_rpc2_cleanup(void *private_data)
{
    struct flowbench_evpl_shared *shared = private_data;
    struct flowbench_config      *config = shared->config;

    if (config->role == FLOWBENCH_ROLE_SERVER) {
        evpl_rpc2_server_destroy(shared->server);
    }

    free(shared->states);
    free(shared);

} /* flowbench_evpl_cleanup */

void
flowbench_evpl_rpc2_start(void *private_data)
{
    struct flowbench_evpl_shared  *shared = private_data;
    const struct flowbench_config *config = shared->config;
    struct evpl_thread_config     *thread_config;
    int                            i;
    struct flowbench_evpl_state   *state;

    switch (config->mode) {
        case FLOWBENCH_MODE_STREAM:
            fprintf(stderr, "RPC2 does not support stream mode.\n");
            exit(1);
            break;
        case FLOWBENCH_MODE_MSG:
            switch (config->protocol) {
                case FLOWBENCH_PROTO_TCP:
                    shared->protocol = EVPL_STREAM_SOCKET_TCP;
                    break;
                case FLOWBENCH_PROTO_TLS:
                    shared->protocol = EVPL_STREAM_SOCKET_TLS;
                    break;
                case FLOWBENCH_PROTO_UDP:
                    fprintf(stderr, "UDP is not supported for RPC2.\n");
                    exit(1);
                    break;
                case FLOWBENCH_PROTO_RDMACM_RC:
                    shared->protocol = EVPL_DATAGRAM_RDMACM_RC;
                    break;
                case FLOWBENCH_PROTO_RDMACM_UD:
                    fprintf(stderr, "RDMA-UD is not supported for RPC2.\n");
                    exit(1);
                    break;
                case FLOWBENCH_PROTO_XLIO_TCP:
                    shared->protocol = EVPL_STREAM_XLIO_TCP;
                    break;
                case FLOWBENCH_PROTO_IO_URING_TCP:
                    shared->protocol = EVPL_STREAM_IO_URING_TCP;
                    break;
                default:
                    fprintf(stderr, "Unsupported protocol %d\n", config->protocol);
                    exit(1);
            } /* switch */
            break;
        default:
            fprintf(stderr, "Unsupported mode %d\n", config->mode);
            exit(1);
    } /* switch */

    shared->client = (config->role == FLOWBENCH_ROLE_CLIENT);

    shared->local  = evpl_endpoint_create(config->local, config->local_port);
    shared->remote = evpl_endpoint_create(config->peer, config->peer_port);

    for (i = 0; i < config->num_threads; ++i) {

        state = &shared->states[i];

        thread_config = evpl_thread_config_init();

        evpl_thread_config_set_poll_mode(thread_config, 1);

        state->thread = evpl_thread_create(thread_config,
                                           flowbench_evpl_rpc2_thread_init,
                                           flowbench_evpl_rpc2_thread_destroy,
                                           state);

    }

    if (config->role == FLOWBENCH_ROLE_SERVER) {
        evpl_rpc2_server_start(shared->server, shared->protocol, shared->local);
    }
} /* flowbench_evpl_start */

void
flowbench_evpl_rpc2_stop(void *private_data)
{
    struct flowbench_evpl_shared *shared = private_data;
    struct flowbench_config      *config = shared->config;
    int                           i;

    for (i = 0; i < config->num_threads; ++i) {
        evpl_thread_destroy(shared->states[i].thread);
    }
} /* flowbench_evpl_stop */

struct flowbench_framework framework_evpl_rpc2 = {
    .init    = flowbench_evpl_rpc2_init,
    .cleanup = flowbench_evpl_rpc2_cleanup,
    .start   = flowbench_evpl_rpc2_start,
    .stop    = flowbench_evpl_rpc2_stop,
};
