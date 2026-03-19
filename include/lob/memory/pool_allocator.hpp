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
    
    // * ─── Constructor (on the cold path - could allocate from the heap) ───── *
    explicit PoolAllocator(std::size_t capacity) 
    : storage_(nullptr),
    free_stack_(nullptr),
    capacity_(capacity),
    free_top_(capacity)
    #ifndef NODEBUG
    , allocated_(capacity, false)
    #endif
    {
        if (capacity_ == 0) return;
        
        storage_ = static_cast<std::byte*>(
            ::operator new(capacity_ * sizeof(T), std::align_val_t(alignof(T)))
        );
        
        try {
            free_stack_ = new T*[capacity_];
        } catch (...) {
            ::operator delete(storage_, std::align_val_t(alignof(T)));
            throw;
        }
        
        init_free_stack();
    }
    
    ~PoolAllocator() {
        delete[] free_stack_;
        ::operator delete(storage_, std::align_val_t(alignof(T)));
    }
    
    // * ─── Non Copyable, Non Movable ───────────────────────────────────────── *
    // Moving would invalidate all outstanding T* if anyone held a reference
    // to the allocator itself and there's no use case for copying a pool.
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&&) = delete;
    PoolAllocator& operator=(PoolAllocator&&) = delete;
    
    /* 
    * ┌─────────────┐
    * │ Diagnostics │
    * └─────────────┘ 
    */

    // * ─── Is the pointer from the pool? ───────────────────────────────────── *
    
    // Returns true if the pointer points to a correctly aligned slot within this pool's storage.
    [[nodiscard]] bool owns(const T* ptr) const noexcept {
        if (ptr == nullptr || storage_ == nullptr) return false;

        const auto* byte_ptr = reinterpret_cast<const std::byte*> (ptr);
        const auto* begin = storage_;
        const auto* end = storage_ + capacity_ * sizeof(T);
        
        if (byte_ptr < begin || byte_ptr >= end) return false;
        
        const auto offset = static_cast<std::size_t>(byte_ptr - begin);
        
        // must point to the start of the slot
        // not into the middle of one
        return (offset % sizeof(T) == 0);
    }
    
    /* 
    * ┌────────────────────────────┐
    * │ Operations on the hot path │
    * └────────────────────────────┘ 
    */

    // * ─── Construct a T ───────────────────────────────────────────────────── *
    
    // Construct a T in a free slot 
    // Returns nullptr if the pool is exhausted
    // Zero interactions on the heap
    template <typename... Args>
    [[nodiscard]] T* construct(Args&&... args) noexcept {
        if (free_top_ == 0) [[unlikely]] {
            return nullptr;
        }
        
        T* slot = free_stack_[--free_top_];
        
        #ifndef NODEBUG
            const std::size_t idx = index_of(slot);
            assert(!allocated_[idx] && "[PoolAllocator::construct]: slot already in use (corrupted free stack)");
            allocated_[idx] = true;
        #endif
        
        return ::new (static_cast<void*>(slot)) T{std::forward<Args>(args)...};
    }
    
    // * ─── Return an object to the pool ────────────────────────────────────── *
    
    // Returns an object to the pool.
    // The pointer MUST have come from this pool.
    // !! T is trivially destructible 
    // !! DOES NOT CALL ~T
    void destroy(T* ptr) noexcept {
        assert(ptr != nullptr && "[PoolAllocator::destroy]: null pointer.");
        assert(owns(ptr) && "[PoolAllocator::destroy]: pointer not from this pool.");
        
        #ifndef NODEBUG
            const std::size_t idx = index_of(ptr);
            // * WHY??
            assert(allocated_[idx] && "[PoolAllocator::destroy]: double free detected");
            allocated_[idx] = false;
        #endif
        
        free_stack_[free_top_++] = ptr;
    }
    
    /* 
    * ┌──────────────────┐
    * │ Capacity Queries │
    * └──────────────────┘ 
    */
   
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t used() const noexcept { return capacity_ - free_top_; }
    [[nodiscard]] std::size_t available() const noexcept { return free_top_; }
    [[nodiscard]] bool full() const noexcept { return free_top_ == 0; }
   
    
    /* 
    * ┌───────┐
    * │ Reset │
    * └───────┘ 
    */

    // * ─── Reset (no destructors called) ───────────────────────────────────── *

    void reset() noexcept {
        free_top_ = capacity_;
        init_free_stack();

        #ifndef NODEBUG
            std::fill(allocated_.begin(), allocated_.end(), false);
        #endif
    }

}; // class lob::PoolAllocator

} // namespace lob
