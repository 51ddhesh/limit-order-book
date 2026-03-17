# Copyright 2026 51ddhesh
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at
# http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.20)
project(limit-order-book VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(lob_warnings INTERFACE)
target_compile_options(lob_warnings INTERFACE 
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion
        -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Woverloaded-virtual -Wsign-conversion -Wnull-dereference
        -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough
    >
)

add_library(lob INTERFACE)

target_include_directories(lob INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(lob INTERFACE lob_warnings)

set(CMAKE_CXX_FLAGS_ASAN
    "-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
    CACHE STRING "Flags used by the CXX compiler during ASAN builds." FORCE)

option(LOB_BUILD_TESTS      "Build unit tests"  ON)
option(LOB_BUILD_BENCHMARKS "Build benchmarks"  OFF)

if(LOB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LOB_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()