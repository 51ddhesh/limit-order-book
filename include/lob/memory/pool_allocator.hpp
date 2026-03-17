// Copyright 2026 51ddhesh
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/*
    lob::memory::pool_allocator
    Fixed size object pool
*/

/*
    Preallocates storage for N objects of type T at construction time.
*/


#pragma once


#include <cassert>
#include <type_traits>
#include <utility>

namespace lob {

template <typename T>
class PoolAllocator {
    static_assert(std::is_trivially_destructible_v<T>);

private:
    std::byte* storage_; // contiguous block for `capacity_` objects
    T** free_stack_; // LIFO stack of pointers to free slots
    std::size_t capacity_; // total number of slots
    std::size_t free_top_; // number of entries currently in the free_stack_    

}; // class lob::PoolAllocator

} // namespace lob
