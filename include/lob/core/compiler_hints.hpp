// Copyright 2026 51ddhesh
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/*
    lob::core::compiler_hints
    Compiler specific intrinsics
*/

#pragma once


#include <cstddef>

namespace lob {

// * ─── Cache Line ──────────────────────────────────────────────────────── *
inline constexpr std::size_t kCacheLineSize = 64;    
    
} // namespace lob



/* 
   * ┌──────────────────────────┐
   * │ GCC (and clang) specific │
   * │ attributes and builtins  │
   * └──────────────────────────┘ 
*/

#if defined(__GNUC__) || defined(__clang__) 
    #define LOB_FORCE_INLINE [[gnu::always_inline]] inline
    #define LOB_NOINLINE [[gnu::noinline]]
    #define LOB_COLD [[gnu::cold]]
    #define LOB_HOT [[gnu::hot]]
    #define LOB_PREFETCH(p) __builtin_prefetch(p)
    #define LOB_LIKELY(x) (__builtin_expect(!!(x), 1))
    #define LOB_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else 
    #define LOB_FORCE_INLINE inline
    #define LOB_NOINLINE
    #define LOB_COLD 
    #define LOB_HOT 
    #define LOB_PREFETCH(p) ((void)(p))
    #define LOB_LIKELY(x) (!!(x))
    #define LOB_UNLIKELY(x) (!!(x))
#endif
