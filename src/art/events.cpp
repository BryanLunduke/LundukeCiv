// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Event screens as 64x40 pixel scenes, painted from simple shapes in the
// manner of the Wonder plaques: a study by lamplight for a new advance, a
// hall with the people gathered for a new government, and walls under
// smoke for a city that changes hands. All original.

#include "sprites.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace art {

namespace {

constexpr int W = 64, H = 40;

struct Scene {
    Pixmap pm{W, H};

    void px(int x, int y, Argb c) { pm.set(x, y, c); }
    void rect(int x, int y, int w, int h, Argb c) { pm.fill_rect(x, y, w, h, c); }
    void hline(int x0, int x1, int y, Argb c)
    {
        for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x) px(x, y, c);
    }
    void vline(int x, int y0, int y1, Argb c)
    {
        for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y) px(x, y, c);
    }
    void line(int x0, int y0, int x1, int y1, Argb c)
    {
        const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
        const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        for (;;) {
            px(x0, y0, c);
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
    void disc(int cx, int cy, int r, Argb c)
    {
        for (int y = -r; y <= r; ++y)
            for (int x = -r; x <= r; ++x)
                if (x * x + y * y <= r * r + r / 2) px(cx + x, cy + y, c);
    }
    void glow(int cx, int cy, int r, Argb c)
    {
        // A soft ring of light: blended, fading outward.
        for (int y = -r; y <= r; ++y)
            for (int x = -r; x <= r; ++x) {
                const double d = std::sqrt(static_cast<double>(x * x + y * y));
                if (d > r) continue;
                const unsigned a = static_cast<unsigned>(110 * (1.0 - d / r));
                pm.blend(cx + x, cy + y, (a << 24) | (c & 0xFFFFFFu));
            }
    }
    void tri(int ax, int ay, int bx0, int bx1, int by, Argb c)
    {
        for (int y = ay; y <= by; ++y) {
            const double t = by == ay ? 1.0 : static_cast<double>(y - ay) / (by - ay);
            hline(static_cast<int>(std::lround(ax + (bx0 - ax) * t)), static_cast<int>(std::lround(ax + (bx1 - ax) * t)), y, c);
        }
    }
    void sky(Argb top, Argb low, int horizon)
    {
        for (int y = 0; y < horizon; ++y)
            for (int x = 0; x < W; ++x) {
                const int band = y * 4 / std::max(1, horizon);
                const bool lower = band >= 2 && (band == 3 || ((x + y) & 1));
                px(x, y, lower ? low : top);
            }
    }
    void frame()
    {
        const Argb g = rgb(0xF2C14E), d = rgb(0x8A6418);
        hline(0, W - 1, 0, g);
        vline(0, 0, H - 1, g);
        hline(0, W - 1, H - 1, d);
        vline(W - 1, 0, H - 1, d);
    }
    // A little standing figure, 3 wide and 6 tall, feet at (x, y).
    void person(int x, int y, Argb body, Argb head)
    {
        px(x + 1, y - 5, head);
        hline(x, x + 2, y - 4, body);
        hline(x, x + 2, y - 3, body);
        px(x + 1, y - 2, body);
        px(x, y - 1, body);
        px(x + 2, y - 1, body);
        px(x, y, body);
        px(x + 2, y, body);
    }
    // A flag on a pole: pole foot at (x, y), height h, cloth to the right.
    void flag(int x, int y, int h, const CivColors &c, bool tattered = false)
    {
        vline(x, y - h, y, rgb(0x5A3618));
        for (int j = 0; j < 4; ++j)
            for (int i = 1; i <= 6; ++i) {
                if (tattered && i > 3 + ((j * 3) % 4)) continue;
                const bool wave = ((i + j) % 3) == 0;
                px(x + i, y - h + j + (i > 3 ? 1 : 0), j == 0 ? c.light : (wave ? c.dark : c.main));
            }
    }
};

const Argb kNight = rgb(0x141C34), kNightLow = rgb(0x26345A), kDay = rgb(0x6FA8DC), kDayLow = rgb(0xA9CCEB);
const Argb kDusk = rgb(0x3B2C4A), kDuskLow = rgb(0xB0583C);
const Argb kStone = rgb(0xB8B0A0), kStoneD = rgb(0x6C6458), kStoneL = rgb(0xE4DED2);
const Argb kWood = rgb(0x8A5A2E), kWoodD = rgb(0x5A3618), kWoodL = rgb(0xB07A44);
const Argb kInk = rgb(0x1A1620), kPaper = rgb(0xF6F2E8), kPaperD = rgb(0xD8CCB0), kGold = rgb(0xF0C040), kGoldD = rgb(0xA87820);
const Argb kGrass = rgb(0x5CA040), kGrassD = rgb(0x2E6224), kSmoke = rgb(0x7A7680), kSmokeL = rgb(0xA8A4AC);
const Argb kFire = rgb(0xF08A30), kFireL = rgb(0xFFD060), kRoof = rgb(0xA0442C), kRoofD = rgb(0x6A2818);

void stars(Scene &s, int horizon, int seed)
{
    for (int i = 0; i < 26; ++i) {
        const int x = (i * 37 + seed * 11) % W, y = (i * 13 + seed * 7) % std::max(1, horizon - 2);
        s.px(x, y, i % 5 == 0 ? kGold : rgb(0xC8D0E8));
    }
}

void paint_advance(Scene &s, int advance)
{
    s.sky(kNight, kNightLow, 40);
    stars(s, 26, advance);
    // A window on the night, top right, with the moon.
    s.rect(42, 4, 16, 14, kStoneD);
    s.rect(43, 5, 14, 12, rgb(0x1E2A4C));
    s.disc(52, 9, 3, rgb(0xF4ECC8));
    s.disc(53, 8, 2, rgb(0x1E2A4C));
    s.vline(50, 5, 16, kStoneD);
    s.hline(43, 56, 11, kStoneD);
    // The back wall in lamplight.
    s.rect(1, 20, 62, 2, rgb(0x2A2440));
    // The table.
    s.rect(4, 28, 56, 3, kWood);
    s.hline(4, 59, 28, kWoodL);
    s.hline(4, 59, 30, kWoodD);
    s.rect(7, 31, 3, 8, kWoodD);
    s.rect(54, 31, 3, 8, kWoodD);
    // An open book, its pages lit.
    s.tri(22, 19, 10, 22, 27, kPaper);
    s.tri(22, 19, 22, 34, 27, kPaperD);
    s.rect(10, 26, 25, 2, kPaper);
    s.vline(22, 19, 27, kInk);
    for (int j = 21; j <= 25; j += 2) {
        s.hline(14 + (25 - j) / 2, 20, j, rgb(0x6A6070));
        s.hline(24, 30 - (25 - j) / 2, j, rgb(0x6A6070));
    }
    // A figure on the right page: each advance draws its own little mark.
    switch (advance % 4) {
    case 0: s.disc(28, 22, 2, kGoldD); break;                       // a sun
    case 1: s.tri(28, 20, 25, 31, 24, kGoldD); break;               // a pyramid
    case 2: s.line(25, 24, 31, 20, kGoldD); s.line(25, 20, 31, 24, kGoldD); break;   // a star of lines
    default: s.hline(25, 31, 22, kGoldD); s.vline(28, 20, 24, kGoldD); break;        // a cross-staff
    }
    // A rolled scroll and a quill pot.
    s.rect(38, 25, 10, 3, kPaperD);
    s.disc(38, 26, 1, kPaper);
    s.disc(47, 26, 1, kPaper);
    s.rect(52, 24, 3, 4, kInk);
    s.line(53, 24, 57, 17, kPaper);
    s.line(54, 23, 58, 17, kPaperD);
    // The oil lamp and its glow.
    s.rect(4, 25, 6, 3, kGoldD);
    s.rect(5, 24, 4, 1, kGold);
    s.px(10, 24, kGoldD);
    s.glow(9, 19, 16, rgb(0xFFD070));
    s.px(9, 22, kFireL);
    s.px(9, 21, kFire);
    s.px(9, 20, kFireL);
    s.frame();
}

void emblem(Scene &s, int govt, int cx, int cy)
{
    using civ::Government;
    switch (static_cast<Government>(govt)) {
    case Government::Despotism:   // a tower
        s.rect(cx - 2, cy - 3, 5, 6, kGoldD);
        s.px(cx - 2, cy - 4, kGoldD);
        s.px(cx, cy - 4, kGoldD);
        s.px(cx + 2, cy - 4, kGoldD);
        break;
    case Government::Monarchy:    // a crown
        s.rect(cx - 3, cy, 7, 2, kGold);
        s.vline(cx - 3, cy - 3, cy, kGold);
        s.vline(cx, cy - 4, cy, kGold);
        s.vline(cx + 3, cy - 3, cy, kGold);
        break;
    case Government::Communism:   // a five-point star
        s.tri(cx, cy - 4, cx - 2, cx + 2, cy, kGold);
        s.hline(cx - 4, cx + 4, cy - 1, kGold);
        s.line(cx - 2, cy, cx - 3, cy + 3, kGold);
        s.line(cx + 2, cy, cx + 3, cy + 3, kGold);
        break;
    case Government::Republic:    // a wreath
        for (int a = 0; a < 20; ++a) {
            if (a > 7 && a < 12) continue;
            const double t = a * 6.2831853 / 20 + 1.5708;
            s.px(cx + static_cast<int>(std::lround(4 * std::cos(t))), cy + static_cast<int>(std::lround(3 * std::sin(t))), kGold);
        }
        break;
    case Government::Democracy:   // a torch
        s.vline(cx, cy - 1, cy + 3, kGoldD);
        s.rect(cx - 1, cy - 2, 3, 1, kGold);
        s.px(cx, cy - 3, kFire);
        s.px(cx, cy - 4, kFireL);
        break;
    default:                      // anarchy: a broken ring
        s.line(cx - 3, cy - 3, cx + 3, cy + 3, kGoldD);
        break;
    }
}

void paint_government(Scene &s, int govt, const CivColors &civ)
{
    s.sky(kDay, kDayLow, 26);
    s.disc(8, 6, 3, rgb(0xFFE890));
    // Hills behind.
    s.tri(54, 14, 40, 68, 26, rgb(0x7AA888));
    s.tri(12, 17, -4, 28, 26, rgb(0x86B494));
    // The great hall: steps, columns, pediment.
    s.rect(14, 12, 36, 2, kStoneL);
    s.tri(32, 4, 13, 51, 12, kStone);
    s.hline(13, 51, 12, kStoneD);
    emblem(s, govt, 32, 9);
    s.rect(15, 14, 34, 12, rgb(0x8C8478));
    for (int x = 16; x <= 46; x += 5) {
        s.vline(x, 14, 25, kStoneL);
        s.vline(x + 1, 14, 25, kStone);
        s.vline(x + 2, 14, 25, kStoneD);
    }
    s.rect(29, 18, 6, 8, kInk);
    s.rect(11, 26, 42, 2, kStone);
    s.rect(9, 28, 46, 2, kStoneD);
    // Banners in the people's colours.
    for (int bx : {12, 50}) {
        s.vline(bx, 6, 27, kWoodD);
        s.rect(bx - 2, 7, 5, 9, civ.main);
        s.vline(bx - 2, 7, 15, civ.light);
        s.px(bx - 1, 16, civ.dark);
        s.px(bx + 1, 16, civ.dark);
    }
    // The square and the crowd.
    s.rect(1, 30, 62, 9, kGrass);
    s.hline(1, 62, 30, kGrassD);
    for (int i = 0; i < 15; ++i) {
        const int x = 3 + i * 4 + (i % 2), y = 37 + (i % 3 == 0 ? 0 : (i % 3 == 1 ? 1 : -1));
        s.person(x, std::min(38, y), i % 4 == 0 ? civ.dark : (i % 4 == 2 ? civ.main : rgb(0x3A3448)), rgb(0xE0B898));
    }
    s.frame();
}

void paint_city(Scene &s, const CivColors &winner, const CivColors &loser, bool lost)
{
    s.sky(lost ? kNight : kDusk, kDuskLow, 24);
    // Smoke rising from the city.
    for (int i = 0; i < 3; ++i) {
        const int sx = 18 + i * 14;
        for (int j = 0; j < 14; ++j) {
            const int wob = static_cast<int>(std::lround(2 * std::sin((j + i * 3) * 0.7)));
            s.disc(sx + wob + j / 3, 18 - j, 1 + (j > 6 ? 1 : 0), j % 3 ? kSmoke : kSmokeL);
        }
    }
    // Rooftops behind the wall, two of them burning.
    const int roofs[][2] = {{12, 6}, {20, 8}, {30, 7}, {40, 9}, {48, 6}};
    int n = 0;
    for (const auto &r : roofs) {
        s.rect(r[0], 22 - r[1] + 4, 7, r[1], rgb(0xC8B898));
        s.tri(r[0] + 3, 22 - r[1], r[0] - 1, r[0] + 7, 22 - r[1] + 4, kRoof);
        s.hline(r[0] - 1, r[0] + 7, 22 - r[1] + 4, kRoofD);
        if (n == 1 || n == 3) {
            s.px(r[0] + 2, 22 - r[1] - 1, kFire);
            s.px(r[0] + 3, 22 - r[1] - 2, kFireL);
            s.px(r[0] + 4, 22 - r[1] - 1, kFire);
        }
        ++n;
    }
    // The wall, its towers and the broken gate.
    s.rect(4, 22, 56, 10, kStone);
    s.hline(4, 59, 22, kStoneL);
    for (int x = 4; x < 60; x += 4) s.rect(x, 20, 2, 2, kStone);
    for (int j = 24; j < 32; j += 3)
        for (int x = 4 + (j % 2) * 2; x < 60; x += 6) s.hline(x, x + 3, j, kStoneD);
    for (int tx : {2, 54}) {
        s.rect(tx, 16, 8, 16, kStone);
        s.vline(tx, 16, 31, kStoneL);
        s.vline(tx + 7, 16, 31, kStoneD);
        for (int x = tx; x < tx + 8; x += 3) s.rect(x, 14, 2, 2, kStone);
    }
    s.rect(28, 24, 8, 8, kInk);
    s.line(28, 31, 31, 26, kWoodL);   // the gate, thrown down
    s.line(35, 31, 33, 27, kWood);
    // The new owner's flag on the towers, the old one torn on the ground.
    s.flag(6, 14, 7, winner);
    s.flag(58, 14, 7, winner);
    s.rect(1, 32, 62, 7, rgb(0x6A5A3C));
    s.hline(1, 62, 32, rgb(0x4A3E28));
    for (int i = 0; i < 4; ++i) s.rect(40 + i * 2, 35 + (i % 2), 2, 1, i % 2 ? loser.dark : loser.main);
    // Soldiers at the gate in the victors' colours.
    for (int i = 0; i < 5; ++i) s.person(18 + i * 6 + (i > 1 ? 4 : 0), 37, i % 2 ? winner.main : winner.dark, rgb(0xE0B898));
    s.frame();
}

} // namespace

Pixmap event_scene(EventScene kind, int arg, const CivColors &civ, const CivColors &other)
{
    Scene s;
    switch (kind) {
    case EventScene::Advance: paint_advance(s, arg); break;
    case EventScene::Government: paint_government(s, arg, civ); break;
    case EventScene::CityTaken: paint_city(s, civ, other, false); break;
    case EventScene::CityLost: paint_city(s, other, civ, true); break;
    }
    return s.pm;
}

int event_scene_width() { return W; }
int event_scene_height() { return H; }

} // namespace art
