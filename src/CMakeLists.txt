include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(flowbench flowbench.c framework_evpl.c)

target_link_libraries(flowbench evpl)
