#pragma once

#include "common.h"

#define STAT_WINDOW_NUM_BUCKETS     128
#define STAT_WINDOW_INTERVAL (1000000000UL)
#define STAT_WINDOW_BUCKET_INTERVAL (STAT_WINDOW_INTERVAL / STAT_WINDOW_NUM_BUCKETS)

struct stat_window {
    uint64_t                  buckets[STAT_WINDOW_NUM_BUCKETS];
    struct timespec           bucket_start;
    uint64_t                  count;
    unsigned int              current;
};

static inline void
stat_window_init(struct stat_window *sw)
{
    memset(sw, 0, sizeof(*sw));
}

static inline void
stat_window_add(
    struct stat_window *sw,
    struct timespec *current_time,
    uint64_t value)
{
    uint64_t delta;

    delta = ts_interval(current_time, &sw->bucket_start);

    if (delta > STAT_WINDOW_BUCKET_INTERVAL) {

        sw->bucket_start = *current_time;
        sw->current++;

        if (sw->current == STAT_WINDOW_NUM_BUCKETS) {
            sw->current = 0;
        }
    
        sw->count -= sw->buckets[sw->current];
        sw->buckets[sw->current] = 0;
    }

    sw->buckets[sw->current] += value; 
    sw->count += value;
}

 
