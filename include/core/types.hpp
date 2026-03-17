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

/// Fixed point price. raw() == real_price * 10 ^ SCALE 
using Price = StrongType<detail::PriceTag, std::int64_t>;

/// Quantity. Always >= 0
using Quantity = StrongType<detail::QuantityTag, std::int64_t>;

/// Unique. Monotonically increasing order identifier.
using OrderID = StrongType<detail::OrderIDTag, std::int64_t>;

/// TSC
using Timestamp = StrongType<detail::TimestampTag, std::int64_t>;

} // namespace lob
