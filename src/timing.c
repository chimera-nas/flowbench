// SPDX-FileCopyrightText: 2026 Ben Jarvis
//
// SPDX-License-Identifier: LGPL-2.1-only

#include "stopwatch.h"
#include "timing.h"

static struct stopwatch_context clock_context;
static struct stopwatch         clock_epoch;

void
flowbench_clock_init(void)
{
    /* Initialize before starting any workers. Only read the context after
     * this point, so every thread uses the same epoch and conversion. */
    stopwatch_context_init(&clock_context);
    stopwatch_start(&clock_context, &clock_epoch);
} /* flowbench_clock_init */

uint64_t
flowbench_now_ns(void)
{
    return stopwatch_elapsed_ns(&clock_context, &clock_epoch);
} /* flowbench_now_ns */
