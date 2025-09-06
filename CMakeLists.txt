# SPDX-FileCopyrightText: 2024 - 2025 Ben Jarvis
#
# SPDX-License-Identifier: LGPL-2.1-only

cmake_minimum_required(VERSION 3.22)

project(flowbench C)

enable_testing()

if (NOT FLOWBENCH_VERSION)
    set(FLOWBENCH_VERSION "0.0.1")
endif()

add_definitions(-DFLOWBENCH_VERSION="${FLOWBENCH_VERSION}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext/libevpl/ext/prometheus-c)

message(STATUS "Building ${CMAKE_BUILD_TYPE} for ${CMAKE_SYSTEM_NAME}")


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Enabling AddressSanitizer")
    add_definitions(-fsanitize=address -fno-omit-frame-pointer
                    -fno-optimize-sibling-calls -fstack-protector-all)
    add_link_options(-fsanitize=address)
endif()    

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-O3)
endif()

add_definitions(-g -Wall -Werror -Wno-unused-function)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext/libevpl/include)
include_directories(3rdparty)

add_subdirectory(ext)
add_subdirectory(src)

execute_process(COMMAND ln -sf ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}/compile_commands.json)
