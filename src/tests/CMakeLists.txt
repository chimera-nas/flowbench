set(FLOWBENCH_RUN ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh)


add_test(NAME flowbench/show_version COMMAND ${FLOWBENCH_BIN} -v)
add_test(NAME flowbench/show_help COMMAND ${FLOWBENCH_BIN} -h)

add_test(NAME flowbench/pingpong_tcp_msg COMMAND ${FLOWBENCH_RUN} ${FLOWBENCH_BIN} 
        "-Q -t pingpong -m msg -p tcp -d 5"
        "-Q -t pingpong -m msg -p tcp -d 5 -a 127.0.0.1:32500")

add_test(NAME flowbench/throughput_tcp_msg COMMAND ${FLOWBENCH_RUN} ${FLOWBENCH_BIN} 
        "-Q -t throughput -m msg -p tcp -d 5"
        "-Q -t throughput -m msg -p tcp -d 5 -a 127.0.0.1:32500")

add_test(NAME flowbench/throughput_tcp_stream COMMAND ${FLOWBENCH_RUN} ${FLOWBENCH_BIN} 
        "-Q -t throughput -m stream -p tcp -d 5"
        "-Q -t throughput -m stream -p tcp -d 5 -a 127.0.0.1:32500")

add_test(NAME flowbench/throughput_udp_msg COMMAND ${FLOWBENCH_RUN} ${FLOWBENCH_BIN} 
        "-Q -t throughput -m msg -p udp -d 5 -s 4k -l 127.0.0.1:32500"
        "-Q -t throughput -m msg -p udp -d 5 -s 4k -l 127.0.0.1:32501 -a 127.0.0.1:32500")

#add_test(NAME flowbench/throughput_io_uring_tcp_stream COMMAND ${FLOWBENCH_RUN} ${FLOWBENCH_BIN} 
#        "-Q -t throughput -m stream -p io_uring_tcp -d 5"
#        "-Q -t throughput -m stream -p io_uring_tcp -d 5 -a 127.0.0.1:32500")