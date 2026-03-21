// Copyright 2026 51ddhesh
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/*
    lob::core::price_level
    FIFO queue of all resting orders at a single price
*/

/*
    Design
   --------
    - Internally an intrusive doubly-linked list threaded through Order::prev/next. 
    - Maintains aggregate statistics (total quantity, order count) that are updated on every mutation

    Layout
   --------
    - 40 bytes, sits in a single cache line
*/

/*
    This is a plain aggregate. No constructors, no virtual functions.
    Trivially destructible so it can live in a PoolAllocator pool.
*/

#pragma once


#include <cassert>
#include <cstdint>
#include "lob/core/order.hpp"
#include "lob/core/types.hpp"


namespace lob {

struct PriceLevel {
    // * ─── Data (40 bytes) ─────────────────────────────────────────────────── *
    Order* head;               //  0: front of the FIFO (oldest, first to match)
    Order* tail;               //  8: back of the FIFO (newest, most recently added)
    Quantity total_qty;        // 16: Sum of the remaining quantity across all orders
    Price price;               // 24: The price this level represents
    std::uint32_t order_count; // 32: Number of orders in the queue

    // * ─── Queue operations ────────────────────────────────────────────────── *
    
    // Add an order to the back of the FIFO queue.
    // Sets order -> level to this, updates total_qty and order_count.
    // Precondition: order is not in any level (prev/next/level are null).
    void append(Order* order) noexcept {
        assert(order != nullptr);
        assert(order -> prev == nullptr && "Order already linked (prev)");
        assert(order -> next == nullptr && "Order already linked (next)");
        assert(order -> level == nullptr && "Order already in a level");

        order -> level = this;

        if (tail == nullptr) {
            head = tail = order;
        } else {
            tail -> next = order;
            order -> prev = tail;
            tail = order;
        }

        total_qty += order -> remaining;
        ++order_count;
    }

    // Unlink an order from anywhere in the queue (head, tail, or middle)
    // Clears order -> prev, order -> next, order -> level
    // Updates total_qty and order_count
    // Precondition: order belongs to this level
    void remove(Order* order) noexcept {
        assert(order != nullptr);
        assert(order -> level == this && "Order does not belong to this level");

        if (order -> prev != nullptr) {
            order -> prev -> next = order -> next;
        } else {
            head = order -> next;
        }

        if (order -> next != nullptr) {
            order -> next -> prev = order -> prev;
        } else {
            tail = order -> prev;
        }

        total_qty -= order -> remaining;
        --order_count;

        order -> prev = nullptr;
        order -> next = nullptr;
        order -> level = nullptr;
    }

    // Remove and return the front (oldest) order
    // Returns nullptr if the level is emtpy
    [[nodiscard]] Order* pop_front() noexcept {
        if (head == nullptr) return nullptr;
        Order* front_order = head;
        remove(front_order);
        return front_order;
    }

    // Peek the front order without removing it
    [[nodiscard]] Order* front() const noexcept {
        return head;
    }

    // Returns true if there are no orders at this price level
    [[nodiscard]] bool is_empty() const noexcept {
        return order_count == 0;
    }

    // Subtract quantity from the level total after a partiall full.
    // The caller is responsible for also updating order -> remaining.
    // This keeps total_qty consistent without a full remove/re-add.
    void reduce_qty(Quantity delta) noexcept {
        assert(!is_empty() && "reduce_qty on empty level!");
        assert(delta.raw() <= total_qty.raw() && "reducing more than total");
        total_qty -= delta;
    }

    // * ─── Debug invariant checker ─────────────────────────────────────────── *

    // Walk the entire list and verify all internal invariants.
    // !! For tests and debug assertions only.
    void assert_invariants() const noexcept {
    #ifndef NODEBUG
        if (order_count == 0) {
            assert(head == nullptr && "empty level has non-null head");
            assert(tail == nullptr && "empty level has non-null tail");
            assert(total_qty == kZeroQuantity && "empty level has non-zero qty");
            return;
        }

        assert(head != nullptr && "non-empty level has null head");
        assert(tail != nullptr && "non-empty level has null tail");
        assert(head -> prev == nullptr && "head->prev is not null");
        assert(tail -> next == nullptr && "tail->next is not null");

        std::uint32_t counted = 0;
        Quantity summed{0ULL};

        const Order* prev_seen = nullptr;

        for (const Order* curr = head; curr != nullptr; curr = curr -> next) {
            assert(curr -> level == this && "order -> level does not point to this level");
            assert(curr -> prev == prev_seen && "broken prev link");
            summed += curr -> remaining;
            counted++;
            prev_seen = curr;
        }

        assert(prev_seen == tail && "list walk did not end at tail");
        assert(counted == order_count && "order_count mismatch");
        assert(summed == total_qty && "total_qty mismatch");
    #endif
    }    
};

// * ─── Compile time layout checks ──────────────────────────────────────── *

static_assert(std::is_trivially_destructible_v<PriceLevel>);
static_assert(std::is_standard_layout_v<PriceLevel>);
static_assert(sizeof(PriceLevel) <= kCacheLineSize);

} // namespace lob
