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

set_source_files_properties(
    ${XDR_C} PROPERTIES COMPILE_OPTIONS "-Wno-unused;-Wno-format-truncation"
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(flowbench flowbench.c ui.c framework_evpl.c framework_evpl_rpc2.c ${XDR_C} ${XDR_H})

target_link_libraries(flowbench evpl evpl_rpc2 ncurses pthread)

set_target_properties(flowbench PROPERTIES
    INSTALL_RPATH "/usr/local/lib"
    BUILD_WITH_INSTALL_RPATH FALSE)

set(FLOWBENCH_BIN ${CMAKE_CURRENT_BINARY_DIR}/flowbench)

install(TARGETS flowbench DESTINATION bin)

add_subdirectory(tests)
