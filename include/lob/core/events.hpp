// Copyright 2026 51ddhesh
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/*
    lob::core::events
    Event: any action that modifies the state of the order book
*/

/*
    Design
   --------
    1. Plain aggregates emitted by the matching engine to report what happened.
    2. The listener (templated, not virtual) receives events with zero overhead.
    3. NullListener compiles to nothing — the compiler elides every callback.
*/

/*
    Event Types
   -------------
    FillEvent: one per maker order touched during matching
    AcceptEvent: order (or remainder) rested on the book
    CancelEvent: order removed from the book
    RejectEvent: order rejected before entering the book
*/

/*
    All Events are trivially copyable, standard layout and are smaller than one cache line
*/

#pragma once


#include <concepts>
#include <type_traits>
#include "lob/core/compiler_hints.hpp"
#include "lob/core/types.hpp"


namespace lob {

/* 
* ┌────────────┐
* │ Fill Event │
* └────────────┘ 
*/

// Emitted once per resting (maker) order touched during a match.
// The matching engine may emit several FillEvents for a single aggressive (taker) order
// when it sweeps through the price levels.

struct FillEvent {
    OrderID maker_id;    //  0: resting order that provided liquidity
    OrderID taker_id;    //  8: aggressing order that crossed the spread
    Price price;         // 16: execution price (always the maker's limit price)
    Quantity fill_qty;   // 24: quantity traded in this fill
    Timestamp timestamp; // 32: fill time
    Side taker_side;     // 40: which side aggressed? (Buy: taker bought, Sell: taker sold)
    bool maker_filled;    // 41: true if maker's remaining quantity hit zero
};

static_assert(std::is_trivially_copyable_v<FillEvent>);
static_assert(std::is_trivially_destructible_v<FillEvent>);
static_assert(std::is_standard_layout_v<FillEvent>);
static_assert(std::is_aggregate_v<FillEvent>);
static_assert(sizeof(FillEvent) <= kCacheLineSize);

/* 
* ┌──────────────┐
* │ Accept Event │
* └──────────────┘ 
*/

// Emitted when an order (or its unmatched remainder after a partial 
// match) is placed on the book as a resting limit order.

struct AcceptEvent {
    OrderID   id;        //  0: order that rested
    Price     price;     //  8: limit price
    Quantity  quantity;  // 16: resting quantity (may be < original after partial match)
    Timestamp timestamp; // 24: accept time
    Side      side;      // 32: buy or sell
};

static_assert(std::is_trivially_copyable_v<AcceptEvent>);
static_assert(std::is_trivially_destructible_v<AcceptEvent>);
static_assert(std::is_standard_layout_v<AcceptEvent>);
static_assert(std::is_aggregate_v<AcceptEvent>);
static_assert(sizeof(AcceptEvent) <= kCacheLineSize);

/* 
* ┌──────────────┐
* │ Cancel Event │
* └──────────────┘ 
*/

// Emitted when a resting order is removed from the book
// (user-initiated cancel or IOC remainder killed).

struct CancelEvent {
    OrderID   id;            //  0: order that was cancelled
    Quantity  cancelled_qty; //  8: quantity removed from the book
    Timestamp timestamp;     // 16: cancel time
};

static_assert(std::is_trivially_copyable_v<CancelEvent>);
static_assert(std::is_trivially_destructible_v<CancelEvent>);
static_assert(std::is_standard_layout_v<CancelEvent>);
static_assert(std::is_aggregate_v<CancelEvent>);
static_assert(sizeof(CancelEvent) <= kCacheLineSize);

/* 
* ┌──────────────┐
* │ Reject Event │
* └──────────────┘ 
*/

// Emitted when an order fails validation before entering
// the matching engine (bad price, duplicate ID, FOK that
// cannot fill, pool exhausted, etc.).

struct RejectEvent {
    OrderID    id;        //  0: order that was rejected
    Timestamp  timestamp; //  8: reject time
    OrderError reason;    // 16: why the order was rejected
};

static_assert(std::is_trivially_copyable_v<RejectEvent>);
static_assert(std::is_trivially_destructible_v<RejectEvent>);
static_assert(std::is_standard_layout_v<RejectEvent>);
static_assert(std::is_aggregate_v<RejectEvent>);
static_assert(sizeof(RejectEvent) <= kCacheLineSize);

/* 
* ┌───────────────────────┐
* │ EventListener concept │
* └───────────────────────┘ 
*/

// The order book and the matching engine are templated on a type 
// satisfying this concept. The compiler inlines every callback.

template <typename T>
concept EventListener = requires(
    T& listener,
    const FillEvent& fill,
    const AcceptEvent& accept,
    const CancelEvent& cancel,
    const RejectEvent& reject
) {
    { listener.on_fill(fill) }     -> std::same_as<void>;
    { listener.on_accept(accept) } -> std::same_as<void>;
    { listener.on_cancel(cancel) } -> std::same_as<void>;
    { listener.on_reject(reject) } -> std::same_as<void>;
};


/* 
* ┌──────────────┐
* │ NullListener │
* └──────────────┘ 
*/

struct NullListener {
    void on_fill(const FillEvent&)     noexcept {}
    void on_accept(const AcceptEvent&) noexcept {}
    void on_cancel(const CancelEvent&) noexcept {}
    void on_reject(const RejectEvent&) noexcept {}
};

static_assert(EventListener<NullListener>);

} // namespace lob
