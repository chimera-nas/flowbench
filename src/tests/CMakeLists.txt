# SPDX-FileCopyrightText: 2025 Ben Jarvis
#
# SPDX-License-Identifier: LGPL-2.1-only

set(FLOWBENCH_RUN ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh)
set(FLOWBENCH_WRAP ${CMAKE_CURRENT_SOURCE_DIR}/wrap_test.sh)


add_test(NAME flowbench/show_version COMMAND ${FLOWBENCH_BIN} -v)
add_test(NAME flowbench/show_help COMMAND ${FLOWBENCH_BIN} -h)

add_test(NAME flowbench/pingpong_tcp_msg COMMAND ${FLOWBENCH_WRAP} ${FLOWBENCH_RUN} ${FLOWBENCH_BIN} 
        "-Q -t pingpong -m msg -p tcp -d 5"
        "-Q -t pingpong -m msg -p tcp -d 5 -a 127.0.0.1:32500")

add_test(NAME flowbench/throughput_tcp_msg COMMAND ${FLOWBENCH_WRAP} ${FLOWBENCH_RUN} ${FLOWBENCH_BIN} 
        "-Q -t throughput -m msg -p tcp -d 5"
        "-Q -t throughput -m msg -p tcp -d 5 -a 127.0.0.1:32500")

add_test(NAME flowbench/throughput_tcp_stream COMMAND ${FLOWBENCH_WRAP} ${FLOWBENCH_RUN} ${FLOWBENCH_BIN} 
        "-Q -t throughput -m stream -p tcp -d 5"
        "-Q -t throughput -m stream -p tcp -d 5 -a 127.0.0.1:32500")

add_test(NAME flowbench/throughput_udp_msg COMMAND ${FLOWBENCH_WRAP} ${FLOWBENCH_RUN} ${FLOWBENCH_BIN} 
        "-Q -t throughput -m msg -p udp -d 5 -s 4k -l 127.0.0.1:32500"
        "-Q -t throughput -m msg -p udp -d 5 -s 4k -l 127.0.0.1:32501 -a 127.0.0.1:32500")

#add_test(NAME flowbench/throughput_io_uring_tcp_stream COMMAND ${FLOWBENCH_RUN} ${FLOWBENCH_BIN} 
#        "-Q -t throughput -m stream -p io_uring_tcp -d 5"
#        "-Q -t throughput -m stream -p io_uring_tcp -d 5 -a 127.0.0.1:32500")


add_test(NAME flowbench/pingpong_rpc2_tcp_msg COMMAND ${FLOWBENCH_WRAP} ${FLOWBENCH_RUN} ${FLOWBENCH_BIN} 
        "-f evpl_rpc2 -Q -t pingpong -m msg -p tcp -s 64 -d 5"
        "-f evpl_rpc2 -Q -t pingpong -m msg -p tcp -s 64 -d 5 -a 127.0.0.1:32500")

add_test(NAME flowbench/throughput_rpc2_tcp_msg COMMAND ${FLOWBENCH_WRAP} ${FLOWBENCH_RUN} ${FLOWBENCH_BIN} 
        "-f evpl_rpc2 -Q -t throughput -m msg -s 1m -p tcp -d 5"
        "-f evpl_rpc2 -Q -t throughput -m msg -s 1m -p tcp -d 5 -a 127.0.0.1:32500")