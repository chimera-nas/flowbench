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

#include "framework.h"

extern struct flowbench_framework framework_evpl;

int SigInt = 0;

void sigint_handler(int signo) {
    SigInt = 1;
}

int main(int argc, char *argv[])
{
    struct flowbench_config config;
    struct flowbench_stats stats;
    struct flowbench_event *event;
    struct flowbench_framework *framework;
    void *framework_private;
    char *ch;
    int opt;
    struct timespec start_time, end_time, now;
    uint64_t elapsed;

    signal(SIGINT, sigint_handler);

    config.framework_id = FLOWBENCH_FRAMEWORK_EVPL;
    config.role = FLOWBENCH_ROLE_SERVER;
    config.mode = FLOWBENCH_MODE_MSG;
    config.protocol = FLOWBENCH_PROTO_TCP;
    config.test = FLOWBENCH_TEST_THROUGHPUT;
    config.interactive = 1;
    config.bidirectional =0;
    config.reverse = 0;
    config.local = "0.0.0.0";
    config.local_port = 32500;
    config.peer = "127.0.0.1";
    config.peer_port = 32500;
    config.num_threads = 1;
    config.num_flows = 1;
    config.msg_size = 65536;
    config.max_inflight_bytes = 1*1024*1024;
    config.max_inflight_msgs  = 64;
    config.duration = 10UL * 1000000000UL;;

    while ((opt = getopt(argc, argv, "a:Bd:f:l:m:n:r:Rp:P:qs:t:v")) != -1) {
        switch (opt) {
        case 'a':

            ch = index(optarg,':');

            if (ch) {
                *ch = '\0';
                config.peer_port = atoi(ch+1);
            }

            config.peer = optarg;
            break;
        case 'B':
            config.bidirectional = 1;
            break;
        case 'd':
            config.duration = atoi(optarg) * 1000000000UL;
            break;
        case 'f':
            config.framework_id = map_framework(optarg);
            if (config.framework_id == FLOWBENCH_FRAMEWORK_INVALID) {
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
        case 'n':
            config.num_flows = atoi(optarg);
            break;
        case 'r':
            config.role = map_role(optarg);
            if (config.role == FLOWBENCH_ROLE_INVALID) {
                fprintf(stderr,"Invalid role '%s'\n", optarg);
                return 1;
            }
            break;
        case 'R':
            config.reverse = 1;
            break;
        case 'p':
            config.protocol = map_protocol(optarg);
            if (config.protocol == FLOWBENCH_PROTO_INVALID) {
                fprintf(stderr,"Invalid protocol '%s'\n", optarg);
                return 1;
            }
            break;
        case 'P':
            config.num_threads = atoi(optarg);
            break;
        case 'q':
            config.interactive = 0;
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
        case 'v':
            printf("flowbench version %s\n", FLOWBENCH_VERSION);
            return 0;
        default:
            fprintf(stderr, "Usage: %s [options]\n"
                    "Options:\n"
                    "  -a addr[:port]    Peer address and optional port (default: 127.0.0.1:32500)\n"
                    "  -B                Enable bidirectional mode\n"
                    "  -d seconds        Test duration in seconds (default: 10)\n"
                    "  -f framework      Framework to use (default: evpl)\n"
                    "  -l addr[:port]    Local address and optional port (default: 0.0.0.0:32500)\n"
                    "  -m mode           Mode (default: msg)\n"
                    "  -n num_flows      Number of flows (default: 1)\n"
                    "  -p protocol       Protocol (default: tcp)\n"
                    "  -P num_threads    Number of threads (default: 1)\n"
                    "  -q                Quiet mode (disable interactive display)\n"
                    "  -r role           Role: client or server (default: server)\n"
                    "  -R                Reverse mode\n"
                    "  -s size           Message size (default: 65536)\n"
                    "  -t test           Test type (default: throughput)\n"
                    "  -v                Show version\n", argv[0]);
            return 1;
        }
    }

    memset(&stats, 0, sizeof(stats));

    pthread_mutex_init(&stats.lock, NULL);

    switch (config.framework_id) {
    case FLOWBENCH_FRAMEWORK_EVPL:
        framework = &framework_evpl;
        break;
    default:
        fprintf(stderr,"Unknown framework %d", config.framework_id);
        exit(1);
    }

    fprintf(stderr,"Initializing...\n");

    framework_private = framework->init(&config, &stats);

    if (!framework_private) {
        fprintf(stderr,"Failed to initialize framework.\n");
        return 1;
    }

    switch (config.role) {
    case FLOWBENCH_ROLE_SERVER:

        fprintf(stderr,"Running in server mode.\n");

        framework->start(framework_private);

        while (!SigInt) {
            sleep(1);
        }

        framework->stop(framework_private);

        break;

    case FLOWBENCH_ROLE_CLIENT:

        fprintf(stderr,"Warming up...");

        framework->start(framework_private);

        sleep(3);

        framework->stop(framework_private);

        while (stats.flows) {
            usleep(100);
        }

        sleep(1);

        flowbench_clear_stats(&stats);


        if (config.interactive) {
            ui_init(250);
        }

        fprintf(stderr,"Begin measurement...\n");

        clock_gettime(CLOCK_MONOTONIC, &start_time);

        framework->start(framework_private);

        do {
            clock_gettime(CLOCK_MONOTONIC, &now);

            elapsed = ts_interval(&now, &start_time);

            if (config.interactive) {
                ui_update(&stats);
            } else {
                usleep(1000);
            }

        } while (elapsed < config.duration && !SigInt);

        framework->stop(framework_private);

        while (stats.flows) {
            usleep(1);
        }

        clock_gettime(CLOCK_MONOTONIC, &end_time);

        elapsed = ts_interval(&end_time, &start_time);

        if (config.interactive) {
            ui_cleanup();
        }

        ui_print_stats(&stats, elapsed);

        break;
    default:
        abort();
    }

   
    framework->cleanup(framework_private);

    while (stats.events) {
        event = stats.events;
        DL_DELETE(stats.events, event);
        free(event);
    }

    return 0;
}

