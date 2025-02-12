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

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif /* unlikely */

struct flowbench_evpl_flow {
    struct evpl_bind              *bind;
    struct flowbench_flow          stats;
    struct evpl_deferral           dispatch;
    const struct flowbench_config *config;
    struct flowbench_evpl_state   *state;

    uint64_t                       inflight_msgs;
    uint64_t                       inflight_bytes;
    uint64_t                       inflight_pings;
    struct timespec               *ping_times;
    int                           *ping_slots;
    int                            ping_head;
    int                            ping_tail;
    int                            ping_ring_mask;
    int                            connected;
    struct evpl_iovec              iovec;

    struct flowbench_evpl_flow    *prev;
    struct flowbench_evpl_flow    *next;

};

struct flowbench_evpl_state {
    const struct flowbench_config *config;
    struct flowbench_stats        *stats;
    struct evpl_thread            *thread;
    struct evpl                   *evpl;
    struct evpl_endpoint          *local;
    struct evpl_endpoint          *remote;
    struct flowbench_evpl_flow    *flows;
    enum evpl_protocol_id protocol;
    int                            error;
    int                            stream;
    int                            connected;
    int                            client;
    int                            stopped;
};

void
close_flow(struct flowbench_evpl_flow *flow)
{
    evpl_iovec_release(&flow->iovec);
    if (flow->ping_times) {
        free(flow->ping_times);
    }
    if (flow->ping_slots) {
        free(flow->ping_slots);
    }
    free(flow);
} /* close_flow */

static int
segment_callback(
    struct evpl      *evpl,
    struct evpl_bind *bind,
    void             *private_data)
{
    struct flowbench_evpl_flow *flow = private_data;

    return flow->config->msg_size;
} /* segment_callback */

static inline int
can_send(struct flowbench_evpl_flow *flow)
{
    struct flowbench_evpl_state   *state  = flow->state;
    const struct flowbench_config *config = flow->config;

    switch (config->test) {
        case FLOWBENCH_TEST_THROUGHPUT:
            if ((config->role == FLOWBENCH_ROLE_CLIENT && !config->reverse) ||
                (config->role == FLOWBENCH_ROLE_SERVER && config->reverse) ||
                config->bidirectional) {
                if (state->config->mode == FLOWBENCH_MODE_STREAM) {
                    return (flow->inflight_bytes + config->msg_size <= config->max_inflight);
                } else {
                    return (flow->inflight_msgs < config->max_inflight);
                }
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
flow_dispatch_callback(
    struct evpl *evpl,
    void        *private_data)
{
    struct flowbench_evpl_flow    *flow   = private_data;
    struct flowbench_evpl_state   *state  = flow->state;
    const struct flowbench_config *config = flow->config;

    if (unlikely(flow->bind == NULL)) {
        return;
    }

    while (can_send(flow)) {
        if (config->test == FLOWBENCH_TEST_PINGPONG) {
            clock_gettime(CLOCK_MONOTONIC, &flow->ping_times[flow->ping_head]);
            flow->ping_slots[flow->ping_head] = 1;
            flow->ping_head                   = (flow->ping_head + 1) & flow->ping_ring_mask;
            flow->inflight_pings++;
        }

        evpl_iovec_addref(&flow->iovec);

        if (flow->state->connected) {
            evpl_sendv(evpl, flow->bind, &flow->iovec, 1, config->msg_size);
        } else {
            evpl_sendtoepv(evpl, flow->bind, state->remote, &flow->iovec, 1, config->msg_size);
        }

        flow->inflight_bytes += config->msg_size;
        flow->inflight_msgs++;
    }
} /* flow_dispatch_callback */

static void
notify_callback(
    struct evpl        *evpl,
    struct evpl_bind   *bind,
    struct evpl_notify *notify,
    void               *private_data)
{
    struct flowbench_evpl_flow    *flow   = private_data;
    struct flowbench_evpl_state   *state  = flow->state;
    const struct flowbench_config *config = flow->config;
    struct evpl_iovec              iovecs[8];
    struct timespec                now;
    int                            niovecs, i;

    clock_gettime(CLOCK_MONOTONIC, &now);

    switch (notify->notify_type) {
        case EVPL_NOTIFY_CONNECTED:
            flow->connected = 1;
            evpl_defer(state->evpl, &flow->dispatch);
            break;
        case EVPL_NOTIFY_DISCONNECTED:
            flowbench_remove_flow(state->stats, &flow->stats);
            DL_DELETE(state->flows, flow);
            close_flow(flow);
            break;
        case EVPL_NOTIFY_SENT:
            flowbench_flow_add_sent_bytes(&flow->stats, &now, notify->sent.bytes);
            flowbench_flow_add_sent_msgs(&flow->stats, &now, notify->sent.msgs);
            flow->inflight_bytes -= notify->sent.bytes;
            flow->inflight_msgs  -= notify->sent.msgs;
            evpl_defer(state->evpl, &flow->dispatch);
            break;
        case EVPL_NOTIFY_RECV_DATA:
            do {

                niovecs = evpl_readv(evpl, flow->bind, iovecs, 8, 4 * 1024 *
                                     1024);

                for (i = 0; i < niovecs; ++i) {
                    flowbench_flow_add_recv_bytes(&flow->stats, &now, iovecs[i].length);
                    evpl_iovec_release(&iovecs[i]);
                }

            } while (niovecs);
            break;
        case EVPL_NOTIFY_RECV_MSG:
            flowbench_flow_add_recv_bytes(&flow->stats, &now, notify->recv_msg.length);
            flowbench_flow_add_recv_msgs(&flow->stats, &now, 1);

            if (config->test == FLOWBENCH_TEST_PINGPONG) {
                if (config->role == FLOWBENCH_ROLE_CLIENT) {
                    if (flow->ping_slots[flow->ping_tail]) {
                        flowbench_flow_add_latency(&flow->stats,
                                                   ts_interval(&now, &flow->ping_times[flow->ping_tail]));
                        flow->ping_slots[flow->ping_tail] = 0;
                        flow->ping_tail                   = (flow->ping_tail + 1) & flow->ping_ring_mask;
                        flow->inflight_pings--;
                    }
                    evpl_defer(state->evpl, &flow->dispatch);
                } else {
                    evpl_iovec_addref(&flow->iovec);
                    if (flow->state->connected) {
                        evpl_sendv(evpl, flow->bind, &flow->iovec, 1, notify->recv_msg.length);
                    } else {
                        evpl_sendtov(evpl, flow->bind, notify->recv_msg.addr,
                                     &flow->iovec, 1, notify->recv_msg.length);
                    }
                }
            }

            break;
        default:
            fprintf(stderr, "Unhandled evpl notification type %u", notify->
                    notify_type);
    } /* switch */
} /* notify_callback */

static struct flowbench_evpl_flow *
create_flow(
    struct flowbench_evpl_state *state,
    const char                  *srcaddr,
    int                          srcport,
    const char                  *dstaddr,
    int                          dstport)
{
    struct flowbench_evpl_flow *flow;
    int                         ping_ring_size;

    flow = calloc(1, sizeof(*flow));

    snprintf(flow->stats.src, sizeof(flow->stats.src), "%s:%d", srcaddr, srcport);
    snprintf(flow->stats.dst, sizeof(flow->stats.dst), "%s:%d", dstaddr, dstport);

    flow->state  = state;
    flow->config = state->config;

    if (state->config->test == FLOWBENCH_TEST_PINGPONG) {
        ping_ring_size = 8;

        while (ping_ring_size < state->config->max_inflight) {
            ping_ring_size <<= 1;
        }

        flow->ping_times     = calloc(ping_ring_size, sizeof(struct timespec));
        flow->ping_slots     = calloc(ping_ring_size, sizeof(int));
        flow->ping_ring_mask = ping_ring_size - 1;
        flow->ping_head      = 0;
        flow->ping_tail      = 0;
    }

    evpl_iovec_alloc(state->evpl, state->config->msg_size, 4096, 1, &flow->iovec);

    flowbench_add_flow(state->stats, &flow->stats);

    evpl_deferral_init(&flow->dispatch, flow_dispatch_callback, flow);

    evpl_defer(state->evpl, &flow->dispatch);

    return flow;
} /* create_flow */

static void
accept_callback(
    struct evpl             *evpl,
    struct evpl_bind        *listen_bind,
    struct evpl_bind        *accepted_bind,
    evpl_notify_callback_t  *a_notify_callback,
    evpl_segment_callback_t *a_segment_callback,
    void                   **conn_private_data,
    void                    *private_data)
{
    struct flowbench_evpl_state   *state  = private_data;
    const struct flowbench_config *config = state->config;
    struct flowbench_evpl_flow    *flow;

    flow = create_flow(state, config->local, config->local_port, config->
                       peer, config->peer_port);
    flow->bind = accepted_bind;

    flowbench_add_flow(state->stats, &flow->stats);

    evpl_bind_request_send_notifications(evpl, accepted_bind);

    *a_notify_callback = notify_callback;

    if (state->stream && state->config->mode == FLOWBENCH_MODE_MSG) {
        *a_segment_callback = segment_callback;
    }

    *conn_private_data = flow;

    DL_APPEND(state->flows, flow);
} /* accept_callback */

static void *
flowbench_evpl_thread_init(
    struct evpl *evpl,
    void        *private_data)
{
    struct flowbench_evpl_state   *state  = private_data;
    const struct flowbench_config *config = state->config;
    evpl_segment_callback_t        a_segment_callback;
    struct evpl_bind              *bind;
    struct flowbench_evpl_flow    *flow;
    int                            i;

    state->evpl = evpl;

    switch (config->mode) {
        case FLOWBENCH_MODE_STREAM:
            switch (config->protocol) {
                case FLOWBENCH_PROTO_TCP:
                    state->protocol  = EVPL_STREAM_SOCKET_TCP;
                    state->stream    = 1;
                    state->connected = 1;
                    break;
                case FLOWBENCH_PROTO_RDMACM_RC:
                    state->protocol  = EVPL_STREAM_RDMACM_RC;
                    state->stream    = 1;
                    state->connected = 1;
                case FLOWBENCH_PROTO_XLIO_TCP:
                    state->protocol  = EVPL_STREAM_XLIO_TCP;
                    state->stream    = 1;
                    state->connected = 1;
                    break;
                default:
                    state->error = 1;
                    return state;
            } /* switch */
            break;

        case FLOWBENCH_MODE_MSG:
            switch (config->protocol) {
                case FLOWBENCH_PROTO_TCP:
                    state->protocol  = EVPL_STREAM_SOCKET_TCP;
                    state->stream    = 1;
                    state->connected = 1;
                    break;
                case FLOWBENCH_PROTO_UDP:
                    state->protocol  = EVPL_DATAGRAM_SOCKET_UDP;
                    state->stream    = 0;
                    state->connected = 0;
                    break;
                case FLOWBENCH_PROTO_RDMACM_RC:
                    state->protocol  = EVPL_DATAGRAM_RDMACM_RC;
                    state->stream    = 0;
                    state->connected = 1;
                    break;
                case FLOWBENCH_PROTO_RDMACM_UD:
                    state->protocol  = EVPL_DATAGRAM_RDMACM_UD;
                    state->stream    = 0;
                    state->connected = 0;
                    break;
                case FLOWBENCH_PROTO_XLIO_TCP:
                    state->protocol  = EVPL_STREAM_XLIO_TCP;
                    state->stream    = 1;
                    state->connected = 1;
                    break;
                default:
                    state->error = 1;
                    return state;
            } /* switch */
            break;
        default:
            state->error = 1;
            return state;
    } /* switch */

    state->client = (config->role == FLOWBENCH_ROLE_CLIENT);

    state->local = evpl_endpoint_create(state->evpl, config->local, config->
                                        local_port);
    state->remote = evpl_endpoint_create(state->evpl, config->peer, config->
                                         peer_port);

    switch (config->role) {
        case FLOWBENCH_ROLE_SERVER:
            if (state->connected) {
                evpl_listen(state->evpl, state->protocol, state->local,
                            accept_callback, state);
            } else {
                flow = create_flow(state, config->local, config->
                                   local_port, config->peer, config->peer_port);
                bind = evpl_bind(state->evpl, state->protocol, state->
                                 local, notify_callback, flow);
                flow->bind = bind;
                evpl_bind_request_send_notifications(state->evpl, bind);

                DL_APPEND(state->flows, flow);
            }
            break;
        case FLOWBENCH_ROLE_CLIENT:
            if (state->connected) {

                if (state->stream && state->config->mode == FLOWBENCH_MODE_MSG) {
                    a_segment_callback = segment_callback;
                } else {
                    a_segment_callback = NULL;
                }

                for (i = 0; i < config->num_flows; ++i) {
                    flow = create_flow(state, config->local, config->
                                       local_port, config->peer, config->
                                       peer_port);
                    bind = evpl_connect(state->evpl, state->protocol,
                                        NULL, state->remote,
                                        notify_callback, a_segment_callback,
                                        flow);
                    flow->bind = bind;
                    evpl_bind_request_send_notifications(state->evpl, bind);
                    DL_APPEND(state->flows, flow);
                }
            } else {
                flow = create_flow(state, config->local, config->
                                   local_port, config->peer, config->peer_port);
                bind = evpl_bind(state->evpl, state->protocol, state->
                                 local, notify_callback, flow);
                flow->bind = bind;
                evpl_bind_request_send_notifications(state->evpl, bind);
                DL_APPEND(state->flows, flow);
            }

            break;
        default:
            abort();
    } /* switch */

    return state;
} /* flowbench_evpl_thread_init */

void *
flowbench_evpl_init(
    struct flowbench_config *config,
    struct flowbench_stats  *stats)
{
    struct flowbench_evpl_state *states, *state;
    int                          i;
    struct evpl_global_config   *evpl_config;

    evpl_config = evpl_global_config_init();

    if (config->huge_pages) {
        evpl_global_config_set_huge_pages(evpl_config, 1);
    }

    evpl_global_config_set_rdmacm_tos(evpl_config, 104);
    evpl_global_config_set_rdmacm_srq_prefill(evpl_config, 1);
    evpl_global_config_set_max_datagram_size(evpl_config, config->msg_size);

    evpl_init(evpl_config);

    states = calloc(config->num_threads, sizeof(*state));

    for (i = 0; i < config->num_threads; ++i) {

        state = &states[i];

        state->config = config;
        state->stats  = stats;
    }

    return states;
} /* flowbench_evpl_init */

void
flowbench_evpl_cleanup(void *private_data)
{
    struct flowbench_evpl_state *states = private_data;

    free(states);

} /* flowbench_evpl_cleanup */

void
flowbench_evpl_start(void *private_data)
{
    struct flowbench_evpl_state   *states = private_data, *state;
    const struct flowbench_config *config = states[0].config;
    int                            i;


    for (i = 0; i < config->num_threads; ++i) {

        state = &states[i];

        state->thread = evpl_thread_create(NULL,
                                           flowbench_evpl_thread_init,
                                           NULL,
                                           state);

    }

} /* flowbench_evpl_start */

void
flowbench_evpl_stop(void *private_data)
{
    struct flowbench_evpl_state *states = private_data;
    int                          i;

    for (i = 0; i < states[0].config->num_threads; ++i) {
        evpl_thread_destroy(states[i].thread);
    }
} /* flowbench_evpl_stop */

struct flowbench_framework framework_evpl = {
    .init    = flowbench_evpl_init,
    .cleanup = flowbench_evpl_cleanup,
    .start   = flowbench_evpl_start,
    .stop    = flowbench_evpl_stop,
};
