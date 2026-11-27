// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Milestone 4 tests: the Civ 1 rules filled in -- diplomacy, the Senate,
// civil war, happiness and celebrations, buying and selling, nuclear
// attack, disasters, tribes and worlds. One named group per meson test.

#include "game.hpp"

#include <algorithm>
#include <cstdio>
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

static bool said(const Game &g, const std::string &part)
{
    for (const auto &m : g.messages) {
        if (m.text.find(part) != std::string::npos) return true;
    }
    return false;
}

// ---------------------------------------------------------------- B: rules

static void test_empire_size()
{
    Game g = flat_game();
    g.difficulty = Difficulty::Prince;   // level 2
    g.civs[0].govt = Government::Despotism;
    // (1/2 + 2) x ((14 - 4) / 2) = 2 x 5 = 10
    CHECK_EQ(g.empire_size_step(0), 10);
    g.civs[0].govt = Government::Democracy;   // (5/2 + 2) x 5 = 20
    CHECK_EQ(g.empire_size_step(0), 20);
    g.difficulty = Difficulty::Emperor;       // (2 + 2) x 3 = 12
    CHECK_EQ(g.empire_size_step(0), 12);
    g.difficulty = Difficulty::Prince;
    g.civs[0].govt = Government::Despotism;
    // Twelve cities against a step of 10: ((id % 10) + 12 - 10) / 10.
    for (int i = 0; i < 12; ++i) town(g, 0, 4 + (i % 6) * 5, 6 + (i / 6) * 6, 4);
    for (const City &c : g.cities) CHECK_EQ(g.empire_unhappy(c), ((c.id % 10) + 2) / 10);
    int extra = 0;
    for (const City &c : g.cities) extra += g.empire_unhappy(c);
    CHECK(extra > 0);
    // Computer rulers do not feel it.
    for (City &c : g.cities) c.owner = 1;
    for (const City &c : g.cities) CHECK_EQ(g.empire_unhappy(c), 0);
}

static void test_celebration()
{
    Game g = flat_game();
    g.civs[0].govt = Government::Monarchy;
    City &c = town(g, 0, 10, 10, 4);
    c.stats.unhappy = 0;
    c.stats.happy = 2;
    CHECK(g.celebration_due(c));
    c.stats.happy = 1;
    CHECK(!g.celebration_due(c));
    c.stats.happy = 3;
    c.size = 2;
    CHECK(!g.celebration_due(c));   // size 3 at least
    c.size = 4;
    g.civs[0].govt = Government::Anarchy;
    CHECK(!g.celebration_due(c));
    // Tile trade: Monarchy gets +1 on squares with trade while celebrating.
    g.civs[0].govt = Government::Monarchy;
    g.map.at(11, 10).road = true;
    g.map.at(11, 10).terrain = Terrain::Grassland;
    const Yield plain = g.tile_yield(11, 10, 0, false, false);
    const Yield party = g.tile_yield(11, 10, 0, false, true);
    CHECK_EQ(party.trade, plain.trade + 1);
    CHECK_EQ(g.tile_yield(12, 12, 0, false, true).trade, 0);   // no trade, no bonus
    // Despotism loses its -1 penalty: a 3-food special square keeps 3.
    g.civs[0].govt = Government::Despotism;
    g.map.at(13, 10).irrigation = true;   // grassland 2 + 1 = 3
    CHECK_EQ(g.tile_yield(13, 10, 0, false, false).food, 2);
    CHECK_EQ(g.tile_yield(13, 10, 0, false, true).food, 3);
    // Rapture growth under a Republic: lots of luxuries make a city party
    // and then grow one a turn.
    Game r = flat_game();
    r.civs[0].govt = Government::Republic;
    r.civs[0].lux = 6;
    r.civs[0].tax = 2;
    r.civs[0].sci = 2;
    City &rc = town(r, 0, 10, 10, 4);
    for (int x = 8; x <= 12; ++x)
        for (int y = 8; y <= 12; ++y) r.map.at(x, y).irrigation = r.map.at(x, y).road = true;
    rc.add(Building::Temple);
    rc.add(Building::Marketplace);
    r.update_city(rc);
    const int id = rc.id;
    int start = rc.size, partied = 0;
    for (int t = 0; t < 4; ++t) {
        r.end_turn();
        partied += r.city(id)->celebrating;
    }
    CHECK(partied > 0);
    CHECK(said(r, "Day celebrated in"));
    CHECK(r.city(id)->size >= start + 2);
}

static void test_democracy_collapse()
{
    Game g = flat_game();
    g.civs[0].govt = Government::Democracy;
    g.civs[0].lux = 0;
    g.civs[0].tax = 4;
    g.civs[0].sci = 6;
    City &c = town(g, 0, 10, 10, 8);
    c.manual = true;   // keep everyone working: no entertainers to calm them
    const int id = c.id;
    for (int t = 0; t < 3 && g.civs[0].govt == Government::Democracy; ++t) g.end_turn();
    CHECK(g.city(id) != nullptr);
    CHECK(g.civs[0].govt == Government::Anarchy);
    CHECK(said(g, "the government collapses"));
    CHECK(g.civs[0].anarchy >= 1 && g.civs[0].anarchy <= 4);
}

static void test_anarchy_length()
{
    Game g = flat_game();
    for (int t = 1; t <= 8; ++t) {
        g.turn = t;
        const int n = g.anarchy_turns(0);
        CHECK(n >= 1 && n <= 4);
        CHECK_EQ((t + n) % 4, 0);
    }
}

static void test_buy_cost()
{
    Game g = flat_game();
    City &c = town(g, 0, 10, 10, 3);
    c.build_unit = true;
    c.build = static_cast<int>(UnitType::Musketeers);   // 30 shields
    c.shields = 10;                                     // r = 20: 400/20 + 40 = 60
    CHECK_EQ(g.buy_cost(c), 60);
    c.shields = 0;                                      // r = 30: (45 + 60) x 2
    CHECK_EQ(g.buy_cost(c), 210);
    c.build_unit = false;
    c.build = static_cast<int>(Building::Temple);       // 40: r = 30 -> 60
    c.shields = 10;
    CHECK_EQ(g.buy_cost(c), 2 * (building_info(Building::Temple).cost - 10));
    c.shields = 0;
    CHECK_EQ(g.buy_cost(c), 4 * building_info(Building::Temple).cost);
}

static void test_sell()
{
    Game g = flat_game();
    City &c = town(g, 0, 10, 10, 3);
    c.add(Building::Temple);
    c.add(Building::Granary);
    const int gold = g.civs[0].gold;
    CHECK(!g.can_sell(c, static_cast<int>(Building::Palace)));
    CHECK(g.sell_building(c.id, static_cast<int>(Building::Temple)));
    CHECK_EQ(g.civs[0].gold, gold + building_info(Building::Temple).cost);
    CHECK(!c.has(Building::Temple));
    CHECK(!g.sell_building(c.id, static_cast<int>(Building::Granary)));   // one a turn
    g.end_turn();
    CHECK(g.sell_building(g.city_at(10, 10)->id, static_cast<int>(Building::Granary)));
}

static void test_barracks()
{
    Game g = flat_game();
    g.difficulty = Difficulty::Chieftain;
    const int b = static_cast<int>(Building::Barracks);
    CHECK_EQ(g.building_upkeep(0, b), 1);
    g.difficulty = Difficulty::Prince;
    CHECK_EQ(g.building_upkeep(0, b), 2);
    g.difficulty = Difficulty::Emperor;
    CHECK_EQ(g.building_upkeep(0, b), 3);
    City &c = town(g, 0, 10, 10, 3);
    c.add(Building::Barracks);
    g.learn(0, Advance::Gunpowder);
    CHECK(!g.city_at(10, 10)->has(Building::Barracks));
    CHECK(said(g, "Barracks obsolete"));
    CHECK_EQ(g.building_upkeep(0, b), 4);
}

static void test_capture()
{
    Game g = flat_game();
    g.civs[1].ai = false;
    g.learn(1, Advance::Writing);
    g.learn(1, Advance::Literacy);
    // A size-1 city is razed when taken; the conqueror takes an advance.
    town(g, 1, 10, 10, 1, "Smalltown");
    g.declare_war(0, 1);
    const int a = place(g, 0, UnitType::Armor, 11, 10);
    CHECK(g.capture_tech(0, 1) == Advance::Literacy);
    g.move(a, -1, 0);
    CHECK(g.city_at(10, 10) == nullptr);
    CHECK(g.has(0, Advance::Literacy));
    // A bigger one shrinks by one.
    town(g, 1, 20, 10, 5, "Bigtown");
    const int b = place(g, 0, UnitType::Armor, 21, 10);
    g.move(b, -1, 0);
    CHECK(g.city_at(20, 10) && g.city_at(20, 10)->owner == 0);
    CHECK_EQ(g.city_at(20, 10)->size, 4);
    CHECK(g.has(0, Advance::Writing));
}

static void test_nuke()
{
    Game g = flat_game();
    g.declare_war(0, 1);
    City &c = town(g, 1, 20, 20, 8, "Target");
    const int id = c.id;
    place(g, 1, UnitType::Musketeers, 20, 20);
    place(g, 1, UnitType::Musketeers, 21, 21);
    const int mine = place(g, 0, UnitType::Armor, 19, 19);   // ours dies too
    const int safe = place(g, 1, UnitType::Musketeers, 23, 20);
    const int n = place(g, 0, UnitType::Nuclear, 18, 20);
    g.move(n, 1, 0);   // to 19,20, next to the city
    MoveResult r = g.move(n, 1, 0);
    CHECK(r == MoveResult::Nuked);
    CHECK(g.unit(n) == nullptr);
    CHECK(g.unit(mine) == nullptr);
    CHECK(g.unit(safe) != nullptr);
    CHECK(g.units_at(21, 21).empty());
    CHECK_EQ(g.city(id)->size, 4);
    CHECK((g.civs[1].vendetta & 1u) != 0);
    // SDI Defense within 3 squares shoots it down.
    Game s = flat_game();
    s.declare_war(0, 1);
    const int tid = town(s, 1, 20, 20, 8, "Shielded").id;
    town(s, 1, 23, 20, 3, "Guard").add(Building::SdiDefense);
    place(s, 1, UnitType::Musketeers, 20, 20);
    const int m = place(s, 0, UnitType::Nuclear, 19, 20);
    CHECK(s.sdi_cover(0, 20, 20) != nullptr);
    CHECK(s.move(m, 1, 0) == MoveResult::Nuked);
    CHECK(s.unit(m) == nullptr);
    CHECK_EQ(s.city(tid)->size, 8);
    CHECK(!s.units_at(20, 20).empty());
    CHECK(said(s, "shoots down"));
}

static void test_trireme()
{
    // Out on open water a Trireme may go, but is lost half the time.
    Game g = flat_game(Terrain::Ocean);
    for (int x = 2; x < 5; ++x) g.map.at(x, 10).terrain = Terrain::Grassland;
    g.map.label_continents();
    const int t = place(g, 0, UnitType::Trireme, 5, 10);
    CHECK(g.can_enter(*g.unit(t), 7, 10));
    CHECK(g.move(t, 1, 0) == MoveResult::Moved);
    CHECK(g.move(t, 1, 0) == MoveResult::Moved);
    int lost = 0;
    for (int k = 0; k < 200; ++k) {
        Game h = g;
        h.rng.reseed(1000 + k);
        h.triremes_at_sea(0);
        lost += h.unit(t) == nullptr;
    }
    CHECK(lost > 60 && lost < 140);
    // Next to land it is safe; a computer ruler's is never lost.
    Game c = g;
    c.unit(t)->x = 5;
    for (int k = 0; k < 20; ++k) c.triremes_at_sea(0);
    CHECK(c.unit(t) != nullptr);
    Game a = g;
    a.civs[0].ai = true;
    for (int k = 0; k < 20; ++k) a.triremes_at_sea(0);
    CHECK(a.unit(t) != nullptr);
}

static void test_air_targets()
{
    CHECK((unit_info(UnitType::Fighter).flags & kSight2) != 0);
    Game g = flat_game();
    g.declare_war(0, 1);
    const int bomber = place(g, 1, UnitType::Bomber, 11, 10);
    const int armor = place(g, 0, UnitType::Armor, 10, 10);
    CHECK(g.move(armor, 1, 0) == MoveResult::FighterOnly);
    CHECK(g.unit(bomber) != nullptr);
    const int fighter = place(g, 0, UnitType::Fighter, 10, 11);
    const MoveResult r = g.move(fighter, 1, -1);
    CHECK(r != MoveResult::FighterOnly);
}

static void test_orders()
{
    Game g = flat_game();
    Tile &t = g.map.at(15, 15);
    t.road = true;
    t.railroad = true;
    t.irrigation = true;
    const int u = place(g, 0, UnitType::Legion, 15, 15);
    CHECK(g.pillage(u));
    CHECK(!t.irrigation && t.railroad);
    CHECK_EQ(g.unit(u)->moves, 0);
    g.unit(u)->moves = 3;
    CHECK(g.pillage(u));
    CHECK(!t.railroad && t.road);
    g.unit(u)->moves = 3;
    CHECK(g.pillage(u));
    CHECK(!t.road);
    g.unit(u)->moves = 3;
    CHECK(!g.pillage(u));   // nothing left
    const int dip = place(g, 0, UnitType::Diplomat, 15, 15);
    t.road = true;
    CHECK(!g.can_pillage(*g.unit(dip)));
    // Home city.
    const int a = town(g, 0, 10, 10, 3, "Alpha").id;
    const int b = town(g, 0, 20, 10, 3, "Beta").id;
    const int w = place(g, 0, UnitType::Phalanx, 20, 10);
    g.unit(w)->home = a;
    CHECK(g.set_home_city(w));
    CHECK_EQ(g.unit(w)->home, b);
    CHECK(!g.set_home_city(w));
    // Unload wakes the cargo.
    const int ship = place(g, 0, UnitType::Sail, 0, 12);
    const int p1 = place(g, 0, UnitType::Phalanx, 0, 12);
    const int p2 = place(g, 0, UnitType::Phalanx, 0, 12);
    for (int id : {p1, p2}) {
        g.unit(id)->carried_by = ship;
        g.unit(id)->order = Order::Sentry;
    }
    CHECK_EQ(g.unload(ship), 2);
    CHECK(g.unit(p1)->order == Order::None);
}

static void test_disasters()
{
    using D = Game::Disaster;
    auto fresh = [](Terrain around) {
        Game g = flat_game();
        for (int j = -1; j <= 1; ++j)
            for (int i = -1; i <= 1; ++i)
                if (i || j) g.map.at(20 + i, 20 + j).terrain = around;
        g.map.at(19, 21).terrain = Terrain::Ocean;
        town(g, 0, 20, 20, 8, "Unlucky");
        town(g, 0, 30, 30, 3, "Second");
        return g;
    };
    {   // Plague: Construction known, no Aqueduct, no Medicine.
        Game g = fresh(Terrain::Grassland);
        City &c = *g.city_at(20, 20);
        CHECK(!g.disaster(c, D::Plague));   // cannot build an Aqueduct yet
        g.learn(0, Advance::Construction);
        CHECK(g.disaster(c, D::Plague));
        CHECK_EQ(c.size, 6);
        c.add(Building::Aqueduct);
        CHECK(!g.disaster(c, D::Plague));
    }
    {   // Flood: by a river, Masonry known, no City Walls.
        Game g = fresh(Terrain::River);
        City &c = *g.city_at(20, 20);
        g.learn(0, Advance::Masonry);
        CHECK(g.disaster(c, D::Flood));
        CHECK_EQ(c.size, 6);
        c.add(Building::CityWalls);
        CHECK(!g.disaster(c, D::Flood));
    }
    {   // Volcano: by mountains, Ceremonial Burial, no Temple.
        Game g = fresh(Terrain::Mountains);
        City &c = *g.city_at(20, 20);
        g.learn(0, Advance::CeremonialBurial);
        CHECK(g.disaster(c, D::Volcano));
        CHECK_EQ(c.size, 6);
        c.add(Building::Temple);
        CHECK(!g.disaster(c, D::Volcano));
    }
    {   // Famine: Pottery, no Granary.
        Game g = fresh(Terrain::Grassland);
        City &c = *g.city_at(20, 20);
        g.learn(0, Advance::Pottery);
        c.food = 30;
        CHECK(g.disaster(c, D::Famine));
        CHECK_EQ(c.size, 6);
        CHECK_EQ(c.food, 0);
        c.add(Building::Granary);
        CHECK(!g.disaster(c, D::Famine));
    }
    {   // Fire: a building burns; an Aqueduct prevents it.
        Game g = fresh(Terrain::Grassland);
        City &c = *g.city_at(20, 20);
        g.learn(0, Advance::Construction);
        c.add(Building::Library);
        CHECK(g.disaster(c, D::Fire));
        CHECK(!c.has(Building::Library));
        c.add(Building::Library);
        c.add(Building::Aqueduct);
        CHECK(!g.disaster(c, D::Fire));
    }
    {   // Pirates: on the coast, no Barracks.
        Game g = fresh(Terrain::Grassland);
        City &c = *g.city_at(20, 20);
        c.shields = 25;
        c.food = 12;
        CHECK(g.disaster(c, D::Pirates));
        CHECK_EQ(c.shields, 0);
        CHECK_EQ(c.food, 0);
        c.add(Building::Barracks);
        CHECK(!g.disaster(c, D::Pirates));
    }
    {   // Earthquake: hills nearby, a building falls.
        Game g = fresh(Terrain::Hills);
        City &c = *g.city_at(20, 20);
        CHECK(!g.disaster(c, D::Earthquake));   // only the Palace: nothing to lose
        c.add(Building::Temple);
        CHECK(g.disaster(c, D::Earthquake));
        CHECK(!c.has(Building::Temple));
    }
    {   // None before turn 51, and only to big cities.
        Game g = fresh(Terrain::River);
        g.learn(0, Advance::Masonry);
        g.learn(0, Advance::Pottery);
        g.learn(0, Advance::Construction);
        g.turn = 40;
        for (int k = 0; k < 200; ++k) g.natural_disasters();
        CHECK_EQ(g.city_at(20, 20)->size, 8);
        g.turn = 60;
        bool hit = false;
        for (int k = 0; k < 2000 && !hit; ++k) {
            g.natural_disasters();
            hit = g.city_at(20, 20)->size < 8;
        }
        CHECK(hit);
        CHECK_EQ(g.city_at(30, 30)->size, 3);
    }
}

// ---------------------------------------------------------------- runner

int main(int argc, char **argv)
{
    const std::map<std::string, std::function<void()>> tests = {
        {"empire_size", test_empire_size}, {"celebration", test_celebration}, {"democracy_collapse", test_democracy_collapse},
        {"anarchy_length", test_anarchy_length}, {"buy_cost", test_buy_cost}, {"sell", test_sell},
        {"barracks", test_barracks}, {"capture", test_capture}, {"nuke", test_nuke}, {"trireme", test_trireme},
        {"air_targets", test_air_targets}, {"orders", test_orders}, {"disasters", test_disasters},
    };
    if (argc < 2) {
        for (const auto &t : tests) t.second();
    } else {
        auto it = tests.find(argv[1]);
        if (it == tests.end()) {
            std::fprintf(stderr, "unknown test %s\n", argv[1]);
            return 2;
        }
        it->second();
    }
    if (failures) std::fprintf(stderr, "%d check(s) failed\n", failures);
    return failures ? 1 : 0;
}
