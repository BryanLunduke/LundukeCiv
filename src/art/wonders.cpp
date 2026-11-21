// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// The Wonders of the World as 32x32 plaques, painted pixel by pixel from
// simple shapes. Each one is an original little scene: a sky, a ground
// line and the wonder in the middle, with a gold frame.

#include "sprites.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace art {

namespace {

struct Painter {
    Pixmap pm{32, 32};

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
    void ring(int cx, int cy, int r, Argb c)
    {
        for (int a = 0; a < 64; ++a) {
            const double t = a * 6.2831853 / 64;
            px(cx + static_cast<int>(std::lround(r * std::cos(t))), cy + static_cast<int>(std::lround(r * std::sin(t))), c);
        }
    }
    // Filled triangle with a flat base: apex (ax, ay), base from bx0..bx1 at by.
    void tri(int ax, int ay, int bx0, int bx1, int by, Argb c)
    {
        for (int y = ay; y <= by; ++y) {
            const double t = by == ay ? 1.0 : static_cast<double>(y - ay) / (by - ay);
            const int l = static_cast<int>(std::lround(ax + (bx0 - ax) * t));
            const int r = static_cast<int>(std::lround(ax + (bx1 - ax) * t));
            hline(l, r, y, c);
        }
    }
    void sky(Argb top, Argb low, int horizon)
    {
        for (int y = 0; y < horizon; ++y)
            for (int x = 0; x < 32; ++x) {
                // Two-colour ordered dither across the middle band.
                const int band = y * 4 / std::max(1, horizon);
                const bool lower = band >= 2 && (band == 3 || ((x + y) & 1));
                px(x, y, lower ? low : top);
            }
    }
    void ground(int y, Argb c, Argb edge)
    {
        rect(0, y, 32, 32 - y, c);
        hline(0, 31, y, edge);
    }
    void water(int y, Argb c, Argb wave)
    {
        rect(0, y, 32, 32 - y, c);
        for (int j = y + 1; j < 32; j += 3)
            for (int i = (j * 5) % 7; i < 32; i += 7) hline(i, i + 2, j, wave);
    }
    void frame()
    {
        const Argb g = rgb(0xF2C14E), d = rgb(0x8A6418);
        for (int i = 0; i < 32; ++i) {
            px(i, 0, g);
            px(0, i, g);
            px(i, 31, d);
            px(31, i, d);
        }
    }
};

// Palette for the plaques.
const Argb kDay = rgb(0x6FA8DC), kDayLow = rgb(0xA9CCEB), kDusk = rgb(0x3B4C7A), kDuskLow = rgb(0xC07850);
const Argb kNight = rgb(0x141C34), kNightLow = rgb(0x26345A);
const Argb kSand = rgb(0xD8B878), kSandD = rgb(0xA8864C), kSandL = rgb(0xF0D8A0);
const Argb kGrass = rgb(0x5CA040), kGrassD = rgb(0x2E6224);
const Argb kStone = rgb(0xB8B0A0), kStoneD = rgb(0x6C6458), kStoneL = rgb(0xE4DED2);
const Argb kSea = rgb(0x2C5C9C), kSeaL = rgb(0x6C9CD8);
const Argb kInk = rgb(0x1A1620), kWhite = rgb(0xF6F2E8), kGold = rgb(0xF0C040), kGoldD = rgb(0xA87820);
const Argb kRed = rgb(0xC83A2E), kRedD = rgb(0x7E2018), kWood = rgb(0x8A5A2E), kWoodD = rgb(0x5A3618);
const Argb kBronze = rgb(0xC08848), kBronzeD = rgb(0x7C5228), kSteel = rgb(0xA8B4C4), kSteelD = rgb(0x5C6878);
const Argb kLeaf = rgb(0x3E8A3A), kLeafL = rgb(0x7CC860), kBlue = rgb(0x3A6CCF);

void columns(Painter &p, int x0, int x1, int y0, int y1, int step)
{
    for (int x = x0; x <= x1; x += step) {
        p.vline(x, y0, y1, kStoneL);
        p.vline(x + 1, y0, y1, kStoneD);
    }
}

void paint(Painter &p, civ::Wonder w)
{
    using civ::Wonder;
    switch (w) {
    case Wonder::Colossus:
        p.sky(kDay, kDayLow, 24);
        p.water(24, kSea, kSeaL);
        p.rect(4, 22, 7, 4, kStoneD);
        p.rect(21, 22, 7, 4, kStoneD);
        // Legs astride the harbour, body, raised arm with a flame.
        p.line(9, 22, 14, 13, kBronzeD);
        p.line(10, 22, 15, 13, kBronze);
        p.line(23, 22, 18, 13, kBronzeD);
        p.line(22, 22, 17, 13, kBronze);
        p.rect(14, 7, 5, 7, kBronze);
        p.vline(18, 7, 13, kBronzeD);
        p.disc(16, 5, 2, kBronze);
        p.line(18, 8, 22, 3, kBronze);
        p.disc(22, 2, 1, kGold);
        p.px(23, 1, rgb(0xFFF0A0));
        p.line(14, 8, 12, 12, kBronzeD);
        break;
    case Wonder::Pyramids:
        p.sky(kDay, kDayLow, 23);
        p.disc(26, 6, 3, rgb(0xFFE070));
        p.ground(23, kSand, kSandD);
        p.tri(14, 5, 1, 27, 23, kSand);
        p.tri(14, 5, 14, 27, 23, kSandD);
        for (int y = 9; y < 23; y += 4) p.hline(14 - (y - 5) * 13 / 18, 14, y, kSandL);
        p.tri(25, 14, 20, 30, 23, kSandL);
        p.tri(25, 14, 25, 30, 23, kSand);
        break;
    case Wonder::GreatWall:
        p.sky(kDay, kDayLow, 18);
        p.ground(18, kGrass, kGrassD);
        p.tri(6, 10, -6, 18, 18, kGrassD);
        p.tri(24, 8, 12, 36, 18, kGrassD);
        // The wall snakes over the hills toward a watchtower.
        for (int x = 0; x < 32; ++x) {
            const int y = 20 + static_cast<int>(std::lround(3 * std::sin(x * 0.35)));
            p.vline(x, y, y + 4, kStone);
            p.px(x, y + 4, kStoneD);
            if (x % 3 == 0) p.vline(x, y - 2, y - 1, kStone);
        }
        p.rect(22, 11, 6, 11, kStone);
        p.vline(27, 11, 21, kStoneD);
        for (int x = 22; x < 28; x += 2) p.px(x, 10, kStone);
        p.rect(24, 14, 2, 3, kInk);
        break;
    case Wonder::HangingGardens:
        p.sky(kDusk, kDuskLow, 26);
        p.ground(26, kSand, kSandD);
        for (int t = 0; t < 4; ++t) {
            const int w = 26 - t * 6, x = (32 - w) / 2, y = 22 - t * 5;
            p.rect(x, y, w, 5, kSandL);
            p.hline(x, x + w - 1, y + 4, kSandD);
            for (int i = x; i < x + w; i += 2) {
                p.px(i, y, kLeafL);
                p.vline(i + 1, y, y + 1 + (i * 7 + t) % 3, kLeaf);
            }
        }
        p.vline(16, 2, 6, kLeaf);
        p.disc(16, 3, 2, kLeafL);
        break;
    case Wonder::Lighthouse:
        p.sky(kNight, kNightLow, 24);
        p.water(24, kSea, kSeaL);
        p.tri(16, 18, 6, 26, 26, kStoneD);
        p.rect(12, 8, 8, 16, kStone);
        p.rect(18, 8, 2, 16, kStoneD);
        p.rect(11, 6, 10, 2, kStoneL);
        p.rect(13, 3, 6, 3, kGold);
        p.tri(16, 0, 13, 19, 2, kStoneL);
        // Beams.
        p.line(12, 4, 1, 1, rgb(0xFFF0A0));
        p.line(12, 5, 1, 7, rgb(0xC8B860));
        p.line(19, 4, 30, 1, rgb(0xFFF0A0));
        p.line(19, 5, 30, 7, rgb(0xC8B860));
        for (int y = 11; y < 22; y += 4) p.rect(15, y, 2, 2, kInk);
        break;
    case Wonder::GreatLibrary:
        p.sky(kDay, kDayLow, 26);
        p.ground(26, kSand, kSandD);
        p.tri(16, 5, 3, 29, 11, kStoneL);
        p.hline(3, 29, 11, kStoneD);
        p.rect(4, 12, 24, 2, kStone);
        columns(p, 5, 26, 14, 23, 4);
        p.rect(3, 24, 26, 2, kStoneD);
        // A scroll at the door.
        p.rect(13, 19, 6, 3, kSandL);
        p.vline(13, 18, 22, kWood);
        p.vline(18, 18, 22, kWood);
        p.hline(14, 17, 20, kInk);
        break;
    case Wonder::Oracle:
        p.sky(kDusk, kDuskLow, 25);
        p.ground(25, kGrass, kGrassD);
        p.tri(16, 6, 6, 26, 11, kRed);
        p.hline(6, 26, 11, kRedD);
        p.rect(7, 12, 18, 1, kStone);
        columns(p, 8, 23, 13, 22, 5);
        p.rect(5, 23, 22, 2, kStoneD);
        // Sacred vapours rising.
        for (int y = 2; y < 12; ++y) p.px(16 + static_cast<int>(std::lround(2 * std::sin(y * 0.9))), y - 1, rgb(0xD8D0F0));
        p.disc(16, 19, 2, kGold);
        break;
    case Wonder::Copernicus:
        p.sky(kNight, kNightLow, 24);
        for (int i = 0; i < 12; ++i) p.px((i * 11 + 3) % 31, (i * 7 + 2) % 18, kWhite);
        p.ground(24, kGrassD, kInk);
        p.rect(8, 16, 16, 8, kStone);
        p.vline(23, 16, 23, kStoneD);
        p.disc(16, 16, 7, kStoneL);
        p.rect(8, 17, 16, 7, kStone);
        p.vline(16, 9, 15, kInk);
        p.line(17, 13, 26, 5, kSteel);
        p.line(17, 14, 26, 6, kSteelD);
        p.rect(14, 19, 4, 5, kInk);
        p.disc(6, 6, 2, rgb(0xF0E8C0));
        break;
    case Wonder::Michelangelo:
        p.sky(kDay, kDayLow, 26);
        p.ground(26, kStoneD, kInk);
        p.rect(5, 16, 22, 10, kStoneL);
        p.rect(5, 16, 22, 1, kStoneD);
        p.disc(16, 13, 7, kSteel);
        p.rect(8, 14, 16, 2, kStoneL);
        for (int x = 11; x < 22; x += 3) p.vline(x, 7, 13, kSteelD);
        p.rect(15, 3, 2, 4, kGold);
        p.px(16, 2, kGold);
        for (int x = 7; x < 26; x += 4) p.rect(x, 19, 2, 4, kInk);
        break;
    case Wonder::Shakespeare:
        p.sky(kDay, kDayLow, 25);
        p.ground(25, kGrass, kGrassD);
        // A round timber playhouse.
        p.rect(5, 12, 22, 13, kWhite);
        for (int x = 5; x < 27; x += 4) p.vline(x, 12, 24, kWoodD);
        p.hline(5, 26, 18, kWoodD);
        p.tri(16, 6, 3, 29, 12, rgb(0xC8A048));
        p.hline(3, 29, 12, kWoodD);
        p.vline(16, 1, 6, kWoodD);
        p.rect(17, 1, 5, 3, kRed);
        p.rect(14, 20, 4, 5, kWoodD);
        break;
    case Wonder::Bach:
        p.sky(kDusk, kDuskLow, 27);
        p.ground(27, kWoodD, kInk);
        p.rect(3, 22, 26, 5, kWood);
        p.hline(3, 28, 22, kGoldD);
        for (int i = 0; i < 13; ++i) {
            const int x = 4 + i * 2;
            const int top = 6 + std::abs(i - 6) * 2;
            p.vline(x, top, 21, kSteel);
            p.px(x, top, kStoneL);
            p.vline(x + 1, top + 1, 21, kSteelD);
            p.px(x, top + 4, kInk);
        }
        p.hline(6, 25, 24, kWhite);
        break;
    case Wonder::Magellan:
        p.sky(kDay, kDayLow, 22);
        p.water(22, kSea, kSeaL);
        p.tri(16, 22, 4, 28, 26, kWood);
        p.rect(6, 21, 20, 3, kWood);
        p.hline(6, 25, 21, kWoodD);
        p.vline(12, 4, 21, kWoodD);
        p.vline(20, 6, 21, kWoodD);
        p.rect(8, 6, 9, 7, kWhite);
        p.rect(9, 14, 7, 5, kWhite);
        p.rect(17, 8, 7, 6, kWhite);
        p.rect(18, 15, 5, 4, kWhite);
        p.rect(11, 2, 3, 2, kRed);
        break;
    case Wonder::Newton:
        p.sky(kDay, kDayLow, 26);
        p.ground(26, kGrass, kGrassD);
        p.rect(3, 12, 17, 14, rgb(0xB07448));
        p.tri(11, 5, 2, 20, 12, kRedD);
        for (int y = 15; y < 24; y += 4)
            for (int x = 5; x < 19; x += 4) p.rect(x, y, 2, 2, kDayLow);
        p.vline(26, 14, 25, kWoodD);
        p.disc(26, 11, 5, kLeaf);
        p.disc(24, 9, 2, kLeafL);
        p.disc(27, 19, 1, kRed);   // the falling apple
        p.px(28, 18, kRedD);
        break;
    case Wonder::Darwin:
        p.sky(kDay, kDayLow, 21);
        p.water(21, kSea, kSeaL);
        p.tri(23, 13, 16, 31, 21, kSand);
        p.line(25, 20, 26, 9, kWoodD);
        p.line(26, 9, 21, 8, kLeaf);
        p.line(26, 9, 30, 7, kLeaf);
        p.line(26, 9, 29, 12, kLeafL);
        // A little survey ship with one sail.
        p.rect(3, 23, 13, 3, kWood);
        p.hline(3, 15, 23, kWoodD);
        p.vline(9, 11, 22, kWoodD);
        p.tri(9, 11, 9, 15, 21, kWhite);
        p.tri(9, 13, 4, 9, 21, kStoneL);
        break;
    case Wonder::Suffrage:
        p.sky(kDay, kDayLow, 26);
        p.ground(26, kStoneD, kInk);
        p.rect(8, 14, 16, 12, kWood);
        p.rect(8, 14, 16, 2, kWoodD);
        p.rect(13, 11, 6, 1, kInk);
        // A ballot going in.
        p.rect(14, 3, 5, 8, kWhite);
        p.hline(15, 17, 5, kInk);
        p.line(15, 8, 16, 9, kInk);
        p.line(16, 9, 18, 6, kInk);
        p.rect(11, 19, 10, 3, kGold);
        break;
    case Wonder::HooverDam:
        p.sky(kDay, kDayLow, 10);
        p.rect(0, 10, 32, 22, kSandD);
        p.rect(0, 10, 10, 22, kSea);
        for (int y = 11; y < 31; y += 3) p.hline(1, 8, y, kSeaL);
        // The curved concrete wall and the river below.
        for (int y = 8; y < 30; ++y) {
            const int x = 10 + (y - 8) / 4;
            p.hline(x, x + 7, y, kStoneL);
            p.px(x + 7, y, kStone);
        }
        p.rect(20, 26, 12, 6, kSea);
        p.hline(18, 31, 27, kSeaL);
        p.vline(14, 10, 26, kStone);
        break;
    case Wonder::Manhattan:
        p.sky(kNight, kNightLow, 32);
        p.disc(16, 16, 3, kRed);
        p.disc(15, 15, 1, rgb(0xF0A080));
        for (int k = 0; k < 3; ++k) {
            for (int a = 0; a < 80; ++a) {
                const double t = a * 6.2831853 / 80;
                const double ex = 13 * std::cos(t), ey = 5 * std::sin(t);
                const double rot = k * 1.0471976;
                const int x = 16 + static_cast<int>(std::lround(ex * std::cos(rot) - ey * std::sin(rot)));
                const int y = 16 + static_cast<int>(std::lround(ex * std::sin(rot) + ey * std::cos(rot)));
                p.px(x, y, rgb(0x60C8D8));
            }
        }
        p.disc(29, 16, 1, kWhite);
        p.disc(9, 5, 1, kWhite);
        break;
    case Wonder::UnitedNations:
        p.sky(rgb(0x5888D8), rgb(0x7CA4E4), 32);
        p.disc(16, 15, 9, kWhite);
        p.disc(16, 15, 8, rgb(0x8CB0F0));
        p.ring(16, 15, 5, kWhite);
        p.vline(16, 7, 23, kWhite);
        p.hline(8, 24, 15, kWhite);
        for (int i = 0; i < 6; ++i) {
            p.px(6 + i / 2, 24 - i * 2, kLeafL);
            p.px(25 - i / 2, 24 - i * 2, kLeafL);
            p.px(5 + i / 2, 23 - i * 2, kLeaf);
            p.px(26 - i / 2, 23 - i * 2, kLeaf);
        }
        p.hline(10, 22, 27, kLeaf);
        break;
    case Wonder::Apollo:
        p.sky(kNight, kNightLow, 27);
        for (int i = 0; i < 10; ++i) p.px((i * 13 + 5) % 31, (i * 5 + 1) % 20, kWhite);
        p.ground(27, kStoneD, kInk);
        p.rect(19, 6, 3, 21, kSteelD);
        for (int y = 8; y < 27; y += 3) p.hline(19, 23, y, kSteel);
        p.rect(12, 6, 6, 20, kWhite);
        p.vline(17, 6, 25, kStone);
        p.tri(14, 1, 12, 17, 6, kWhite);
        p.rect(12, 12, 6, 1, kInk);
        p.rect(12, 18, 6, 1, kInk);
        p.tri(10, 21, 10, 12, 26, kStone);
        p.tri(19, 21, 17, 19, 26, kStone);
        p.rect(13, 26, 4, 1, kGold);
        break;
    case Wonder::Seti:
        p.sky(kNight, kNightLow, 26);
        for (int i = 0; i < 9; ++i) p.px((i * 9 + 4) % 31, (i * 5 + 2) % 14, kWhite);
        p.ground(26, kSandD, kInk);
        // The dish faces the sky.
        for (int y = 0; y < 7; ++y) p.hline(6 + y, 26 - y / 2 - y, 11 + y, y == 0 ? kStoneL : kStone);
        p.line(16, 14, 22, 5, kSteelD);
        p.disc(22, 5, 1, kRed);
        p.vline(15, 18, 25, kSteelD);
        p.tri(15, 20, 11, 19, 25, kSteelD);
        for (int r = 3; r <= 7; r += 2) p.px(25 + r / 2, 3 - r / 3, rgb(0x60C8D8));
        break;
    case Wonder::CureForCancer:
        p.sky(rgb(0xE8E4DA), rgb(0xD0D6E0), 32);
        for (int y = 2; y < 30; ++y) {
            const double t = y * 0.42;
            const int a = 16 + static_cast<int>(std::lround(7 * std::sin(t)));
            const int b = 16 - static_cast<int>(std::lround(7 * std::sin(t)));
            if (y % 3 == 0) p.hline(std::min(a, b), std::max(a, b), y, kStone);
            p.px(a, y, kBlue);
            p.px(a + 1, y, kBlue);
            p.px(b, y, kRed);
            p.px(b + 1, y, kRed);
        }
        p.rect(24, 22, 6, 2, kRed);
        p.rect(26, 20, 2, 6, kRed);
        break;
    default: break;
    }
}

} // namespace

Pixmap wonder_icon(int w)
{
    Painter p;
    if (w >= 0 && w < civ::kWonderCount) paint(p, static_cast<civ::Wonder>(w));
    p.frame();
    return p.pm;
}

} // namespace art
