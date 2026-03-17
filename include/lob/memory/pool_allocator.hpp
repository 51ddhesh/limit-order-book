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

/*
    Internal Structure

    - storage_
    ┌────────┬────────┬────────┬─────┬──────────────┐
    │ Slot 0 │ Slot 1 │ Slot 2 │ ... │ Slot (N - 1) │
    └────────┴────────┴────────┴─────┴──────────────┘ 
        Contiguous and aligned


    - free_stack_
    ┌───────────────┬───────────────┬───────────────┬─────┬─────────┬─────────┐
    │ &Slot (N - 1) │ &Slot (N - 2) │ &Slot (N - 3) │ ... │ &Slot 1 │ &Slot 0 │
    └───────────────┴───────────────┴───────────────┴─────┴─────────┴─────────┘
        LIFO stack of available slots
        
    - free_top_
        index of next avalable entry in `free_stack_`
*/

#pragma once


#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#ifndef NODEBUG
    #include <vector>
#endif

namespace lob {

template <typename T>
class PoolAllocator {
    static_assert(std::is_trivially_destructible_v<T>);

private:

    // * ─── Data Members ────────────────────────────────────────────────────── *
    
    std::byte* storage_; // contiguous block for `capacity_` objects
    T** free_stack_; // LIFO stack of pointers to free slots
    std::size_t capacity_; // total number of slots
    std::size_t free_top_; // number of entries currently in the free_stack_    
    
#ifndef NODEBUG
    std::vector<bool> allocated_; // per slot `is_allocated` flag 
#endif
    
    /* 
    * ┌──────────────────┐
    * │ Internal Helpers │
    * └──────────────────┘ 
    */
   
    // * ─── Typed Pointer ───────────────────────────────────────────────────── *
    /// Get a typed pointer to the i'th slot (no object may exist there yet)
    [[nodiscard]] T* slot_at(std::size_t index) noexcept {
        return reinterpret_cast<T*>(storage_ + index * sizeof(T));
    }
    
    // * ─── Slot Index ──────────────────────────────────────────────────────── *
    /// From a given pointer, compute the slot index
    [[nodiscard]] std::size_t index_of(const T* ptr) const noexcept {
        const std::size_t offset = static_cast<std::size_t>(
            reinterpret_cast<const std::byte*>(ptr) - storage_
        );
        
        return offset / sizeof(T);
    }
    
    // * ─── Fill the free stack ─────────────────────────────────────────────── *
    void init_free_stack() noexcept {
        if (capacity_ == 0) return;

        for (std::size_t i = 0; i < capacity_; i++) {
            free_stack_[i] = slot_at(capacity_ - i - 1);
        }
    }

public:

    PoolAllocator() {}

}; // class lob::PoolAllocator

} // namespace lob
