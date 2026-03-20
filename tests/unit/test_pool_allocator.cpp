// Copyright 2026 51ddhesh
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/*
    lob::tests::test_pool_allocator
*/

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "lob/memory/pool_allocator.hpp"

namespace lob::test {

/* 
* ┌────────────┐
* │ Test Types │
* └────────────┘ 
*/

struct Widget {
    int x;
    int y;

    bool operator==(const Widget&) const = default;
};

static_assert(std::is_trivially_destructible_v<Widget>);
static_assert(std::is_trivially_copyable_v<Widget>);

struct alignas(64) CacheAligned {
    std::uint64_t data;
};

static_assert(std::is_trivially_destructible_v<CacheAligned>);
static_assert(alignof(CacheAligned) == 64);

// a realistic large object 
struct FatObject {
    std::uint64_t fields[12];
};

static_assert(std::is_trivially_destructible_v<FatObject>);
static_assert(sizeof(FatObject) == 96);


/* 
* ┌─────────────────────────────────────┐
* │ 1. Test Construction and Properties │
* └─────────────────────────────────────┘ 
*/

TEST(PoolAllocator, ConstructionWithCapacity) {
    PoolAllocator<Widget> pool(100);

    EXPECT_EQ(pool.capacity(), 100u);
    EXPECT_EQ(pool.used(), 0u);
    EXPECT_EQ(pool.available(), 100u);
    EXPECT_FALSE(pool.full());
}

TEST(PoolAllocator, ZeroCapacityIsValidAndImmdiatelyFull) {
    PoolAllocator<Widget> pool(0);
    
    EXPECT_EQ(pool.capacity(), 0u);
    EXPECT_EQ(pool.used(), 0u);
    EXPECT_EQ(pool.available(), 0u);
    EXPECT_TRUE(pool.full());
    EXPECT_EQ(pool.construct(), nullptr);
}

TEST(PoolAllocator, CapacityOfOne) {
    PoolAllocator<Widget> pool(1);
    
    EXPECT_EQ(pool.capacity(), 1u);
    EXPECT_FALSE(pool.full());

    auto* p = pool.construct(Widget{7, 8});
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(pool.full());
    EXPECT_EQ(pool.construct(), nullptr);

    pool.destroy(p);
    EXPECT_FALSE(pool.full());

    auto* q = pool.construct(Widget{9, 10});
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q -> x, 9);
    EXPECT_EQ(q -> y, 10);
}

/* 
* ┌───────────────────────────┐
* │ 2. Test Basic Allocations │
* └───────────────────────────┘ 
*/

TEST(PoolAllocator, ConstructReturnsNonNull) {
    PoolAllocator<Widget> pool(10);

    auto* p = pool.construct();
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(pool.used(), 1u);
}

TEST(PoolAllocator, ConsecutiveAllocationsReturnDistinctPointers) {
    PoolAllocator<Widget> pool(10);
    auto* a = pool.construct(Widget{1, 1});
    auto* b = pool.construct(Widget{1, 1});
    auto* c = pool.construct(Widget{1, 1});

    EXPECT_NE(a, b);
    EXPECT_NE(c, b);
    EXPECT_NE(a, c);
    EXPECT_EQ(pool.used(), 3u);
}

TEST(PoolAllocator, SequentialAllocationsAreContiguous) {
    // First time allocations from a fresh pool should be sequential in memory.
    // This is good for cache pre-fetching when building the book
    PoolAllocator<Widget> pool(4);

    auto* a = pool.construct();
    auto* b = pool.construct();
    auto* c = pool.construct();

    auto addr_a = reinterpret_cast<std::uintptr_t>(a); 
    auto addr_b = reinterpret_cast<std::uintptr_t>(b); 
    auto addr_c = reinterpret_cast<std::uintptr_t>(c);
    
    EXPECT_EQ(addr_b - addr_a, sizeof(Widget));
    EXPECT_EQ(addr_c - addr_b, sizeof(Widget));
}

TEST(PoolAllocator, AlignmentRespectedForDefaultTypes) {
    PoolAllocator<Widget> pool(10);

    for (int i = 0; i < 8; i++) {
        auto* p = pool.construct();
        ASSERT_NE(p, nullptr);
        auto addr = reinterpret_cast<std::uintptr_t>(p);
        EXPECT_EQ(addr % alignof(Widget), 0u) << "Slot " << i << " at " << p << " not aligned to " << alignof(Widget);
    }
}

TEST(PoolAllocator, AlignmentRespectedForCacheAligned) {
    PoolAllocator<CacheAligned> pool(4);

    for (int i = 0; i < 4; ++i) {
        auto* p = pool.construct();
        ASSERT_NE(p, nullptr);
        auto addr = reinterpret_cast<std::uintptr_t>(p);
        EXPECT_EQ(addr % 64, 0u) << "Slot " << i << " at " << p << " not 64-byte aligned";
    }
}

TEST(PoolAllocator, UsedCountTracksAllocations) {
    PoolAllocator<Widget> pool(5);

    EXPECT_EQ(pool.used(), 0u);
    pool.construct(); EXPECT_EQ(pool.used(), 1u);
    pool.construct(); EXPECT_EQ(pool.used(), 2u);
    pool.construct(); EXPECT_EQ(pool.used(), 3u);
    pool.construct(); EXPECT_EQ(pool.used(), 4u);
    pool.construct(); EXPECT_EQ(pool.used(), 5u);
}

/* 
* ┌────────────────────────────────┐
* │ 3. Test Deallocation and Reuse │
* └────────────────────────────────┘ 
*/

TEST(PoolAllocator, DestroyFreesSlot) {
    PoolAllocator<Widget> pool(10);

    auto* p = pool.construct(Widget{1, 2});
    EXPECT_EQ(pool.used(), 1u);

    pool.destroy(p);
    EXPECT_EQ(pool.used(), 0u);
    EXPECT_EQ(pool.available(), 10u);
}

TEST(PoolAllocator, DestroyedSlotIsReusable) {
    PoolAllocator<Widget> pool(1);

    auto* first = pool.construct(Widget{1, 2});
    pool.destroy(first);

    auto* second = pool.construct(Widget{3, 4});
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second -> x, 3);
    EXPECT_EQ(second -> y, 4);
    // In a pool of size 1, the same slot must be reused.
    EXPECT_EQ(first, second);
}

TEST(PoolAllocator, DestroyAndReallocMultipleTimes) {
    PoolAllocator<Widget> pool(1);

    for (int i = 0; i < 100; ++i) {
        auto* p = pool.construct(Widget{i, i * 10});
        ASSERT_NE(p, nullptr) << "Allocation failed on iteration " << i;
        EXPECT_EQ(p -> x, i);
        EXPECT_EQ(p -> y, i * 10);
        pool.destroy(p);
    }

    EXPECT_EQ(pool.used(), 0u);
}

/* 
* ┌────────────────────┐
* │ 4. Test Exhaustion │
* └────────────────────┘ 
*/

TEST(PoolAllocator, ExhaustionReturnsNullptr) {
    constexpr std::size_t kCap = 5;
    PoolAllocator<Widget> pool(kCap);

    std::vector<Widget*> ptrs;
    for (std::size_t i = 0; i < kCap; ++i) {
        auto* p = pool.construct();
        ASSERT_NE(p, nullptr) << "Failed at allocation " << i;
        ptrs.push_back(p);
    }

    EXPECT_TRUE(pool.full());
    EXPECT_EQ(pool.construct(), nullptr);
    EXPECT_EQ(pool.construct(), nullptr); // still nullptr, doesn't corrupt state

    // Free one, allocate one - works again
    pool.destroy(ptrs.back());
    ptrs.pop_back();
    EXPECT_FALSE(pool.full());

    auto* recovered = pool.construct(Widget{99, 99});
    ASSERT_NE(recovered, nullptr);
    EXPECT_EQ(recovered -> x, 99);
}

TEST(PoolAllocator, AllocateExactlyCapacity) {
    constexpr std::size_t kCap = 100;
    PoolAllocator<Widget> pool(kCap);

    std::unordered_set<Widget*> unique_ptrs;
    for (std::size_t i = 0; i < kCap; ++i) {
        auto* p = pool.construct(Widget{static_cast<int>(i), 0});
        ASSERT_NE(p, nullptr);
        unique_ptrs.insert(p);
    }

    // Every pointer must be unique: no aliasing of live objects.
    EXPECT_EQ(unique_ptrs.size(), kCap);
    EXPECT_TRUE(pool.full());
    EXPECT_EQ(pool.available(), 0u);
}

/* 
* ┌────────────────────┐
* │ 5. Test Full Cycle │
* └────────────────────┘ 
*/

TEST(PoolAllocator, FullCycleAllocFreeRealloc) {
    constexpr std::size_t kCap = 50;
    PoolAllocator<Widget> pool(kCap);

    // Step 1: fill the pool
    std::vector<Widget*> ptrs;
    ptrs.reserve(kCap);
    for (std::size_t i = 0; i < kCap; ++i) {
        ptrs.push_back(pool.construct(Widget{static_cast<int>(i), 0}));
    }
    EXPECT_TRUE(pool.full());

    // Step 2: free everything
    for (auto* p : ptrs) {
        pool.destroy(p);
    }
    ptrs.clear();
    EXPECT_EQ(pool.used(), 0u);
    EXPECT_EQ(pool.available(), kCap);

    // Step 3: reallocate everything, must succeed
    for (std::size_t i = 0; i < kCap; ++i) {
        auto* p = pool.construct(Widget{static_cast<int>(i + 100), 0});
        ASSERT_NE(p, nullptr) << "Reallocation failed at " << i;
        EXPECT_EQ(p->x, static_cast<int>(i + 100));
        ptrs.push_back(p);
    }
    EXPECT_TRUE(pool.full());

    // All new pointers are unique
    std::unordered_set<Widget*> unique(ptrs.begin(), ptrs.end());
    EXPECT_EQ(unique.size(), kCap);
}

/* 
* ┌─────────────────────────────┐
* │ 6. Test Interleaved Pattern │
* └─────────────────────────────┘ 
*/

TEST(PoolAllocator, InterleavedAllocDealloc) {
    constexpr std::size_t kCap = 10;
    PoolAllocator<Widget> pool(kCap);

    std::vector<Widget*> live;

    std::function<void(std::size_t)> check = [&](std::size_t expected_used) {
        EXPECT_EQ(pool.used(), expected_used);
        EXPECT_EQ(pool.available(), kCap - expected_used);
        EXPECT_EQ(pool.used() + pool.available(), pool.capacity());
        EXPECT_EQ(live.size(), expected_used);

        std::unordered_set<Widget*> unique(live.begin(), live.end());
        EXPECT_EQ(unique.size(), live.size());
    };

    // Allocate 5 objects
    for (int i = 0; i < 5; i++) {
        live.push_back(pool.construct(Widget{i, 0}));
    }

    check(5);

    // Free 2 from the back
    pool.destroy(live.back());
    live.pop_back();
    pool.destroy(live.back());
    live.pop_back();

    check(3);

    // Allocate 3

    for (int i = 0; i < 3; i++) {
        live.push_back(pool.construct(Widget{i + 10, 0}));
    }

    check(6);

    // free 4
    for (int i = 0; i < 4; i++) {
        pool.destroy(live.back());
        live.pop_back();
    }

    check(2);

    // allocate 8
    for (int i = 0; i < 8; i++) {
        live.push_back(pool.construct(Widget{i + 20, 0}));
    }

    check(10);

    EXPECT_TRUE(pool.full());

    // Free all
    while (!live.empty()) {
        pool.destroy(live.back());
        live.pop_back();
    }

    check(0);
}

/* 
* ┌────────────────────────────┐
* │ 7. Test the owns() utility │
* └────────────────────────────┘ 
*/

TEST(PoolAllocator, OwnsReturnsTrueForPoolPointers) {
    PoolAllocator<Widget> pool(4);

    auto* a = pool.construct(Widget{1, 2});
    auto* b = pool.construct(Widget{3, 4});

    EXPECT_TRUE(pool.owns(a));
    EXPECT_TRUE(pool.owns(b));
    
    // !! owns() checks the address range, not whether the slot is currently allocated
    pool.destroy(a);
    EXPECT_TRUE(pool.owns(a));
}

TEST(PoolAllocator, OwnsReturnsFalseForForeignPointers) {
    PoolAllocator<Widget> pool(4);

    Widget stack_widget{10, 20};
    auto* heap_widget = new Widget{30, 40};

    EXPECT_FALSE(pool.owns(nullptr));
    EXPECT_FALSE(pool.owns(&stack_widget));
    EXPECT_FALSE(pool.owns(heap_widget));

    delete heap_widget;
}

TEST(PoolAllocator, OwnsReturnsFalseForMisalignedPointer) {
    PoolAllocator<Widget> pool(4);

    auto* p = pool.construct();
    // Offset by 1 byte — still within the storage block but not a valid slot
    auto* misaligned = reinterpret_cast<Widget*>(
        reinterpret_cast<std::byte*>(p) + 1);

    EXPECT_FALSE(pool.owns(misaligned));
    pool.destroy(p);
}

TEST(PoolAllocator, OwnsReturnsFalseForZeroCapacityPool) {
    PoolAllocator<Widget> pool(0);
    Widget w{1, 2};
    EXPECT_FALSE(pool.owns(&w));
    EXPECT_FALSE(pool.owns(nullptr));
}

/* 
* ┌─────────────────────────────┐
* │ 8. Test the reset() utility │
* └─────────────────────────────┘ 
*/

TEST(PoolAllocator, ResetRestoresFullCapacity) {
    PoolAllocator<Widget> pool(10);

    for (int i = 0; i < 7; i++) {
        pool.construct(Widget{i, 0});
    }
    EXPECT_EQ(pool.used(), 7u);

    pool.reset();
    EXPECT_EQ(pool.used(), 0u);
    EXPECT_EQ(pool.available(), 10u);
    EXPECT_FALSE(pool.full());
}

TEST(PoolAllocator, ResetAllowsFullReallocation) {
    constexpr std::size_t kCap = 10;
    PoolAllocator<Widget> pool(kCap);

    // Fill, reset, refill: must succeed.
    for (std::size_t i = 0; i < kCap; i++) pool.construct();
    EXPECT_TRUE(pool.full());

    pool.reset();

    for (std::size_t i = 0; i < kCap; ++i) {
        auto* p = pool.construct(Widget{static_cast<int>(i), 42});
        ASSERT_NE(p, nullptr) << "Post-reset allocation failed at " << i;
        EXPECT_EQ(p -> x, static_cast<int>(i));
        EXPECT_EQ(p -> y, 42);
    }
    EXPECT_TRUE(pool.full());
}

TEST(PoolAllocator, ResetOnEmptyPoolIsNoOp) {
    PoolAllocator<Widget> pool(5);
    pool.reset();

    EXPECT_EQ(pool.used(), 0u);
    EXPECT_EQ(pool.available(), 5u);

    auto* p = pool.construct(Widget{1, 1});
    EXPECT_NE(p, nullptr);
}

/* 
* ┌─────────────────────────────┐
* │ 9. Test Object Construction │
* └─────────────────────────────┘ 
*/

TEST(PoolAllocator, ConstructWithNoArgumentValueInitialization) {
    PoolAllocator<Widget> pool(4);
    
    auto* p = pool.construct();
    ASSERT_NE(p, nullptr);

    EXPECT_EQ(p -> x, 0);
    EXPECT_EQ(p -> y, 0);
}

TEST(PoolAllocator, ConstructWithAggregateInit) {
    PoolAllocator<Widget> pool(4);

    auto* p = pool.construct(Widget{42, 99});
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p -> x, 42);
    EXPECT_EQ(p -> y, 99);
}

TEST(PoolAllocator, ConstructedObjectSurvivesUntilDestroy) {
    PoolAllocator<Widget> pool(4);

    auto* a = pool.construct(Widget{1, 10});
    auto* b = pool.construct(Widget{2, 20});
    auto* c = pool.construct(Widget{3, 30});

    // All three are live simultaneously — no clobbering.
    EXPECT_EQ(a -> x, 1); EXPECT_EQ(a->y, 10);
    EXPECT_EQ(b -> x, 2); EXPECT_EQ(b->y, 20);
    EXPECT_EQ(c -> x, 3); EXPECT_EQ(c->y, 30);

    pool.destroy(b);

    // a and c still intact after destroying b.
    EXPECT_EQ(a -> x, 1); EXPECT_EQ(a->y, 10);
    EXPECT_EQ(c -> x, 3); EXPECT_EQ(c->y, 30);
}

TEST(PoolAllocator, ConstructOverwritesPreviousData) {
    PoolAllocator<Widget> pool(1);

    auto* p = pool.construct(Widget{111, 222});
    EXPECT_EQ(p -> x, 111);
    pool.destroy(p);

    auto* q = pool.construct(Widget{333, 444});
    EXPECT_EQ(q -> x, 333);
    EXPECT_EQ(q -> y, 444);
}

/* 
* ┌─────────────────────┐
* │ 10. Test Fat Object │
* └─────────────────────┘ 
*/

TEST(PoolAllocator, WorksWithLargeObjects) {
    PoolAllocator<FatObject> pool(100);

    std::vector<FatObject*> ptrs;
    for (std::size_t i = 0; i < 100; ++i) {
        auto* p = pool.construct();
        ASSERT_NE(p, nullptr);
        p -> fields[0] = i;
        p -> fields[11] = i * 100;
        ptrs.push_back(p);
    }
    EXPECT_TRUE(pool.full());

    // Verify no clobbering
    for (std::size_t i = 0; i < 100; ++i) {
        EXPECT_EQ(ptrs[i] -> fields[0], i);
        EXPECT_EQ(ptrs[i] -> fields[11], i * 100);
    }

    // Free every other one, reallocate
    for (std::size_t i = 0; i < 100; i += 2) {
        pool.destroy(ptrs[i]);
        ptrs[i] = nullptr;
    }
    EXPECT_EQ(pool.used(), 50u);

    for (std::size_t i = 0; i < 100; i += 2) {
        ptrs[i] = pool.construct();
        ASSERT_NE(ptrs[i], nullptr);
        ptrs[i] -> fields[0] = i + 1000;
    }
    EXPECT_TRUE(pool.full());

    // Verify old ones intact, new ones correct
    for (std::size_t i = 0; i < 100; ++i) {
        if (i % 2 == 0) {
            EXPECT_EQ(ptrs[i] -> fields[0], i + 1000);
        } else {
            EXPECT_EQ(ptrs[i] -> fields[0], i);
            EXPECT_EQ(ptrs[i] -> fields[11], i * 100);
        }
    }
}

/* 
* ┌──────────────────────┐
* │ 11. Staic Guarantees │
* └──────────────────────┘ 
*/

TEST(PoolAllocator, NonCopyableNonMovable) {
    static_assert(!std::is_copy_constructible_v<PoolAllocator<Widget>>);
    static_assert(!std::is_copy_assignable_v<PoolAllocator<Widget>>);
    static_assert(!std::is_move_constructible_v<PoolAllocator<Widget>>);
    static_assert(!std::is_move_assignable_v<PoolAllocator<Widget>>);
}

} // namespace lob::test
