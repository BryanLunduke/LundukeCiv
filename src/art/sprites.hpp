// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

// Lunduke Civ pixel art. Every sprite here is hand-authored as character
// rows in art.cpp, or painted procedurally in map_render.cpp. Native size
// is 32x32 per map tile, drawn at a whole-number scale.

#include "pixmap.hpp"
#include "rules.hpp"

#include <string>
#include <vector>

namespace art {

constexpr int kTile = 32;

enum class Terrain { Ocean, Grassland, Plains, Desert, Tundra, Arctic, Forest, Jungle, Swamp, Hills, Mountains, Count };
enum class Special { None, Fish, Horses, Oasis, Oil, Game, Gems, Gold, Coal, Seals, Shield, Count };

struct CivColors {
    Argb main;
    Argb light;
    Argb dark;
};

const char *terrain_name(Terrain t);
const char *special_name(Special s);
// The terrain a special sits on (for the sheet).
Terrain special_home(Special s);

// Hand-authored sprite rows. Characters map to the fixed sprite palette;
// C/L/D take the owning civilization's colours.
Pixmap sprite(const std::vector<std::string> &rows, const CivColors *civ = nullptr);

// A board-game counter: civ-coloured card, parchment window with the
// figure, attack-defense-moves strip, optional status letter (F, S, I...).
Pixmap unit_card(civ::UnitType u, const CivColors &civ, char status = 0, bool veteran = false);
// The bare figure (22x17) for lists and the Civilopedia.
Pixmap unit_figure(civ::UnitType u, const CivColors &civ);
// A village hut (goody hut) and a fortress rampart overlay.
Pixmap hut_sprite();
Pixmap fortress_sprite();
Pixmap special_sprite(Special s);
Pixmap mine_sprite();
// A city drawn on its own 32x32 tile: houses grow with size, a temple or
// palace for the capital, an optional stone wall, and a size plate.
Pixmap city_tile(int size, bool walls, bool capital, const CivColors &civ, int seed);

// The 3x5 pixel font for counters and plates.
void tiny_text(Pixmap &pm, int x, int y, const std::string &text, Argb ink);
int tiny_width(const std::string &text);

// A Wonder of the World as a framed 32x32 plaque (wonders.cpp).
Pixmap wonder_icon(int wonder);

// The spaceship blueprint (space.cpp): frame sections, fuel/engine pairs
// and modules in build order; empty slots dashed; engines firing in flight.
Pixmap spaceship(int structurals, int components, int modules, bool firing);
int spaceship_width();
int spaceship_height();
// 32x32 icon for a spaceship part (0 structural, 1 component, 2 module).
Pixmap space_part_icon(int part);

// Food, shield and trade glyphs (7x7) for yield readouts.
Pixmap yield_icon(int kind);

} // namespace art
