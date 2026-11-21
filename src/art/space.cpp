// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// The spaceship, painted pixel by pixel: girder frame sections, fuel tanks
// and rocket engines, and three kinds of module, laid out on a fixed
// blueprint so the ship grows from the engines toward the nose.

#include "sprites.hpp"

#include <algorithm>
#include <cmath>

namespace art {

namespace {

constexpr int kCell = 16;
constexpr int kCols = 24;
constexpr int kRows = 11;

void line(Pixmap &pm, int x0, int y0, int x1, int y1, Argb c)
{
    const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        pm.set(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// A girder square: a riveted frame with a cross brace.
void structural(Pixmap &pm, int ox, int oy)
{
    const Argb lit = rgb(0xC4CCD8), mid = rgb(0x8C96A4), dark = rgb(0x4A5260), rivet = rgb(0xE8ECF2);
    for (int i = 0; i < kCell; ++i) {
        pm.set(ox + i, oy, lit);
        pm.set(ox + i, oy + 1, mid);
        pm.set(ox + i, oy + kCell - 2, mid);
        pm.set(ox + i, oy + kCell - 1, dark);
        pm.set(ox, oy + i, lit);
        pm.set(ox + 1, oy + i, mid);
        pm.set(ox + kCell - 2, oy + i, mid);
        pm.set(ox + kCell - 1, oy + i, dark);
    }
    line(pm, ox + 2, oy + 2, ox + kCell - 3, oy + kCell - 3, mid);
    line(pm, ox + 2, oy + kCell - 3, ox + kCell - 3, oy + 2, mid);
    line(pm, ox + 3, oy + 2, ox + kCell - 3, oy + kCell - 4, dark);
    for (int k : {1, kCell - 2}) {
        pm.set(ox + k, oy + 1, rivet);
        pm.set(ox + k, oy + kCell - 2, rivet);
    }
}

// A fuel tank lying along the ship: pale hull, two red bands, rounded caps.
void fuel_tank(Pixmap &pm, int ox, int oy)
{
    for (int y = 3; y <= 12; ++y) {
        const Argb c = y <= 4 ? rgb(0xF4F0E4) : (y <= 8 ? rgb(0xD0CABC) : (y <= 10 ? rgb(0xA49E90) : rgb(0x6E685E)));
        const int inset = (y == 3 || y == 12) ? 2 : ((y == 4 || y == 11) ? 1 : 0);
        for (int x = 1 + inset; x <= 14 - inset; ++x) pm.set(ox + x, oy + y, c);
    }
    for (int y = 4; y <= 11; ++y) {
        pm.set(ox + 5, oy + y, y <= 6 ? rgb(0xE86A5A) : rgb(0xA82A22));
        pm.set(ox + 10, oy + y, y <= 6 ? rgb(0xE86A5A) : rgb(0xA82A22));
    }
    // A strut back to the frame.
    pm.fill_rect(ox + 14, oy + 7, 2, 2, rgb(0x8C96A4));
}

// A rocket engine, its bell opening toward the stern (left).
void engine(Pixmap &pm, int ox, int oy, bool firing)
{
    for (int x = 0; x < kCell; ++x) {
        const int half = x >= 10 ? 2 : 2 + (10 - x) * 5 / 10;   // bell widens to the left
        for (int y = 8 - half; y <= 7 + half; ++y) {
            const int d = y - (8 - half);
            Argb c = rgb(0x6A7280);
            if (d <= 1) c = rgb(0xAAB2BE);
            if (y >= 6 + half) c = rgb(0x3A404C);
            pm.set(ox + x, oy + y, c);
        }
    }
    for (int y = 3; y <= 12; ++y) pm.set(ox + 1, oy + y, rgb(0x2A2E36));   // the bell's mouth
    pm.fill_rect(ox + 10, oy + 5, 6, 6, rgb(0x8C96A4));                    // pump housing
    pm.fill_rect(ox + 11, oy + 6, 4, 1, rgb(0xC4CCD8));
    if (firing) {
        for (int y = 4; y <= 11; ++y) pm.set(ox + 1, oy + y, rgb(0xFFF0B0));
    }
}

// The exhaust plume behind a firing engine: three cells, hot to cool.
void plume(Pixmap &pm, int ox, int oy)
{
    for (int x = 0; x < 3 * kCell; ++x) {
        const double t = 1.0 - x / (3.0 * kCell);   // 1 at the engine
        const int half = static_cast<int>(1 + 5 * t + 2 * std::sin(x * 0.7));
        for (int y = 8 - half; y <= 7 + half; ++y) {
            const int d = std::abs(2 * y - 15);
            const double heat = t - d / 40.0;
            Argb c;
            if (heat > 0.7) c = rgb(0xFFF6D0);
            else if (heat > 0.45) c = rgb(0xF8D058);
            else if (heat > 0.2) c = rgb(0xE88A2A);
            else c = rgb(0xA8401C);
            if (heat < 0.25 && ((x + y) & 1)) continue;   // a ragged, dithered tail
            pm.set(ox + 3 * kCell - 1 - x, oy + y, c);
        }
    }
}

void hull(Pixmap &pm, int ox, int oy)
{
    const int s = 2 * kCell;
    for (int y = 2; y < s - 2; ++y) {
        for (int x = 1; x < s - 1; ++x) {
            const bool corner = (y == 2 || y == s - 3) && (x < 3 || x > s - 4);
            if (corner) continue;
            Argb c = rgb(0xC8CCD4);
            if (y <= 3) c = rgb(0xE8ECF2);
            if (y >= s - 5) c = rgb(0x8A909C);
            if (x == 1 || x == s - 2 || y == 2 || y == s - 3) c = rgb(0x4A5260);
            pm.set(ox + x, oy + y, c);
        }
    }
}

void habitation(Pixmap &pm, int ox, int oy)
{
    hull(pm, ox, oy);
    for (int row = 0; row < 3; ++row) {
        for (int k = 0; k < 6; ++k) {
            const int x = ox + 5 + k * 4, y = oy + 8 + row * 6;
            pm.fill_rect(x, y, 2, 3, (k + row) % 3 ? rgb(0xF2D060) : rgb(0x6A5A30));
        }
    }
    pm.fill_rect(ox + 14, oy + 3, 4, 2, rgb(0x6AA0E0));   // docking light
}

void life_support(Pixmap &pm, int ox, int oy)
{
    hull(pm, ox, oy);
    // A glass dome with greenery inside.
    const int cx = ox + 16, cy = oy + 18;
    for (int y = -10; y <= 0; ++y) {
        for (int x = -11; x <= 11; ++x) {
            if (x * x + y * y * 1.4 > 120) continue;
            Argb c = rgb(0x9ED8E8);
            if (y > -5) c = ((x * 3 + y * 5) & 3) ? rgb(0x3E9A3A) : rgb(0x90D880);
            if (x * x + y * y * 1.4 > 100) c = rgb(0xD8F0F8);
            pm.set(cx + x, cy + y, c);
        }
    }
    pm.fill_rect(ox + 5, oy + 22, 22, 2, rgb(0x4A5260));
    for (int k = 0; k < 4; ++k) pm.fill_rect(ox + 7 + k * 5, oy + 25, 2, 2, rgb(0x6EE07A));
}

void solar(Pixmap &pm, int ox, int oy)
{
    // A mast across the middle and two panel wings.
    pm.fill_rect(ox + 14, oy + 1, 4, 30, rgb(0x8C96A4));
    pm.fill_rect(ox + 15, oy + 1, 1, 30, rgb(0xC4CCD8));
    for (int wing = 0; wing < 2; ++wing) {
        const int wx = ox + (wing ? 19 : 1);
        for (int y = 3; y < 29; ++y) {
            for (int x = 0; x < 12; ++x) {
                Argb c = rgb(0x2E5A9E);
                if (x % 4 == 0 || y % 5 == 3) c = rgb(0x8CB4E8);
                else if ((x + y) % 7 == 0) c = rgb(0x4A7AC0);
                pm.set(wx + x, oy + y, c);
            }
        }
    }
}

void slot_outline(Pixmap &pm, int ox, int oy, int w, int h)
{
    const Argb c = rgb(0x3E5A80);
    for (int x = 0; x < w; x += 2) {
        pm.set(ox + x, oy, c);
        pm.set(ox + x, oy + h - 1, c);
    }
    for (int y = 0; y < h; y += 2) {
        pm.set(ox, oy + y, c);
        pm.set(ox + w - 1, oy + y, c);
    }
}

void nose(Pixmap &pm, int ox, int oy)
{
    // A pointed cap three cells tall and two long.
    for (int x = 0; x < 2 * kCell; ++x) {
        const int half = static_cast<int>((2 * kCell - x) * 24 / (2 * kCell));
        for (int y = 24 - half; y < 24 + half; ++y) {
            Argb c = rgb(0xC8CCD4);
            if (y < 24 - half + 3) c = rgb(0xE8ECF2);
            if (y >= 24 + half - 3) c = rgb(0x8A909C);
            pm.set(ox + x, oy + y, c);
        }
    }
    pm.fill_rect(ox + 4, oy + 22, 6, 4, rgb(0xC83A2E));
}

// Where each part goes on the blueprint, in build order.
int component_row(int pair)
{
    static const int rows[8] = {5, 4, 6, 3, 7, 2, 8, 1};
    return rows[pair % 8];
}

} // namespace

int spaceship_width() { return kCols * kCell; }
int spaceship_height() { return kRows * kCell; }

Pixmap spaceship(int structurals, int components, int modules, bool firing)
{
    Pixmap pm(kCols * kCell, kRows * kCell);
    // Frame: middle row first, then the rows either side, stern to bow.
    for (int k = 0; k < civ::kMaxStructurals; ++k) {
        const int row = k < 13 ? 5 : (k % 2 ? 4 : 6);
        const int col = 8 + (k < 13 ? k : (k - 13) / 2);
        const int ox = col * kCell, oy = row * kCell;
        if (k < structurals) structural(pm, ox, oy);
        else slot_outline(pm, ox + 2, oy + 2, kCell - 4, kCell - 4);
    }
    if (structurals > 0) nose(pm, 21 * kCell, 4 * kCell - 8);
    // Engines and tanks: pair p has its tank at column 7 and engine at 6.
    for (int k = 0; k < civ::kMaxComponents; ++k) {
        const int pair = k / 2, row = component_row(pair);
        const bool tank = k % 2 == 0;   // fuel first, then propulsion
        const int ox = (tank ? 7 : 6) * kCell, oy = row * kCell;
        if (k < components) {
            if (tank) {
                fuel_tank(pm, ox, oy);
            } else {
                const bool fed = k - 1 < components;
                engine(pm, ox, oy, firing && fed);
                if (firing && fed) plume(pm, 3 * kCell, oy);
            }
        } else {
            slot_outline(pm, ox + 2, oy + 2, kCell - 4, kCell - 4);
        }
    }
    // Modules above and below the frame.
    for (int k = 0; k < civ::kMaxModules; ++k) {
        const int col = 9 + (k / 2) * 2, row = k % 2 ? 7 : 2;
        const int ox = col * kCell, oy = row * kCell;
        if (k < modules) {
            switch (k % 3) {
            case 0: habitation(pm, ox, oy); break;
            case 1: life_support(pm, ox, oy); break;
            default: solar(pm, ox, oy); break;
            }
        } else {
            slot_outline(pm, ox + 2, oy + 2, 2 * kCell - 4, 2 * kCell - 4);
        }
    }
    return pm;
}

Pixmap space_part_icon(int part)
{
    Pixmap pm(32, 32, rgb(0x0E1424));
    // A few stars.
    const int stars[][2] = {{3, 4}, {27, 3}, {6, 27}, {29, 22}, {16, 2}, {2, 15}};
    for (const auto &s : stars) pm.set(s[0], s[1], rgb(0xC8D4F0));
    switch (part) {
    case 0:
        structural(pm, 0, 8);
        structural(pm, 16, 8);
        break;
    case 1:
        engine(pm, 0, 8, true);
        fuel_tank(pm, 16, 8);
        break;
    default: {
        Pixmap m(32, 32);
        habitation(m, 0, 0);
        pm.blit(m, 0, 0);
        break;
    }
    }
    for (int i = 0; i < 32; ++i) {
        pm.set(i, 0, rgb(0x4A5A78));
        pm.set(i, 31, rgb(0x4A5A78));
        pm.set(0, i, rgb(0x4A5A78));
        pm.set(31, i, rgb(0x4A5A78));
    }
    return pm;
}

} // namespace art
