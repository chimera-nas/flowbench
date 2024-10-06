#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "framework_evpl.h"

int main(int argc, char *argv[])
{
    struct flowbench_config config;
    char *ch;
    int opt, rc;

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
    config.max_inflight = 512*1024;
    config.duration = 10UL * 1000000000UL;;

    while ((opt = getopt(argc, argv, "a:f:l:m:r:p:s:t:")) != -1) {
        switch (opt) {
        case 'a':

            ch = index(optarg,':');

            if (ch) {
                *ch = '\0';
                config.peer_port = atoi(ch+1);
            }

            config.peer = optarg;
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

    switch (config.framework) {
    case FLOWBENCH_FRAMEWORK_EVPL:
        rc = run_evpl(&config);
        break;
    default:
        rc = -1;
    }

    return rc;
}

