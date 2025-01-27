include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(flowbench flowbench.c ui.c framework_evpl.c)

target_link_libraries(flowbench evpl ncurses pthread)

set_target_properties(flowbench PROPERTIES
    INSTALL_RPATH "/usr/local/lib"
    BUILD_WITH_INSTALL_RPATH FALSE)

set(FLOWBENCH_BIN ${CMAKE_CURRENT_BINARY_DIR}/flowbench)

install(TARGETS flowbench DESTINATION bin)

add_subdirectory(tests)
