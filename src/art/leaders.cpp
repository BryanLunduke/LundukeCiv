// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.
//
// Leader portraits for audiences: a 32x32 bust painted square by square in
// the game's palette, with headwear for each of the fourteen peoples and a
// face that smiles, waits or scowls. Our own drawings.

#include "sprites.hpp"

#include <algorithm>
#include <cstdlib>

namespace art {

namespace {

Argb ink(char ch) { return sprite({std::string(1, ch)}).get(0, 0); }

Argb shade(Argb c, double f)
{
    auto ch = [&](int s) { return std::clamp(static_cast<int>(((c >> s) & 255) * f), 0, 255); };
    return 0xFF000000u | (static_cast<Argb>(ch(16)) << 16) | (static_cast<Argb>(ch(8)) << 8) | static_cast<Argb>(ch(0));
}

struct Canvas {
    Pixmap pm{32, 32};
    void dot(int x, int y, Argb c) { pm.set(x, y, c); }
    void dot(int x, int y, char c) { pm.set(x, y, ink(c)); }
    void hline(int x0, int x1, int y, Argb c)
    {
        for (int x = x0; x <= x1; ++x) pm.set(x, y, c);
    }
    void hline(int x0, int x1, int y, char c) { hline(x0, x1, y, ink(c)); }
    void rect(int x0, int y0, int x1, int y1, char c)
    {
        for (int y = y0; y <= y1; ++y) hline(x0, x1, y, c);
    }
    // Rows from a picture: '.' leaves the square alone.
    void rows(int x0, int y0, const std::vector<std::string> &r)
    {
        for (size_t j = 0; j < r.size(); ++j)
            for (size_t i = 0; i < r[j].size(); ++i)
                if (r[j][i] != '.') pm.set(x0 + static_cast<int>(i), y0 + static_cast<int>(j), ink(r[j][i]));
    }
};

// Head widths from row 8 down to row 21, centred on x 15.5.
const int kHead[14] = {6, 8, 10, 12, 12, 12, 12, 12, 12, 12, 10, 10, 8, 6};

} // namespace

Pixmap leader_portrait(int tribe, const CivColors &civ, int mood)
{
    Canvas c;
    // Backdrop: the people's colour, darkening downwards, in a black frame.
    for (int y = 0; y < 32; ++y) c.hline(0, 31, y, shade(civ.dark, 1.0 - y * 0.012));
    for (int y = 2; y < 30; y += 4)
        for (int x = (y / 4) % 2 * 2 + 1; x < 31; x += 4) c.dot(x, y, shade(civ.dark, 1.15));
    const char skin_of[14] = {'s', 'n', 's', 'n', 's', 's', 'S', 's', 'S', 's', 'n', 's', 's', 'n'};
    const char skin = tribe >= 0 && tribe < 14 ? skin_of[tribe] : 's';
    const char dark = skin == 's' ? 'n' : (skin == 'n' ? 'N' : 'm');
    const char hair_of[14] = {'h', 'h', 'G', 'K', 'h', 'b', 'g', 'H', 'K', 'h', 'K', 'K', 'o', 'K'};
    const char hair = tribe >= 0 && tribe < 14 ? hair_of[tribe] : 'B';
    // Shoulders in a robe of the people's colour.
    for (int y = 23; y < 32; ++y) {
        const int half = std::min(13, 6 + (y - 23) * 2);
        c.hline(16 - half, 15 + half, y, civ.main);
        c.dot(16 - half, y, civ.dark);
        c.dot(15 + half, y, civ.dark);
    }
    c.hline(12, 19, 23, civ.light);
    c.hline(13, 18, 24, civ.light);
    // Neck and head.
    c.rect(13, 20, 18, 23, dark);
    for (int j = 0; j < 14; ++j) {
        const int w = kHead[j], y = 8 + j;
        c.hline(16 - w / 2, 15 + w / 2, y, skin);
        c.dot(16 - w / 2, y, 'K');
        c.dot(15 + w / 2, y, 'K');
    }
    c.hline(13, 18, 22, 'K');
    c.hline(13, 18, 7, 'K');
    // Ears.
    c.dot(9, 14, dark);
    c.dot(22, 14, dark);
    c.dot(9, 15, dark);
    c.dot(22, 15, dark);
    // Hair round the crown and temples.
    const bool bald = tribe == 6;
    if (!bald) {
        c.hline(12, 19, 8, hair);
        c.hline(11, 20, 9, hair);
        c.rect(10, 10, 11, 13, hair);
        c.rect(20, 10, 21, 13, hair);
    }
    // Eyes, brows, nose and mouth.
    c.dot(12, 14, 'w');
    c.dot(13, 14, 'K');
    c.dot(18, 14, 'K');
    c.dot(19, 14, 'w');
    if (mood < 0) {   // scowl: brows down to the nose
        c.dot(11, 12, 'K');
        c.dot(12, 12, 'K');
        c.dot(13, 13, 'K');
        c.dot(18, 13, 'K');
        c.dot(19, 12, 'K');
        c.dot(20, 12, 'K');
    } else {
        c.hline(11, 13, 12, 'K');
        c.hline(18, 20, 12, 'K');
    }
    c.dot(15, 16, dark);
    c.dot(16, 16, dark);
    c.dot(15, 17, dark);
    c.dot(16, 17, dark);
    if (mood > 0) {
        c.dot(13, 18, 'K');
        c.hline(14, 17, 19, 'K');
        c.dot(18, 18, 'K');
    } else if (mood < 0) {
        c.hline(14, 17, 18, 'K');
        c.dot(13, 19, 'K');
        c.dot(18, 19, 'K');
    } else {
        c.hline(14, 17, 19, 'K');
    }
    // What each people's leader wears.
    switch (tribe) {
    case 0:   // laurel wreath
        for (int x = 10; x <= 21; ++x) c.dot(x, 9 + (x % 2), x % 3 ? 'e' : 'E');
        c.rows(9, 10, {"e", "E"});
        c.rows(22, 10, {"e", "E"});
        break;
    case 1:   // tall ribbed cap
        c.rect(11, 1, 20, 8, 'y');
        for (int y = 2; y <= 8; y += 2) c.hline(11, 20, y, 'Y');
        c.hline(10, 21, 9, 'K');
        c.rows(10, 18, {".hhhhhhhhhhh", "..hhhhhhhhh.", "...hhhhhhh..", "....hhhhh..."});
        break;
    case 2:   // spiked helmet
        c.rect(15, 1, 16, 4, 'g');
        c.rows(10, 4, {"..GgggggggG.", ".GggggggggGG", "GggggggggggG", "GgggggggggGG", "KKKKKKKKKKKK"});
        c.rows(11, 9, {"yyyyyyyyyy"});
        break;
    case 3:   // striped head-cloth
        c.rows(8, 5, {"...yuyuyuyuyu...", "..yuyuyuyuyuyuy.", ".yuyuyuyuyuyuyuy", ".yu..........yu.", "yuy..........yuy"});
        for (int y = 10; y <= 21; ++y) {
            c.dot(8, y, y % 2 ? 'y' : 'u');
            c.dot(9, y, y % 2 ? 'u' : 'y');
            c.dot(22, y, y % 2 ? 'u' : 'y');
            c.dot(23, y, y % 2 ? 'y' : 'u');
        }
        c.rect(15, 19, 16, 22, 'Y');   // false beard
        break;
    case 4:   // stovepipe hat and chin beard
        c.rect(11, 0, 20, 7, 'K');
        c.hline(11, 20, 6, 'k');
        c.hline(8, 23, 8, 'K');
        c.rows(10, 19, {"h..........h", "hh........hh", ".hhh....hhh.", "..hhhhhhhh..", "....hhhh...."});
        break;
    case 5:   // crested bronze helmet
        c.rows(9, 1, {"....rrrrrr....", "..rrRrrrrRrr..", ".rr........rr.", ""});
        c.rows(10, 4, {"..YyyyyyyY..", ".YyyyyyyyyY.", "YyyyyyyyyyyY", "Yyy......yyY", "Yy........yY", "Yy........yY",
                       "Y..........Y"});
        break;
    case 6:   // round spectacles
        c.rows(11, 13, {"KKK..KKK", "K.K..K.K", "KKKKKKKK"});
        c.dot(12, 14, 'c');
        c.dot(18, 14, 'c');
        c.dot(9, 12, 'g');
        c.dot(22, 12, 'g');
        c.rows(21, 25, {"ww", "ww"});   // a homespun shawl
        c.hline(7, 24, 26, 'w');
        c.hline(6, 25, 27, 'W');
        break;
    case 7:   // peaked army cap with a star; moustache
        c.rows(9, 4, {"..EEEEEEEEEE..", ".EeeeeerreeeE.", "EeeeeerrrreeeE", "EEEEEEEEEEEEEE"});
        c.hline(9, 22, 8, 'r');
        c.hline(11, 20, 9, 'K');
        c.hline(12, 19, 17, 'h');
        c.hline(13, 18, 18, 'h');
        break;
    case 8:   // feathered head-ring and shield rim
        c.hline(10, 21, 9, 'w');
        for (int x = 10; x <= 21; x += 2) c.dot(x, 9, 'K');
        c.rows(11, 2, {"w.K..w..K.w", "wK.w.Kw.wKw", "Kw.wKwKw.wK", ".Kww.w.wwK.", "..wKwwwKw..", "...KwKwK..."});
        c.rows(2, 22, {".mm", "mMm", "mwm", "mMm", "mwm", "mMm", "mwm", "mMm", ".mm"});
        break;
    case 9:   // bicorne with cockade
        c.rows(5, 3, {"......KKKKKKKKKKK.....", "....KKKKKKKKKKKKKKK...", "..KKKKKKKuwrKKKKKKKKK.",
                      ".KKKKKKKKKKKKKKKKKKKKK", "KKKk..........kKKKKKKK", "Kk................kKKK"});
        c.hline(9, 22, 9, 'h');
        break;
    case 10:   // crown of feathers
        for (int i = 0; i < 9; ++i) {
            const int x = 8 + i * 2;
            const char f = "euryeuryu"[i];
            const int top = 1 + std::abs(4 - i) / 2;
            for (int y = top; y <= 8; ++y) c.dot(x, y, f);
            c.dot(x + 1, top + 1, f);
        }
        c.hline(9, 22, 8, 'y');
        c.hline(9, 22, 9, 'Y');
        c.dot(15, 8, 'e');
        c.dot(16, 8, 'e');
        break;
    case 11:   // plain worker's cap with a red star
        c.rows(9, 5, {"..EEEEEEEEEE..", ".EeeeeerreeeE.", "EeeeeeeeeeeeeE", "KKKKKKKKKKKKKK"});
        c.hline(10, 21, 9, 'h');
        c.rows(12, 24, {"EEEEEEEE"});
        c.rows(15, 25, {"y", "y", "y"});
        break;
    case 12:   // crown, red curls, ruff
        c.rows(10, 2, {"y..y.y..y..y", "yy.yyy.yy.yy", "yryyyuyyyryy", "yyyyyyyyyyyy", "YYYYYYYYYYYY"});
        c.rect(9, 7, 10, 15, 'o');
        c.rect(21, 7, 22, 15, 'o');
        c.hline(11, 20, 7, 'o');
        for (int x = 7; x <= 24; ++x) {
            c.dot(x, 22, x % 2 ? 'w' : 'W');
            c.dot(x, 23, x % 2 ? 'W' : 'w');
        }
        c.hline(8, 23, 21, 'w');
        c.dot(15, 26, 'y');
        c.dot(16, 26, 'r');
        break;
    case 13:   // fur-trimmed hat; long moustache
        c.rows(10, 1, {".....rr.....", "....rrrr....", "...yrrrry...", "..rrrrrrrr..", ".rrrrrrrrrr."});
        c.rows(8, 6, {".bBbBbBbBbBbBbB.", "bBbBbBbBbBbBbBbB", "BbBbBbBbBbBbBbBb", ".BbBbBbBbBbBbBb."});
        c.hline(12, 19, 17, 'K');
        c.rows(11, 18, {"KK......KK", "K........K", "K........K"});
        break;
    default:   // a ruler of the player's own making: a plain circlet
        c.hline(10, 21, 9, 'y');
        c.dot(15, 8, 'u');
        c.dot(16, 8, 'u');
        c.hline(10, 21, 10, 'Y');
        break;
    }
    // Frame.
    for (int i = 0; i < 32; ++i) {
        c.dot(i, 0, 'K');
        c.dot(i, 31, 'K');
        c.dot(0, i, 'K');
        c.dot(31, i, 'K');
    }
    return c.pm;
}

} // namespace art
