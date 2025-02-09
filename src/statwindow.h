#pragma once

#include "common.h"

#define STAT_WINDOW_NUM_BUCKETS     128
#define STAT_WINDOW_INTERVAL        (1000000000UL)
#define STAT_WINDOW_BUCKET_INTERVAL (STAT_WINDOW_INTERVAL / STAT_WINDOW_NUM_BUCKETS)

struct stat_window {
    uint64_t        buckets[STAT_WINDOW_NUM_BUCKETS];
    struct timespec bucket_start;
    uint64_t        count;
    unsigned int    current;
};

static inline void
stat_window_init(struct stat_window *sw)
{
    memset(sw, 0, sizeof(*sw));
    clock_gettime(CLOCK_MONOTONIC, &sw->bucket_start);
} /* stat_window_init */

static inline void
stat_window_reset(struct stat_window *sw)
{
    for (int i = 0; i < STAT_WINDOW_NUM_BUCKETS; i++) {
        sw->buckets[i] = 0;
    }
} /* stat_window_reset */

static inline void
stat_window_add(
    struct stat_window *sw,
    struct timespec    *current_time,
    uint64_t            value)
{
    int64_t delta;

    delta = ts_interval(current_time, &sw->bucket_start);

    if (delta < 0) {
        delta = 0;
    }

    while (delta > STAT_WINDOW_BUCKET_INTERVAL) {

        sw->bucket_start.tv_nsec += STAT_WINDOW_BUCKET_INTERVAL;
        if (sw->bucket_start.tv_nsec > 1000000000UL) {
            sw->bucket_start.tv_sec++;
            sw->bucket_start.tv_nsec -= 1000000000UL;
        }

        sw->current++;

        if (sw->current == STAT_WINDOW_NUM_BUCKETS) {
            sw->current = 0;
        }

        sw->count               -= sw->buckets[sw->current];
        sw->buckets[sw->current] = 0;

        delta -= STAT_WINDOW_BUCKET_INTERVAL;
    }

    sw->buckets[sw->current] += value;
    sw->count                += value;
} /* stat_window_add */


