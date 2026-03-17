// Copyright 2026 51ddhesh
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/*
    lob::core::types
    Core vocabulary types for the Limit Order Book
*/

/*
    Design Principles
   -------------------
    1. All prices are fixed point integers. No floating point values on the hot path.
    2. Every quantity has its own distinct C++ type so that the compiler can catch mismatches.
    3. The wrapper is constexpr, trivially copyable and the same size as the underlying integer.
*/

#pragma once


#include <compare>
#include <concepts>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>


namespace lob {

/* 
   * ┌────────────────────────────────────┐
   * │ The tags live in the lob::detail,  │
   * │ users wouldn't spell them directly │
   * └────────────────────────────────────┘ 
*/
namespace detail {

struct PriceTag{};
struct QuantityTag{};
struct OrderIDTag{};
struct TimestampTag{};

} // namespace lob::detail

template <typename Tag, typename Underlying = std::int64_t>
    requires std::is_arithmetic_v<Underlying>
class StrongType {
private:
    Underlying value_{};
    
public:
    using underlying_type = Underlying;
    using tag_type = Tag;
    
    constexpr StrongType() noexcept = default;
    constexpr explicit StrongType(Underlying v) noexcept : value_(v) {}
    
    // * ─── Access ──────────────────────────────────────────────────────────── *
    
    [[nodiscard]] constexpr Underlying raw() const noexcept { return value_; }
    constexpr explicit operator Underlying() const noexcept { return value_; }
    
    // * ─── Comparison (Compiler Generated) ─────────────────────────────────── *
    
    constexpr auto operator<=>(const StrongType&) const noexcept = default;
    constexpr bool operator==(const StrongType&) const noexcept = default;
    
    
    // * ─── Sentinel / invalid value ────────────────────────────────────────── *
    // Useful for "no value" semantics in pool-based systems where 
    // std::optional would add a bool (and a branch) we don't want.
    
    [[nodiscard]] static constexpr StrongType invalid() noexcept {
        return StrongType{std::numeric_limits<Underlying>::max()};
    }
    
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value_ != std::numeric_limits<Underlying>::max();
    }
    
}; // class lob::StrongType


// * ─── Concept satisfied by any StrongType instantiation ───────────────── *

template <typename T>
concept StrongNumeric = requires (T t) {
    typename T::underlying_type;
    typename T::tag_type;
    
    { t.raw() } -> std::same_as<typename T::underlying_type>;
};


/* 
* ┌─────────────────────┐
* │ Domain Type aliases │
* └─────────────────────┘ 
*/

using Price = StrongType<detail::PriceTag, std::int64_t>;
using Quantity = StrongType<detail::QuantityTag, std::uint64_t>;
using OrderID = StrongType<detail::OrderIDTag, std::uint64_t>;
using Timestamp = StrongType<detail::TimestampTag,std::uint64_t>;

    
/* 
* ┌─────────────────────┐
* │ Quantity Arithmetic │
* └─────────────────────┘ 
*/

// Only the StrongType get arithmetic because the matching engine does quantity math on every fill.
// Price arithmetic goes through named free functions below to keep intent explicit.
constexpr Quantity operator+(Quantity a, Quantity b) noexcept {
    return Quantity{a.raw() + b.raw()};
}

constexpr Quantity operator-(Quantity a, Quantity b) noexcept {
    return Quantity{a.raw() - b.raw()};
}

constexpr Quantity& operator+=(Quantity& a, Quantity b) noexcept {
    a = Quantity{a.raw() + b.raw()};
    return a;
}

constexpr Quantity& operator-=(Quantity& a, Quantity b) noexcept {
    a = Quantity{a.raw() - b.raw()};
    return a;
}

inline constexpr Quantity kZeroQuantity{0ULL};


/* 
* ┌──────┐
* │ Side │
* └──────┘ 
*/

enum class Side : std::uint8_t {
    Buy = 0,
    Sell = 1,
};

[[nodiscard]] constexpr Side opposite_side(Side s) noexcept {
    return s == Side::Buy ? Side::Sell : Side::Buy;
}

[[nodiscard]] constexpr const char* to_string(Side s) noexcept {
    return s == Side::Buy ? "Buy" : "Sell";
}


/* 
* ┌───────────────┐
* │ Time in Force │
* └───────────────┘ 
*/

enum class TimeInForce : std::uint8_t {
    GTC = 0, // GOod till Cancel
    IOC = 1, // Immediate or Cancel
    FOK = 2, // Fill or Kill
    DAY = 3, // Day order
};

[[nodiscard]] constexpr const char* to_string(TimeInForce tif) noexcept {
    switch (tif) {
        case TimeInForce::GTC: return "GTC";
        case TimeInForce::IOC: return "IOC";
        case TimeInForce::FOK: return "FOK";
        case TimeInForce::DAY: return "DAY";
    }
    
    std::unreachable();
}


/* 
* ┌────────────┐
* │ Order Type │
* └────────────┘ 
*/

enum class OrderType : std::uint8_t {
    Limit = 0,
    Market = 1,
};

[[nodiscard]] constexpr const char* to_string(OrderType type) noexcept {
    switch (type) {
        case OrderType::Limit: return "Limit";
        case OrderType::Market: return "Market";
    }
    
    std::unreachable();
}

/* 
* ┌─────────────┐
* │ Order Error │
* └─────────────┘ 
*/

// Used with std::expected on the hot path

enum class OrderError : std::uint8_t {
    InvalidPrice,
    InvalidQuantity,
    InvalidOrderID,
    DuplicateOrderID,
    OrderNotFound,
    BookFull,
    PoolExhausted,
    SelfTrade,
    WouldNotFill, // FOK rejected
};

[[nodiscard]] constexpr const char* to_string(OrderError e) noexcept {
    switch (e) {
        case OrderError::InvalidPrice: return "InvalidPrice";
        case OrderError::InvalidQuantity: return "InvalidQuantity";
        case OrderError::InvalidOrderID: return "InvalidOrderID";
        case OrderError::DuplicateOrderID: return "DuplicateOrderID";
        case OrderError::OrderNotFound: return "OrderNotFound";
        case OrderError::BookFull: return "BookFull";
        case OrderError::PoolExhausted: return "PoolExhausted";
        case OrderError::SelfTrade: return "SelfTrade";
        case OrderError::WouldNotFill: return "WouldNotFill";
    } 
    
    std::unreachable();
}


/* 
* ┌─────────────────┐
* │ Price Utilities │
* └─────────────────┘ 
*/

/*
Price is left out of arithmetic operators. The operations allowed are:
- Comparison (with the spaceship operator <=>)
- Tick Offset (add_ticks: returns a new price)
- Conversion (from_double / to_double - on the cold path)
*/ 

namespace price {
    // * ─── 10 ** 8 - eight decimal places ──────────────────────────────────── *
    inline constexpr int kDefaultScale = 8;
    
    // * ─── Convert double to fixed point ───────────────────────────────────── *
    // Done on the cold path
    // Uses 'round half away from zero'
    [[nodiscard]] constexpr Price from_double(double p, int scale = kDefaultScale) noexcept {
        // Build the multiplier 
        std::int64_t m = 1;
        for (int i = 0; i < scale; i++) m *= 10;
        
        double scaled = p * static_cast<double>(m);
        std::int64_t rounded = static_cast<std::int64_t>(scaled + (scaled >= 0.0 ? 0.5 : -0.5));
        
        return Price{rounded};
    }
    
    // * ─── Convert fixed point to double ───────────────────────────────────── *
    // For logging and displaying
    // Done on the cold path
    [[nodiscard]] constexpr double to_double(Price p, int scale = kDefaultScale) noexcept {
        std::int64_t m = 1;
        for (int i = 0; i < scale; i++) m *= 10;
        
        return static_cast<double>(p.raw()) / static_cast<double>(m);
    }
    
    // * ─── Move a price by a number of ticks ───────────────────────────────── *
    // Minimum price increments
    [[nodiscard]] constexpr Price add_ticks(Price p, std::int64_t ticks) noexcept {
        return Price{p.raw() + ticks};
    }
    
    // * ─── Difference between two prices in raw ticks ──────────────────────── *
    [[nodiscard]] constexpr std::int64_t ticksBetween(Price a, Price b) noexcept {
        return a.raw() - b.raw();
    }
    
} // namespace lob::price
} // namespace lob

/* 
* ┌─────────┐
* │ Hashing │
* └─────────┘ 

    - Required for OrderID in hash maps
*/

template <typename Tag, typename U>
struct std::hash<lob::StrongType<Tag, U>> {
    static std::size_t operator() (lob::StrongType<Tag, U> v) noexcept {
        return std::hash<U>{}(v.raw());
    }
};


/* 
* ┌────────────┐
* │ Formatting │
* └────────────┘ 

    * FOR DIAGNOSTICS AND LOGGING.
    !! NEVER ON THE HOT PATH
*/

template <typename Tag, typename U>
struct std::formatter<lob::StrongType<Tag, U>> : std::formatter<U> {
    auto format(lob::StrongType<Tag, U> v, std::format_context& ctx) const {
        return std::formatter<U>::format(v.raw(), ctx);
    }
};

template <>
struct std::formatter<lob::Side> : std::formatter<const char*> {
    auto format(lob::Side s, std::format_context& ctx) const {
        return std::formatter<const char*>::format(lob::to_string(s), ctx);
    } 
};

template <>
struct std::formatter<lob::OrderError> : std::formatter<const char*> {
    auto format(lob::OrderError e, std::format_context& ctx) const {
        return std::formatter<const char*>::format(lob::to_string(e), ctx);
    }
};

template <>
struct std::formatter<lob::TimeInForce> : std::formatter<const char*> {
    auto format(lob::TimeInForce t, std::format_context& ctx) const {
        return std::formatter<const char*>::format(lob::to_string(t), ctx);
    }
};

template <>
struct std::formatter<lob::OrderType> : std::formatter<const char*> {
    auto format(lob::OrderType type, std::format_context& ctx) const {
        return std::formatter<const char*>::format(lob::to_string(type), ctx);
    }
};
