#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include "utlist.h"
#include "statwindow.h"

struct flowbench_flow {
    struct stat_window     recv_bytes_window;
    struct stat_window     recv_msgs_window;
    struct stat_window     sent_bytes_window;
    struct stat_window     sent_msgs_window;

    uint64_t               sent_msgs;
    uint64_t               sent_bytes;
    uint64_t               recv_msgs;
    uint64_t               recv_bytes;

    uint64_t               min_latency;
    uint64_t               max_latency;
    uint64_t               total_latency;
    uint64_t               latency_samples;

    char                   src[256];
    char                   dst[256];

    struct flowbench_flow *prev;
    struct flowbench_flow *next;
};

struct flowbench_event {
    char                    msg[80];
    struct flowbench_event *prev;
    struct flowbench_event *next;
};

struct flowbench_stats {
    struct flowbench_flow   saved;
    struct flowbench_flow  *flows;
    struct flowbench_event *events;
    pthread_mutex_t         lock;
};

static inline struct flowbench_event *
flowbench_create_event(
    const char *fmt,
    ...)
{
    struct flowbench_event *event;
    va_list                 argp;

    event = calloc(1, sizeof(*event));

    va_start(argp, fmt);
    vsnprintf(event->msg, sizeof(event->msg), fmt, argp);
    va_end(argp);

    return event;
} /* flowbench_create_event */
static inline void
flowbench_flow_add_latency(
    struct flowbench_flow *flow,
    uint64_t               ns)
{
    if (ns < flow->min_latency || flow->min_latency == 0) {
        flow->min_latency = ns;
    }

    if (ns > flow->max_latency) {
        flow->max_latency = ns;
    }

    flow->total_latency += ns;
    flow->latency_samples++;

} /* flowbench_flow_add_latency */

static inline void
flowbench_add_flow(
    struct flowbench_stats *stats,
    struct flowbench_flow  *flow)
{
    struct flowbench_event *event;

    event = flowbench_create_event("Flow created.");

    stat_window_init(&flow->recv_bytes_window);
    stat_window_init(&flow->sent_bytes_window);
    stat_window_init(&flow->recv_msgs_window);
    stat_window_init(&flow->sent_msgs_window);

    pthread_mutex_lock(&stats->lock);
    DL_APPEND(stats->flows, flow);
    DL_APPEND(stats->events, event);
    pthread_mutex_unlock(&stats->lock);
} /* flowbench_add_flow */

static inline void
flowbench_remove_flow(
    struct flowbench_stats *stats,
    struct flowbench_flow  *flow)
{
    struct flowbench_event *event;

    event = flowbench_create_event("Flow closed.");

    pthread_mutex_lock(&stats->lock);

    stats->saved.sent_msgs  += flow->sent_msgs;
    stats->saved.sent_bytes += flow->sent_bytes;
    stats->saved.recv_msgs  += flow->recv_msgs;
    stats->saved.recv_bytes += flow->recv_bytes;

    if (flow->min_latency < stats->saved.min_latency ||
        stats->saved.min_latency == 0) {
        stats->saved.min_latency = flow->min_latency;
    }

    if (flow->max_latency > stats->saved.max_latency) {
        stats->saved.max_latency = flow->max_latency;
    }

    stats->saved.total_latency   += flow->total_latency;
    stats->saved.latency_samples += flow->latency_samples;

    DL_DELETE(stats->flows, flow);
    DL_APPEND(stats->events, event);
    pthread_mutex_unlock(&stats->lock);
} /* flowbench_remove_flow */

static inline void
flowbench_flow_add_recv_bytes(
    struct flowbench_flow *flow,
    struct timespec       *now,
    uint64_t               bytes)
{
    flow->recv_bytes += bytes;
    stat_window_add(&flow->recv_bytes_window, now, bytes);
} /* flowbench_flow_add_recv_bytes */

static inline void
flowbench_flow_add_sent_bytes(
    struct flowbench_flow *flow,
    struct timespec       *now,
    uint64_t               bytes)
{
    flow->sent_bytes += bytes;
    stat_window_add(&flow->sent_bytes_window, now, bytes);
} /* flowbench_flow_add_sent_bytes */

static inline void
flowbench_flow_add_recv_msgs(
    struct flowbench_flow *flow,
    struct timespec       *now,
    uint64_t               msgs)
{
    flow->recv_msgs += msgs;
    stat_window_add(&flow->recv_msgs_window, now, msgs);
} /* flowbench_flow_add_recv_msgs */

static inline void
flowbench_flow_add_sent_msgs(
    struct flowbench_flow *flow,
    struct timespec       *now,
    uint64_t               msgs)
{
    flow->sent_msgs += msgs;
    stat_window_add(&flow->sent_msgs_window, now, msgs);
} /* flowbench_flow_add_sent_msgs */

static void
flowbench_clear_stats(struct flowbench_stats *stats)
{
    struct flowbench_flow *flow;

    pthread_mutex_lock(&stats->lock);
    memset(&stats->saved, 0, sizeof(stats->saved));

    DL_FOREACH(stats->flows, flow)
    {
        stat_window_reset(&flow->recv_bytes_window);
        stat_window_reset(&flow->sent_bytes_window);
        stat_window_reset(&flow->recv_msgs_window);
        stat_window_reset(&flow->sent_msgs_window);

        flow->sent_msgs  = 0;
        flow->sent_bytes = 0;
        flow->recv_msgs  = 0;
        flow->recv_bytes = 0;

        flow->min_latency     = 0;
        flow->max_latency     = 0;
        flow->total_latency   = 0;
        flow->latency_samples = 0;

    }

    pthread_mutex_unlock(&stats->lock);
} /* flowbench_clear_stats */
