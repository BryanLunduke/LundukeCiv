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

// ---------------------------------------------------------------- barbarians

TEST(barbarian_restraint)
{
    // DOS: barbarians leave a people with one city and under 100 gold
    // alone, and a band with no city within eight squares breaks up.
    Game g = flat_game();
    const int bc = g.barbarian_civ();
    CHECK(bc >= 0);
    if (bc < 0) return;
    City &c = town(g, 0, 20, 20, 3, "Lonely");
    const int cid = c.id;
    g.civs[0].gold = 20;
    CHECK(g.barbarians_spare(0));
    const int raider = place(g, bc, UnitType::Legion, 22, 20);
    for (int t = 0; t < 6; ++t) {
        g.turn = 101 + t * 4;   // ((id + turn) & 3) != 0 for most: the band stays
        for (Unit &u : g.units) u.moves = g.full_moves(u);
        g.ai_barbarians(bc);
    }
    CHECK(g.city(cid) != nullptr && g.city(cid)->owner == 0);
    // Rich, or with a second city, they come.
    g.civs[0].gold = 150;
    CHECK(!g.barbarians_spare(0));
    g.civs[0].gold = 20;
    town(g, 0, 20, 30, 3, "Second");
    CHECK(!g.barbarians_spare(0));
    bool attacked = false;
    for (int t = 0; t < 8 && !attacked; ++t) {
        g.turn = 130 + t;
        for (Unit &u : g.units) u.moves = g.full_moves(u);
        const size_t before = g.units.size();
        g.ai_barbarians(bc);
        const Unit *r = g.unit(raider);
        attacked = !r || g.units.size() != before || g.city(cid) == nullptr || g.city(cid)->owner != 0 ||
                   (r && g.map.distance(r->x, r->y, 20, 20) <= 1);
    }
    CHECK(attacked);

    // A band far from any city breaks up within four turns.
    Game h = flat_game();
    town(h, 0, 10, 10, 3, "Home");
    const int lost = place(h, h.barbarian_civ(), UnitType::Legion, 40, 30);
    for (int t = 0; t < 4; ++t) {
        h.turn = 100 + t;
        for (Unit &u : h.units) u.moves = h.full_moves(u);
        h.ai_barbarians(h.barbarian_civ());
    }
    CHECK(h.unit(lost) == nullptr);
}

TEST(hut_barbarians_gentle)
{
    // No horde from a village before turn 50 or while the people that
    // opens it has a single city; with two cities, later on, there can be.
    Game g = flat_game();
    town(g, 0, 10, 10, 3, "Home");
    g.turn = 20;
    CHECK(g.hut_outcome(0, 30, 30, 3) == HutOutcome::Mercenaries);
    g.turn = 120;
    CHECK(g.hut_outcome(0, 30, 30, 3) == HutOutcome::Mercenaries);
    town(g, 0, 10, 20, 3, "Two");
    CHECK(g.hut_outcome(0, 30, 30, 3) == HutOutcome::Barbarians);
    g.turn = Game::kBarbarianStartTurn - 1;
    CHECK(g.hut_outcome(0, 30, 30, 3) == HutOutcome::Mercenaries);
}

// ---------------------------------------------------------------- civil war

TEST(civil_war_stacks)
{
    // When a people splits, a square in the field keeps to one side and
    // cargo stays with its ship: no square holds two peoples' units.
    Game g = flat_game(Terrain::Grassland, 3);
    g.civs[1].ai = true;
    g.civs[1].tribe = 0;
    g.civs[1].gold = 400;
    const int xs[] = {10, 16, 22, 28, 34, 40};
    for (int i = 0; i < 6; ++i) town(g, 1, xs[i], 20, 6, "Roma");
    g.civs[2].alive = false;
    for (Unit &u : g.units) u.owner = 1;
    const int rebel_home = g.city_at(40, 20)->id, loyal_home = g.city_at(16, 20)->id;
    int stack[3];
    for (int k = 0; k < 3; ++k) {
        stack[k] = place(g, 1, UnitType::Phalanx, 30, 24);
        g.unit(stack[k])->home = k < 2 ? rebel_home : loyal_home;
    }
    // A ship of the old people's at sea with a rebel-homed passenger.
    for (int y = 0; y < g.map.h; ++y) g.map.at(50, y).terrain = Terrain::Ocean;
    g.map.label_continents();
    const int ship = place(g, 1, UnitType::Trireme, 50, 22);
    g.unit(ship)->home = loyal_home;
    const int rider = place(g, 1, UnitType::Legion, 50, 22);
    g.unit(rider)->home = rebel_home;
    g.unit(rider)->carried_by = ship;
    g.declare_war(0, 1);
    const int a = place(g, 0, UnitType::Armor, 9, 20);
    g.move(a, 1, 0);
    CHECK_EQ(g.civil_wars, 1);
    const int r = g.city_at(40, 20)->owner;
    CHECK(r != 1);
    CHECK_EQ(g.unit(stack[0])->owner, r);
    CHECK_EQ(g.unit(stack[1])->owner, r);
    CHECK_EQ(g.unit(stack[2])->owner, r);   // outvoted: goes with the square
    CHECK_EQ(g.unit(rider)->owner, g.unit(ship)->owner);
    CHECK_EQ(g.unit(rider)->carried_by, ship);
    std::map<std::pair<int, int>, int> owner_of;
    for (const Unit &u : g.units) {
        auto it = owner_of.find({u.x, u.y});
        if (it == owner_of.end()) owner_of[{u.x, u.y}] = u.owner;
        else CHECK_EQ(it->second, u.owner);
        // Support is with one's own side.
        if (const City *h = g.city(u.home)) CHECK_EQ(h->owner, u.owner);
    }
}

// ---------------------------------------------------------------- saves

TEST(ai_save_roundtrip)
{
    // A game of computer rulers loads exactly as it was saved, cities'
    // specialists included, all through the game.
    for (unsigned long long seed : {202ull, 505ull}) {
        GameSettings s;
        s.seed = seed;
        s.civs = 7;
        s.all_ai = true;
        s.player_tribe = 0;
        Game g = Game::create(s);
        int differ = 0;
        for (int t = 0; t < 240; ++t) {
            g.end_turn();
            if (t % 20 != 19) continue;
            const std::string a = save_game(g);
            Game h;
            std::string err;
            CHECK(load_game(a, h, &err));
            if (save_game(h) != a) ++differ;
        }
        CHECK_EQ(differ, 0);
    }
}

// ---------------------------------------------------------------- computer rulers

TEST(ai_space_part_balance)
{
    // The frame grows with what it must hold: 13 structurals for 11
    // components and 11 modules are not enough, 22 are; and parts other
    // cities are building count, so a ship from nothing comes out viable.
    Game g = flat_game();
    const City &a = town(g, 0, 10, 10, 8, "Cape");
    const int aid = a.id;
    Civ &c = g.civs[0];
    c.structurals = 13;
    c.components = 11;
    c.modules = 11;
    CHECK_EQ(g.ai_space_part(0, aid), static_cast<int>(SpacePart::Structural));
    c.structurals = 22;
    CHECK_EQ(g.ai_space_part(0, aid), -1);
    c.structurals = c.components = c.modules = 0;
    for (int i = 0; i < 60; ++i) {
        const int p = g.ai_space_part(0, aid);
        if (p < 0) break;
        if (p == static_cast<int>(SpacePart::Structural)) ++c.structurals;
        else if (p == static_cast<int>(SpacePart::Component)) ++c.components;
        else ++c.modules;
    }
    CHECK(g.space_stats(0).viable);
    CHECK(c.structurals >= c.components + c.modules);
    // A module under way in a second city counts towards the target.
    c.structurals = 10;
    c.components = 4;
    c.modules = 2;
    City &b = town(g, 0, 20, 10, 8, "Bay");
    b.build_unit = false;
    b.build = kSpaceBase + static_cast<int>(SpacePart::Module);
    CHECK(g.ai_space_part(0, aid) != static_cast<int>(SpacePart::Module));
}

TEST(ai_government)
{
    // DOS: the Republic when trade outweighs troops in the field and no
    // war with a people ahead in science; Communism for an empire of more
    // than ten cities; otherwise Monarchy.
    Game g = flat_game();
    for (int i = 0; i < 3; ++i) town(g, 0, 6 + i * 5, 10, 3, "Town");
    Civ &c = g.civs[0];
    c.techs[static_cast<int>(Advance::Monarchy)] = true;
    CHECK(g.ai_government(0) == Government::Monarchy);
    c.techs[static_cast<int>(Advance::Republic)] = true;
    CHECK(g.ai_government(0) == Government::Republic);
    // An army in the field: Monarchy.
    const int home = g.cities.front().id;
    for (int i = 0; i < 3; ++i) g.unit(place(g, 0, UnitType::Legion, 30 + i, 30))->home = home;
    CHECK(g.ai_government(0) == Government::Monarchy);
    // A big empire with Communism known.
    c.techs[static_cast<int>(Advance::Communism)] = true;
    for (int i = 0; i < 8; ++i) town(g, 0, 6 + i * 5, 20, 3, "More");
    CHECK(g.ai_government(0) == Government::Communism);
    // Peace at home, but war with a people further ahead: no Republic.
    for (size_t i = 0; i < g.units.size();)
        if (g.units[i].type == UnitType::Legion) g.units.erase(g.units.begin() + static_cast<long>(i)); else ++i;
    c.techs[static_cast<int>(Advance::Communism)] = false;
    CHECK(g.ai_government(0) == Government::Republic);
    for (int a = 0; a < 20; ++a) g.civs[1].techs[a] = true;
    g.declare_war(1, 0);
    CHECK(g.ai_government(0) == Government::Monarchy);
}
