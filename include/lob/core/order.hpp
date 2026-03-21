// Copyright 2026 51ddhesh
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/*
    lob::core::order
    A single resting order in the book
*/

/*
    Design
   --------
    - This is a intrusive doubly-linked list node. 
    - PriceLevel thread orders through prev/next pointers to forms its FIFO queue.

    Layout
   --------
    - All hot fields (accessed during matching and cancel) are packed in 64 bytes.
    - Each order shares a single cache line.
*/

/*
    This is a plain aggregate. No constructors, no methods.
    All manipulation is done by PriceLevel and the order book
*/

#pragma once


#include <cstddef>
#include <cstdint>
#include "lob/core/compiler_hints.hpp"  
#include "lob/core/types.hpp"

namespace lob {

struct PriceLevel;

/* 
* ┌──────────────────┐
* │ Order Definition │
* └──────────────────┘ 
*/

struct Order {
    // * ─── Cache Line 1 ────────────────────────────────────────────────────── *
    // Hot
    // Touched on every match/cancel
    
    Order* prev;          // 0: intrusive list: previous order at same price
    Order* next;          // 8: intrusive list: 
    OrderID id;           // 16: Unique identifier to the order
    Price price;          // 24: limit price (fill events report maker's price)
    Quantity remaining;   // 32: quantity still resting (decremented on partial fill)
    Timestamp timestamp;  // 40: arrival time (FIFO tiebreaker, fill event reporting)
    PriceLevel* level;    // 48: back pointer to the owning level 
    Side side;            // 56: buy/sell
    TimeInForce tif;      // 57: GTC / IOC / FOK / DAY
    OrderType type;       // 58: Limit / Market
    
    // 3 bytes padding to 8-byte boundary, then end of cache line at 64 bytes
    
    // * ─── Cache Line 2 ────────────────────────────────────────────────────── *
    // Cold
    // Only for event reporting

    Quantity original_qty; // 64: quantity at order creation

}; // struct lob::Order


/* 
* ┌──────────────────────────────────┐
* │ Compile Time layout verification │
* └──────────────────────────────────┘ 
*/

static_assert(
    std::is_trivially_copyable_v<Order>, 
    "Order must be trivially copyable for pool allocation and memcpy safety"
);

static_assert(
    std::is_trivially_destructible_v<Order>,
    "Order must be trivially destructible so PoolAllocator can skip destructor calls"
);

static_assert(
    std::is_standard_layout_v<Order>, 
    "Order must be standard layout for well-defined offsetof behavior"
);

static_assert(offsetof(Order, prev) < kCacheLineSize);
static_assert(offsetof(Order, next) < kCacheLineSize);
static_assert(offsetof(Order, id) < kCacheLineSize);
static_assert(offsetof(Order, price) < kCacheLineSize);
static_assert(offsetof(Order, remaining) < kCacheLineSize);
static_assert(offsetof(Order, timestamp) < kCacheLineSize);
static_assert(offsetof(Order, level) < kCacheLineSize);
static_assert(offsetof(Order, side) < kCacheLineSize);
static_assert(offsetof(Order, tif) < kCacheLineSize);
static_assert(offsetof(Order, type) < kCacheLineSize);

// Cold fields start at or beyond the cache line boundary.
static_assert(offsetof(Order, original_qty) >= kCacheLineSize);

// Size sanity: should be ~72 bytes (64 hot + 8 cold).
// Must not silently balloon due to unexpected padding.
static_assert(
    sizeof(Order) <= 80,
    "Order grew beyond expected size — check for unexpected padding"
);

} // namespace lob

