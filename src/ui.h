// SPDX-FileCopyrightText: 2025 Ben Jarvis
//
// SPDX-License-Identifier: LGPL-2.1-only

#pragma once

void ui_init(
    int interval_ms);
void ui_update(
    struct flowbench_stats *stats);
void ui_cleanup();

void ui_print_stats(
    struct flowbench_stats *stats,
    uint64_t                duration_ns);

