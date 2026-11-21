// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

#include <cstdint>

namespace civ {

// Small, saveable generator (xorshift64*), so games replay the same way.
struct Rng {
    std::uint64_t state = 0x9E3779B97F4A7C15ull;

    explicit Rng(std::uint64_t seed = 1) { reseed(seed); }
    void reseed(std::uint64_t seed) { state = seed ? seed * 0x9E3779B97F4A7C15ull : 0x9E3779B97F4A7C15ull; }
    std::uint64_t next()
    {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 2685821657736338717ull;
    }
    // [0, n)
    int below(int n) { return n <= 0 ? 0 : static_cast<int>(next() % static_cast<std::uint64_t>(n)); }
    double unit() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
    bool chance(double p) { return unit() < p; }
};

} // namespace civ
