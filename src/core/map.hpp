// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

// The 80x50 world, wrapping east to west, and its generator.

#include "rng.hpp"
#include "rules.hpp"

#include <cstdint>
#include <vector>

namespace civ {

constexpr int kMapW = 80;
constexpr int kMapH = 50;

struct Tile {
    Terrain terrain = Terrain::Ocean;
    bool special = false;
    bool hut = false;
    bool road = false;
    bool irrigation = false;
    bool mine = false;
    bool fortress = false;
    bool railroad = false;     // on a road: free movement, +50% shields
    bool pollution = false;    // halves the square's yield until cleaned
    std::uint8_t explored = 0; // bit per civ
    std::uint8_t visible = 0;  // bit per civ, refreshed each turn
    std::uint8_t continent = 0;
};

struct Map {
    int w = kMapW;
    int h = kMapH;
    std::vector<Tile> tiles;

    Map() : tiles(static_cast<size_t>(kMapW) * kMapH) {}
    int wrap_x(int x) const { return ((x % w) + w) % w; }
    bool valid_y(int y) const { return y >= 0 && y < h; }
    Tile &at(int x, int y) { return tiles[static_cast<size_t>(y) * w + wrap_x(x)]; }
    const Tile &at(int x, int y) const { return tiles[static_cast<size_t>(y) * w + wrap_x(x)]; }
    bool land(int x, int y) const { return valid_y(y) && at(x, y).terrain != Terrain::Ocean; }
    bool ocean(int x, int y) const { return valid_y(y) && at(x, y).terrain == Terrain::Ocean; }
    // Shortest east-west offset (the map wraps).
    int dx(int x0, int x1) const
    {
        int d = wrap_x(x1) - wrap_x(x0);
        if (d > w / 2) d -= w;
        if (d < -w / 2) d += w;
        return d;
    }
    int distance(int x0, int y0, int x1, int y1) const;  // Chebyshev, with wrap
    bool coastal(int x, int y) const;                    // land next to ocean
    bool next_to_land(int x, int y) const;
    void label_continents();
};

struct MapSettings {
    std::uint64_t seed = 1;
    int land_pct = 32;     // share of the map that is land
    bool huts = true;
};

Map generate_map(const MapSettings &s);

// Specials appear on a fixed pattern of tiles, so a few seen grasslands
// let a player guess where the rest are (as in the original).
bool special_pattern(int x, int y);

} // namespace civ
