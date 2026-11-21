// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#include "sprites.hpp"

#include <algorithm>
#include <array>
#include <map>

namespace art {

namespace {

Argb ink_for(char ch, const CivColors *civ)
{
    switch (ch) {
    case 'K': return rgb(0x1A1620);
    case 'k': return rgb(0x3A3440);
    case 's': return rgb(0xE6A874);
    case 'S': return rgb(0xB07448);
    case 'h': return rgb(0x3A2416);
    case 'b': return rgb(0x8A5A2E);
    case 'B': return rgb(0x5A3618);
    case 'n': return rgb(0xC8A070);
    case 'N': return rgb(0x947048);
    case 'w': return rgb(0xF2EADA);
    case 'W': return rgb(0xC4BAA6);
    case 'g': return rgb(0xD0D6E0);
    case 'G': return rgb(0x8A92A0);
    case 'H': return rgb(0x4A505C);
    case 'y': return rgb(0xF0C040);
    case 'Y': return rgb(0xB08020);
    case 'r': return rgb(0xC83A2E);
    case 'R': return rgb(0x7E2018);
    case 'e': return rgb(0x5CA040);
    case 'E': return rgb(0x2E6224);
    case 'u': return rgb(0x5888D8);
    case 'U': return rgb(0x2C4C98);
    case 'c': return rgb(0xA8D4F0);
    case 'm': return rgb(0x9A6A3C);
    case 'M': return rgb(0x5E3A1C);
    case 'q': return rgb(0xE0D0B0);
    case 'x': return rgb(0x202028);
    case 'X': return rgb(0x585868);
    case 'p': return rgb(0xE070B0);
    case 'P': return rgb(0x8E2E66);
    case 't': return rgb(0x60C8D8);
    case 'T': return rgb(0x2A7080);
    case 'o': return rgb(0xE88A30);
    case 'v': return rgb(0x8C8478);  // stone
    case 'V': return rgb(0x5C564E);  // stone dark
    case 'i': return rgb(0xB8B0A0);  // stone light
    case 'C': return civ ? civ->main : rgb(0x3A6CCF);
    case 'L': return civ ? civ->light : rgb(0x8CB0F0);
    case 'D': return civ ? civ->dark : rgb(0x1E3C80);
    default: return kClear;
    }
}

// ---- 3x5 font -------------------------------------------------------------

const std::map<char, std::array<const char *, 5>> &glyphs()
{
    static const std::map<char, std::array<const char *, 5>> g = {
        {'0', {"###", "#.#", "#.#", "#.#", "###"}}, {'1', {".#.", "##.", ".#.", ".#.", "###"}},
        {'2', {"##.", "..#", ".#.", "#..", "###"}}, {'3', {"##.", "..#", ".#.", "..#", "##."}},
        {'4', {"#.#", "#.#", "###", "..#", "..#"}}, {'5', {"###", "#..", "##.", "..#", "##."}},
        {'6', {".##", "#..", "###", "#.#", "###"}}, {'7', {"###", "..#", ".#.", ".#.", ".#."}},
        {'8', {"###", "#.#", "###", "#.#", "###"}}, {'9', {"###", "#.#", "###", "..#", "##."}},
        {'A', {".#.", "#.#", "###", "#.#", "#.#"}}, {'B', {"##.", "#.#", "##.", "#.#", "##."}},
        {'C', {".##", "#..", "#..", "#..", ".##"}}, {'D', {"##.", "#.#", "#.#", "#.#", "##."}},
        {'E', {"###", "#..", "##.", "#..", "###"}}, {'F', {"###", "#..", "##.", "#..", "#.."}},
        {'G', {".##", "#..", "#.#", "#.#", ".##"}}, {'H', {"#.#", "#.#", "###", "#.#", "#.#"}},
        {'I', {"###", ".#.", ".#.", ".#.", "###"}}, {'K', {"#.#", "#.#", "##.", "#.#", "#.#"}},
        {'L', {"#..", "#..", "#..", "#..", "###"}}, {'M', {"#.#", "###", "###", "#.#", "#.#"}},
        {'N', {"##.", "#.#", "#.#", "#.#", "#.#"}}, {'O', {".#.", "#.#", "#.#", "#.#", ".#."}},
        {'P', {"##.", "#.#", "##.", "#..", "#.."}}, {'R', {"##.", "#.#", "##.", "#.#", "#.#"}},
        {'S', {".##", "#..", ".#.", "..#", "##."}}, {'T', {"###", ".#.", ".#.", ".#.", ".#."}},
        {'U', {"#.#", "#.#", "#.#", "#.#", "###"}}, {'W', {"#.#", "#.#", "###", "###", "#.#"}},
        {'X', {"#.#", "#.#", ".#.", "#.#", "#.#"}}, {'Y', {"#.#", "#.#", ".#.", ".#.", ".#."}},
        {'-', {"...", "...", "###", "...", "..."}}, {'.', {"...", "...", "...", "...", ".#."}},
        {' ', {"...", "...", "...", "...", "..."}},
    };
    return g;
}

// ---- unit figures (22x17, drawn in the counter's parchment window) --------

const std::vector<std::string> &figure(civ::UnitType u)
{
    static const std::vector<std::string> settlers = {
        "......................",
        "........KKKK......KK..",
        ".......KnnnnK....KGGK.",
        "......KnnnnnnK..KGGK..",
        ".......KsKsKK..KbKK...",
        ".......KssssK.KbK.....",
        "........KSSK.KbK......",
        "......KKCCCCKbK.......",
        ".....KsKCCCCKsK.......",
        ".....KsKCCCCKK........",
        ".....KsKCDDCK.KKKKK...",
        ".....KSKCCCCK.KnnnnK..",
        "......KKBBBBK.KnNnnK..",
        ".......KnKKnK.KnnNnK..",
        ".......KsK.KsKKKKKKK..",
        "......KBBK.KBBK.......",
        "......................",
    };
    static const std::vector<std::string> warriors = {
        "................KKK...",
        ".........KKKK..KbbbK..",
        "........KhhhhK.KbBbbK.",
        "........KhsshK..KbbK..",
        "........KsKsKK..KbK...",
        ".........KssK..KbK....",
        "........KKSSKK.KbK....",
        ".......KCCCCCCKsK.....",
        "......KsKCCCCKKsK.....",
        "......KsKCDDCKKK......",
        "......KsKCCCCK........",
        "......KSKBBBBK........",
        "........KnKKnK........",
        "........KsK.KsK.......",
        "........KsK.KsK.......",
        ".......KBBK.KBBK......",
        "......................",
    };
    static const std::vector<std::string> phalanx = {
        ".......KRRRK......K...",
        "......KrrrrrK....KgK..",
        ".......KGggGK....KGK..",
        ".......KgKsGK.....Kb..",
        ".......KGssGK.....Kb..",
        "..KKKK..KssK......Kb..",
        ".KyyyyK.KSSK.....KsK..",
        "KyyYYyyKCCCCKK..KsKb..",
        "KyYyyYyKCCCCCsKKsK.b..",
        "KyYyyYyKCCCCKKsK...b..",
        "KyyYYyyKCDDCK.KK...b..",
        "KyyyyyyKCCCCK......b..",
        ".KyyyyKKBBBBK......b..",
        "..KKKK..KnKnK......b..",
        "........KsKKsK.....b..",
        ".......KBBK.KBBK...K..",
        "......................",
    };
    static const std::vector<std::string> horsemen = {
        "...........KKKK.......",
        "..........KhhhhK......",
        "..........KsKssK..K...",
        "...........KssK..KgK..",
        "..........KCCCCKKbK...",
        ".........KsKCCKsbK....",
        "..KK.....KsKCDKKK.....",
        ".KmmK....KKCCCCK......",
        "KmqmmKKKKmKBBKmKKK....",
        "KqKmmmmmmmmmmmmmmmKK..",
        ".KK.KmmmmmmmmmmmmmMmK.",
        ".....KmmMMMMMMMmmmKKmK",
        ".....KmK.KmK..KmKmK..K",
        ".....KmK.KmK..KmKmK...",
        "....KMK..KMK..KMK.KMK.",
        "....KKK..KKK..KKK.KKK.",
        "......................",
    };
    static const std::vector<std::string> chariot = {
        "..............KKKK....",
        ".............KgggGK...",
        ".............KsKssK...",
        "..............KSSK....",
        "..KK.........KCCCCK...",
        ".KmmK.......KsCCCCK...",
        "KmqmmK.....KbKCDDCK...",
        "KqKmmmK...Kb.KCCCCK...",
        ".KK.KmmmmmmKKKKKKKKKK.",
        "....KmmmmmmmKCCCCCCCK.",
        "....KmMMMMmmKDDDDDDDK.",
        "....KmK.KmKKKKKyyyKKK.",
        "....KmK.KmK..KyYYYyK..",
        "...KMK..KMK.KyYKKKYyK.",
        "...KKK..KKK.KyYKyKYyK.",
        ".............KyYYYyK..",
        "..............KKKKK...",
    };
    static const std::vector<std::string> catapult = {
        "..KKK.................",
        ".KxXxK................",
        ".KxxxKK...............",
        "..KKKbbK..............",
        ".....KbbK.............",
        "......KbbK.....KK.....",
        ".......KbbK...KbbK....",
        "........KbbK.KbbK.....",
        ".........KbbKbbK......",
        "...KKKKKKKKbbbKKKKKK..",
        "...KbbbbbbbbbbbbbbbK..",
        "...KBBBBBBBBBBBBBBBK..",
        "..KKKKK........KKKKK..",
        ".KbYYbK.......KbYYbK..",
        ".KYyyYK.......KYyyYK..",
        ".KbYYbK.......KbYYbK..",
        "..KKKK.........KKKK...",
    };
    static const std::vector<std::string> trireme = {
        "..........KK..........",
        "......KKKKKKKKKK......",
        "......KwCwCwCwCK......",
        "......KwCwCwCwCK......",
        "......KwCwCwCwCK......",
        "......KwCwCwCwCK......",
        "......KWDWDWDWDK......",
        "......KKKKbKKKKK......",
        "KK........bK........KK",
        "KbK.......bK.......KbK",
        ".KbKKKKKKKKKKKKKKKKbK.",
        "..KbbbbbbbbbbbbbbbbK..",
        "..KBByBByBByBByBByBK..",
        "...KKbKKbKKbKKbKKbK...",
        "...ub.ub.ub.ub.ub.....",
        "uuUuuuUuuuUuuuUuuuUuuu",
        "......................",
    };
    static const std::vector<std::string> legion = {
        "........KKKKK.........",
        "........KrrrrK........",
        ".......KKGgggK........",
        ".......KgKsKgK........",
        "........KssssK........",
        ".........KSSK.........",
        "..KKKKK.KCCCCKK.......",
        ".KCLLLCKCCCCCsK..K....",
        ".KCLyLCKCCCCKKsKgK....",
        ".KCyyyCKCDDCK.KgK.....",
        ".KCLyLCKCCCCK.KHK.....",
        ".KCLLLCKBBBBK.........",
        ".KCCCCCKKnKnK.........",
        "..KKKKK.KsKKsK........",
        "........KsK.KsK.......",
        ".......KBBK.KBBK......",
        "......................",
    };
    static const std::vector<std::string> knights = {
        "..K...................",
        "...K.....KKKK.........",
        "....K...KgggGK........",
        ".....K..KgKKgK........",
        "......K..KggK.........",
        ".......KKCCCCK........",
        "........KCCCCsK.......",
        "..KK....KKCDCKgK......",
        ".KgGK..KKCCCCKKK......",
        "KgKgGKKCLCLCLCLCKK....",
        ".KK.KCLCLCLCLCLCLCK...",
        ".....KCCCCCCCCCCCCK...",
        ".....KDDDDDDDDDDDDK...",
        ".....KgK.KgK..KgKgK...",
        ".....KgK.KgK..KgKgK...",
        "....KHK..KHK..KHK.KHK.",
        "....KKK..KKK..KKK.KKK.",
    };
    static const std::vector<std::string> musketeers = {
        "..............K.......",
        ".......KKKKK..KK......",
        "......KxxxxxK.KxK.....",
        ".....KxxxxxxxKKxK.....",
        "........KssK..KxK.....",
        "........KsKsK.KbK.....",
        ".........KSSK.KbK.....",
        "........KCwwCKKbK.....",
        ".......KCCwCwCsbK.....",
        ".......KsCCwCCKbK.....",
        ".......KsCwCwCKbK.....",
        ".......KKCCCCCKbK.....",
        "........KwwwwwKKK.....",
        "........KxK.KxK.......",
        "........KxK.KxK.......",
        ".......KBBK.KBBK......",
        "......................",
    };
    static const std::vector<std::string> riflemen = {
        "..................K...",
        ".................KgK..",
        "........KKKK.....KgK..",
        ".......KEEEEK...KHK...",
        "......KEEEEEEK.KbK....",
        ".......KssssK.KbK.....",
        ".......KsKsKK.KbK.....",
        "........KSSK.KbK......",
        ".......KCCCCKKsK......",
        "......KCCCCCCsK.......",
        "......KsCCCCCKK.......",
        "......KsCDDCK.........",
        "......KKCCCCK.........",
        ".......KNNNNK.........",
        ".......KNK.KNK........",
        "......KBBK.KBBK.......",
        "......................",
    };
    static const std::vector<std::string> cannon = {
        "......................",
        "......................",
        "......................",
        "......................",
        "..........KK..........",
        ".....KKKKKxxK.........",
        "...KKxxxxxxxXK........",
        "KKKxxXXXXXxxxXK.......",
        "KxKxxxxxxxxxxxKK......",
        "KKKKKKKbbbKKKKbbK.....",
        "......KbbbbbbbbbbbK...",
        ".....KKKKKbbBBBBBBBK..",
        "....KbKbKbK.KKKKKKK...",
        "....KKyKyKK...........",
        "....KbKbKbK...........",
        ".....KKKKK............",
        "......................",
    };
    static const std::vector<std::string> artillery = {
        "...................KK.",
        ".................KKGK.",
        "...............KKGGK..",
        ".............KKGGKK...",
        "...........KKGGKK.....",
        ".........KKGGKK.......",
        ".......KKGGKK.........",
        "....KKKGGGKK..........",
        "...KGGGGGGGGK.........",
        "...KGHHHHHHGK.........",
        "..KKKKKKKKKKKKK.......",
        ".KGGGGGGGGGGGGGKKKK...",
        ".KHHHHHHHHHHHHHHHHHKK.",
        "..KxxK.....KxxK....KK.",
        ".KxXXxK...KxXXxK......",
        ".KxXXxK...KxXXxK......",
        "..KxxK.....KxxK.......",
    };
    static const std::vector<std::string> armor = {
        "......................",
        "......................",
        "......................",
        "......................",
        "........KKKKKK........",
        ".......KCLLLLCKKKKKKKK",
        ".......KCCCCCCGGGGGGGK",
        "......KKCDDDDCKKKKKKKK",
        "...KKKKKKKKKKKKKKKK...",
        "..KCLLLLLLLLLLLLLLCK..",
        ".KCCCCCCCCCCCCCCCCCCK.",
        ".KDDDDDDDDDDDDDDDDDDK.",
        "KxxxxxxxxxxxxxxxxxxxxK",
        "KxXKXKXKXKXKXKXKXKXxxK",
        ".KxxxxxxxxxxxxxxxxxxK.",
        "..KKKKKKKKKKKKKKKKKK..",
        "......................",
    };
    static const std::vector<std::string> mechinf = {
        "......................",
        "......................",
        "......................",
        ".........KKK..........",
        "........KEEEK.........",
        "........KsssK.........",
        ".....KKKKKKKKKKKKK....",
        "....KCLLLLLLLLLLLCK...",
        "...KCCCKccKCCKccKCKK..",
        "..KCCCCKKKKCCKKKKCCCK.",
        "..KCCCCCCCCCCCCCCCCCK.",
        "..KDDDDDDDDDDDDDDDDDK.",
        "..KKKKKKKKKKKKKKKKKKK.",
        "...KxxK..KxxK..KxxK...",
        "..KxXXxKKxXXxKKxXXxK..",
        "...KxxK..KxxK..KxxK...",
        "......................",
    };
    static const std::vector<std::string> diplomat = {
        "........KKKKK.........",
        "........KxxxK.........",
        "........KxxxK.........",
        ".......KKKKKKK........",
        "........KssK..........",
        "........KsKsK.........",
        ".........KSK..........",
        ".......KxCCCxK........",
        "......KxxCwCxxK.......",
        "......KsKxwxKsKKKK....",
        "......KsKxxxKKwwwwK...",
        "......KKKxCxKKwWWwK...",
        ".......KxxCxxKKKKK....",
        ".......KxxxxxK........",
        "........KxKxK.........",
        ".......KxxKxxK........",
        "......................",
    };
    static const std::vector<std::string> caravan = {
        "......................",
        "...KK.................",
        "..KnnK.......KK.......",
        "..KnKnK.....KnnK......",
        "...KKnK....KnnnnK.....",
        ".....KnK..KCLCnnnK....",
        ".....KnnKKCCCCKnnnK...",
        "......KnnnCDDCnnnnnK..",
        "......KnnnnnnnnnnnnnK.",
        ".......KnnnnnnnnnnnK..",
        "........KnnNNNNnnnK...",
        "........KnK.KnK.KnK...",
        "........KnK.KnK.KnK...",
        "........KnK.KnK.KnK...",
        ".......KNK.KNK.KNK....",
        ".......KKK.KKK.KKK....",
        "......................",
    };
    static const std::vector<std::string> sail = {
        "........K.......K.....",
        ".......KCK.....KCK....",
        "......KwwwK...KwwwK...",
        ".....KwwwwwK.KwwwwwK..",
        ".....KwCwCwK.KwCwCwK..",
        ".....KwwwwwK.KwwwwwK..",
        ".....KwCwCwK.KwCwCwK..",
        ".....KwwwwwK.KwwwwwK..",
        "......KWWWK...KWWWK...",
        ".......KbK.....KbK....",
        "KK.....KbK.....KbK.KK.",
        "KbKKKKKKKKKKKKKKKKKbK.",
        ".KbbbbbbbbbbbbbbbbbK..",
        "..KBBBBBBBBBBBBBBBK...",
        "...KKKKKKKKKKKKKKK....",
        "uuuUuuuUuuuUuuuUuuuUuu",
        "......................",
    };
    static const std::vector<std::string> frigate = {
        "...K......K......K....",
        "..KCK....KCK....KCK...",
        ".KwwwK..KwwwwK..KwwK..",
        ".KwwwK.KwwwwwwK.KwwK..",
        ".KwCwK.KwCwwCwK.KwCK..",
        ".KwwwK.KwwwwwwK.KwwK..",
        ".KwwwK.KwwwwwwK.KwwK..",
        "..KbK...KWWWWK...KbK..",
        "..KbK....KbbK....KbK..",
        "KKKKKKKKKKKKKKKKKKKKKK",
        "KbbbbbbbbbbbbbbbbbbbbK",
        ".KbxbbxbbxbbxbbxbbxbK.",
        "..KBBBBBBBBBBBBBBBBK..",
        "...KKKKKKKKKKKKKKKK...",
        "uuUuuuUuuuUuuuUuuuUuuu",
        "......................",
        "......................",
    };
    static const std::vector<std::string> ironclad = {
        "..........gg..........",
        ".........gGGg.........",
        "..........gg..........",
        "..........KK..........",
        ".........KxxK.........",
        ".........KxxK.........",
        ".......KKKxxKKK.......",
        "......KGGGGGGGGK......",
        "......KGKGKGKGGK......",
        "....KKKCCCCCCCCKKK....",
        "KKKKxxxxxxxxxxxxxxxKKK",
        "KxxxxxxxxxxxxxxxxxxxxK",
        ".KXXXXXXXXXXXXXXXXXXK.",
        "..KKKKKKKKKKKKKKKKKK..",
        "uuuUuuuUuuuUuuuUuuuUuu",
        "......................",
        "......................",
    };
    static const std::vector<std::string> transport = {
        "......................",
        "......................",
        "...............KKK....",
        "...............KxK....",
        "..............KKCKK...",
        "..............KwwwK...",
        "..KKKK.KKKK...KwcwK...",
        ".KnnnnKnnnnK.KKwwwKK..",
        "KKKKKKKKKKKKKKKKKKKKKK",
        "KCCCCCCCCCCCCCCCCCCCCK",
        "KGGGGGGGGGGGGGGGGGGGGK",
        ".KGGKGGKGGKGGKGGKGGGK.",
        "..KHHHHHHHHHHHHHHHHK..",
        "...KKKKKKKKKKKKKKKK...",
        "uuUuuuUuuuUuuuUuuuUuuu",
        "......................",
        "......................",
    };
    static const std::vector<std::string> cruiser = {
        "......................",
        "...........K..........",
        "...........K..........",
        "..........KGK.........",
        ".........KKgKK........",
        ".........KgggK........",
        "...KK..KKKgggKKK..KK..",
        "KKKGGKKGGGGGGGGGKKGGKK",
        "..KKKKKKKKKKKKKKKKKKK.",
        "KKgggggggggggggggggggK",
        "KgGGGGGGCCCCCGGGGGGGK.",
        ".KGGGGGGGGGGGGGGGGGK..",
        "..KHHHHHHHHHHHHHHHK...",
        "...KKKKKKKKKKKKKKK....",
        "uuUuuuUuuuUuuuUuuuUuuu",
        "......................",
        "......................",
    };
    static const std::vector<std::string> battleship = {
        "..........K...........",
        "..........K...........",
        ".........KGK..........",
        "........KKgKK.........",
        "........KgggK.........",
        ".......KKgggKK........",
        ".KKK...KgGgGgK...KKK..",
        "KGGGKKKKKgggKKKKKGGGKK",
        ".KKKGGKKGGGGGKKGGKKK..",
        "KggggggggggggggggggggK",
        "KgGGGCCCCCCCCCCCCGGGGK",
        "KGGGGGGGGGGGGGGGGGGGGK",
        ".KHHHHHHHHHHHHHHHHHHK.",
        "..KKKKKKKKKKKKKKKKKK..",
        "uuUuuuUuuuUuuuUuuuUuuu",
        "......................",
        "......................",
    };
    static const std::vector<std::string> carrier = {
        "......................",
        "......................",
        "......................",
        "................KK....",
        "...............KGGK...",
        "...............KgGK...",
        "....K..........KgGK...",
        "...KxKK........KgGK...",
        "..KKxxxK......KKGGKK..",
        "KKKKKKKKKKKKKKKKKKKKKK",
        "KgwgggwgggwgggwgggwggK",
        "KCCCCCCCCCCCCCCCCCCCCK",
        "KGGGGGGGGGGGGGGGGGGGGK",
        ".KHHHHHHHHHHHHHHHHHHK.",
        "..KKKKKKKKKKKKKKKKKK..",
        "uuUuuuUuuuUuuuUuuuUuuu",
        "......................",
    };
    static const std::vector<std::string> submarine = {
        "......................",
        "......................",
        "......................",
        "......................",
        "..........K...........",
        "..........K...........",
        ".........KKKKK........",
        "........KxxxxxK.......",
        "........KxCCxxK.......",
        "....KKKKKxxxxxKKKKK...",
        "..KKxxxxxxxxxxxxxxxKK.",
        ".KxxXXXXXXXXXXXXXXXxxK",
        "uuuUuuuUuuuUuuuUuuuUuu",
        "uccuuucuuuuccuuuuucuuu",
        "uuuuuuuuuuuuuuuuuuuuuu",
        "......................",
        "......................",
    };
    static const std::vector<std::string> fighter = {
        "..........KK..........",
        ".........KggK.........",
        ".........KgcK.........",
        ".........KccK.........",
        "........KgggGK........",
        "........KgCCGK........",
        ".......KKgggGKK.......",
        ".....KKgggggggGKK.....",
        "...KKggCCggggCCgGKK...",
        ".KKgggggggggggggggGKK.",
        "KGGGGGGGgggggGGGGGGGGK",
        ".KKKKKKKKgggGKKKKKKKK.",
        "........KgggGK........",
        ".......KKgggGKK.......",
        "......KgggCCggGK......",
        "......KKKKKKKKKK......",
        "......................",
    };
    static const std::vector<std::string> bomber = {
        "..........KK..........",
        ".........KgcK.........",
        ".........KccK.........",
        ".........KggK.........",
        ".....KK..KggK..KK.....",
        "....KxxK.KggK.KxxK....",
        "KKKKKxxKKKggKKKxxKKKKK",
        "KgggggggggggggggggggGK",
        "KgCCgggggggggggggCCgGK",
        "KGGGGGGGGgggGGGGGGGGGK",
        ".KKKKKKKKKggKKKKKKKKK.",
        ".........KggK.........",
        ".........KggK.........",
        "......KKKKggKKKK......",
        "......KgCCggCCgGK.....",
        "......KKKKKKKKKKK.....",
        "......................",
    };
    static const std::vector<std::string> nuclear = {
        "..........KK..........",
        ".........KrrK.........",
        "........KrrrrK........",
        "........KwwwwK........",
        "........KwyywK........",
        "........KwYYwK........",
        "........KwyywK........",
        "........KwwwwK........",
        "........KCCCCK........",
        "........KwwwwK........",
        "........KwwwwK........",
        ".......KKwwwwKK.......",
        "......KrKwwwwKrK......",
        ".....KrrKwWWwKrrK.....",
        ".....KKKKKKKKKKKK.....",
        ".........KooK.........",
        "..........oo..........",
    };
    // Indexed by civ::UnitType. Militia reuse the old warrior figure; the
    // Cavalry figure is the light horseman.
    static const std::vector<std::string> *const table[] = {
        &settlers,
        &warriors,
        &phalanx,
        &legion,
        &chariot,
        &horsemen,
        &knights,
        &catapult,
        &musketeers,
        &riflemen,
        &cannon,
        &artillery,
        &armor,
        &mechinf,
        &diplomat,
        &caravan,
        &trireme,
        &sail,
        &frigate,
        &ironclad,
        &transport,
        &cruiser,
        &battleship,
        &carrier,
        &submarine,
        &fighter,
        &bomber,
        &nuclear,
    };
    return *table[static_cast<int>(u)];
}

const std::vector<std::string> &special_rows(Special s)
{
    static const std::vector<std::string> fish = {
        "..............",
        "..KK..........",
        ".KccK....KK...",
        "KcKcccKKKcK...",
        "KcccccccKK....",
        ".KccccKKKcK...",
        "..KK.....KK...",
        ".......KK.....",
        "......KccK..K.",
        ".....KcKcccKcK",
        ".....KcccccKK.",
        "......KccK..K.",
        ".......KK.....",
    };
    static const std::vector<std::string> horses = {
        "..............",
        "..KK..........",
        ".KmmKK........",
        "KmqmmmK.......",
        "KqKKmmmKKKKK..",
        ".K..KmmmmmmmK.",
        "....KmmmmmmmMK",
        "....KmMMMMMmKMK",
        "....KmK.KmKmK.",
        "....KmK.KmKmK.",
        "...KMK.KMKKMK.",
        "...KK..KK.KK..",
    };
    static const std::vector<std::string> oasis = {
        "....KK.KK.....",
        "...KeeKeeK....",
        "..KeEKKKEeK...",
        "..KeK.b.KeK...",
        "......b.......",
        ".....Kb.......",
        ".....Kb.......",
        "..KKKKbKKKK...",
        ".KtttttttttK..",
        "KtTtttTtttttK.",
        ".KtttttttTtK..",
        "..KKKKKKKKK...",
    };
    static const std::vector<std::string> oil = {
        "......K.......",
        ".....KxK......",
        "....KxXxK.....",
        "...KxXxxxK....",
        "...KxXxxxK....",
        "....KxxxK.....",
        ".....KKK......",
        "...KKKKKKKK...",
        "..KxxpxtxxxK..",
        ".KxxxxxxxxxxK.",
        "..KKKKKKKKKK..",
    };
    static const std::vector<std::string> game = {
        ".K.K..........",
        ".KbK.K........",
        "..KbbK........",
        "...KbK........",
        "..KmmK........",
        ".KmqmmK.......",
        "KqKmmmKKKKKK..",
        ".KK.KmmmmmmmK.",
        "....KmmqqqmmK.",
        "....KmKKKKmK..",
        "....KmK..KmK..",
        "...KMK..KMK...",
    };
    static const std::vector<std::string> gems = {
        "..............",
        "....KKKK......",
        "...KpwppK.....",
        "..KpppppPK....",
        "..KPpppPPK.KK.",
        "...KPPPPK.KtwK",
        "....KPPK.KtttTK",
        ".....KK..KTttTK",
        "...KKK....KTTK.",
        "..KcwcK....KK..",
        "..KccuK.......",
        "...KKK........",
    };
    static const std::vector<std::string> gold = {
        "..............",
        "..............",
        ".....KKK......",
        "....KywyK.....",
        "...KyyyYYK....",
        "..KKYyYKKK....",
        ".KywyKKyyyK...",
        "KyyyYKyywyYK..",
        "KYyYYKYyyYYK..",
        ".KKKKKKKKKK...",
    };
    static const std::vector<std::string> coal = {
        "..............",
        "..............",
        ".....KKK......",
        "....KXxxK.....",
        "...KXxxxxK....",
        "..KKKxxKKKK...",
        ".KXxxKKXxxxK..",
        "KXxxxxKxxxxxK.",
        "KxxxxxKxxxxxK.",
        ".KKKKKKKKKKK..",
    };
    static const std::vector<std::string> seals = {
        "..............",
        "..............",
        "...KKK........",
        "..KXXXK.......",
        "..KxXXK.......",
        "...KXXXK......",
        "....KXXXKK....",
        "....KXXXXXK...",
        "...KXXXGXXXK..",
        "..KXXXGGGXXXK.",
        "..KKKKKKKKKKKK",
    };
    static const std::vector<std::string> shield = {
        "KKKKKKK",
        "KyYyYyK",
        "KYbbbYK",
        "KybbbyK",
        ".KYbYK.",
        "..KyK..",
        "...K...",
    };
    static const std::vector<std::string> none = {};
    switch (s) {
    case Special::Fish: return fish;
    case Special::Horses: return horses;
    case Special::Oasis: return oasis;
    case Special::Oil: return oil;
    case Special::Game: return game;
    case Special::Gems: return gems;
    case Special::Gold: return gold;
    case Special::Coal: return coal;
    case Special::Seals: return seals;
    case Special::Shield: return shield;
    default: return none;
    }
}

} // namespace

const char *terrain_name(Terrain t)
{
    static const char *n[] = {"Ocean", "Grassland", "Plains", "Desert", "Tundra", "Arctic",
                              "Forest", "Jungle", "Swamp", "Hills", "Mountains"};
    return n[static_cast<int>(t)];
}

const char *special_name(Special s)
{
    static const char *n[] = {"", "Fish", "Horses", "Oasis", "Oil", "Game", "Gems", "Gold", "Coal", "Seals", "Shield"};
    return n[static_cast<int>(s)];
}

Terrain special_home(Special s)
{
    switch (s) {
    case Special::Fish: return Terrain::Ocean;
    case Special::Horses: return Terrain::Plains;
    case Special::Oasis: return Terrain::Desert;
    case Special::Oil: return Terrain::Swamp;
    case Special::Game: return Terrain::Forest;
    case Special::Gems: return Terrain::Jungle;
    case Special::Gold: return Terrain::Mountains;
    case Special::Coal: return Terrain::Hills;
    case Special::Seals: return Terrain::Arctic;
    case Special::Shield: return Terrain::Grassland;
    default: return Terrain::Grassland;
    }
}

Pixmap sprite(const std::vector<std::string> &rows, const CivColors *civ)
{
    int w = 0;
    for (const auto &r : rows) {
        w = std::max(w, static_cast<int>(r.size()));
    }
    Pixmap pm(w, static_cast<int>(rows.size()));
    for (int y = 0; y < pm.h; ++y) {
        for (int x = 0; x < static_cast<int>(rows[y].size()); ++x) {
            pm.set(x, y, ink_for(rows[y][x], civ));
        }
    }
    return pm;
}

int tiny_width(const std::string &text)
{
    return text.empty() ? 0 : static_cast<int>(text.size()) * 4 - 1;
}

void tiny_text(Pixmap &pm, int x, int y, const std::string &text, Argb ink)
{
    for (char ch : text) {
        auto it = glyphs().find(ch);
        if (it != glyphs().end()) {
            for (int j = 0; j < 5; ++j) {
                for (int i = 0; i < 3; ++i) {
                    if (it->second[j][i] == '#') {
                        pm.set(x + i, y + j, ink);
                    }
                }
            }
        }
        x += 4;
    }
}

Pixmap unit_card(civ::UnitType u, const CivColors &civ, char status, bool veteran)
{
    Pixmap pm(kTile, kTile);
    const Argb outline = rgb(0x14121A);
    const Argb parch = rgb(0xEADFC0);
    const Argb parch_hi = rgb(0xF6EEDA);
    const Argb parch_lo = rgb(0xCDBE96);
    const int x0 = 2, y0 = 1, x1 = 29, y1 = 28; // 28x28 counter
    // Drop shadow, one pixel down-right, like a cardboard counter on a board.
    for (int y = y0 + 1; y <= y1 + 1; ++y) {
        pm.blend(x1 + 1, y, 0x70000000);
    }
    for (int x = x0 + 1; x <= x1 + 1; ++x) {
        pm.blend(x, y1 + 1, 0x70000000);
    }
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const bool corner = (x == x0 || x == x1) && (y == y0 || y == y1);
            if (corner) {
                continue; // clipped corners read as a die-cut card
            }
            Argb c = civ.main;
            if (x == x0 || x == x1 || y == y0 || y == y1) {
                c = outline;
            } else if (x == x0 + 1 || y == y0 + 1) {
                c = civ.light;
            } else if (x == x1 - 1 || y == y1 - 1) {
                c = civ.dark;
            }
            pm.set(x, y, c);
        }
    }
    // Parchment window for the figure: x 5..26, y 4..20 (22x17).
    const int wx = 5, wy = 4, ww = 22, wh = 17;
    for (int y = wy - 1; y <= wy + wh; ++y) {
        for (int x = wx - 1; x <= wx + ww; ++x) {
            const bool edge = x == wx - 1 || y == wy - 1 || x == wx + ww || y == wy + wh;
            pm.set(x, y, edge ? civ.dark : parch);
        }
    }
    for (int x = wx; x < wx + ww; ++x) {
        pm.set(x, wy, parch_lo);
        if (((x + wy + 1) & 1) == 0) {
            pm.set(x, wy + wh - 1, parch_hi);
        }
    }
    for (int y = wy; y < wy + wh; ++y) {
        pm.set(wx, y, parch_lo);
    }
    Pixmap fig = sprite(figure(u), &civ);
    for (int y = 0; y < fig.h && y < wh; ++y) {
        for (int x = 0; x < fig.w && x < ww; ++x) {
            const Argb c = fig.get(x, y);
            if (c >> 24) {
                pm.set(wx + x, wy + y, c);
            }
        }
    }
    // Attack-defense-moves strip, board-game style.
    const civ::UnitInfo &info = civ::unit_info(u);
    const std::string stats = std::to_string(info.attack) + "-" + std::to_string(info.defense) + "-" + std::to_string(info.moves);
    const int sx = 16 - tiny_width(stats) / 2;
    tiny_text(pm, sx + 1, 23, stats, civ.dark);
    tiny_text(pm, sx, 22, stats, rgb(0xFFF8E8));
    if (status) {
        // Status tab hangs off the top-left corner: F fortified, S sentry...
        pm.fill_rect(0, 0, 7, 9, outline);
        pm.fill_rect(1, 1, 5, 7, rgb(0xF2C14E));
        tiny_text(pm, 2, 2, std::string(1, status), outline);
    }
    if (veteran) {
        // Veterans wear a gold chevron on the top-right corner.
        const Argb gold = rgb(0xF2C14E);
        for (int i = 0; i < 4; ++i) {
            pm.set(24 + i, 2 + i, outline);
            pm.set(31 - i, 2 + i, outline);
            pm.set(24 + i, 1 + i, gold);
            pm.set(31 - i, 1 + i, gold);
        }
        pm.set(27, 5, gold);
        pm.set(28, 5, gold);
        pm.set(27, 6, outline);
        pm.set(28, 6, outline);
    }
    return pm;
}

Pixmap unit_figure(civ::UnitType u, const CivColors &civ)
{
    return sprite(figure(u), &civ);
}

Pixmap hut_sprite()
{
    static const std::vector<std::string> rows = {
        "......KK......",
        ".....KyyK.....",
        "....KyYyyK....",
        "...KyyyYyyK...",
        "..KyYyyyyYyK..",
        ".KyyyyYyyyyyK.",
        "KKKKKKKKKKKKKK",
        ".KnnnnKKnnnnK.",
        ".KnNnnKhKnnnK.",
        ".KnnnnKhKnNnK.",
        ".KNNNNKKKNNNK.",
        "..KKKKKKKKKK..",
    };
    return sprite(rows);
}

Pixmap fortress_sprite()
{
    // An earth rampart with timber corners around the square's edge.
    Pixmap pm(kTile, kTile);
    const Argb out = rgb(0x1A1620), earth = rgb(0x8A6A3E), lit = rgb(0xB08C56), post = rgb(0x5A3618);
    for (int i = 2; i < 30; ++i) {
        for (int t = 0; t < 3; ++t) {
            const Argb c = t == 1 ? lit : earth;
            pm.set(i, 2 + t, c);
            pm.set(i, 27 + t, c);
            pm.set(2 + t, i, c);
            pm.set(27 + t, i, c);
        }
        pm.set(i, 1, out);
        pm.set(i, 30, out);
        pm.set(1, i, out);
        pm.set(30, i, out);
    }
    const int corners[4][2] = {{1, 1}, {26, 1}, {1, 26}, {26, 26}};
    for (const auto &k : corners) {
        pm.fill_rect(k[0], k[1], 5, 5, out);
        pm.fill_rect(k[0] + 1, k[1] + 1, 3, 3, post);
    }
    return pm;
}

Pixmap special_sprite(Special s)
{
    return sprite(special_rows(s));
}

Pixmap mine_sprite()
{
    static const std::vector<std::string> rows = {
        "....KKKKKKKK....",
        "...KbbbbbbbbK...",
        "...KBKKKKKKBK...",
        "...KbKxxxxKbK...",
        "...KbKxXxxKbK...",
        "...KbKxxxxKbK...",
        "...KbKxxxxKbK...",
        ".KKKKKKKKKKKKKK.",
        "KGGGGK.KKK......",
        "KHHHHK.KvvK.....",
        ".KXK.XKKviivK...",
        ".KKK.KKvviivvK..",
        "......KKKKKKKK..",
    };
    return sprite(rows);
}

Pixmap yield_icon(int kind)
{
    static const std::vector<std::string> food = {
        "..KKK..", ".KeeeK.", "KeEeEeK", "KeeEeeK", ".KeeeK.", "..KbK..", "...K...",
    };
    static const std::vector<std::string> shield = {
        "KKKKKKK", "KiviviK", "KvVVVvK", "KiVVViK", ".KvVvK.", "..KiK..", "...K...",
    };
    static const std::vector<std::string> trade = {
        "...K...", "..KyK..", ".KyyyK.", "KyyYyyK", "..KyK..", "..KyK..", "..KKK..",
    };
    return sprite(kind == 0 ? food : (kind == 1 ? shield : trade));
}

Pixmap city_tile(int size, bool walls, bool capital, const CivColors &civ, int seed)
{
    Pixmap pm(kTile, kTile);
    // Packed-earth plaza.
    for (int y = 3; y < 30; ++y) {
        for (int x = 2; x < 30; ++x) {
            const double n = hashf(x, y, seed + 7);
            Argb c = rgb(0xA4875A);
            if (n < 0.18) {
                c = rgb(0x8C7048);
            } else if (n > 0.86) {
                c = rgb(0xBC9E6E);
            }
            pm.set(x, y, c);
        }
    }
    static const std::vector<std::string> house_a = {
        ".KKKKKKK.",
        "KwwwwwwwK",
        "KnnnnnnnK",
        "KnKKnnnnK",
        "KnKKnnKnK",
        "KNKKNNNNK",
        "KKKKKKKKK",
    };
    static const std::vector<std::string> house_b = {
        "...KKK...",
        "..KrrrK..",
        ".KrrRrrK.",
        "KrrrrrRrK",
        "KKnnnnnKK",
        ".KnKKnnK.",
        ".KNKKNNK.",
        ".KKKKKKK.",
    };
    static const std::vector<std::string> house_c = {
        "KKKKK",
        "KwwwK",
        "KnnnK",
        "KnKnK",
        "KKKKK",
    };
    static const std::vector<std::string> temple = {
        "....KKKKK....",
        "....KwwwK....",
        "..KKKKKKKKK..",
        "..KwwwwwwwK..",
        "..KnKnKnKnK..",
        "KKKKKKKKKKKKK",
        "KwwwwwwwwwwwK",
        "KnKnKnKnKnKnK",
        "KnKnKnKnKnKnK",
        "KKKKKKKKKKKKK",
    };
    static const std::vector<std::string> palace = {
        "......K......",
        ".....KCK.....",
        ".....KCLK....",
        ".....KK......",
        "..KKKKKKKKK..",
        "..KyiiiiiyK..",
        "KKKiiiiiiiKKK",
        "KiiiKiKiKiiiK",
        "KviiKiKiKiivK",
        "KvvvvvvvvvvvK",
        "KKKKKKKKKKKKK",
    };
    struct Slot {
        int x, y, kind;
    };
    // Buildings fill the plaza from the back rows forward as the city grows.
    std::vector<Slot> slots = {{4, 5, 0}, {18, 4, 1}, {10, 13, 2}, {3, 18, 1}, {20, 15, 0}, {14, 21, 0}, {22, 23, 2}, {4, 24, 2}};
    const int count = std::min(static_cast<int>(slots.size()), 3 + size * 2);
    if (capital) {
        pm.blit(sprite(palace, &civ), 15, 3);
    } else if (size >= 3) {
        pm.blit(sprite(temple), 16, 5);
    }
    for (int i = 0; i < count; ++i) {
        const Slot &s = slots[i];
        if ((capital || size >= 3) && s.x >= 14 && s.y < 15) {
            continue; // the palace or temple stands there
        }
        const auto &rows = s.kind == 0 ? house_a : (s.kind == 1 ? house_b : house_c);
        pm.blit(sprite(rows), s.x, s.y);
    }
    if (walls) {
        const Argb stone = rgb(0x9A9284), stone_hi = rgb(0xC4BCAC), stone_lo = rgb(0x5E584E), out = rgb(0x1A1620);
        auto wall_px = [&](int x, int y, bool lit) { pm.set(x, y, lit ? stone_hi : stone); };
        for (int i = 1; i < 31; ++i) {
            for (int t = 0; t < 2; ++t) {
                wall_px(i, 1 + t, t == 0);
                wall_px(i, 29 + t, t == 0);
                wall_px(1 + t, i, t == 0);
                wall_px(29 + t, i, t == 0);
            }
            pm.set(i, 3, stone_lo);
            pm.set(3, i, stone_lo);
            pm.set(i, 31, out);
            pm.set(31, i, out);
            pm.set(i, 0, (i % 4 < 2) ? out : kClear);
            pm.set(0, i, (i % 4 < 2) ? out : kClear);
        }
        for (int i = 1; i < 31; i += 4) {
            pm.set(i, 0, stone_hi);
            pm.set(i + 1, 0, stone_hi);
            pm.set(0, i, stone_hi);
            pm.set(0, i + 1, stone_hi);
        }
    }
    // Size plate: civ colour with the population number.
    const std::string n = std::to_string(size);
    const int pw = tiny_width(n) + 6;
    const int px = 1, py = 20;
    pm.fill_rect(px, py, pw, 9, rgb(0x14121A));
    pm.fill_rect(px + 1, py + 1, pw - 2, 7, civ.main);
    for (int x = px + 1; x < px + pw - 1; ++x) {
        pm.set(x, py + 1, civ.light);
    }
    tiny_text(pm, px + 3, py + 2, n, rgb(0xFFF8E8));
    return pm;
}

} // namespace art
