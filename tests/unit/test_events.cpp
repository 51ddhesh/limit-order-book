// Copyright 2026 51ddhesh
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/*
    lob::tests::test_events
*/


#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>
#include "lob/core/events.hpp"

namespace lob::test {

/* 
* ┌────────────────┐
* │ VectorListener │
* └────────────────┘ 
*/

// Collects events for testing.
// Pushes a copy of every received event into the corresponding vector.
struct VectorListener {
    std::vector<FillEvent> fills;
    std::vector<AcceptEvent> accepts;
    std::vector<CancelEvent> cancels;
    std::vector<RejectEvent> rejects;

    void on_fill(const FillEvent& e) {
        fills.push_back(e);
    }

    void on_accept(const AcceptEvent& e) {
        accepts.push_back(e);
    }

    void on_cancel(const CancelEvent& e) {
        cancels.push_back(e);
    }

    void on_reject(const RejectEvent& e) {
        rejects.push_back(e);
    }

    void clear() {
        fills.clear();
        accepts.clear();
        cancels.clear();
        rejects.clear();
    }

    [[nodiscard]] std::size_t total() const noexcept {
        return fills.size() + accepts.size() + cancels.size() + rejects.size();
    }
};

static_assert(EventListener<VectorListener>);

/* 
* ┌────────────────────────┐
* │ Negative Concept Check │
* └────────────────────────┘ 
*/

// Types that MUST NOT satisfy EventListener
// Ensuring the concept rejects bad listeners

struct MissingOnReject {
    void on_fill(const FillEvent&) {}
    void on_accept(const AcceptEvent&) {}
    void on_cancel(const CancelEvent&) {}
};

static_assert(!EventListener<MissingOnReject>);

struct WrongReturnType {
    void on_fill(const FillEvent&) {}
    void on_accept(const AcceptEvent&) {}
    void on_cancel(const CancelEvent&) {}
    int  on_reject(const RejectEvent&) { return 0; } 
};

static_assert(!EventListener<WrongReturnType>);


/*
* ╔═════════════╗
* ║ BEGIN TESTS ║
* ╚═════════════╝
*/

/* 
* ┌───────────────────────────────┐
* │ 1. Test Layout and Properties │
* └───────────────────────────────┘ 
*/

TEST(EventLayout, FillEventProperties) {
    static_assert(std::is_trivially_copyable_v<FillEvent>);
    static_assert(std::is_trivially_destructible_v<FillEvent>);
    static_assert(std::is_standard_layout_v<FillEvent>);
    static_assert(std::is_aggregate_v<FillEvent>);
    static_assert(sizeof(FillEvent) <= 64);
}

TEST(EventLayout, AcceptEventProperties) {
    static_assert(std::is_trivially_copyable_v<AcceptEvent>);
    static_assert(std::is_trivially_destructible_v<AcceptEvent>);
    static_assert(std::is_standard_layout_v<AcceptEvent>);
    static_assert(std::is_aggregate_v<AcceptEvent>);
    static_assert(sizeof(AcceptEvent) <= 64);
}

TEST(EventLayout, CancelEventProperties) {
    static_assert(std::is_trivially_copyable_v<CancelEvent>);
    static_assert(std::is_trivially_destructible_v<CancelEvent>);
    static_assert(std::is_standard_layout_v<CancelEvent>);
    static_assert(std::is_aggregate_v<CancelEvent>);
    static_assert(sizeof(CancelEvent) <= 64);
}

TEST(EventLayout, RejectEventProperties) {
    static_assert(std::is_trivially_copyable_v<RejectEvent>);
    static_assert(std::is_trivially_destructible_v<RejectEvent>);
    static_assert(std::is_standard_layout_v<RejectEvent>);
    static_assert(std::is_aggregate_v<RejectEvent>);
    static_assert(sizeof(RejectEvent) <= 64);
}

/* 
* ┌───────────────────────────────────────────┐
* │ 2. Test Aggregate Construction and Access │
* └───────────────────────────────────────────┘ 
*/

TEST(EventConstruction, FillEventFieldAccess) {
    FillEvent e{
        .maker_id     = OrderID{1},
        .taker_id     = OrderID{2},
        .price        = Price{10050},
        .fill_qty     = Quantity{100},
        .timestamp    = Timestamp{9999},
        .taker_side   = Side::Buy,
        .maker_filled = true,
    };

    EXPECT_EQ(e.maker_id,     OrderID{1});
    EXPECT_EQ(e.taker_id,     OrderID{2});
    EXPECT_EQ(e.price,        Price{10050});
    EXPECT_EQ(e.fill_qty,     Quantity{100});
    EXPECT_EQ(e.timestamp,    Timestamp{9999});
    EXPECT_EQ(e.taker_side,   Side::Buy);
    EXPECT_TRUE(e.maker_filled);
}

TEST(EventConstruction, FillEventPartialFillFlag) {
    FillEvent partial{
        .maker_id = OrderID{1}, .taker_id = OrderID{2},
        .price = Price{100}, .fill_qty = Quantity{50},
        .timestamp = Timestamp{0}, .taker_side = Side::Buy,
        .maker_filled = false,
    };
    EXPECT_FALSE(partial.maker_filled);

    FillEvent full{
        .maker_id = OrderID{1}, .taker_id = OrderID{2},
        .price = Price{100}, .fill_qty = Quantity{50},
        .timestamp = Timestamp{0}, .taker_side = Side::Buy,
        .maker_filled = true,
    };
    EXPECT_TRUE(full.maker_filled);
}

TEST(EventConstruction, AcceptEventFieldAccess) {
    AcceptEvent e{
        .id        = OrderID{42},
        .price     = Price{2000},
        .quantity  = Quantity{500},
        .timestamp = Timestamp{1234},
        .side      = Side::Sell,
    };

    EXPECT_EQ(e.id,        OrderID{42});
    EXPECT_EQ(e.price,     Price{2000});
    EXPECT_EQ(e.quantity,  Quantity{500});
    EXPECT_EQ(e.timestamp, Timestamp{1234});
    EXPECT_EQ(e.side,      Side::Sell);
}

TEST(EventConstruction, CancelEventFieldAccess) {
    CancelEvent e{
        .id            = OrderID{7},
        .cancelled_qty = Quantity{300},
        .timestamp     = Timestamp{5555},
    };

    EXPECT_EQ(e.id,            OrderID{7});
    EXPECT_EQ(e.cancelled_qty, Quantity{300});
    EXPECT_EQ(e.timestamp,     Timestamp{5555});
}

TEST(EventConstruction, RejectEventFieldAccess) {
    RejectEvent e{
        .id        = OrderID{99},
        .timestamp = Timestamp{7777},
        .reason    = OrderError::InvalidPrice,
    };

    EXPECT_EQ(e.id,        OrderID{99});
    EXPECT_EQ(e.timestamp, Timestamp{7777});
    EXPECT_EQ(e.reason,    OrderError::InvalidPrice);
}

TEST(EventConstruction, RejectEventAllReasons) {
    // Every OrderError variant can be stored in a RejectEvent.
    const OrderError reasons[] = {
        OrderError::InvalidPrice,
        OrderError::InvalidQuantity,
        OrderError::InvalidOrderID,
        OrderError::DuplicateOrderID,
        OrderError::OrderNotFound,
        OrderError::BookFull,
        OrderError::PoolExhausted,
        OrderError::SelfTrade,
        OrderError::WouldNotFill,
    };

    for (auto reason : reasons) {
        RejectEvent e{
            .id = OrderID{1}, .timestamp = Timestamp{0},
            .reason = reason,
        };
        EXPECT_EQ(e.reason, reason) << "Failed for " << to_string(reason);
    }
}

/* 
* ┌───────────────────────────────┐
* │ 3. Test EventListener Concept │
* └───────────────────────────────┘ 
*/

TEST(EventListenerConcept, NullListenerSatisfies) {
    static_assert(EventListener<NullListener>);
}

TEST(EventListenerConcept, VectorListenerSatisfies) {
    static_assert(EventListener<VectorListener>);
}

TEST(EventListenerConcept, IncompleteListenerRejected) {
    static_assert(!EventListener<MissingOnReject>);
}

TEST(EventListenerConcept, WrongReturnTypeRejected) {
    static_assert(!EventListener<WrongReturnType>);
}

TEST(EventListenerConcept, PrimitiveTypesRejected) {
    static_assert(!EventListener<int>);
    static_assert(!EventListener<double>);
    static_assert(!EventListener<void*>);
}

TEST(EventListenerConcept, EmptyStructRejected) {
    struct Empty {};
    static_assert(!EventListener<Empty>);
}

/* 
* ┌──────────────────────┐
* │ 4. Test NullListener │
* └──────────────────────┘ 
*/

TEST(NullListener, CompilesAndDoesNothing) {
    NullListener listener;

    // These must compile and not crash.  That is the entire contract.
    listener.on_fill(FillEvent{
        .maker_id = OrderID{1}, .taker_id = OrderID{2},
        .price = Price{100}, .fill_qty = Quantity{10},
        .timestamp = Timestamp{0}, .taker_side = Side::Buy,
        .maker_filled = false,
    });

    listener.on_accept(AcceptEvent{
        .id = OrderID{1}, .price = Price{100},
        .quantity = Quantity{10}, .timestamp = Timestamp{0},
        .side = Side::Buy,
    });

    listener.on_cancel(CancelEvent{
        .id = OrderID{1}, .cancelled_qty = Quantity{10},
        .timestamp = Timestamp{0},
    });

    listener.on_reject(RejectEvent{
        .id = OrderID{1}, .timestamp = Timestamp{0},
        .reason = OrderError::InvalidPrice,
    });
}

TEST(NullListener, IsNoexcept) {
    NullListener listener;
    FillEvent f{
        .maker_id = OrderID{1}, .taker_id = OrderID{2},
        .price = Price{100}, .fill_qty = Quantity{10},
        .timestamp = Timestamp{0}, .taker_side = Side::Buy,
        .maker_filled = false,
    };
    AcceptEvent a{
        .id = OrderID{1}, .price = Price{100},
        .quantity = Quantity{10}, .timestamp = Timestamp{0},
        .side = Side::Buy,
    };
    CancelEvent c{
        .id = OrderID{1}, .cancelled_qty = Quantity{10},
        .timestamp = Timestamp{0},
    };
    RejectEvent r{
        .id = OrderID{1}, .timestamp = Timestamp{0},
        .reason = OrderError::InvalidPrice,
    };

    static_assert(noexcept(listener.on_fill(f)));
    static_assert(noexcept(listener.on_accept(a)));
    static_assert(noexcept(listener.on_cancel(c)));
    static_assert(noexcept(listener.on_reject(r)));
}

/* 
* ┌────────────────────────┐
* │ 5. Test VectorListener │
* └────────────────────────┘ 
*/

TEST(VectorListener, RecordsFillEvents) {
    VectorListener listener;

    listener.on_fill(FillEvent{
        .maker_id = OrderID{10}, .taker_id = OrderID{20},
        .price = Price{5000}, .fill_qty = Quantity{100},
        .timestamp = Timestamp{1}, .taker_side = Side::Buy,
        .maker_filled = false,
    });

    listener.on_fill(FillEvent{
        .maker_id = OrderID{30}, .taker_id = OrderID{20},
        .price = Price{5001}, .fill_qty = Quantity{200},
        .timestamp = Timestamp{2}, .taker_side = Side::Buy,
        .maker_filled = true,
    });

    ASSERT_EQ(listener.fills.size(), 2u);

    EXPECT_EQ(listener.fills[0].maker_id,     OrderID{10});
    EXPECT_EQ(listener.fills[0].fill_qty,     Quantity{100});
    EXPECT_FALSE(listener.fills[0].maker_filled);

    EXPECT_EQ(listener.fills[1].maker_id,     OrderID{30});
    EXPECT_EQ(listener.fills[1].fill_qty,     Quantity{200});
    EXPECT_TRUE(listener.fills[1].maker_filled);
}

TEST(VectorListener, RecordsAllEventTypes) {
    VectorListener listener;

    listener.on_fill(FillEvent{
        .maker_id = OrderID{1}, .taker_id = OrderID{2},
        .price = Price{100}, .fill_qty = Quantity{50},
        .timestamp = Timestamp{1}, .taker_side = Side::Sell,
        .maker_filled = true,
    });

    listener.on_accept(AcceptEvent{
        .id = OrderID{3}, .price = Price{99},
        .quantity = Quantity{200}, .timestamp = Timestamp{2},
        .side = Side::Buy,
    });

    listener.on_cancel(CancelEvent{
        .id = OrderID{4}, .cancelled_qty = Quantity{75},
        .timestamp = Timestamp{3},
    });

    listener.on_reject(RejectEvent{
        .id = OrderID{5}, .timestamp = Timestamp{4},
        .reason = OrderError::DuplicateOrderID,
    });

    EXPECT_EQ(listener.fills.size(),   1u);
    EXPECT_EQ(listener.accepts.size(), 1u);
    EXPECT_EQ(listener.cancels.size(), 1u);
    EXPECT_EQ(listener.rejects.size(), 1u);
    EXPECT_EQ(listener.total(),        4u);

    EXPECT_EQ(listener.rejects[0].reason, OrderError::DuplicateOrderID);
}

TEST(VectorListener, ClearResetsAllVectors) {
    VectorListener listener;

    listener.on_fill(FillEvent{
        .maker_id = OrderID{1}, .taker_id = OrderID{2},
        .price = Price{100}, .fill_qty = Quantity{10},
        .timestamp = Timestamp{0}, .taker_side = Side::Buy,
        .maker_filled = false,
    });
    listener.on_accept(AcceptEvent{
        .id = OrderID{3}, .price = Price{100},
        .quantity = Quantity{10}, .timestamp = Timestamp{0},
        .side = Side::Buy,
    });

    EXPECT_EQ(listener.total(), 2u);

    listener.clear();

    EXPECT_EQ(listener.total(), 0u);
    EXPECT_TRUE(listener.fills.empty());
    EXPECT_TRUE(listener.accepts.empty());
    EXPECT_TRUE(listener.cancels.empty());
    EXPECT_TRUE(listener.rejects.empty());
}

TEST(VectorListener, MultipleFillsPreserveInsertionOrder) {
    VectorListener listener;

    for (std::uint64_t i = 0; i < 10; ++i) {
        listener.on_fill(FillEvent{
            .maker_id     = OrderID{i},
            .taker_id     = OrderID{100},
            .price        = Price{static_cast<std::int64_t>(1000 + i)},
            .fill_qty     = Quantity{i * 10 + 10},
            .timestamp    = Timestamp{i},
            .taker_side   = Side::Buy,
            .maker_filled = (i == 9),
        });
    }

    ASSERT_EQ(listener.fills.size(), 10u);
    for (std::uint64_t i = 0; i < 10; ++i) {
        EXPECT_EQ(listener.fills[i].maker_id,  OrderID{i});
        EXPECT_EQ(listener.fills[i].timestamp,  Timestamp{i});
        EXPECT_EQ(listener.fills[i].fill_qty,   Quantity{i * 10 + 10});
    }
    EXPECT_TRUE(listener.fills[9].maker_filled);
    EXPECT_FALSE(listener.fills[0].maker_filled);
}

/* 
* ┌────────────────────────────┐
* │ Matching Engine Simulation │
* └────────────────────────────┘ 
*/

TEST(EventIntegration, SweepThroughMultipleMakers) {
    VectorListener listener;

    listener.on_fill(FillEvent{
        .maker_id = OrderID{1}, .taker_id = OrderID{100},
        .price = Price{1000}, .fill_qty = Quantity{100},
        .timestamp = Timestamp{1}, .taker_side = Side::Buy,
        .maker_filled = true,
    });

    listener.on_fill(FillEvent{
        .maker_id = OrderID{2}, .taker_id = OrderID{100},
        .price = Price{1001}, .fill_qty = Quantity{150},
        .timestamp = Timestamp{2}, .taker_side = Side::Buy,
        .maker_filled = false,
    });

    ASSERT_EQ(listener.fills.size(), 2u);
    EXPECT_EQ(listener.accepts.size(), 0u);

    // First fill: maker fully consumed
    EXPECT_EQ(listener.fills[0].maker_id, OrderID{1});
    EXPECT_EQ(listener.fills[0].fill_qty, Quantity{100});
    EXPECT_TRUE(listener.fills[0].maker_filled);

    // Second fill: maker partially consumed
    EXPECT_EQ(listener.fills[1].maker_id, OrderID{2});
    EXPECT_EQ(listener.fills[1].fill_qty, Quantity{150});
    EXPECT_FALSE(listener.fills[1].maker_filled);

    // Total quantity traded
    Quantity total = listener.fills[0].fill_qty + listener.fills[1].fill_qty;
    EXPECT_EQ(total, Quantity{250});
}

TEST(EventIntegration, PartialMatchThenRest) {
    VectorListener listener;

    listener.on_fill(FillEvent{
        .maker_id = OrderID{1}, .taker_id = OrderID{50},
        .price = Price{1000}, .fill_qty = Quantity{200},
        .timestamp = Timestamp{1}, .taker_side = Side::Buy,
        .maker_filled = true,
    });

    listener.on_accept(AcceptEvent{
        .id = OrderID{50}, .price = Price{1000},
        .quantity = Quantity{300}, .timestamp = Timestamp{2},
        .side = Side::Buy,
    });

    EXPECT_EQ(listener.fills.size(),   1u);
    EXPECT_EQ(listener.accepts.size(), 1u);
    EXPECT_EQ(listener.accepts[0].quantity, Quantity{300});
    EXPECT_EQ(listener.accepts[0].side,     Side::Buy);
}

TEST(EventIntegration, FOKRejection) {
    VectorListener listener;

    listener.on_reject(RejectEvent{
        .id = OrderID{77}, .timestamp = Timestamp{42},
        .reason = OrderError::WouldNotFill,
    });

    ASSERT_EQ(listener.rejects.size(), 1u);
    EXPECT_EQ(listener.rejects[0].id,     OrderID{77});
    EXPECT_EQ(listener.rejects[0].reason, OrderError::WouldNotFill);
    EXPECT_EQ(listener.fills.size(),      0u);
    EXPECT_EQ(listener.accepts.size(),    0u);
}

TEST(EventIntegration, CancelRestingOrder) {
    VectorListener listener;

    listener.on_accept(AcceptEvent{
        .id = OrderID{10}, .price = Price{500},
        .quantity = Quantity{1000}, .timestamp = Timestamp{1},
        .side = Side::Sell,
    });

    listener.on_cancel(CancelEvent{
        .id = OrderID{10}, .cancelled_qty = Quantity{1000},
        .timestamp = Timestamp{5},
    });

    EXPECT_EQ(listener.accepts.size(), 1u);
    EXPECT_EQ(listener.cancels.size(), 1u);
    EXPECT_EQ(listener.cancels[0].id,            OrderID{10});
    EXPECT_EQ(listener.cancels[0].cancelled_qty, Quantity{1000});
}

/* 
* ┌─────────────────────────────────┐
* │ Generic Template Listener Usage │
* └─────────────────────────────────┘ 
*/

// Verifies that a function templated on 
// EventListener works with both NullListener
// and VectorListener (the pattern the matching
// engine will use).

namespace {

template <EventListener L>
void emit_sample_fill(L& listener) {
    listener.on_fill(FillEvent{
        .maker_id     = OrderID{1},
        .taker_id     = OrderID{2},
        .price        = Price{100},
        .fill_qty     = Quantity{50},
        .timestamp    = Timestamp{0},
        .taker_side   = Side::Buy,
        .maker_filled = true,
    });
}

template <EventListener L>
void emit_sample_sequence(L& listener) {
    listener.on_fill(FillEvent{
        .maker_id = OrderID{1}, .taker_id = OrderID{10},
        .price = Price{100}, .fill_qty = Quantity{30},
        .timestamp = Timestamp{1}, .taker_side = Side::Buy,
        .maker_filled = true,
    });
    listener.on_accept(AcceptEvent{
        .id = OrderID{10}, .price = Price{100},
        .quantity = Quantity{70}, .timestamp = Timestamp{2},
        .side = Side::Buy,
    });
}

} // anonymous namespace

TEST(GenericListener, NullListenerCompiles) {
    NullListener listener;
    emit_sample_fill(listener);     // must compile and not crash
    emit_sample_sequence(listener); // likewise
}

TEST(GenericListener, VectorListenerRecords) {
    VectorListener listener;
    emit_sample_fill(listener);

    ASSERT_EQ(listener.fills.size(), 1u);
    EXPECT_EQ(listener.fills[0].fill_qty, Quantity{50});
    EXPECT_TRUE(listener.fills[0].maker_filled);
}

TEST(GenericListener, VectorListenerRecordsSequence) {
    VectorListener listener;
    emit_sample_sequence(listener);

    ASSERT_EQ(listener.fills.size(),   1u);
    ASSERT_EQ(listener.accepts.size(), 1u);
    EXPECT_EQ(listener.fills[0].fill_qty,     Quantity{30});
    EXPECT_EQ(listener.accepts[0].quantity,   Quantity{70});
}

} // namespace lob::test
