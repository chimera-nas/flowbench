#pragma once

struct flowbench_config;
struct flowbench_stats;

void *
flowbench_evpl_start(
    struct flowbench_config *config,
    struct flowbench_stats *stats);

void
flowbench_evpl_stop(void *private_data);
