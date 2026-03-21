// Copyright 2026 51ddhesh
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/*
    lob::tests::test_price_level
*/

#include <gtest/gtest.h>
#include <vector>

#include "lob/core/price_level.hpp" 
#include "lob/memory/pool_allocator.hpp"

namespace lob::test {

/* 
* ┌──────────────┐
* │ Test Fixture │
* └──────────────┘ 
*/

class PriceLevelTest : public ::testing::Test {
protected:
    static constexpr std::size_t kPoolCapacity = 64;
    PoolAllocator<Order> pool_{kPoolCapacity};

    // Create an order in the pool with sensible defaults.
    // Uses OrderID as the timestamp so FIFO ordering is trivially verifiable.
    Order* make_order(OrderID id, Quantity qty, Price price = Price{1000}) {
        auto* o = pool_.construct(Order{
            .prev = nullptr,
            .next = nullptr,
            .id = id,
            .price = price,
            .remaining = qty,
            .timestamp = Timestamp{id.raw()},
            .level = nullptr,
            .side = Side::Buy,
            .tif = TimeInForce::GTC,
            .type = OrderType::Limit,
            .original_qty = qty,
        });

        assert(o != nullptr && "test pool exhausted");
        return o;
    }

    // Create an empty PriceLevel at a given price
    PriceLevel make_level(Price price = Price{1000}) {
        return PriceLevel{
            .head = nullptr,
            .tail = nullptr,
            .total_qty = kZeroQuantity,
            .price = price,
            .order_count = 0,
        };
    }
};

/* 
* ┌─────────────────────┐
* │ 1. Test Empty Level │
* └─────────────────────┘ 
*/

TEST_F(PriceLevelTest, NewLevelIsEmpty) {
    auto level = make_level();
    EXPECT_TRUE(level.is_empty());
    EXPECT_EQ(level.order_count, 0u);
    EXPECT_EQ(level.total_qty, kZeroQuantity);
    EXPECT_EQ(level.head, nullptr);
    EXPECT_EQ(level.tail, nullptr);
    EXPECT_EQ(level.front(), nullptr);
    EXPECT_EQ(level.price, Price{1000});

    level.assert_invariants();
}

TEST_F(PriceLevelTest, PopFrontOnEmptyReturnsNull) {
    auto level = make_level();

    EXPECT_EQ(level.pop_front(), nullptr);
    EXPECT_TRUE(level.is_empty());

    level.assert_invariants();
}   

/* 
* ┌────────────────┐
* │ 2. Test Append │
* └────────────────┘ 
*/

TEST_F(PriceLevelTest, AppendSingleOrder) {
    auto level = make_level();
    auto* a = make_order(OrderID{1}, Quantity{100});

    level.append(a);

    EXPECT_FALSE(level.is_empty());
    EXPECT_EQ(level.order_count, 1u);
    EXPECT_EQ(level.total_qty, Quantity{100});
    EXPECT_EQ(level.head, a);
    EXPECT_EQ(level.tail, a);
    EXPECT_EQ(level.front(), a);

    // Links
    EXPECT_EQ(a -> prev, nullptr);
    EXPECT_EQ(a -> next, nullptr);

    // back pointer
    EXPECT_EQ(a -> level, &level);

    level.assert_invariants();
}

TEST_F(PriceLevelTest, AppendMultipleINFIFOOrder) {
    auto level = make_level();
    auto* a = make_order(OrderID{1}, Quantity{100});
    auto* b = make_order(OrderID{2}, Quantity{200});
    auto* c = make_order(OrderID{3}, Quantity{300});

    level.append(a);
    level.append(b);
    level.append(c);

    // Counts
    EXPECT_EQ(level.order_count, 3u);
    EXPECT_EQ(level.total_qty, Quantity{600});

    // Head/tail
    EXPECT_EQ(level.head, a);
    EXPECT_EQ(level.tail, c);
    EXPECT_EQ(level.front(), a);

    // Forward links: A -> B -> C -> null
    EXPECT_EQ(a -> next, b);
    EXPECT_EQ(b -> next, c);
    EXPECT_EQ(c -> next, nullptr);

    // Backward links: null <- A <- B <- C
    EXPECT_EQ(a -> prev, nullptr);
    EXPECT_EQ(b -> prev, a);
    EXPECT_EQ(c -> prev, b);

    // Back-pointers
    EXPECT_EQ(a -> level, &level);
    EXPECT_EQ(b -> level, &level);
    EXPECT_EQ(c -> level, &level);

    level.assert_invariants();
}

TEST_F(PriceLevelTest, AppendUpdatesQuantityCumulatively) {
    auto level = make_level();
    
    level.append(make_order(OrderID{1}, Quantity{100}));
    EXPECT_EQ(level.total_qty, Quantity{100});
    
    level.append(make_order(OrderID{2}, Quantity{200}));
    EXPECT_EQ(level.total_qty, Quantity{300});
    
    level.append(make_order(OrderID{2}, Quantity{50}));
    EXPECT_EQ(level.total_qty, Quantity{350});

    level.assert_invariants();
}

/* 
* ┌─────────────────────────────────────────┐
* │ 3. Test Remove from different positions │
* └─────────────────────────────────────────┘ 
*/

TEST_F(PriceLevelTest, RemoveHead) {
    auto level = make_level();
    auto* a = make_order(OrderID{1}, Quantity{100});
    auto* b = make_order(OrderID{2}, Quantity{200});
    auto* c = make_order(OrderID{3}, Quantity{300});

    level.append(a);
    level.append(b);
    level.append(c);

    level.remove(a);

    EXPECT_EQ(level.order_count, 2u);
    EXPECT_EQ(level.total_qty, Quantity{500});
    EXPECT_EQ(level.head, b);
    EXPECT_EQ(level.tail, c);
    EXPECT_EQ(b -> prev, nullptr);

    // Removed order is fully unlinked
    EXPECT_EQ(a -> prev, nullptr);
    EXPECT_EQ(a -> next, nullptr);
    EXPECT_EQ(a -> level, nullptr);

    level.assert_invariants();
}

TEST_F(PriceLevelTest, RemoveTail) {
    auto level = make_level();
    auto* a = make_order(OrderID{1}, Quantity{100});
    auto* b = make_order(OrderID{2}, Quantity{200});
    auto* c = make_order(OrderID{3}, Quantity{300});

    level.append(a);
    level.append(b);
    level.append(c);

    level.remove(c);

    EXPECT_EQ(level.order_count, 2u);
    EXPECT_EQ(level.total_qty, Quantity{300});
    EXPECT_EQ(level.head, a);
    EXPECT_EQ(level.tail, b);
    EXPECT_EQ(b -> next, nullptr);

    // Removed order is fully unlinked
    EXPECT_EQ(c -> next, nullptr);
    EXPECT_EQ(c -> prev, nullptr);
    EXPECT_EQ(c -> level, nullptr);

    level.assert_invariants();
}

TEST_F(PriceLevelTest, RemoveMiddleOrder) {
    auto level = make_level();
    auto* a = make_order(OrderID{1}, Quantity{100});
    auto* b = make_order(OrderID{2}, Quantity{200});
    auto* c = make_order(OrderID{3}, Quantity{300});

    level.append(a);
    level.append(b);
    level.append(c);

    level.remove(b);

    EXPECT_EQ(level.order_count, 2u);
    EXPECT_EQ(level.total_qty, Quantity{400});
    EXPECT_EQ(level.head, a);
    EXPECT_EQ(level.tail, c);
    EXPECT_EQ(a -> next, c);
    EXPECT_EQ(c -> prev, a);

    // Removed order is fully unlinked
    EXPECT_EQ(b -> next, nullptr);
    EXPECT_EQ(b -> prev, nullptr);
    EXPECT_EQ(b -> level, nullptr);

    level.assert_invariants();
}

TEST_F(PriceLevelTest, RemoveOnlyOrder) {
    auto level = make_level();
    auto* a = make_order(OrderID{1}, Quantity{10});
    level.append(a);

    level.remove(a);

    EXPECT_TRUE(level.is_empty());
    EXPECT_EQ(level.order_count, 0u);
    EXPECT_EQ(level.total_qty, kZeroQuantity);
    EXPECT_EQ(level.head, nullptr);
    EXPECT_EQ(level.tail, nullptr);
    
    EXPECT_EQ(a -> next, nullptr);
    EXPECT_EQ(a -> prev, nullptr);
    EXPECT_EQ(a -> level, nullptr);

    level.assert_invariants();
}

/* 
* ┌───────────────────┐
* │ 4. Test pop_front │
* └───────────────────┘ 
*/

TEST_F(PriceLevelTest, PopFront_ReturnsInFIFOOrder) {
    auto level = make_level();
    auto* a = make_order(OrderID{1}, Quantity{100});
    auto* b = make_order(OrderID{2}, Quantity{100});
    auto* c = make_order(OrderID{3}, Quantity{100});

    level.append(a);
    level.append(b);
    level.append(c);

    EXPECT_EQ(level.pop_front(), a);
    EXPECT_EQ(level.order_count, 2u);
    EXPECT_EQ(level.head, b);
    level.assert_invariants();

    EXPECT_EQ(level.pop_front(), b);
    EXPECT_EQ(level.order_count, 1u);
    EXPECT_EQ(level.head, c);
    level.assert_invariants();

    EXPECT_EQ(level.pop_front(), c);
    EXPECT_TRUE(level.is_empty());
    level.assert_invariants();

    EXPECT_EQ(level.pop_front(), nullptr);
}

TEST_F(PriceLevelTest, PopFront_UnlinksOrder) {
    auto level = make_level();
    auto* a = make_order(OrderID{1}, Quantity{100});
    level.append(a);

    auto* popped = level.pop_front();

    EXPECT_EQ(popped, a);
    EXPECT_EQ(a -> prev, nullptr);
    EXPECT_EQ(a -> next, nullptr);
    EXPECT_EQ(a -> level, nullptr);
}

/* 
* ┌────────────────────┐
* │ 5. Test reduce_qty │
* └────────────────────┘ 
*/

TEST_F(PriceLevelTest, ReduceQty_Basic) {
    auto level = make_level();
    level.append(make_order(OrderID{1}, Quantity{500}));

    level.reduce_qty(Quantity{200});
    EXPECT_EQ(level.total_qty, Quantity{300});

    level.reduce_qty(Quantity{300});
    EXPECT_EQ(level.total_qty, kZeroQuantity);
}

TEST_F(PriceLevelTest, PartialFillPattern) {
    // Simulates exactly what the matching engine will do on a partial fill:
    //   1. fill_qty = min(order -> remaining, incoming)
    //   2. order -> remaining -= fill_qty
    //   3. level.reduce_qty(fill_qty)
    //   (order stays in the queue)

    auto level = make_level();
    auto* order = make_order(OrderID{1}, Quantity{500});
    level.append(order);

    // Incoming buy wants 200, order has 500 -> partial fill of 200
    Quantity fill{200};
    order -> remaining -= fill;
    level.reduce_qty(fill);

    EXPECT_EQ(order -> remaining, Quantity{300});
    EXPECT_EQ(level.total_qty, Quantity{300});
    EXPECT_EQ(level.order_count, 1u);
    EXPECT_EQ(level.front(), order);  // order still resting

    level.assert_invariants();
}

TEST_F(PriceLevelTest, FullFillAndRemovePattern) {
    // Simulates what the matching engine does on a full fill:
    //   1. fill_qty = order->remaining
    //   2. order -> remaining = 0
    //   3. level.reduce_qty(fill_qty)
    //   4. level.pop_front() — remove the filled order

    auto level = make_level();
    auto* a = make_order(OrderID{1}, Quantity{100});
    auto* b = make_order(OrderID{2}, Quantity{200});
    level.append(a);
    level.append(b);

    // Fully fill a
    Quantity fill = a -> remaining;
    a -> remaining -= fill;
    level.reduce_qty(fill);
    auto* filled = level.pop_front();

    EXPECT_EQ(filled, a);
    EXPECT_EQ(filled -> remaining, kZeroQuantity);
    EXPECT_EQ(level.total_qty, Quantity{200});
    EXPECT_EQ(level.order_count, 1u);
    EXPECT_EQ(level.front(), b);

    level.assert_invariants();
}

TEST_F(PriceLevelTest, MultiFillSweep) {
    // Simulates an aggressive incoming order sweeping through multiple
    // resting orders at this price level.

    auto level = make_level();
    auto* a = make_order(OrderID{1}, Quantity{100});
    auto* b = make_order(OrderID{2}, Quantity{200});
    auto* c = make_order(OrderID{3}, Quantity{300});
    level.append(a);
    level.append(b);
    level.append(c);

    Quantity incoming{450};

    // Fill a fully (100)
    {
        Quantity fill = std::min(incoming, a -> remaining);
        EXPECT_EQ(fill, Quantity{100});
        a -> remaining -= fill;
        level.reduce_qty(fill);
        incoming -= fill;
        level.pop_front();
    }
    EXPECT_EQ(incoming, Quantity{350});
    EXPECT_EQ(level.total_qty, Quantity{500});
    level.assert_invariants();

    // Fill b fully (200)
    {
        Quantity fill = std::min(incoming, b -> remaining);
        EXPECT_EQ(fill, Quantity{200});
        b -> remaining -= fill;
        level.reduce_qty(fill);
        incoming -= fill;
        level.pop_front();
    }
    EXPECT_EQ(incoming, Quantity{150});
    EXPECT_EQ(level.total_qty, Quantity{300});
    level.assert_invariants();

    // Partially fill c (150 of 300)
    {
        Quantity fill = std::min(incoming, c -> remaining);
        EXPECT_EQ(fill, Quantity{150});
        c -> remaining -= fill;
        level.reduce_qty(fill);
        incoming -= fill;
        // c stays in the queue — NOT popped
    }
    EXPECT_EQ(incoming, kZeroQuantity);
    EXPECT_EQ(c -> remaining, Quantity{150});
    EXPECT_EQ(level.total_qty, Quantity{150});
    EXPECT_EQ(level.order_count, 1u);
    EXPECT_EQ(level.front(), c);
    level.assert_invariants();
}

/* 
* ┌─────────────────────────────────────────────────┐
* │ 6. Test multiple removes from various positions │
* └─────────────────────────────────────────────────┘ 
*/

TEST_F(PriceLevelTest, RemoveInArbitraryOrder) {
    auto level = make_level();
    auto* a = make_order(OrderID{1}, Quantity{10});
    auto* b = make_order(OrderID{2}, Quantity{20});
    auto* c = make_order(OrderID{3}, Quantity{30});
    auto* d = make_order(OrderID{4}, Quantity{40});
    auto* e = make_order(OrderID{5}, Quantity{50});
    level.append(a);
    level.append(b);
    level.append(c);
    level.append(d);
    level.append(e);
    level.assert_invariants();

    // Remove middle (c)
    level.remove(c);
    EXPECT_EQ(level.order_count, 4u);
    EXPECT_EQ(level.total_qty, Quantity{120});
    level.assert_invariants();

    // Remove head (a)
    level.remove(a);
    EXPECT_EQ(level.order_count, 3u);
    EXPECT_EQ(level.head, b);
    level.assert_invariants();

    // Remove tail (e)
    level.remove(e);
    EXPECT_EQ(level.order_count, 2u);
    EXPECT_EQ(level.tail, d);
    level.assert_invariants();

    // Remove d (new tail)
    level.remove(d);
    EXPECT_EQ(level.order_count, 1u);
    EXPECT_EQ(level.head, b);
    EXPECT_EQ(level.tail, b);
    level.assert_invariants();

    // Remove b (last remaining)
    level.remove(b);
    EXPECT_TRUE(level.is_empty());
    level.assert_invariants();
}

/* 
* ┌─────────────────────────────┐
* │ 7. Test re-add after remove │
* └─────────────────────────────┘ 
*/

TEST_F(PriceLevelTest, OrderCanBeReaddedAfterRemove) {
    // After remove, order's links are nulled, so it can be appended
    // to the same or a different level.

    auto level_a = make_level(Price{1000});
    auto level_b = make_level(Price{2000});
    auto* order = make_order(OrderID{1}, Quantity{100});

    // Add to level_a
    level_a.append(order);
    EXPECT_EQ(order -> level, &level_a);
    level_a.assert_invariants();

    // Remove from level_a
    level_a.remove(order);
    EXPECT_EQ(order -> level, nullptr);
    EXPECT_TRUE(level_a.is_empty());

    // Re-add to level_b (simulates a modify-price: cancel + re-add)
    order -> remaining = Quantity{100};  // reset qty for re-add
    level_b.append(order);
    EXPECT_EQ(order -> level, &level_b);
    EXPECT_EQ(level_b.order_count, 1u);
    EXPECT_EQ(level_b.total_qty, Quantity{100});
    level_b.assert_invariants();
}

/* 
* ┌──────────────────────────┐
* │ 8. Test Pool Integration │
* └──────────────────────────┘ 
*/

TEST_F(PriceLevelTest, PoolAllocateAddRemoveReturnReuse) {
    // Full lifecycle: allocate from pool -> add to level -> remove ->
    // return to pool -> reallocate -> add to level again.

    PoolAllocator<PriceLevel> level_pool(4);
    auto* level = level_pool.construct(PriceLevel{
        .head = nullptr, .tail = nullptr,
        .total_qty = kZeroQuantity, .price = Price{1000},
        .order_count = 0,
    });
    ASSERT_NE(level, nullptr);

    // Allocate orders, add to level
    auto* o1 = make_order(OrderID{1}, Quantity{100});
    auto* o2 = make_order(OrderID{2}, Quantity{200});
    level -> append(o1);
    level -> append(o2);
    level -> assert_invariants();

    // Remove o1, return to order pool
    level -> remove(o1);
    pool_.destroy(o1);

    // Reallocate from order pool — may get the same memory
    auto* o3 = make_order(OrderID{3}, Quantity{300});
    level -> append(o3);

    EXPECT_EQ(level -> order_count, 2u);
    EXPECT_EQ(level -> total_qty, Quantity{500});
    EXPECT_EQ(level -> front(), o2);  // o2 was first, still at head
    level -> assert_invariants();

    // Clean up: remove remaining orders, destroy level
    level -> remove(o2);
    pool_.destroy(o2);
    level -> remove(o3);
    pool_.destroy(o3);

    EXPECT_TRUE(level -> is_empty());
    level_pool.destroy(level);
}

} // namespace lob::test
