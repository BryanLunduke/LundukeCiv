// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

// The terrain painter's view of the world: what each square looks like,
// with no rules attached. The game converts its map into this, paints the
// whole world once into a cache, and repaints a few squares when settlers
// change them.

#include "map.hpp"
#include "sprites.hpp"

#include <functional>
#include <vector>

namespace art {

struct PaintTile {
    Terrain terrain = Terrain::Ocean;
    Special special = Special::None;
    bool river = false;
    bool road = false;
    bool rail = false;
    bool pollution = false;
    bool irrigated = false;
    bool mine = false;
    bool hut = false;
    bool city = false;
};

struct PaintWorld {
    int w = civ::kMapW;
    int h = civ::kMapH;
    std::vector<PaintTile> tiles;

    PaintWorld() : tiles(static_cast<size_t>(civ::kMapW) * civ::kMapH) {}
    PaintTile &at(int x, int y);
    const PaintTile &at(int x, int y) const;
    bool land(int x, int y) const { return at(x, y).terrain != Terrain::Ocean; }
    bool city_at(int x, int y) const { return at(x, y).city; }
};

// What one rules square looks like.
PaintTile paint_tile(const civ::Tile &t, bool city);
PaintWorld paint_world(const civ::Map &m, const std::vector<std::pair<int, int>> &city_squares);

// Paint squares [x0, x0+cols) x [y0, y0+rows) at native size (32 px a square).
Pixmap render_terrain(const PaintWorld &w, int x0, int y0, int cols, int rows);

// Black out unexplored squares of a pixmap whose top-left is square (tx0, ty0),
// with a ragged dithered edge.
void apply_fog(Pixmap &pm, int tx0, int ty0, const std::function<bool(int, int)> &explored);

// The gold corner brackets around the active unit.
void bracket(Pixmap &pm, int ox, int oy, Argb c, Argb shadow);

// One isolated square, for the art sheet.
Pixmap render_terrain_sample(Terrain t, bool river = false, bool road = false, bool irrigated = false, bool mine = false,
                             Special special = Special::None, int variant = 0);

Argb minimap_color(civ::Terrain t);

} // namespace art
