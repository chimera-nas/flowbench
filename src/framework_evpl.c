#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#include "core/evpl.h"
#include "thread/thread.h"
#include "utlist.h"

#include "framework_evpl.h"
#include "config.h"
#include "common.h"
#include "stats.h"

struct flowbench_evpl_flow {
    struct evpl_bind           *bind;
    struct flowbench_flow       stats;
    const struct flowbench_config     *config;
    struct flowbench_evpl_state *state;
    struct evpl_uevent *dispatch;

    uint64_t    	inflight_msgs;
    uint64_t    	inflight_bytes;
    int         	ping;
    struct timespec	ping_time;

    struct evpl_bvec    bvec;

};
 
struct flowbench_evpl_state {
    const struct flowbench_config *config;
    struct flowbench_stats *stats;
    struct evpl_thread *thread;
    struct evpl *evpl;
    struct evpl_endpoint *local;
    struct evpl_endpoint *remote;
    int run;
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

    if (config->role == FLOWBENCH_ROLE_SERVER) {
        return 0;
    }

    if (flow->bind == NULL) return 0;

    switch (config->test) {
    case FLOWBENCH_TEST_THROUGHPUT:
        if (state->stream) {
            return (flow->inflight_bytes + config->msg_size <= config->max_inflight_bytes);
        } else {
            return (flow->inflight_msgs < config->max_inflight_msgs);
        }
    case FLOWBENCH_TEST_PINGPONG:
        return (flow->ping == 0);
        break;
    default:
        return 0;
    }
}


static void
dispatch_callback(
    struct evpl *evpl,
    void *private_data)
{
    struct flowbench_evpl_flow *flow = private_data;
    struct flowbench_evpl_state *state = flow->state;
    const struct flowbench_config *config = flow->config;

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
    const struct evpl_notify *notify,
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
        flowbench_remove_flow(state->stats, &flow->stats);
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
                flowbench_flow_add_recv_bytes(&flow->stats, &now, bvecs[i].length);
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

    evpl_bvec_alloc(state->evpl, state->config->msg_size + 4, 4096, 1, &flow->bvec);

    flowbench_add_flow(state->stats, &flow->stats);

    flow->dispatch = evpl_add_uevent(state->evpl, dispatch_callback, flow);

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
} /* accept_callback */

static void
flowbench_evpl_thread_init(
    struct evpl *evpl,
    void *private_data)
{
    struct flowbench_evpl_state *state = private_data;
    const struct flowbench_config *config = state->config;
    struct flowbench_evpl_flow  *flow;
    enum evpl_protocol_id evpl_protocol;
    struct evpl_bind *bind;

    state->evpl = evpl;

    switch (config->mode) {
    case FLOWBENCH_MODE_STREAM:
        switch (config->protocol) {
        case FLOWBENCH_PROTO_TCP:
            evpl_protocol = EVPL_STREAM_SOCKET_TCP;
            state->stream = 1;
            state->connected = 1;
            break;
        case FLOWBENCH_PROTO_RDMACM_RC:
            evpl_protocol = EVPL_STREAM_RDMACM_RC;
            state->stream = 1;
            state->connected = 1;
        default:
            state->error = 1;
            return;
        }
        break;

    case FLOWBENCH_MODE_MSG:
        switch (config->protocol) {
        case FLOWBENCH_PROTO_TCP:
            evpl_protocol = EVPL_STREAM_SOCKET_TCP;
            state->stream = 1;
            state->connected = 1;
            break;
        case FLOWBENCH_PROTO_UDP:
            evpl_protocol = EVPL_DATAGRAM_SOCKET_UDP;
            state->stream = 0;
            state->connected = 0;
            break;
        case FLOWBENCH_PROTO_RDMACM_RC:
            evpl_protocol = EVPL_DATAGRAM_RDMACM_RC;
            state->stream = 0;
            state->connected = 1;
            break;
        case FLOWBENCH_PROTO_RDMACM_UD:
            evpl_protocol = EVPL_DATAGRAM_RDMACM_UD;
            state->stream = 0;
            state->connected = 0;
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
            evpl_listen(state->evpl, evpl_protocol, state->local, accept_callback, state);
        } else {
            flow = create_flow(state, config->local, config->local_port, config->peer, config->peer_port);
            bind = evpl_bind(state->evpl, evpl_protocol, state->local, notify_callback, flow);
            flow->bind = bind;
            evpl_bind_request_send_notifications(state->evpl, bind);
        }
        break;
    case FLOWBENCH_ROLE_CLIENT:
        flow = create_flow(state, config->local, config->local_port, config->peer, config->peer_port);
        if (state->connected) {
            bind = evpl_connect(state->evpl, evpl_protocol, state->remote, notify_callback, segment_callback, flow);
        } else {
            bind = evpl_bind(state->evpl, evpl_protocol, state->local, notify_callback, flow);
        }
        flow->bind = bind;
        evpl_bind_request_send_notifications(state->evpl, bind);
        break;
    default:
        abort();
    }
}

void *
flowbench_evpl_start(
    struct flowbench_config *config,
    struct flowbench_stats *stats)
{
    struct flowbench_evpl_state *state;

    state = calloc(1, sizeof(*state));

    state->config = config;
    state->stats = stats;

    //evpl_init_auto(NULL);
  
    state->run = 1;

    state->thread = evpl_thread_create(flowbench_evpl_thread_init, state);

    return state;
}

void
flowbench_evpl_stop(void *private_data)
{
    struct flowbench_evpl_state *state = private_data;

    evpl_thread_destroy(state->thread);

    free(state);

}
