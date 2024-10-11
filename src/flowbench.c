#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

#include "utlist.h"

#include "config.h"
#include "common.h"
#include "stats.h"
#include "ui.h"

#include "framework_evpl.h"

int SigInt = 0;

void sigint_handler(int signo) {
    SigInt = 1;
}

int main(int argc, char *argv[])
{
    struct flowbench_config config;
    struct flowbench_stats stats;
    void *framework_private;
    char *ch;
    int opt;
    struct timespec start_time, now;
    uint64_t elapsed;

    signal(SIGINT, sigint_handler);

    config.framework = FLOWBENCH_FRAMEWORK_EVPL;
    config.role = FLOWBENCH_ROLE_SERVER;
    config.mode = FLOWBENCH_MODE_MSG;
    config.protocol = FLOWBENCH_PROTO_TCP;
    config.test = FLOWBENCH_TEST_THROUGHPUT;
    config.local = "0.0.0.0";
    config.local_port = 32500;
    config.peer = "127.0.0.1";
    config.peer_port = 32500;
    config.msg_size = 65536;
    config.max_inflight_bytes = 1*1024*1024;
    config.max_inflight_msgs  = 64;
    config.duration = 10UL * 1000000000UL;;

    while ((opt = getopt(argc, argv, "a:d:f:l:m:r:p:s:t:")) != -1) {
        switch (opt) {
        case 'a':

            ch = index(optarg,':');

            if (ch) {
                *ch = '\0';
                config.peer_port = atoi(ch+1);
            }

            config.peer = optarg;
            break;
        case 'd':
            config.duration = atoi(optarg) * 1000000000UL;
            break;
        case 'f':
            config.framework = map_framework(optarg);
            if (config.framework == FLOWBENCH_FRAMEWORK_INVALID) {
                fprintf(stderr,"Invalid framework '%s'\n", optarg);
                return 1;
            }  
            break;
        case 'l':

            ch = index(optarg,':');

            if (ch) {
                *ch = '\0';
                config.local_port = atoi(ch+1);
            }

            config.local = optarg;
            break;
        case 'm':
            config.mode = map_mode(optarg);
            if (config.mode == FLOWBENCH_MODE_INVALID) {
                fprintf(stderr,"Invalid mode '%s'\n", optarg);
                return 1;
            }
            break;
        case 'r':
            config.role = map_role(optarg);
            if (config.role == FLOWBENCH_ROLE_INVALID) {
                fprintf(stderr,"Invalid role '%s'\n", optarg);
                return 1;
            }
            break;
        case 'p':
            config.protocol = map_protocol(optarg);
            if (config.protocol == FLOWBENCH_PROTO_INVALID) {
                fprintf(stderr,"Invalid protocol '%s'\n", optarg);
                return 1;
            }
            break;
        case 's':
            if (parse_size(&config.msg_size, optarg)) {
                fprintf(stderr,"Invalid msg size '%s'\n", optarg);
                return 1;
            }
            break;
        case 't':
            config.test = map_test(optarg);
            if (config.test == FLOWBENCH_TEST_INVALID) {
                fprintf(stderr,"Invalid test '%s'\n", optarg);
                return 1;
            }
            break;
        default:
            fprintf(stderr, "Usage: %s [-s] [-c server_address]\n", argv[0]);
            return 1;
        }
    }

    fprintf(stderr,"Starting workload...\n");
    memset(&stats, 0, sizeof(stats));

    pthread_mutex_init(&stats.lock, NULL);

    switch (config.framework) {
    case FLOWBENCH_FRAMEWORK_EVPL:
        framework_private = flowbench_evpl_start(&config, &stats);
        break;
    default:
        framework_private = NULL;
    }

    if (!framework_private) {
        fprintf(stderr,"Failed to initialize framework.\n");
        return 1;
    }

    switch (config.role) {
    case FLOWBENCH_ROLE_SERVER:
        fprintf(stderr,"Running in server mode.\n");
        while (!SigInt) {
            sleep(1);
        }
        fprintf(stderr,"Exiting...\n");
        break;
    case FLOWBENCH_ROLE_CLIENT:

        fprintf(stderr,"Waiting for warmup period...\n");
        //sleep(1);

        ui_init(250);
        
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        do {
            clock_gettime(CLOCK_MONOTONIC, &now);

            elapsed = ts_interval(&now, &start_time);

            ui_update(&stats);

        } while (elapsed < config.duration && !SigInt);

        ui_cleanup();

        ui_print_stats(&stats, elapsed);

        break;
    default:
        abort();
    }

    switch (config.framework) {
    case FLOWBENCH_FRAMEWORK_EVPL:
        flowbench_evpl_stop(framework_private);
        break;
    default:
    }

    return 0;
}

