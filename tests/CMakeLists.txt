# Copyright 2026 51ddhesh
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at
# http://www.boost.org/LICENSE_1_0.txt)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

function(lob_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE
        lob lob_warnings GTest::gtest_main GTest::gmock)
    gtest_discover_tests(${name})
endfunction()

lob_add_test(test_types unit/test_types.cpp)
lob_add_test(test_pool_allocator unit/test_pool_allocator.cpp)
lob_add_test(test_price_level unit/test_price_level.cpp)
lob_add_test(test_events unit/test_events.cpp)
