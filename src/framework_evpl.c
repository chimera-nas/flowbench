#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#include "core/evpl.h"
#include "thread/thread.h"
#include "utlist.h"

#include "framework.h"
#include "config.h"
#include "common.h"
#include "stats.h"

struct flowbench_evpl_flow {
    struct evpl_bind           *bind;
    struct flowbench_flow       stats;
    struct evpl_uevent *dispatch;
    const struct flowbench_config     *config;
    struct flowbench_evpl_state *state;

    uint64_t    	inflight_msgs;
    uint64_t    	inflight_bytes;
    int         	ping;
    struct timespec	ping_time;

    struct evpl_bvec    bvec;

    struct flowbench_evpl_flow *prev;
    struct flowbench_evpl_flow *next;

};
 
struct flowbench_evpl_state {
    const struct flowbench_config *config;
    struct flowbench_stats *stats;
    struct evpl_thread *thread;
    struct evpl *evpl;
    struct evpl_endpoint *local;
    struct evpl_endpoint *remote;
    struct flowbench_evpl_flow *flows;
    enum evpl_protocol_id protocol;
    int run;
    int running;
    int error;
    int stream;
    int connected;
    int client;
};

void
close_flow(
    struct evpl *evpl,
    struct flowbench_evpl_flow *flow)
{
    evpl_bvec_release(evpl, &flow->bvec);
}

static int
segment_callback(
    struct evpl      *evpl,
    struct evpl_bind *bind,
    void             *private_data)
{
    struct flowbench_evpl_flow *flow = private_data;

    return flow->config->msg_size;
}

static inline int
can_send(struct flowbench_evpl_flow *flow)
{
    struct flowbench_evpl_state *state = flow->state;
    const struct flowbench_config *config = flow->config;

    switch (config->test) {
    case FLOWBENCH_TEST_THROUGHPUT:
        if ((config->role == FLOWBENCH_ROLE_CLIENT && !config->reverse) ||
            (config->role == FLOWBENCH_ROLE_SERVER && config->reverse) ||
            config->bidirectional) {
            if (state->stream) {
                return (flow->inflight_bytes + config->msg_size <= config->max_inflight_bytes);
            } else {
                return (flow->inflight_msgs < config->max_inflight_msgs);
            }
        } else {
            return 0;
        }
    case FLOWBENCH_TEST_PINGPONG:
        if (config->role == FLOWBENCH_ROLE_SERVER) {
            return 0;
        } else {
            return (flow->ping == 0);
        }
        break;
    default:
        return 0;
    }
}

static void
flow_dispatch_callback(
    struct evpl *evpl,
    void *private_data)
{
    struct flowbench_evpl_flow *flow = private_data;
    struct flowbench_evpl_state *state = flow->state;
    const struct flowbench_config *config = flow->config;

    if (flow->bind == NULL) return;

    while (can_send(flow)) {

	if (config->test == FLOWBENCH_TEST_PINGPONG) {
            flow->ping = 1;
	    clock_gettime(CLOCK_MONOTONIC, &flow->ping_time);
        }

        evpl_bvec_addref(evpl, &flow->bvec);

        if (flow->state->connected) {
            evpl_sendv(evpl, flow->bind, &flow->bvec, 1, config->msg_size);
        } else {
            evpl_sendtoepv(evpl, flow->bind, state->remote, &flow->bvec, 1, config->msg_size);
        }

        flow->inflight_bytes += config->msg_size;

        if (config->mode == FLOWBENCH_MODE_MSG) {
            flow->inflight_msgs++;
        }
    }
}

static void 
notify_callback(
    struct evpl              *evpl,
    struct evpl_bind         *bind,
    struct evpl_notify *notify,
    void                     *private_data)
{
    struct flowbench_evpl_flow *flow = private_data;
    struct flowbench_evpl_state *state = flow->state;
    const struct flowbench_config *config = flow->config;
    struct evpl_bvec bvecs[8];
    struct timespec now;
    int nbvecs, i;

    clock_gettime(CLOCK_MONOTONIC, &now);

    switch (notify->notify_type) {
    case EVPL_NOTIFY_CONNECTED:
        evpl_arm_uevent(state->evpl, flow->dispatch);
        break;
    case EVPL_NOTIFY_DISCONNECTED:
        fprintf(stderr,"flow disconnected recv %lu bytes sent %lu bytes\n",
            flow->stats.recv_bytes, flow->stats.sent_bytes);

        flowbench_remove_flow(state->stats, &flow->stats);
        evpl_destroy_uevent(evpl, flow->dispatch);
        DL_DELETE(state->flows, flow);
        free(flow);
        break;
    case EVPL_NOTIFY_SENT:
        flowbench_flow_add_sent_bytes(&flow->stats, &now, notify->sent.bytes);
        flow->stats.sent_msgs  += notify->sent.msgs;
        flow->inflight_bytes -= notify->sent.bytes;
        flow->inflight_msgs -= notify->sent.msgs;
        evpl_arm_uevent(state->evpl, flow->dispatch);
        break;
    case EVPL_NOTIFY_RECV_DATA:
        do {

            nbvecs = evpl_readv(evpl, flow->bind, bvecs, 8, 4*1024*1024);

            for (i = 0; i < nbvecs; ++i) {
                flowbench_flow_add_recv_bytes(&flow->stats, &now, evpl_bvec_length(&bvecs[i]));
                evpl_bvec_release(evpl, &bvecs[i]);
            }

        } while (nbvecs);
        break;
    case EVPL_NOTIFY_RECV_MSG:
        flowbench_flow_add_recv_bytes(&flow->stats, &now, notify->recv_msg.length);
        flow->stats.recv_msgs++;

        if (config->test == FLOWBENCH_TEST_PINGPONG) {

            if (config->role == FLOWBENCH_ROLE_CLIENT) {
		flowbench_flow_add_latency(&flow->stats, 
                                           ts_interval(&now, &flow->ping_time));
                flow->ping = 0;
                evpl_arm_uevent(state->evpl, flow->dispatch);
            } else {
    
                evpl_bvec_addref(evpl, &flow->bvec);

                if (flow->state->connected) {
                    evpl_sendv(evpl, flow->bind,
                               &flow->bvec, 1, notify->recv_msg.length);
                } else {
                    evpl_sendtov(evpl, flow->bind, notify->recv_msg.addr,
                                 &flow->bvec, 1, notify->recv_msg.length);
                }
            }
        }

        break;
    default:
        fprintf(stderr,"Unhandled evpl notification type %u", notify->notify_type);
    }
}

static struct flowbench_evpl_flow *
create_flow(
    struct flowbench_evpl_state *state,
    const char *srcaddr, int srcport,
    const char *dstaddr, int dstport)
{
    struct flowbench_evpl_flow *flow;

    flow = calloc(1, sizeof(*flow));

    snprintf(flow->stats.src, sizeof(flow->stats.src), "%s:%d", srcaddr, srcport);
    snprintf(flow->stats.dst, sizeof(flow->stats.dst), "%s:%d", dstaddr, dstport);

    flow->state  = state;
    flow->config = state->config;

    evpl_bvec_alloc(state->evpl, state->config->msg_size, 4096, 1, &flow->bvec);

    flowbench_add_flow(state->stats, &flow->stats);

    flow->dispatch = evpl_add_uevent(state->evpl, flow_dispatch_callback, flow);

    evpl_arm_uevent(state->evpl, flow->dispatch);

    return flow;
}

static void
accept_callback(
    struct evpl            *evpl,
    struct evpl_bind       *listen_bind,
    struct evpl_bind       *accepted_bind,
    evpl_notify_callback_t *a_notify_callback,
    evpl_segment_callback_t *a_segment_callback,
    void                  **conn_private_data,
    void                   *private_data)
{
    struct flowbench_evpl_state *state = private_data;
    const struct flowbench_config *config = state->config;
    struct flowbench_evpl_flow *flow;

    flow = create_flow(state, config->local, config->local_port, config->peer, config->peer_port);
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

static void
flowbench_evpl_thread_init(
    struct evpl *evpl,
    void *private_data)
{
    struct flowbench_evpl_state *state = private_data;
    const struct flowbench_config *config = state->config;

    state->evpl = evpl;

    switch (config->mode) {
    case FLOWBENCH_MODE_STREAM:
        switch (config->protocol) {
        case FLOWBENCH_PROTO_TCP:
            state->protocol = EVPL_STREAM_SOCKET_TCP;
            state->stream = 1;
            state->connected = 1;
            break;
        case FLOWBENCH_PROTO_RDMACM_RC:
            state->protocol = EVPL_STREAM_RDMACM_RC;
            state->stream = 1;
            state->connected = 1;
        case FLOWBENCH_PROTO_XLIO_TCP:
            state->protocol = EVPL_STREAM_XLIO_TCP;
            state->stream = 1;
            state->connected = 1;
            break;
        default:
            state->error = 1;
            return;
        }
        break;

    case FLOWBENCH_MODE_MSG:
        switch (config->protocol) {
        case FLOWBENCH_PROTO_TCP:
            state->protocol = EVPL_STREAM_SOCKET_TCP;
            state->stream = 1;
            state->connected = 1;
            break;
        case FLOWBENCH_PROTO_UDP:
            state->protocol = EVPL_DATAGRAM_SOCKET_UDP;
            state->stream = 0;
            state->connected = 0;
            break;
        case FLOWBENCH_PROTO_RDMACM_RC:
            state->protocol = EVPL_DATAGRAM_RDMACM_RC;
            state->stream = 0;
            state->connected = 1;
            break;
        case FLOWBENCH_PROTO_RDMACM_UD:
            state->protocol = EVPL_DATAGRAM_RDMACM_UD;
            state->stream = 0;
            state->connected = 0;
            break;
        case FLOWBENCH_PROTO_XLIO_TCP:
            state->protocol = EVPL_STREAM_XLIO_TCP;
            state->stream = 1;
            state->connected = 1;
            break;
        default:
            state->error = 1;
            return;
        }
        break;
    default:
        state->error = 1;
        return;
    }

    state->client = (config->role == FLOWBENCH_ROLE_CLIENT);

    state->local = evpl_endpoint_create(state->evpl, config->local, config->local_port);
    state->remote = evpl_endpoint_create(state->evpl, config->peer, config->peer_port);

    switch (config->role) {
    case FLOWBENCH_ROLE_SERVER:
        if (state->connected) {
            evpl_listen(state->evpl, state->protocol, state->local, accept_callback, state);
        }
        break;
    default:
        break;
    }
}

void
flowbench_evpl_thread_start(
    struct flowbench_evpl_state *state)
{
    const struct flowbench_config *config = state->config;
    evpl_segment_callback_t a_segment_callback;
    struct evpl_bind *bind;
    struct flowbench_evpl_flow *flow;
    int i;

    switch (config->role) {
    case FLOWBENCH_ROLE_SERVER:

        if (!state->connected) {
            flow = create_flow(state, config->local, config->local_port, config->peer, config->peer_port);
            bind = evpl_bind(state->evpl, state->protocol, state->local, notify_callback, flow);
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
                flow = create_flow(state, config->local, config->local_port, config->peer, config->peer_port);
                bind = evpl_connect(state->evpl, state->protocol, state->remote, notify_callback, a_segment_callback, flow);
                flow->bind = bind;
                evpl_bind_request_send_notifications(state->evpl, bind);
                DL_APPEND(state->flows, flow);
            }
        } else {
            flow = create_flow(state, config->local, config->local_port, config->peer, config->peer_port);
            bind = evpl_bind(state->evpl, state->protocol, state->local, notify_callback, flow);
            flow->bind = bind;
            evpl_bind_request_send_notifications(state->evpl, bind);
            DL_APPEND(state->flows, flow);
        }

        break;
    default:
        abort();
    }
}

void
flowbench_evpl_thread_stop(
    struct flowbench_evpl_state *state)
{
    struct flowbench_evpl_flow *flow;

    DL_FOREACH(state->flows, flow) {
        evpl_disconnect(state->evpl, flow->bind);
    }
}

static void
flowbench_evpl_thread_wake(
    struct evpl *evpl,
    void *private_data)
{
    struct flowbench_evpl_state *state = private_data;

    if (state->run && !state->running) {
        state->running = 1;
        flowbench_evpl_thread_start(state);
    }

    if (!state->run && state->running) {
        state->running = 0;
        flowbench_evpl_thread_stop(state);
    }

}

void *
flowbench_evpl_init(
    struct flowbench_config *config,
    struct flowbench_stats *stats)
{
    struct flowbench_evpl_state *states, *state;
    int i;

    states = calloc(config->num_threads, sizeof(*state));

    for (i = 0; i < config->num_threads; ++i) {

        state = &states[i];

        state->config = config;
        state->stats = stats;

        state->thread = evpl_thread_create(flowbench_evpl_thread_init, flowbench_evpl_thread_wake, state);

    }

    return states;
}

void
flowbench_evpl_cleanup(void *private_data)
{
    struct flowbench_evpl_state *states = private_data;
    int i;

    for (i = 0; i < states[0].config->num_threads; ++i) {
        evpl_thread_destroy(states[i].thread);
    }

    free(states);

}

void
flowbench_evpl_start(void *private_data)
{
    struct flowbench_evpl_state *states = private_data;
    int i;

    for (i = 0; i < states[0].config->num_threads; ++i) {
        states[i].run = 1;
        __sync_synchronize();
        evpl_thread_wake(states[i].thread);
    }
}

void
flowbench_evpl_stop(void *private_data)
{
    struct flowbench_evpl_state *states = private_data;
    int i;

    for (i = 0; i < states[0].config->num_threads; ++i) {
        states[i].run = 0;
        __sync_synchronize();
        evpl_thread_wake(states[i].thread);
    }
}

struct flowbench_framework framework_evpl = {
    .init = flowbench_evpl_init,
    .cleanup = flowbench_evpl_cleanup,
    .start = flowbench_evpl_start,
    .stop = flowbench_evpl_stop,
};
