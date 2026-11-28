// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// QA-pass tests: fixes from the m5 review (ships, huts, the computer
// rulers' economy, governments and spaceships, civil war, saves). One named
// group per meson test.

#include "fame.hpp"
#include "game.hpp"
#include "options.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>

using namespace civ;

static int failures = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                                                 \
    do {                                                                                               \
        const long long va = (a), vb = (b);                                                            \
        if (va != vb) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s == %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb); \
            ++failures;                                                                                \
        }                                                                                              \
    } while (0)

static std::map<std::string, std::function<void()>> &registry()
{
    static std::map<std::string, std::function<void()>> tests;
    return tests;
}

#define TEST(name)                                                                  \
    static void test_##name();                                                      \
    static const bool registered_##name = (registry()[#name] = test_##name, true); \
    static void test_##name()

int main(int argc, char **argv)
{
    if (argc < 2) {
        for (const auto &t : registry()) t.second();
    } else {
        auto it = registry().find(argv[1]);
        if (it == registry().end()) {
            std::fprintf(stderr, "unknown test %s\n", argv[1]);
            return 2;
        }
        it->second();
    }
    if (failures) std::fprintf(stderr, "%d check(s) failed\n", failures);
    return failures ? 1 : 0;
}

static Game flat_game(Terrain fill = Terrain::Grassland, int civs = 2)
{
    GameSettings s;
    s.seed = 7;
    s.civs = civs;
    Game g = Game::create(s);
    for (Tile &t : g.map.tiles) {
        t = Tile();
        t.terrain = fill;
        t.explored = 0xFF;
    }
    for (int y = 0; y < g.map.h; ++y) g.map.at(0, y).terrain = Terrain::Ocean;
    g.map.label_continents();
    g.units.clear();
    g.cities.clear();
    for (Civ &c : g.civs) c.war = 0;
    g.messages.clear();
    return g;
}

static int place(Game &g, int owner, UnitType t, int x, int y)
{
    Unit u;
    u.id = g.next_unit_id++;
    u.type = t;
    u.owner = owner;
    u.x = x;
    u.y = y;
    u.moves = g.full_moves(u);
    u.fuel = unit_info(t).fuel;
    g.units.push_back(u);
    return u.id;
}

static City &town(Game &g, int owner, int x, int y, int size, const char *name = "Town")
{
    const int s = place(g, owner, UnitType::Settlers, x, y);
    g.found_city(s, name);
    City &c = *g.city_at(x, y);
    c.size = size;
    g.update_city(c);
    return c;
}

[[maybe_unused]] static int turn_for_year(int year)
{
    int t = 1;
    while (year_for_turn(t) < year) ++t;
    return t;
}

[[maybe_unused]] static bool said(const Game &g, const std::string &what)
{
    for (const Message &m : g.messages)
        if (m.text.find(what) != std::string::npos) return true;
    return false;
}

// ---------------------------------------------------------------- the end
static Game sea_game()
{
    Game g = flat_game(Terrain::Ocean);
    return g;
}

static void island(Game &g, int x0, int y0, int x1, int y1)
{
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) g.map.at(x, y).terrain = Terrain::Grassland;
    g.map.label_continents();
}


// ---------------------------------------------------------------- ships

TEST(ship_capture)
{
    // A Legion that takes an empty enemy city straight off a ship is
    // ashore: the ship sails on without it.
    Game g = sea_game();
    island(g, 10, 10, 13, 13);
    City &c = town(g, 1, 10, 12, 2, "Shore");
    for (size_t i = 0; i < g.units.size();)
        if (g.units[i].owner == 1) g.units.erase(g.units.begin() + static_cast<long>(i)); else ++i;
    g.declare_war(0, 1);
    const int ship = place(g, 0, UnitType::Trireme, 9, 12);
    const int leg = place(g, 0, UnitType::Legion, 9, 12);
    g.unit(leg)->carried_by = ship;
    g.unit(leg)->order = Order::Sentry;
    const int cid = c.id;
    CHECK(g.move(leg, 1, 0) == MoveResult::Captured);
    CHECK_EQ(g.unit(leg)->x, 10);
    CHECK_EQ(g.unit(leg)->carried_by, -1);
    CHECK(g.city(cid) == nullptr || g.city(cid)->owner == 0);
    CHECK(g.move(ship, 0, -1) == MoveResult::Moved);
    CHECK_EQ(g.unit(leg)->x, 10);
    CHECK_EQ(g.unit(leg)->y, 12);
    // Cargo that somehow is not on the ship's square stays where it is.
    const int stray = place(g, 0, UnitType::Phalanx, 12, 12);
    g.unit(stray)->carried_by = ship;
    CHECK(g.move(ship, 0, -1) == MoveResult::Moved);
    CHECK_EQ(g.unit(stray)->x, 12);
    CHECK_EQ(g.unit(stray)->y, 12);
}
