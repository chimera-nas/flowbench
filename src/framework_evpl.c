#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "core/evpl.h"

#include "framework_evpl.h"
#include "config.h"
#include "common.h"
 
struct flowbench_evpl_state {
    const struct flowbench_config *config;
    struct evpl_endpoint *local;
    struct evpl_endpoint *remote;
    struct evpl_bind *listen_bind;
    struct evpl_bind *bind;
    evpl_notify_callback_t notify;
    int run;
    int stream;
    int connected;
    int client;
    uint64_t    inflight;
    uint64_t    sent;
    uint64_t    recv;
    struct timespec start_time;
    struct timespec end_time;
    uint64_t elapsed;
};

static int
segment_callback(
    struct evpl      *evpl,
    struct evpl_bind *bind,
    void             *private_data)
{
    struct flowbench_evpl_state *state = private_data;

    return state->config->msg_size;
}

static void 
poll_tp_connected_send(
    struct evpl *evpl,
    void *private_data)
{
    struct flowbench_evpl_state *state = private_data;
    struct evpl_bvec bvec[2];
    int nbvecs;
    struct timespec now;
    uint64_t elapsed;

    if (state->sent == 0) {
        evpl_bind_request_send_notifications(evpl, state->bind);
    }

    clock_gettime(CLOCK_MONOTONIC, &now);

    elapsed = ts_interval(&now, &state->start_time);

    if (elapsed > state->config->duration) {
        state->run = 0;
    }

    while (state->inflight + state->config->msg_size <= state->config->max_inflight) {

        nbvecs = evpl_bvec_alloc(evpl, state->config->msg_size, 0, 2, bvec);

        evpl_sendv(evpl, state->bind, bvec, nbvecs, state->config->msg_size);

        state->inflight += state->config->msg_size;
        state->sent += state->config->msg_size;
    }

}

static void
poll_tp_send(
    struct evpl *evpl,
    void *private_data)
{
    struct flowbench_evpl_state *state = private_data;
    struct evpl_bvec bvec[2];
    int nbvecs;
    struct timespec now;
    uint64_t elapsed;

    if (state->sent == 0) {
        evpl_bind_request_send_notifications(evpl, state->bind);
    }

    clock_gettime(CLOCK_MONOTONIC, &now);

    elapsed = ts_interval(&now, &state->start_time);

    if (elapsed > state->config->duration) {
        state->run = 0;
    }

    while (state->inflight + state->config->msg_size <= state->config->max_inflight) {

        nbvecs = evpl_bvec_alloc(evpl, state->config->msg_size, 0, 2, bvec);

        evpl_sendtov(evpl, state->bind, state->remote,bvec, nbvecs, state->config->msg_size);

        state->inflight += state->config->msg_size;
        state->sent += state->config->msg_size;
    }

}

static void  
poll_pingpong(
    struct evpl *evpl,
    void *private_data)
{

}

static void 
notify_tp_send(
    struct evpl              *evpl,
    struct evpl_bind         *bind,
    const struct evpl_notify *notify,
    void                     *private_data)
{
    struct flowbench_evpl_state *state = private_data;

    switch (notify->notify_type) {
    case EVPL_NOTIFY_CONNECTED:
        break;
    case EVPL_NOTIFY_DISCONNECTED:
        break;
    case EVPL_NOTIFY_SENT:
        state->inflight -= notify->sent.bytes;
        break;
    default:
        fprintf(stderr,"Unhandled evpl notification type %u", notify->notify_type);
    }
}

static void
notify_tp_recv(
    struct evpl              *evpl,
    struct evpl_bind         *bind,
    const struct evpl_notify *notify,
    void                     *private_data)
{
    //struct flowbench_evpl_state *state = private_data;
    struct evpl_bvec bvecs[8];
    int i, nbvecs;

    switch (notify->notify_type) {
    case EVPL_NOTIFY_CONNECTED:
        fprintf(stderr,"connected\n");
        break;
    case EVPL_NOTIFY_DISCONNECTED:
        fprintf(stderr,"disconnected\n");
        break;
    case EVPL_NOTIFY_RECV_DATA:
        do {

            nbvecs = evpl_readv(evpl, bind, bvecs, 8, 4*1024*1024);

            for (i = 0; i < nbvecs; ++i) {
                evpl_bvec_release(evpl, &bvecs[i]);
            }

        } while (nbvecs);
        break;
    case EVPL_NOTIFY_RECV_MSG:
        break;
    default:
        fprintf(stderr,"Unhandled evpl notification type %u", notify->notify_type);
    }
}

static void
notify_pp_send(
    struct evpl              *evpl,
    struct evpl_bind         *bind,
    const struct evpl_notify *notify,
    void                     *private_data)
{


    switch (notify->notify_type) {
    case EVPL_NOTIFY_CONNECTED:
        fprintf(stderr,"connected\n");
        break;
    case EVPL_NOTIFY_DISCONNECTED:
        fprintf(stderr,"disconnected\n");
        break;
    default:
        fprintf(stderr,"Unhandled evpl notification type %u", notify->notify_type);
    }
}

static void
notify_pp_recv(
    struct evpl              *evpl,
    struct evpl_bind         *bind,
    const struct evpl_notify *notify,
    void                     *private_data)
{


    switch (notify->notify_type) {
    case EVPL_NOTIFY_CONNECTED:
        fprintf(stderr,"connected\n");
        break;
    case EVPL_NOTIFY_DISCONNECTED:
        fprintf(stderr,"disconnected\n");
        break;
    default:
        fprintf(stderr,"Unhandled evpl notification type %u", notify->notify_type);
    }
}

static void
accept_callback(
    struct evpl_bind       *bind,
    evpl_notify_callback_t *a_notify_callback,
    evpl_segment_callback_t *a_segment_callback,
    void                  **conn_private_data,
    void                   *private_data)
{
    struct flowbench_evpl_state *state = private_data;

    fprintf(stderr,"accept callback\n");

    *a_notify_callback = state->notify;

    if (!state->stream) {
        *a_segment_callback = segment_callback;
    }

    *conn_private_data = private_data;
} /* accept_callback */

int 
run_evpl(struct flowbench_config *config)
{
    struct flowbench_evpl_state *state;
    enum evpl_protocol_id evpl_protocol;
    struct evpl *evpl = NULL;
    evpl_poll_callback_t test_poll = NULL;
    evpl_notify_callback_t test_notify = NULL;
    int rc = 0;
    char tpstr[80];
    double elapsedf;

    state = calloc(1, sizeof(*state));

    state->config = config;

    evpl_init_auto(NULL);

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
            rc = -1;
            goto out;
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
            rc = -1;
            goto out;
        }
        break;
    default:
        rc = -1;
        goto out;
    }
  
    state->client = (config->role == FLOWBENCH_ROLE_CLIENT);
 
    switch (config->test) {
    case FLOWBENCH_TEST_THROUGHPUT:
        if (state->client) {
            test_notify = notify_tp_send;

            if (state->connected) {
                test_poll = poll_tp_connected_send;
            } else {
                test_poll = poll_tp_send;
            }

        } else {
            test_notify = notify_tp_recv;
        }
        break;
    case FLOWBENCH_TEST_PINGPONG:
        if (state->client) {
            test_notify = notify_pp_send;
        } else {
            test_notify = notify_pp_recv;
        }
        break;
    default:
        rc = -1;
        goto out;
    }     
    evpl = evpl_create();

    state->local = evpl_endpoint_create(evpl, config->local, config->local_port);
    state->remote = evpl_endpoint_create(evpl, config->peer, config->peer_port);

    fprintf(stderr,"local %s:%d remote %s:%d\n", config->local, config->local_port, config->peer, config->peer_port);

    state->notify = test_notify;

    switch (config->role) {
    case FLOWBENCH_ROLE_SERVER:
        if (state->connected) {
            state->listen_bind = evpl_listen(evpl, evpl_protocol, state->local, accept_callback, state);
        } else {
            state->bind = evpl_bind(evpl, evpl_protocol, state->local, test_notify, state);
        }
        break;
    case FLOWBENCH_ROLE_CLIENT:
        if (state->connected) {
            state->bind = evpl_connect(evpl, evpl_protocol, state->remote, test_notify, segment_callback, state);
        } else {
            state->bind = evpl_bind(evpl, evpl_protocol, state->local, test_notify, state);
        }
        break;
    default:
        rc = -1;
        goto out;
    }

    state->run = 1;

    if (test_poll) {
        fprintf(stderr,"adding poll\n");
        evpl_add_poll(evpl, test_poll, state);
    }

    fprintf(stderr,"starting test\n");
    clock_gettime(CLOCK_MONOTONIC, &state->start_time);

    while (state->run) {
        evpl_wait(evpl, -1);
    }

    clock_gettime(CLOCK_MONOTONIC, &state->end_time);
    fprintf(stderr,"test complete\n");

    state->elapsed = ts_interval(&state->end_time, &state->start_time);
    
    elapsedf = state->elapsed / 1000000000.0F;

    format_throughput(tpstr, sizeof(tpstr), state->sent, state->elapsed);
    fprintf(stderr,"sent %ld bytes in %.02F seconds at %s\n",
        state->sent, elapsedf, tpstr);
out:

    if (state) {
        free(state);
    }

    if (evpl) {
        evpl_destroy(evpl);
    }
    return rc;
}
