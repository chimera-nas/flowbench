// SPDX-FileCopyrightText: 2026 Ben Jarvis
//
// SPDX-License-Identifier: LGPL-2.1-only

#include <stdio.h>
#include "statwindow.h"

#define CHECK(expr) do { if (!(expr)) { \
                             fprintf(stderr, "line %d: %s\n", __LINE__, #expr); return 1; \
                         } } while (0)

int
main(void)
{
    struct stat_window sw;
    const uint64_t     bucket = STAT_WINDOW_BUCKET_INTERVAL;

    stat_window_init(&sw);
    stat_window_add(&sw, 0, 10);
    stat_window_add(&sw, bucket - 1, 20);
    CHECK(sw.count == 30 && sw.current == 0);
    stat_window_add(&sw, bucket, 40);
    CHECK(sw.count == 70 && sw.current == 1);
    stat_window_add(&sw, STAT_WINDOW_INTERVAL, 50);
    CHECK(sw.count == 90); /* bucket zero expired, bucket one remains */
    stat_window_add(&sw, STAT_WINDOW_INTERVAL + bucket, 0);
    CHECK(sw.count == 50);

    stat_window_add(&sw, 3600ULL * STAT_WINDOW_INTERVAL + bucket / 2, 60);
    CHECK(sw.count == 60);
    CHECK(sw.bucket_start == 3600ULL * STAT_WINDOW_INTERVAL);
    stat_window_add(&sw, bucket, 70); /* clock conversion moved backward */
    CHECK(sw.count == 70);
    CHECK(sw.bucket_start == bucket);
    CHECK(stat_window_get(&sw, STAT_WINDOW_INTERVAL + bucket) == 0);
    stat_window_add(&sw, 2 * STAT_WINDOW_INTERVAL, 80);
    stat_window_reset(&sw);
    CHECK(stat_window_get(&sw, 2 * STAT_WINDOW_INTERVAL) == 0);
    stat_window_add(&sw, 2 * STAT_WINDOW_INTERVAL, 90);
    CHECK(sw.count == 90);
    return 0;
} /* main */
