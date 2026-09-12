// SPDX-FileCopyrightText: 2025 - 2026 Ben Jarvis
//
// SPDX-License-Identifier: LGPL-2.1-only

#pragma once

#include <stdint.h>
#include <string.h>

#define STAT_WINDOW_NUM_BUCKETS     128
#define STAT_WINDOW_INTERVAL        (1000000000UL)
#define STAT_WINDOW_BUCKET_INTERVAL (STAT_WINDOW_INTERVAL / STAT_WINDOW_NUM_BUCKETS)

struct stat_window {
    uint64_t     buckets[STAT_WINDOW_NUM_BUCKETS];
    uint64_t     bucket_start;
    uint64_t     count;
    unsigned int current;
};

static inline void
stat_window_init(struct stat_window *sw)
{
    memset(sw, 0, sizeof(*sw));
} // stat_window_init

/* current_time is elapsed stopwatch time in nanoseconds. */
static inline void
stat_window_advance(
    struct stat_window *sw,
    uint64_t            current_time)
{
    uint64_t delta;

    /* Small inter-core clock skew can produce a backwards sample. */
    if (current_time < sw->bucket_start) {
        stat_window_init(sw);
    }
    delta = current_time - sw->bucket_start;

    if (delta >= STAT_WINDOW_INTERVAL) {
        /* Bound the work after an idle period, regardless of its length. */
        stat_window_init(sw);
        sw->bucket_start = current_time - current_time % STAT_WINDOW_BUCKET_INTERVAL;
    } else {
        while (delta >= STAT_WINDOW_BUCKET_INTERVAL) {
            sw->bucket_start        += STAT_WINDOW_BUCKET_INTERVAL;
            sw->current              = (sw->current + 1) % STAT_WINDOW_NUM_BUCKETS;
            sw->count               -= sw->buckets[sw->current];
            sw->buckets[sw->current] = 0;
            delta                   -= STAT_WINDOW_BUCKET_INTERVAL;
        }
    }

} // stat_window_advance

static inline void
stat_window_reset(struct stat_window *sw)
{
    memset(sw->buckets, 0, sizeof(sw->buckets));
    sw->count   = 0;
    sw->current = 0;
} // stat_window_reset

static inline void
stat_window_add(
    struct stat_window *sw,
    uint64_t            current_time,
    uint64_t            value)
{
    stat_window_advance(sw, current_time);
    sw->buckets[sw->current] += value;
    sw->count                += value;
} // stat_window_add

static inline uint64_t
stat_window_get(
    struct stat_window *sw,
    uint64_t            current_time)
{
    stat_window_advance(sw, current_time);
    return sw->count;
} // stat_window_get
