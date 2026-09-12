# SPDX-FileCopyrightText: 2025 Ben Jarvis
#
# SPDX-License-Identifier: LGPL-2.1-only

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(XDR_C ${CMAKE_CURRENT_BINARY_DIR}/flowbench_xdr.c)
set(XDR_H ${CMAKE_CURRENT_BINARY_DIR}/flowbench_xdr.h)
set(XDR_X ${CMAKE_CURRENT_SOURCE_DIR}/flowbench.x)

# Generate both server (-r) and client (-c) code
add_custom_command(
    OUTPUT ${XDR_C} ${XDR_H}
    COMMAND ${XDRZCC} -r ${XDR_X} ${XDR_C} ${XDR_H}
    DEPENDS ${XDR_X} ${XDRZCC}
    COMMENT "Compiling ${XDR_X} with server and client support"
)

set(XDR_WARN_FLAGS -Wno-unused)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    list(APPEND XDR_WARN_FLAGS -Wno-format-truncation)
endif()

set_source_files_properties(
    ${XDR_C} PROPERTIES COMPILE_OPTIONS "${XDR_WARN_FLAGS}"
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(flowbench flowbench.c timing.c ui.c framework_evpl.c framework_evpl_rpc2.c ${XDR_C} ${XDR_H})

find_package(Curses REQUIRED)
find_package(Threads REQUIRED)
find_path(UTHASH_INCLUDE_DIR utlist.h HINTS /opt/homebrew/include /usr/local/include REQUIRED)
target_include_directories(flowbench PRIVATE ${CURSES_INCLUDE_DIRS} ${UTHASH_INCLUDE_DIR})
# libevpl exports its stopwatch headers and iovec compile definitions.
target_link_libraries(flowbench evpl evpl_rpc2 ${CURSES_LIBRARIES} Threads::Threads)

set_target_properties(flowbench PROPERTIES
    INSTALL_RPATH "/usr/local/lib"
    BUILD_WITH_INSTALL_RPATH FALSE)

set(FLOWBENCH_BIN ${CMAKE_CURRENT_BINARY_DIR}/flowbench)

install(TARGETS flowbench DESTINATION bin)

add_subdirectory(tests)
