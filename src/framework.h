// SPDX-FileCopyrightText: 2025 Ben Jarvis
//
// SPDX-License-Identifier: LGPL-2.1-only

#pragma once

struct flowbench_config;
struct flowbench_stats;

struct flowbench_framework {

    void * (*init)(
        struct flowbench_config *config,
        struct flowbench_stats  *stats);


    void   (*start)(
        void *private_data);
    void   (*stop)(
        void *private_data);

    void   (*cleanup)(
        void *private_data);
};
