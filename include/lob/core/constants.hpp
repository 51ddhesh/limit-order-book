// Copyright 2026 51ddhesh
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/*
    lob::core::constants
    Compile Time tunables for a single order book
*/

#pragma once


#include "types.hpp" 

namespace lob {
// * ─── Passed to the OrderBook constructor ─────────────────────────────── *
struct BookConfig {
    std::size_t max_orders = 1'000'000;
    std::size_t max_price_levels = 10'000;
    int price_scales = price::kDefaultScale;
    std::int64_t tick_size = 1; // minimum price increment
};

// * ─── Compile Time tunable ────────────────────────────────────────────── *
inline constexpr BookConfig kDefaultConfig{};
} // namespace lob
