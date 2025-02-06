# SPDX-FileCopyrightText: 2024 Ben Jarvis
#
# SPDX-License-Identifier: LGPL

cmake_minimum_required(VERSION 3.22)

project(flowbench C)

enable_testing()

if (NOT FLOWBENCH_VERSION)
    set(FLOWBENCH_VERSION "0.0.1")
endif()

add_definitions(-DFLOWBENCH_VERSION="${FLOWBENCH_VERSION}")

message(STATUS "Building ${CMAKE_BUILD_TYPE} for ${CMAKE_SYSTEM_NAME}")


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Enabling AddressSanitizer")
    add_definitions(-fsanitize=address -fno-omit-frame-pointer
                    -fno-optimize-sibling-calls -fstack-protector-all)
    add_link_options(-fsanitize=address)
endif()    

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-O3 -march=native)
endif()

add_definitions(-g -Wall -Werror -Wno-unused-function)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext/libevpl/include)
include_directories(3rdparty)

add_subdirectory(ext)
add_subdirectory(src)
