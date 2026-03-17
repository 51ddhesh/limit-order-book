// Copyright 2026 51ddhesh
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/*
    lob::tests::test_types
*/

#include <gtest/gtest.h>
#include <cstdint>
#include <format>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lob/core/types.hpp"

namespace lob::test {

/* 
* ┌──────────────────────┐
* │ 1. Test Strong Types │
* └──────────────────────┘ 
*/

TEST(StrongType, DifferentTagsProduceIncompatibleTypes) {
    static_assert(!std::is_same_v<Price, Quantity>);
    static_assert(!std::is_same_v<Price, OrderID>);
    static_assert(!std::is_same_v<Price, Timestamp>);
    static_assert(!std::is_same_v<Quantity, OrderID>);
    static_assert(!std::is_same_v<Quantity, Timestamp>);
    static_assert(!std::is_same_v<OrderID, Timestamp>);

    static_assert(!std::is_convertible_v<std::int64_t, Price>);
    static_assert(!std::is_convertible_v<std::uint64_t, Quantity>);
    static_assert(!std::is_convertible_v<std::uint64_t, OrderID>);
}

TEST(StrongType, ZeroSizeOverhead) {
    static_assert(sizeof(Price)     == sizeof(std::int64_t));
    static_assert(sizeof(Quantity)  == sizeof(std::uint64_t));
    static_assert(sizeof(OrderID)   == sizeof(std::uint64_t));
    static_assert(sizeof(Timestamp) == sizeof(std::uint64_t));
}

TEST(StrongType, TriviallyCopyable) {
    static_assert(std::is_trivially_copyable_v<Price>);
    static_assert(std::is_trivially_copyable_v<Quantity>);
    static_assert(std::is_trivially_copyable_v<OrderID>);
    static_assert(std::is_trivially_copyable_v<Timestamp>);
}

TEST(StrongType, DefaultConstructsToZero) {
    EXPECT_EQ(Price{}.raw(), 0);
    EXPECT_EQ(Quantity{}.raw(), 0ULL);
    EXPECT_EQ(OrderID{}.raw(), 0ULL);
    EXPECT_EQ(Timestamp{}.raw(), 0ULL);
}

TEST(StrongType, ExplicitConstructionAndRawAccess) {
    Price p{-42};
    EXPECT_EQ(p.raw(), -42);

    OrderID id{999};
    EXPECT_EQ(id.raw(), 999ULL);
}

TEST(StrongType, InvalidSentinel) {
    constexpr auto inv = OrderID::invalid();
    static_assert(!inv.is_valid());

    constexpr OrderID good{1};
    static_assert(good.is_valid());

    constexpr OrderID zero{};
    static_assert(zero.is_valid());
}

/* 
* ┌────────────────────┐
* │ 2. Test Comparison │
* └────────────────────┘ 
*/

TEST(Comparison, PriceOrdering) {
    constexpr Price lo{100};
    constexpr Price hi{200};
    constexpr Price eq{100};

    static_assert(lo < hi);
    static_assert(hi > lo);
    static_assert(lo <= eq);
    static_assert(lo >= eq);
    static_assert(lo == eq);
    static_assert(lo != hi);

    // Three-way comparison
    static_assert((lo <=> hi) < 0);
    static_assert((hi <=> lo) > 0);
    static_assert((lo <=> eq) == 0);
}

TEST(Comparison, NegativePricesOrderCorrectly) {
    // Negative prices are real (oil futures, April 2020)
    constexpr Price neg{-100};
    constexpr Price zero{0};
    constexpr Price pos{100};

    static_assert(neg < zero);
    static_assert(zero < pos);
    static_assert(neg < pos);
}

/* 
* ┌─────────────────────────────┐
* │ 3. Test Quantity Arithmetic │
* └─────────────────────────────┘ 
*/

TEST(QuantityArithmetic, Addition) {
    constexpr Quantity a{100};
    constexpr Quantity b{250};
    constexpr auto sum = a + b;
    static_assert(sum.raw() == 350);
    
    Quantity x{100};
    Quantity y{250};
    EXPECT_EQ((x + y).raw(), 350ULL);
}

TEST(QuantityArithmetic, Subtraction) {
    constexpr Quantity a{300};
    constexpr Quantity b{120};
    constexpr auto diff = a - b;
    static_assert(diff.raw() == 180);
    
    Quantity x{300};
    Quantity y{120};
    EXPECT_EQ((x - y).raw(), 180ULL);
}

TEST(QuantityArithmetic, CompoundAddition) {
    Quantity q{100};
    q += Quantity{50};
    EXPECT_EQ(q.raw(), 150ULL);
    
    q += Quantity{0};
    EXPECT_EQ(q.raw(), 150ULL);
    
    q += Quantity{850};
    EXPECT_EQ(q.raw(), 1000ULL);
}

TEST(QuantityArithmetic, CompoundSubtraction) {
    Quantity q{500};
    q -= Quantity{200};
    EXPECT_EQ(q.raw(), 300ULL);
    
    q -= Quantity{300};
    EXPECT_EQ(q.raw(), 0ULL);
}

TEST(QuantityArithmetic, FillCalculationPattern) {
    Quantity remaining{300};
    Quantity incoming{500};
    
    Quantity fill = std::min(remaining, incoming);
    EXPECT_EQ(fill, Quantity{300});
    
    remaining -= fill;
    incoming  -= fill;
    
    EXPECT_EQ(remaining, kZeroQuantity);
    EXPECT_EQ(incoming, Quantity{200});
}

TEST(QuantityArithmetic, MultipleFillsInLoop) {
    Quantity incoming{1000};
    
    std::uint64_t resting[] = {300, 400, 500};
    std::uint64_t fills[3] = {};
    
    for (int i = 0; i < 3 && incoming.raw() > 0; ++i) {
        Quantity avail{resting[i]};
        Quantity fill = std::min(incoming, avail);
        fills[i] = fill.raw();
        incoming -= fill;
    }
    
    EXPECT_EQ(fills[0], 300ULL);
    EXPECT_EQ(fills[1], 400ULL);
    EXPECT_EQ(fills[2], 300ULL);
    EXPECT_EQ(incoming, kZeroQuantity);
}

TEST(QuantityArithmetic, ZeroQuantityConstant) {
    static_assert(kZeroQuantity.raw() == 0ULL);
    static_assert(kZeroQuantity == Quantity{0});
}

/* 
* ┌─────────────────────┐
* │ 4. Test Type Safety │
* └─────────────────────┘ 
*/

// !! NOTE !! //

/*
GCC has a known limitation: failed overload resolution inside a 
requires-expression produces a hard error instead of returning false.
SFINAE via std::void_t works correctly on all compilers and tests the
exact same property: "does T support operator+?"                        
*/


// * ─── SFINAE detection traits ─────────────────────────────────────────── *

template <typename T, typename = void>
struct can_add : std::false_type {};
template <typename T>
struct can_add<T, std::void_t<decltype(std::declval<T>() + std::declval<T>())>>
: std::true_type {};

template <typename T, typename = void>
struct can_sub : std::false_type {};
template <typename T>
struct can_sub<T, std::void_t<decltype(std::declval<T>() - std::declval<T>())>>
: std::true_type {};

template <typename T, typename = void>
struct can_add_assign : std::false_type {};
template <typename T>
struct can_add_assign<T, std::void_t<decltype(std::declval<T&>() += std::declval<T>())>>
: std::true_type {};

template <typename T, typename = void>
struct can_sub_assign : std::false_type {};
template <typename T>
struct can_sub_assign<T, std::void_t<decltype(std::declval<T&>() -= std::declval<T>())>>
: std::true_type {};

template <typename T>
constexpr bool has_arithmetic_v =
can_add<T>::value && can_sub<T>::value &&
can_add_assign<T>::value && can_sub_assign<T>::value;


// * ─── Actual Tests ────────────────────────────────────────────────────── *


TEST(TypeSafety, QuantityHasArithmetic) {
    static_assert(has_arithmetic_v<Quantity>);
}

TEST(TypeSafety, PriceHasNoArithmetic) {
    static_assert(!has_arithmetic_v<Price>);
}

TEST(TypeSafety, OrderIDHasNoArithmetic) {
    static_assert(!has_arithmetic_v<OrderID>);
}

TEST(TypeSafety, TimestampHasNoArithmetic) {
    static_assert(!has_arithmetic_v<Timestamp>);
}

TEST(TypeSafety, CrossTypeAssignmentPrevented) {
    static_assert(!std::is_assignable_v<Price&, Quantity>);
    static_assert(!std::is_assignable_v<Quantity&, Price>);
    static_assert(!std::is_assignable_v<OrderID&, Quantity>);
    static_assert(!std::is_constructible_v<Price, Quantity>);
    static_assert(!std::is_constructible_v<Quantity, OrderID>);
}

/* 
* ┌───────────────────────────────────────────┐
* │ 5. Test Price Conversion on the Cold Path │
* └───────────────────────────────────────────┘ 
*/

TEST(PriceConversion, RoundTrip_DefaultScale) {
    // Scale 8: real prices survive the double → int64 → double trip
    // within 1 ULP of the fixed-point representation.
    const std::vector<double> prices = {
        0.01, 0.10, 1.00, 9.99, 10.05, 42.00,
        100.50, 999.99, 1234.56789012, 9999.99,
    };
    for (double orig : prices) {
        auto  p    = price::from_double(orig);
        double back = price::to_double(p);
        EXPECT_NEAR(back, orig, 1e-8)
        << "Round-trip failed for " << orig
        << " (raw=" << p.raw() << ")";
    }
}

TEST(PriceConversion, KnownValues_DefaultScale) {
    // $10.05 at scale 8 → 10.05 × 10^8 = 1'005'000'000
    constexpr auto p = price::from_double(10.05);
    static_assert(p.raw() == 1'005'000'000LL);
    
    constexpr auto back = price::to_double(p);
    static_assert(back > 10.0499 && back < 10.0501);
}

TEST(PriceConversion, CustomScale) {
    // Scale 2 (typical equity: two decimal places, $0.01 ticks)
    auto p = price::from_double(99.95, 2);
    EXPECT_EQ(p.raw(), 9995LL);
    
    double back = price::to_double(p, 2);
    EXPECT_NEAR(back, 99.95, 1e-2);
}

TEST(PriceConversion, ZeroPrice) {
    constexpr auto p = price::from_double(0.0);
    static_assert(p.raw() == 0);
}

TEST(PriceConversion, NegativePrice) {
    auto p = price::from_double(-37.63);
    EXPECT_LT(p.raw(), 0);
    EXPECT_NEAR(price::to_double(p), -37.63, 1e-8);
}

TEST(PriceConversion, VerySmallPrice) {
    // Sub-cent: $0.00000001 at scale 8 → raw = 1
    auto p = price::from_double(0.00000001);
    EXPECT_EQ(p.raw(), 1);
}

TEST(PriceConversion, LargePrice) {
    // $100,000.00 at scale 8 → 10'000'000'000'000
    auto p = price::from_double(100'000.0);
    EXPECT_EQ(p.raw(), 10'000'000'000'000LL);
    EXPECT_NEAR(price::to_double(p), 100'000.0, 1e-8);
}

/* 
* ┌───────────────────────────────┐
* │ 6. Test Price Tick Arithmetic │
* └───────────────────────────────┘ 
*/

TEST(PriceTicks, AddPositiveTicks) {
    constexpr Price p{1000};
    constexpr auto moved = price::add_ticks(p, 5);
    static_assert(moved.raw() == 1005);
}

TEST(PriceTicks, AddNegativeTicks) {
    constexpr Price p{1000};
    constexpr auto moved = price::add_ticks(p, -3);
    static_assert(moved.raw() == 997);
}

TEST(PriceTicks, TicksBetween) {
    constexpr Price bid{10'000};
    constexpr Price ask{10'050};
    constexpr auto spread = price::ticksBetween(ask, bid);
    static_assert(spread == 50);
    
    // Reversed direction gives negative
    static_assert(price::ticksBetween(bid, ask) == -50);
}

TEST(PriceTicks, ZeroTicksIsIdentity) {
    constexpr Price p{42};
    static_assert(price::add_ticks(p, 0) == p);
}

/* 
* ┌──────────────────────────────────┐
* │ 7. Test Hashing (for unique IDs) │
* └──────────────────────────────────┘ 
*/

TEST(Hashing, OrderIDWorksAsUnorderedMapKey) {
    std::unordered_map<OrderID, int> map;
    map[OrderID{1}] = 10;
    map[OrderID{2}] = 20;
    map[OrderID{3}] = 30;
    
    EXPECT_EQ(map.at(OrderID{1}), 10);
    EXPECT_EQ(map.at(OrderID{2}), 20);
    EXPECT_EQ(map.at(OrderID{3}), 30);
    EXPECT_EQ(map.count(OrderID{999}), 0u);
}

TEST(Hashing, PriceWorksAsUnorderedMapKey) {
    // Price → PriceLevel* mapping is a possible index strategy
    std::unordered_set<Price> prices;
    prices.insert(Price{100});
    prices.insert(Price{200});
    prices.insert(Price{100});  // duplicate
    
    EXPECT_EQ(prices.size(), 2u);
    EXPECT_TRUE(prices.contains(Price{100}));
    EXPECT_TRUE(prices.contains(Price{200}));
    EXPECT_FALSE(prices.contains(Price{300}));
}

TEST(Hashing, DifferentValuesProduceDifferentHashes) {
    std::hash<OrderID> h;
    // Not guaranteed by spec, but any sane hash of sequential integers
    // should not collide on the first few values.
    std::unordered_set<std::size_t> seen;
    for (std::uint64_t i = 0; i < 1000; ++i) {
        seen.insert(h(OrderID{i}));
    }
    // Allow some collisions, but not many.
    EXPECT_GT(seen.size(), 990u);
}

/* 
* ┌───────────────┐
* │ 8. Test Enums │
* └───────────────┘ 
*/

TEST(Side, Size) { static_assert(sizeof(Side) == 1); }

TEST(Side, Opposite) {
    static_assert(opposite_side(Side::Buy)  == Side::Sell);
    static_assert(opposite_side(Side::Sell) == Side::Buy);
}

TEST(Side, to_string) {
    EXPECT_STREQ(to_string(Side::Buy),  "Buy");
    EXPECT_STREQ(to_string(Side::Sell), "Sell");
}

TEST(TimeInForce, to_string) {
    EXPECT_STREQ(to_string(TimeInForce::GTC), "GTC");
    EXPECT_STREQ(to_string(TimeInForce::IOC), "IOC");
    EXPECT_STREQ(to_string(TimeInForce::FOK), "FOK");
    EXPECT_STREQ(to_string(TimeInForce::DAY), "DAY");
}

TEST(OrderType, to_string) {
    EXPECT_STREQ(to_string(OrderType::Limit),  "Limit");
    EXPECT_STREQ(to_string(OrderType::Market), "Market");
}

TEST(OrderError, AllVariantsHaveNames) {
    // Ensures every enum value has a to_string entry.
    // If we add a new value and forget the switch case,
    // std::unreachable() will fire in debug or UBSan.
    EXPECT_STREQ(to_string(OrderError::InvalidPrice), "InvalidPrice");
    EXPECT_STREQ(to_string(OrderError::InvalidQuantity), "InvalidQuantity");
    EXPECT_STREQ(to_string(OrderError::InvalidOrderID), "InvalidOrderID");
    EXPECT_STREQ(to_string(OrderError::DuplicateOrderID), "DuplicateOrderID");
    EXPECT_STREQ(to_string(OrderError::OrderNotFound), "OrderNotFound");
    EXPECT_STREQ(to_string(OrderError::BookFull), "BookFull");
    EXPECT_STREQ(to_string(OrderError::PoolExhausted), "PoolExhausted");
    EXPECT_STREQ(to_string(OrderError::SelfTrade), "SelfTrade");
    EXPECT_STREQ(to_string(OrderError::WouldNotFill), "WouldNotFill");
}

TEST(Enums, AllSingleByte) {
    static_assert(sizeof(Side)        == 1);
    static_assert(sizeof(TimeInForce) == 1);
    static_assert(sizeof(OrderType)   == 1);
    static_assert(sizeof(OrderError)  == 1);
}

/* 
* ┌────────────────────┐
* │ 9. Test Formatting │
* └────────────────────┘ 
*/

TEST(Format, StrongTypes) {
    EXPECT_EQ(std::format("{}",  Price{42}), "42");
    EXPECT_EQ(std::format("{}",  Quantity{0}), "0");
    EXPECT_EQ(std::format("{}",  OrderID{999}), "999");
}

TEST(Format, Enums) {
    EXPECT_EQ(std::format("{}", Side::Buy), "Buy");
    EXPECT_EQ(std::format("{}", OrderError::BookFull), "BookFull");
    EXPECT_EQ(std::format("{}", TimeInForce::IOC), "IOC");
    EXPECT_EQ(std::format("{}", OrderType::Market), "Market");
}

TEST(Format, CompositeMessage) {
    auto msg = std::format("[{}] {} {} @ {}",
        OrderID{42}, Side::Sell, Quantity{100}, Price{1005});
    EXPECT_EQ(msg, "[42] Sell 100 @ 1005");
}

/* 
* ┌────────────────────────────────┐
* │ 10. Test StrongNumeric Concept │
* └────────────────────────────────┘ 
*/
TEST(Concept, StrongNumericSatisfied) {
    static_assert(StrongNumeric<Price>);
    static_assert(StrongNumeric<Quantity>);
    static_assert(StrongNumeric<OrderID>);
    static_assert(StrongNumeric<Timestamp>);

    static_assert(!StrongNumeric<int>);
    static_assert(!StrongNumeric<double>);
    static_assert(!StrongNumeric<Side>);
}

}  // namespace lob::test