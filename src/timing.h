// SPDX-FileCopyrightText: 2026 Ben Jarvis
//
// SPDX-License-Identifier: LGPL-2.1-only

#pragma once

#include <stdint.h>

void flowbench_clock_init(
    void);
uint64_t flowbench_now_ns(
    void);

static inline uint64_t
flowbench_interval_ns(
    uint64_t end,
    uint64_t start)
{
    return end >= start ? end - start : 0;
} // flowbench_interval_ns
