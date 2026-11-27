// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Rules tests: run one named group per meson test.

#include "game.hpp"
#include "../src/ui/pad_nav.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
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

// A small, flat, predictable world for rules tests.
static Game flat_game(Terrain fill = Terrain::Grassland)
{
    GameSettings s;
    s.seed = 7;
    s.civs = 2;
    Game g = Game::create(s);
    for (Tile &t : g.map.tiles) {
        t = Tile();
        t.terrain = fill;
        t.explored = 0xFF;
    }
    // A strip of ocean down column 0 so the land is coastal there.
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

static void test_calendar()
{
    CHECK_EQ(year_for_turn(1), -4000);
    CHECK_EQ(year_for_turn(2), -3980);
    CHECK_EQ(year_for_turn(200), -20);
    CHECK_EQ(year_for_turn(201), 1);
    CHECK_EQ(year_for_turn(202), 20);
    CHECK_EQ(year_for_turn(204), 60);
    CHECK_EQ(year_for_turn(252), 1010);
    CHECK_EQ(year_for_turn(300), 1490);
    CHECK_EQ(year_for_turn(304), 1515);
    CHECK_EQ(year_for_turn(352), 1752);
    CHECK_EQ(year_for_turn(404), 1853);
    CHECK(year_text(-4000) == "4000 BC");
    CHECK(year_text(1500) == "1500 AD");
}

static void test_terrain_yields()
{
    Game g = flat_game();
    Civ &c = g.civs[0];
    c.govt = Government::Monarchy;
    auto y = [&](int x, int yy) { return g.tile_yield(x, yy, 0, false); };
    Tile &t = g.map.at(10, 10);
    // Grassland 2/0/0; road +1 trade; irrigation +1 food.
    Yield v = y(10, 10);
    CHECK_EQ(v.food, 2);
    CHECK_EQ(v.shields, 0);
    CHECK_EQ(v.trade, 0);
    t.road = true;
    t.irrigation = true;
    v = y(10, 10);
    CHECK_EQ(v.food, 3);
    CHECK_EQ(v.trade, 1);
    // Despotism takes one from any yield of 3 or more.
    c.govt = Government::Despotism;
    CHECK_EQ(y(10, 10).food, 2);
    // Republic adds one trade where there is already trade.
    c.govt = Government::Republic;
    CHECK_EQ(y(10, 10).trade, 2);
    CHECK_EQ(y(11, 10).trade, 0);
    c.govt = Government::Monarchy;
    // Hills 1/0/0, mine +3 shields; Hills with coal 1/2/0.
    Tile &h = g.map.at(12, 10);
    h.terrain = Terrain::Hills;
    CHECK_EQ(y(12, 10).food, 1);
    h.mine = true;
    CHECK_EQ(y(12, 10).shields, 3);
    h.special = true;
    CHECK_EQ(y(12, 10).shields, 5);
    // Ocean 1/0/2, fish 3/0/2.
    CHECK_EQ(y(0, 10).food, 1);
    CHECK_EQ(y(0, 10).trade, 2);
    g.map.at(0, 10).special = true;
    CHECK_EQ(y(0, 10).food, 3);
    // Mountains with gold 0/1/6.
    Tile &m = g.map.at(14, 10);
    m.terrain = Terrain::Mountains;
    m.special = true;
    CHECK_EQ(y(14, 10).trade, 6);
    c.govt = Government::Despotism;
    CHECK_EQ(y(14, 10).trade, 5);
    // River 2/0/1.
    Tile &r = g.map.at(16, 10);
    r.terrain = Terrain::River;
    c.govt = Government::Monarchy;
    CHECK_EQ(y(16, 10).food, 2);
    CHECK_EQ(y(16, 10).trade, 1);
    // City centre: automatic irrigation effect and at least one shield.
    CHECK_EQ(g.tile_yield(20, 20, 0, true).food, 3);
    CHECK_EQ(g.tile_yield(20, 20, 0, true).shields, 1);
    CHECK_EQ(g.tile_yield(20, 20, 0, true).trade, 1);
    // Table spot checks against the original.
    CHECK_EQ(terrain_info(Terrain::Forest).defense_pct, 150);
    CHECK_EQ(terrain_info(Terrain::Hills).defense_pct, 200);
    CHECK_EQ(terrain_info(Terrain::Mountains).defense_pct, 300);
    CHECK_EQ(terrain_info(Terrain::Mountains).move, 3);
    CHECK_EQ(terrain_info(Terrain::Jungle).with_special.trade, 4);
}

static void test_tech_prereqs()
{
    // Every prerequisite is a real advance; no advance needs itself; the
    // whole tree is reachable from nothing.
    std::set<int> known;
    bool grew = true;
    while (grew) {
        grew = false;
        for (int i = 0; i < kAdvanceCount; ++i) {
            if (known.count(i)) continue;
            const AdvanceInfo &a = advance_info(i);
            const bool r1 = a.req1 == Advance::None || known.count(static_cast<int>(a.req1));
            const bool r2 = a.req2 == Advance::None || known.count(static_cast<int>(a.req2));
            if (r1 && r2) {
                known.insert(i);
                grew = true;
            }
        }
    }
    CHECK_EQ(static_cast<long long>(known.size()), kAdvanceCount);
    CHECK_EQ(kAdvanceCount, 68);
    CHECK(advance_info(Advance::Monarchy).req1 == Advance::CeremonialBurial || advance_info(Advance::Monarchy).req2 == Advance::CeremonialBurial);
    CHECK(advance_info(Advance::Writing).req1 == Advance::Alphabet);
    Game g = flat_game();
    CHECK(g.can_research(0, Advance::Alphabet));
    CHECK(!g.can_research(0, Advance::Writing));
    g.learn(0, Advance::Alphabet);
    CHECK(g.can_research(0, Advance::Writing));
    CHECK(!g.can_research(0, Advance::Alphabet));
    // Unit gating: Phalanx needs Bronze Working; Militia go with Gunpowder.
    CHECK(!g.unit_available(0, UnitType::Phalanx));
    g.learn(0, Advance::BronzeWorking);
    CHECK(g.unit_available(0, UnitType::Phalanx));
    CHECK(g.unit_available(0, UnitType::Militia));
    g.civs[0].techs[static_cast<int>(Advance::Gunpowder)] = true;
    CHECK(!g.unit_available(0, UnitType::Militia));
    CHECK(!g.unit_available(0, UnitType::Phalanx));
    // Research cost grows with knowledge.
    const int c1 = g.research_cost(0);
    g.learn(0, Advance::Pottery);
    CHECK(g.research_cost(0) > c1);
}

static void test_units_table()
{
    CHECK_EQ(kUnitTypeCount, 29);   // 28 from the DOS game plus the barbarian Leader
    struct Row {
        UnitType t;
        int a, d, m, cost;
    };
    const Row rows[] = {
        {UnitType::Settlers, 0, 1, 1, 40},   {UnitType::Militia, 1, 1, 1, 10},    {UnitType::Phalanx, 1, 2, 1, 20},
        {UnitType::Legion, 3, 1, 1, 20},     {UnitType::Chariot, 4, 1, 2, 40},    {UnitType::Cavalry, 2, 1, 2, 20},
        {UnitType::Knights, 4, 2, 2, 40},    {UnitType::Catapult, 6, 1, 1, 40},   {UnitType::Musketeers, 2, 3, 1, 30},
        {UnitType::Riflemen, 3, 5, 1, 30},   {UnitType::Cannon, 8, 1, 1, 40},     {UnitType::Artillery, 12, 2, 2, 60},
        {UnitType::Armor, 10, 5, 3, 80},     {UnitType::MechInf, 6, 6, 3, 50},    {UnitType::Diplomat, 0, 0, 2, 30},
        {UnitType::Caravan, 0, 1, 1, 50},    {UnitType::Trireme, 1, 0, 3, 40},    {UnitType::Sail, 1, 1, 3, 40},
        {UnitType::Frigate, 2, 2, 3, 40},    {UnitType::Ironclad, 4, 4, 4, 60},   {UnitType::Transport, 0, 3, 4, 50},
        {UnitType::Cruiser, 6, 6, 6, 80},    {UnitType::Battleship, 18, 12, 4, 160}, {UnitType::Carrier, 1, 12, 5, 160},
        {UnitType::Submarine, 8, 2, 3, 50},  {UnitType::Fighter, 4, 2, 10, 60},   {UnitType::Bomber, 12, 1, 8, 120},
        {UnitType::Nuclear, 99, 0, 16, 160},
    };
    for (const Row &r : rows) {
        const UnitInfo &i = unit_info(r.t);
        CHECK_EQ(i.attack, r.a);
        CHECK_EQ(i.defense, r.d);
        CHECK_EQ(i.moves, r.m);
        CHECK_EQ(i.cost, r.cost);
    }
}

static void test_combat_odds()
{
    Game g = flat_game();
    g.civs[0].war = 2;
    g.civs[1].war = 1;
    const int a = place(g, 0, UnitType::Legion, 10, 10);
    const int d = place(g, 1, UnitType::Phalanx, 11, 10);
    // Grassland, not fortified: 3 vs 2.
    CHECK(std::fabs(g.attack_strength(*g.unit(a)) - 3.0) < 1e-9);
    CHECK(std::fabs(g.defense_strength(*g.unit(d), g.unit(a)) - 2.0) < 1e-9);
    // Fortified x1.5, hills x2, veteran x1.5.
    g.unit(d)->order = Order::Fortified;
    CHECK(std::fabs(g.defense_strength(*g.unit(d), g.unit(a)) - 3.0) < 1e-9);
    g.map.at(11, 10).terrain = Terrain::Hills;
    CHECK(std::fabs(g.defense_strength(*g.unit(d), g.unit(a)) - 6.0) < 1e-9);
    g.unit(d)->veteran = true;
    CHECK(std::fabs(g.defense_strength(*g.unit(d), g.unit(a)) - 9.0) < 1e-9);
    g.unit(a)->veteran = true;
    CHECK(std::fabs(g.attack_strength(*g.unit(a)) - 4.5) < 1e-9);
    // City walls triple against land attack (and replace the fortify bonus).
    g.map.at(11, 10).terrain = Terrain::Grassland;
    g.unit(d)->veteran = false;
    City c;
    c.id = 99;
    c.owner = 1;
    c.x = 11;
    c.y = 10;
    c.add(Building::CityWalls);
    g.cities.push_back(c);
    CHECK(std::fabs(g.defense_strength(*g.unit(d), g.unit(a)) - 6.0) < 1e-9);
    g.cities.clear();
    // Round-by-round odds: equal strength is a coin toss; more hit points or
    // strength help; the closed form matches a direct count.
    CHECK(std::fabs(combat_win_chance(2, 2, 10, 10) - 0.5) < 1e-9);
    CHECK(std::fabs(combat_win_chance(1, 1, 1, 1) - 0.5) < 1e-9);
    CHECK(std::fabs(combat_win_chance(3, 1, 1, 1) - 0.75) < 1e-9);
    // One hit point each side, two rounds needed against 2: p^2 = 0.36.
    CHECK(std::fabs(combat_win_chance(3, 2, 1, 2) - 0.36) < 1e-9);
    CHECK(combat_win_chance(3, 2, 10, 10) > 0.6);
    CHECK(combat_win_chance(3, 2, 3, 10) < combat_win_chance(3, 2, 10, 10));
    CHECK_EQ(static_cast<long long>(combat_win_chance(5, 0, 1, 10) * 1000), 1000);
    // Empirical: Legion (3) vs Phalanx (2) on grassland, 10 hit points each.
    const double expect = combat_win_chance(3, 2, kUnitHp, kUnitHp);
    CHECK(expect > 0.78 && expect < 0.84);
    int wins = 0;
    const int n = 3000;
    for (int i = 0; i < n; ++i) {
        Game t = flat_game();
        t.rng.reseed(1000 + i);
        t.civs[0].war = 2;
        t.civs[1].war = 1;
        const int ai = place(t, 0, UnitType::Legion, 10, 10);
        place(t, 1, UnitType::Phalanx, 11, 10);
        place(t, 1, UnitType::Settlers, 12, 12); // keeps civ 1 alive
        place(t, 0, UnitType::Settlers, 30, 30); // and us
        t.attack(ai, 11, 10);
        const CombatResult &r = t.last_combat;
        wins += r.attacker_won ? 1 : 0;
        CHECK(std::fabs(r.odds - expect) < 1e-9);
        CHECK(std::fabs(r.round_odds - 0.6) < 1e-9);
        // Every round costs exactly one hit point; the loser ends at zero.
        int att_lost = 0, def_lost = 0;
        for (auto w : r.rounds) (w ? def_lost : att_lost)++;
        CHECK_EQ(att_lost, r.att_hp0 - r.att_hp);
        CHECK_EQ(def_lost, r.def_hp0 - r.def_hp);
        CHECK(r.attacker_won ? (r.def_hp == 0 && r.att_hp > 0) : (r.att_hp == 0 && r.def_hp > 0));
        // The survivor carries its wounds.
        if (r.attacker_won) {
            CHECK_EQ(t.unit(ai)->hp, r.att_hp);
        }
    }
    const double rate = static_cast<double>(wins) / n;
    CHECK(std::fabs(rate - expect) < 0.03);
    // Wounds heal: +1 a turn in the field, +3 fortified, all at once in a city.
    {
        Game h = flat_game();
        const int u1 = place(h, 0, UnitType::Phalanx, 10, 10);
        const int u2 = place(h, 0, UnitType::Phalanx, 20, 10);
        place(h, 1, UnitType::Settlers, 40, 40);
        place(h, 0, UnitType::Settlers, 30, 30); // keeps us alive
        h.unit(u1)->hp = 4;
        h.unit(u2)->hp = 4;
        h.unit(u2)->order = Order::Fortified;
        h.end_turn();
        CHECK_EQ(h.unit(u1)->hp, 5);
        CHECK_EQ(h.unit(u2)->hp, 7);
        const int s = place(h, 0, UnitType::Settlers, 10, 10);
        CHECK(h.found_city(s, "Mend"));
        h.end_turn();
        CHECK_EQ(h.unit(u1)->hp, kUnitHp);
    }
    // A win outside a city destroys the whole stack.
    Game s = flat_game();
    s.civs[0].war = 2;
    s.civs[1].war = 1;
    const int big = place(s, 0, UnitType::Armor, 10, 10);
    place(s, 1, UnitType::Militia, 11, 10);
    place(s, 1, UnitType::Settlers, 11, 10);
    place(s, 1, UnitType::Settlers, 30, 30);
    for (int tries = 0; tries < 20 && s.units_at(11, 10).size() == 2; ++tries) {
        s.unit(big)->moves = 9;
        s.attack(big, 11, 10);
        if (!s.unit(big)) break;
    }
    if (s.unit(big)) CHECK(s.units_at(11, 10).empty());
}

static void test_city_growth()
{
    Game g = flat_game();
    const int sid = place(g, 0, UnitType::Settlers, 10, 10);
    CHECK(g.found_city(sid, "Testville"));
    City &c = g.cities.back();
    CHECK(c.has(Building::Palace));
    CHECK_EQ(c.size, 1);
    CHECK_EQ(g.food_box(c), 20);
    // Grassland around: centre 2+1 irr (despot keeps 2... 3->2) + one grassland 2 = 4; eats 2.
    g.update_city(c);
    CHECK_EQ(static_cast<long long>(c.worked.size()), 1);
    CHECK_EQ(c.stats.food_surplus, 2);
    int turns = 0;
    while (c.size == 1 && turns < 50) {
        g.end_turn();
        ++turns;
    }
    CHECK_EQ(turns, 10);
    CHECK_EQ(c.size, 2);
    // Starvation shrinks the city.
    for (Tile &t : g.map.tiles) {
        if (t.terrain != Terrain::Ocean) t.terrain = Terrain::Tundra;
    }
    g.map.at(c.x, c.y).terrain = Terrain::Tundra;
    c.food = 0;
    g.update_city(c);
    CHECK(c.stats.food_surplus < 0);
    g.end_turn();
    CHECK_EQ(c.size, 1);
    // A granary keeps half the box.
    Game h = flat_game();
    const int s2 = place(h, 0, UnitType::Settlers, 10, 10);
    h.found_city(s2, "Grainy");
    City &k = h.cities.back();
    k.add(Building::Granary);
    k.food = 19;
    h.civs[0].gold = 1000;
    h.end_turn();
    CHECK_EQ(k.size, 2);
    CHECK_EQ(k.food, 15);
    // Size cap 10 without an Aqueduct.
    k.size = 10;
    k.food = 1000;
    h.end_turn();
    CHECK_EQ(k.size, 10);
}

static void test_settlers_work()
{
    Game g = flat_game(Terrain::Plains);
    const int s = place(g, 0, UnitType::Settlers, 1, 10); // next to the ocean
    CHECK(g.can_work(*g.unit(s), Order::Irrigate));
    CHECK(g.start_work(s, Order::Irrigate));
    for (int i = 0; i < 5; ++i) g.end_turn();
    CHECK(g.map.at(1, 10).irrigation);
    CHECK(g.unit(s)->order == Order::None);
    // Away from water there is nothing to irrigate from.
    const int t = place(g, 0, UnitType::Settlers, 20, 20);
    CHECK(!g.can_work(*g.unit(t), Order::Irrigate));
    CHECK(g.start_work(t, Order::Road));
    for (int i = 0; i < 2; ++i) g.end_turn();
    CHECK(g.map.at(20, 20).road);
    // Mining forest is not allowed; irrigating forest turns it to plains.
    g.map.at(30, 30).terrain = Terrain::Forest;
    const int f = place(g, 0, UnitType::Settlers, 30, 30);
    CHECK(!g.can_work(*g.unit(f), Order::Mine));
    CHECK(g.start_work(f, Order::Irrigate));
    for (int i = 0; i < 5; ++i) g.end_turn();
    CHECK(g.map.at(30, 30).terrain == Terrain::Plains);
}

static void test_movement()
{
    Game g = flat_game();
    const int u = place(g, 0, UnitType::Cavalry, 10, 10);
    CHECK_EQ(g.unit(u)->moves, 6);
    g.map.at(11, 10).terrain = Terrain::Forest;
    CHECK(g.move(u, 1, 0) == MoveResult::Moved);
    CHECK_EQ(g.unit(u)->moves, 0);
    // Roads: a third of a point.
    Game r = flat_game();
    const int m = place(r, 0, UnitType::Militia, 10, 10);
    for (int x = 10; x <= 14; ++x) r.map.at(x, 10).road = true;
    CHECK(r.move(m, 1, 0) == MoveResult::Moved);
    CHECK(r.move(m, 1, 0) == MoveResult::Moved);
    CHECK(r.move(m, 1, 0) == MoveResult::Moved);
    CHECK_EQ(r.unit(m)->x, 13);
    CHECK(r.move(m, 1, 0) == MoveResult::NoMoves);
    // Land units can't walk into the sea; the map wraps east-west.
    Game w = flat_game();
    const int k = place(w, 0, UnitType::Militia, 1, 10);
    CHECK(w.move(k, -1, 0) == MoveResult::Blocked);
    w.map.at(0, 10).terrain = Terrain::Grassland;
    CHECK(w.move(k, -1, 0) == MoveResult::Moved);
    w.unit(k)->moves = 3;
    CHECK(w.move(k, -1, 0) == MoveResult::Moved);
    CHECK_EQ(w.unit(k)->x, w.map.w - 1);
    // Peace: moving into a foreign unit needs a declaration of war.
    Game p = flat_game();
    const int a = place(p, 0, UnitType::Legion, 10, 10);
    place(p, 1, UnitType::Militia, 11, 10);
    CHECK(p.move(a, 1, 0) == MoveResult::NeedWar);
    // Zones of control.
    Game z = flat_game();
    z.civs[0].war = 2;
    z.civs[1].war = 1;
    const int zu = place(z, 0, UnitType::Militia, 10, 10);
    place(z, 1, UnitType::Militia, 11, 9);
    CHECK(z.move(zu, 1, 0) == MoveResult::Zoc);
    CHECK(z.move(zu, -1, 0) == MoveResult::Moved);
    // Goto finds a way.
    Game gt = flat_game();
    const int gu = place(gt, 0, UnitType::Militia, 10, 10);
    place(gt, 0, UnitType::Settlers, 40, 40); // keeps both civs alive
    place(gt, 1, UnitType::Settlers, 50, 40);
    gt.civs[1].ai = false;
    gt.set_goto(gu, 13, 12);
    gt.begin_human_phase();
    for (int i = 0; i < 4; ++i) {
        gt.end_turn();
    }
    CHECK_EQ(gt.unit(gu)->x, 13);
    CHECK_EQ(gt.unit(gu)->y, 12);
}

static bool same_games(const Game &a, const Game &b)
{
    return save_game(a) == save_game(b);
}

static void test_save_roundtrip()
{
    GameSettings s;
    s.seed = 424242;
    s.civs = 4;
    s.all_ai = true;
    Game g = Game::create(s);
    for (int i = 0; i < 40; ++i) g.end_turn();
    const std::string text = save_game(g);
    Game h;
    std::string err;
    CHECK(load_game(text, h, &err));
    CHECK(err.empty());
    CHECK(same_games(g, h));
    CHECK_EQ(h.turn, g.turn);
    CHECK_EQ(static_cast<long long>(h.units.size()), static_cast<long long>(g.units.size()));
    CHECK_EQ(static_cast<long long>(h.cities.size()), static_cast<long long>(g.cities.size()));
    // Both copies play on identically.
    for (int i = 0; i < 10; ++i) {
        g.end_turn();
        h.end_turn();
    }
    CHECK(same_games(g, h));
    // Garbage is refused.
    Game bad;
    CHECK(!load_game("hello", bad, &err));
    CHECK(!load_game(text.substr(0, text.size() / 2), bad, &err));
}

static void test_ai_smoke()
{
    GameSettings s;
    s.seed = 99;
    s.civs = 5;
    s.all_ai = true;
    s.difficulty = Difficulty::King;
    Game g = Game::create(s);
    for (int i = 0; i < 150 && g.winner < 0; ++i) g.end_turn();
    int cities = 0;
    cities = static_cast<int>(g.cities.size());
    std::printf("after %d turns (%s): %d cities, %zu units\n", g.turn, year_text(year_for_turn(g.turn)).c_str(), cities,
                g.units.size());
    for (size_t i = 0; i < g.civs.size(); ++i) {
        int n = 0, pop = 0;
        for (const City &c : g.cities) {
            if (c.owner == static_cast<int>(i)) {
                ++n;
                pop += c.size;
            }
        }
        std::printf("  %-14s cities %2d pop %3d techs %2d gold %4d %s%s\n", g.civs[i].name.c_str(), n, pop, g.techs_known(static_cast<int>(i)),
                    g.civs[i].gold, government_info(g.civs[i].govt).name, g.civs[i].alive ? "" : " (destroyed)");
    }
    CHECK(cities >= 10);
    // No two cities share a square; every unit stands on the map.
    std::set<std::pair<int, int>> seen;
    for (const City &c : g.cities) CHECK(seen.insert({c.x, c.y}).second);
    for (const Unit &u : g.units) CHECK(g.map.valid_y(u.y) && u.x >= 0 && u.x < g.map.w);
}

static void test_wonders()
{
    // The table: 21 wonders, each with a cost and an advance.
    CHECK_EQ(kWonderCount, 21);
    for (int w = 0; w < kWonderCount; ++w) {
        CHECK(wonder_info(w).cost >= 200);
        CHECK(wonder_info(w).tech != Advance::None);
        CHECK(std::strcmp(building_info(kWonderBase + w).name, wonder_info(w).name) == 0);
        CHECK_EQ(building_info(kWonderBase + w).upkeep, 0);
    }
    Game g = flat_game();
    place(g, 1, UnitType::Settlers, 40, 30);  // keeps civ 1 alive
    const int sid = place(g, 0, UnitType::Settlers, 10, 10);
    CHECK(g.found_city(sid, "Wonderton"));
    City &c = g.cities.back();
    const int pyr = kWonderBase + static_cast<int>(Wonder::Pyramids);
    CHECK(!g.can_build(c, false, pyr));          // needs Masonry
    g.civs[0].techs[static_cast<int>(Advance::Masonry)] = true;
    CHECK(g.can_build(c, false, pyr));
    g.set_production(c.id, false, pyr);
    CHECK(!c.build_unit);
    CHECK_EQ(c.build, pyr);
    CHECK_EQ(g.production_cost(c), 300);
    c.shields = 299;
    g.civs[0].gold = 1000;
    const int cid = c.id;
    g.end_turn();
    CHECK_EQ(g.wonder_owner(Wonder::Pyramids), 0);
    CHECK(g.wonder_in(*g.city(cid), Wonder::Pyramids));
    bool announced = false;
    for (const Message &m : g.messages) announced |= m.kind == MsgKind::Wonder && m.wonder == static_cast<int>(Wonder::Pyramids);
    CHECK(announced);
    // Built once in the whole world.
    CHECK(!g.can_build(*g.city(cid), false, pyr));
    // Pyramids: one turn of Anarchy, any government.
    g.revolution(0, Government::Democracy);
    CHECK_EQ(g.civs[0].anarchy, 1);
    g.end_turn();
    CHECK(g.civs[0].govt == Government::Democracy);
    // Obsolete once the owner learns Communism.
    g.civs[0].techs[static_cast<int>(Advance::Communism)] = true;
    CHECK(g.wonder_obsolete(Wonder::Pyramids));
    CHECK(!g.wonder_active(0, Wonder::Pyramids));
    g.revolution(0, Government::Monarchy);
    CHECK_EQ(g.civs[0].anarchy, 0);   // no Monarchy advance, no Pyramids: refused

    // Colossus: +1 trade on squares that already have trade.
    Game h = flat_game();
    place(h, 1, UnitType::Settlers, 40, 30);
    const int s2 = place(h, 0, UnitType::Settlers, 1, 10);   // next to the ocean strip
    h.found_city(s2, "Rhodes");
    City &r = h.cities.back();
    r.size = 3;
    r.manual = true;
    r.worked = {{-1, 0}, {-1, 1}, {0, 1}};   // two ocean squares, one grassland
    h.update_city(r);
    const int before = r.stats.raw.trade;
    h.wonder_city[static_cast<int>(Wonder::Colossus)] = r.id;
    h.update_city(r);
    CHECK_EQ(r.stats.raw.trade, before + 3);  // centre + two oceans
    // Copernicus doubles science in the city.
    h.civs[0].sci = 10;
    h.civs[0].tax = 0;
    h.update_city(r);
    const int sci = r.stats.sci;
    h.wonder_city[static_cast<int>(Wonder::Copernicus)] = r.id;
    h.update_city(r);
    CHECK_EQ(r.stats.sci, sci * 2);
    // Lighthouse and Magellan: ships move farther.
    const int tri = place(h, 0, UnitType::Trireme, 0, 12);
    const int base_moves = h.full_moves(*h.unit(tri));
    h.wonder_city[static_cast<int>(Wonder::Lighthouse)] = r.id;
    CHECK_EQ(h.full_moves(*h.unit(tri)), base_moves + 3);
    // Manhattan Project opens Nuclear units to everyone with Rocketry.
    h.civs[1].techs[static_cast<int>(Advance::Rocketry)] = true;
    CHECK(!h.unit_available(1, UnitType::Nuclear));
    h.wonder_city[static_cast<int>(Wonder::Manhattan)] = r.id;
    CHECK(h.unit_available(1, UnitType::Nuclear));
    // Shakespeare's Theatre: nobody unhappy.
    r.size = 8;
    r.manual = false;
    h.civs[0].lux = 0;
    h.civs[0].tax = 4;
    h.civs[0].sci = 6;
    h.update_city(r);
    h.wonder_city[static_cast<int>(Wonder::Shakespeare)] = r.id;
    h.update_city(r);
    CHECK_EQ(r.stats.unhappy, 0);

    // Great Library: an advance known to two others.
    GameSettings gs;
    gs.seed = 5;
    gs.civs = 3;
    Game k = Game::create(gs);
    const int wr = static_cast<int>(Advance::Writing);
    k.civs[1].techs[wr] = true;
    k.civs[2].techs[wr] = true;
    k.civs[0].techs[wr] = false;
    City *cap = k.capital(0);
    if (!cap) {
        for (const Unit &u : k.units) {
            if (u.owner == 0 && u.type == UnitType::Settlers) {
                k.found_city(u.id, "Alexandria");
                break;
            }
        }
        cap = k.capital(0);
    }
    CHECK(cap != nullptr);
    if (cap) {
        const int capid = cap->id;
        k.wonder_city[static_cast<int>(Wonder::GreatLibrary)] = capid;
        k.end_turn();
        CHECK(k.civs[0].techs[wr]);
        // Saved and loaded with the game.
        Game l;
        CHECK(load_game(save_game(k), l));
        CHECK_EQ(l.wonder_city[static_cast<int>(Wonder::GreatLibrary)], capid);
    }
    // Milestone 1 saves (version 1) still load.
    std::string old = save_game(k);
    old.replace(0, std::strlen("LUNDUKECIV 3"), "LUNDUKECIV 1");
    Game m;
    CHECK(load_game(old, m));
}

static void test_envoys()
{
    Game g = flat_game();
    place(g, 1, UnitType::Settlers, 50, 30);
    const int s0 = place(g, 0, UnitType::Settlers, 10, 10);
    const int s1 = place(g, 1, UnitType::Settlers, 22, 10);
    CHECK(g.found_city(s0, "Home"));
    CHECK(g.found_city(s1, "Abroad"));
    const int s1b = place(g, 1, UnitType::Settlers, 30, 20);
    CHECK(g.found_city(s1b, "Capital2"));
    City *abroad = nullptr;
    for (City &c : g.cities) if (c.name == "Abroad") abroad = &c;
    CHECK(abroad != nullptr);
    if (!abroad) return;
    // The first city of civ 1 is its capital: Abroad has the Palace, so it can't be incited.
    const int dip = place(g, 0, UnitType::Diplomat, 21, 10);
    CHECK(g.move(dip, 1, 0) == MoveResult::Diplomat);
    CHECK(g.diplomat_can(dip, 22, 10, DiploAction::Embassy));
    CHECK(g.diplomat_can(dip, 22, 10, DiploAction::Investigate));
    CHECK(!g.diplomat_can(dip, 22, 10, DiploAction::Incite));   // a capital
    CHECK(!g.diplomat_can(dip, 22, 10, DiploAction::Bribe));    // not a lone unit
    g.diplomat_act(dip, 22, 10, DiploAction::Embassy);
    CHECK(g.unit(dip) == nullptr);                              // spent
    CHECK((g.civs[0].embassy >> 1) & 1u);
    CHECK(!g.at_war(0, 1));                                     // peaceful
    // Stealing an advance is an act of war and works once per city.
    g.civs[1].techs[static_cast<int>(Advance::Pottery)] = true;
    g.civs[0].techs[static_cast<int>(Advance::Pottery)] = false;
    const int spy = place(g, 0, UnitType::Diplomat, 21, 10);
    CHECK(g.diplomat_can(spy, 22, 10, DiploAction::StealTech));
    g.diplomat_act(spy, 22, 10, DiploAction::StealTech);
    CHECK(g.at_war(0, 1));
    CHECK(g.civs[0].techs[static_cast<int>(Advance::Pottery)]);
    const int spy2 = place(g, 0, UnitType::Diplomat, 21, 10);
    g.civs[1].techs[static_cast<int>(Advance::Writing)] = true;
    CHECK(!g.diplomat_can(spy2, 22, 10, DiploAction::StealTech));
    // Incite a city that is not a capital: it joins us with its garrison.
    City *other = nullptr;
    for (City &c : g.cities) if (c.name == "Capital2") other = &c;
    if (other) {
        const int guard = place(g, 1, UnitType::Phalanx, other->x, other->y);
        const int agent = place(g, 0, UnitType::Diplomat, other->x - 1, other->y);
        g.civs[0].gold = 100000;
        const int cost = g.incite_cost(*other, 0);
        CHECK(cost > 0);
        CHECK(g.diplomat_can(agent, other->x, other->y, DiploAction::Incite));
        g.diplomat_act(agent, other->x, other->y, DiploAction::Incite);
        CHECK_EQ(other->owner, 0);
        CHECK_EQ(g.unit(guard)->owner, 0);
        CHECK_EQ(g.civs[0].gold, 100000 - cost);
    }
    // Bribe a lone unit.
    const int lone = place(g, 1, UnitType::Legion, 40, 40);
    const int briber = place(g, 0, UnitType::Diplomat, 41, 40);
    g.civs[0].gold = 5000;
    CHECK(g.diplomat_can(briber, 40, 40, DiploAction::Bribe));
    g.diplomat_act(briber, 40, 40, DiploAction::Bribe);
    CHECK_EQ(g.unit(lone)->owner, 0);

    // Caravans: trade routes and help with wonders.
    Game h = flat_game();
    place(h, 1, UnitType::Settlers, 50, 30);
    const int a = place(h, 0, UnitType::Settlers, 10, 10);
    const int b = place(h, 0, UnitType::Settlers, 25, 10);
    h.found_city(a, "West");
    h.found_city(b, "East");
    const int west = h.cities[0].id, east = h.cities[1].id;
    h.city(west)->size = 6;
    h.city(east)->size = 6;
    h.update_city(*h.city(west));
    h.update_city(*h.city(east));
    const int trade0 = h.city(west)->stats.raw.trade;
    Unit cu;
    cu.id = h.next_unit_id++;
    cu.type = UnitType::Caravan;
    cu.owner = 0;
    cu.x = 24;
    cu.y = 10;
    cu.home = west;
    cu.moves = 3;
    h.units.push_back(cu);
    CHECK(h.move(cu.id, 1, 0) == MoveResult::Caravan);    // into East
    CHECK(h.caravan_can_trade(*h.unit(cu.id), *h.city(east)));
    const int gold0 = h.civs[0].gold;
    const int bonus = h.caravan_trade(cu.id, east);
    CHECK(bonus > 0);
    CHECK_EQ(h.civs[0].gold, gold0 + bonus);
    CHECK_EQ(static_cast<long long>(h.city(west)->routes.size()), 1);
    h.update_city(*h.city(west));
    CHECK(h.city(west)->stats.route_trade >= 1);
    CHECK_EQ(h.city(west)->stats.raw.trade, trade0 + h.city(west)->stats.route_trade);
    // Saved with the game.
    Game l;
    CHECK(load_game(save_game(h), l));
    CHECK_EQ(static_cast<long long>(l.city(west)->routes.size()), 1);
    // Help build a wonder: 50 shields.
    h.civs[0].techs[static_cast<int>(Advance::BronzeWorking)] = true;
    h.set_production(east, false, kWonderBase + static_cast<int>(Wonder::Colossus));
    const int sh = h.city(east)->shields;
    const int helper = place(h, 0, UnitType::Caravan, 25, 10);
    h.unit(helper)->home = west;
    CHECK(h.caravan_help(helper, east));
    CHECK_EQ(h.city(east)->shields, sh + 50);
}

static void test_railroads()
{
    // Railroads: free movement between railroad squares, +50% shields.
    Game g = flat_game();
    const int m = place(g, 0, UnitType::Militia, 10, 10);
    for (int x = 10; x <= 14; ++x) {
        g.map.at(x, 10).road = true;
        g.map.at(x, 10).railroad = true;
    }
    for (int k = 0; k < 4; ++k) CHECK(g.move(m, 1, 0) == MoveResult::Moved);
    CHECK_EQ(g.unit(m)->x, 14);
    CHECK_EQ(g.unit(m)->moves, 3);   // not a third spent
    g.map.at(20, 20).terrain = Terrain::Forest;
    CHECK_EQ(g.tile_yield(20, 20, 0, false).shields, 2);
    g.map.at(20, 20).road = g.map.at(20, 20).railroad = true;
    CHECK_EQ(g.tile_yield(20, 20, 0, false).shields, 3);
    // Settlers lay railroad only on a road, once Railroad is known.
    const int s = place(g, 0, UnitType::Settlers, 30, 30);
    CHECK(!g.can_work(*g.unit(s), Order::Railroad));
    g.map.at(30, 30).road = true;
    CHECK(!g.can_work(*g.unit(s), Order::Railroad));
    g.learn(0, Advance::Railroad);
    CHECK(g.can_work(*g.unit(s), Order::Railroad));

    // Saved and loaded.
    g.map.at(40, 40).railroad = g.map.at(40, 40).road = true;
    Game back;
    CHECK(load_game(save_game(g), back));
    CHECK(back.map.at(40, 40).railroad);
}

// Pollution and global warming are not in this game: a big industrial city
// never dirties the land or changes the climate, and saves from before
// (version 2, with polluted squares, a "climate" line, Mass Transit and
// Recycling Centers, a clean-up order) load with all of that dropped.
static void test_no_pollution()
{
    Game g = flat_game(Terrain::Hills);
    const int s = place(g, 0, UnitType::Settlers, 20, 20);
    CHECK(g.found_city(s, "Smokestack"));
    City &c = g.cities.back();
    for (Advance a : {Advance::Industrialization, Advance::Automobile, Advance::MassProduction, Advance::Plastics,
                      Advance::Refining, Advance::Electricity, Advance::Robotics, Advance::Recycling})
        g.learn(0, a);
    c.size = 20;
    c.add(Building::Aqueduct);
    c.add(Building::Factory);
    c.add(Building::PowerPlant);
    c.add(Building::MfgPlant);
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) g.map.at(20 + dx, 20 + dy).mine = true;
    g.civs[0].gold = 100000;
    g.update_city(c);
    // Factory +50%, Power Plant +50%, Mfg. Plant +100%: 300% of the raw shields.
    CHECK_EQ(c.stats.shield_surplus + c.stats.shield_upkeep, c.stats.raw.shields * 300 / 100);
    CHECK(c.stats.raw.shields >= 20);
    std::vector<Terrain> before;
    for (const Tile &t : g.map.tiles) before.push_back(t.terrain);
    for (int t = 0; t < 120; ++t) {
        g.cities.back().size = 20;
        g.end_turn();
    }
    bool same = true;
    for (size_t i = 0; i < before.size(); ++i) same &= g.map.tiles[i].terrain == before[i];
    CHECK(same);
    for (const Message &m : g.messages) {
        CHECK(m.text.find("ollution") == std::string::npos);
        CHECK(m.text.find("warming") == std::string::npos);
    }
    // Mass Transit and the Recycling Center are gone; Recycling stays an advance.
    for (int b = 0; b < kBuildingCount; ++b) {
        CHECK(std::strcmp(building_info(b).name, "Mass Transit") != 0);
        CHECK(std::strcmp(building_info(b).name, "Recycling Center") != 0);
    }
    CHECK(std::strcmp(advance_info(Advance::Recycling).name, "Recycling") == 0);
    const std::string text = save_game(g);
    CHECK(text.rfind("LUNDUKECIV 3\n", 0) == 0);
    CHECK(text.find("climate|") == std::string::npos);

    // A version 2 save: City Walls, Factory, Mass Transit, Mfg. Plant,
    // Recycling Center and SDI Defense (old bits 7, 13, 17, 18, 19, 20), a
    // city building a Recycling Center, polluted squares and a settler on
    // a clean-up job.
    Game v = flat_game();
    const int vs = place(v, 0, UnitType::Settlers, 30, 30);
    CHECK(v.found_city(vs, "Oldtown"));
    const int cleaner = place(v, 0, UnitType::Settlers, 31, 31);
    std::string old = save_game(v);
    old.replace(0, std::strlen("LUNDUKECIV 3"), "LUNDUKECIV 2");
    {
        // City line: field 9 build_unit, 10 build, 11 buildings.
        const size_t at = old.find("city|");
        const size_t eol = old.find('\n', at);
        std::string line = old.substr(at, eol - at);
        std::vector<std::string> f;
        size_t p0 = 0;
        for (size_t p1; (p1 = line.find('|', p0)) != std::string::npos; p0 = p1 + 1) f.push_back(line.substr(p0, p1 - p0));
        f.push_back(line.substr(p0));
        f[9] = "0";
        f[10] = "19";
        f[11] = std::to_string(1u << 0 | 1u << 7 | 1u << 13 | 1u << 17 | 1u << 18 | 1u << 19 | 1u << 20);
        std::string nl;
        for (size_t i = 0; i < f.size(); ++i) nl += (i ? "|" : "") + f[i];
        old.replace(at, eol - at, nl);
        // The settler's order: 10 was "clean up pollution".
        const std::string key = "unit|" + std::to_string(cleaner) + "|";
        const size_t ua = old.find(key);
        size_t q = ua;
        for (int k = 0; k < 8; ++k) q = old.find('|', q) + 1;
        old.replace(q, old.find('|', q) - q, "10");
        // Map row 31: square 31 gets the old pollution bit (19).
        size_t row = old.find("map|");
        row = old.find('\n', row) + 1;
        for (int y = 0; y < 31; ++y) row = old.find('\n', row) + 1;
        const unsigned word = static_cast<unsigned>(std::strtoul(old.substr(row + 31 * 5, 5).c_str(), nullptr, 16)) | (1u << 19);
        char buf[16];
        std::snprintf(buf, sizeof buf, "%05x", word & 0xFFFFFu);
        old.replace(row + 31 * 5, 5, buf);
        old.replace(old.find("space|"), 0, "climate|599|3\n");
    }
    Game w;
    std::string err;
    CHECK(load_game(old, w, &err));
    CHECK(err.empty());
    if (!w.cities.empty()) {
        const City &oc = w.cities.back();
        CHECK(oc.has(Building::Palace));
        CHECK(oc.has(Building::CityWalls));
        CHECK(oc.has(Building::Factory));
        CHECK(oc.has(Building::MfgPlant));
        CHECK(oc.has(Building::SdiDefense));
        CHECK(!oc.has(Building::Aqueduct));
        CHECK_EQ(static_cast<long long>(oc.buildings), static_cast<long long>(1u << 0 | 1u << 7 | 1u << 13 | 1u << 17 | 1u << 18));
        CHECK(oc.build_unit);   // the Recycling Center's shields go to a defender
    }
    if (const Unit *u = w.unit(cleaner)) CHECK(u->order == Order::None);
    // Saved again, the old pollution bit is gone from the map.
    const std::string again = save_game(w);
    CHECK(again.find("climate|") == std::string::npos);
    CHECK(save_game(w) == again);
    Game x;
    CHECK(load_game(again, x));
    CHECK(save_game(x) == again);
}

// Specialists as in the DOS game: an entertainer makes 2 luxuries, a
// taxman 2 gold, a scientist 2 bulbs, all before the Marketplace, Bank,
// Library and University multiply them. Clicking one cycles entertainer ->
// taxman -> scientist -> entertainer, but only in a city of size 5 or more.
static void test_specialists()
{
    Game g = flat_game();
    g.difficulty = Difficulty::Chieftain;   // six content citizens: no disorder to settle
    g.civs[0].govt = Government::Monarchy;
    g.civs[0].tax = 4;
    g.civs[0].lux = 0;
    g.civs[0].sci = 6;
    const int s = place(g, 0, UnitType::Settlers, 20, 20);
    CHECK(g.found_city(s, "Specton"));
    const int cid = g.cities.back().id;
    City *c = g.city(cid);
    c->size = 4;
    g.update_city(*c);
    CHECK_EQ(c->specialists(), 0);
    // Take a worker off the land: an entertainer. Too small for taxmen.
    const auto w0 = c->worked.front();
    g.toggle_tile(cid, w0.first, w0.second);
    c = g.city(cid);
    CHECK_EQ(c->entertainers, 1);
    CHECK(!g.cycle_specialist(cid, 0));
    CHECK_EQ(c->entertainers, 1);
    CHECK_EQ(c->taxmen, 0);

    // Size 6 with two specialists.
    c->size = 6;
    c->manual = false;
    g.update_city(*c);
    for (int k = 0; k < 2; ++k) {
        const auto w = c->worked.back();
        g.toggle_tile(cid, w.first, w.second);
    }
    CHECK(c->manual);
    CHECK_EQ(c->entertainers, 2);
    const CityStats two_ent = c->stats;
    CHECK_EQ(two_ent.happy + two_ent.content + two_ent.unhappy, 4);   // specialists are neither
    CHECK(g.cycle_specialist(cid, 0));                                 // entertainer -> taxman
    CHECK_EQ(c->entertainers, 1);
    CHECK_EQ(c->taxmen, 1);
    CHECK_EQ(c->stats.tax, two_ent.tax + 2);
    CHECK_EQ(c->stats.lux, two_ent.lux - 2);
    CHECK_EQ(c->stats.sci, two_ent.sci);
    CHECK(g.cycle_specialist(cid, 1));                                 // taxman -> scientist
    CHECK_EQ(c->taxmen, 0);
    CHECK_EQ(c->scientists, 1);
    CHECK_EQ(c->stats.tax, two_ent.tax);
    CHECK_EQ(c->stats.sci, two_ent.sci + 2);
    CHECK(g.cycle_specialist(cid, 1));                                 // scientist -> entertainer
    CHECK_EQ(c->scientists, 0);
    CHECK_EQ(c->entertainers, 2);
    CHECK_EQ(c->stats.lux, two_ent.lux);
    CHECK(!g.cycle_specialist(cid, 2));                                // no third specialist

    // One of each kind, then a Library and a Marketplace multiply them.
    CHECK(g.cycle_specialist(cid, 0));                                 // ent, tax
    CHECK(g.cycle_specialist(cid, 0));                                 // tax, tax
    CHECK(g.cycle_specialist(cid, 1));                                 // tax, sci
    CHECK_EQ(c->taxmen, 1);
    CHECK_EQ(c->scientists, 1);
    const int land_tax = two_ent.tax, land_sci = two_ent.sci;
    c->add(Building::Library);
    c->add(Building::Marketplace);
    g.update_city(*c);
    CHECK_EQ(c->stats.sci, (land_sci + 2) * 150 / 100);
    CHECK_EQ(c->stats.tax, (land_tax + 2) * 3 / 2);

    // Saved and loaded.
    Game back;
    CHECK(load_game(save_game(g), back));
    if (const City *bc = back.city(cid)) {
        CHECK_EQ(bc->taxmen, 1);
        CHECK_EQ(bc->scientists, 1);
        CHECK_EQ(bc->stats.tax, c->stats.tax);
    }

    // Putting a specialist back to work takes an entertainer first, then
    // a taxman, then a scientist.
    {
        int fx = 9, fy = 9;
        for (int dy = -2; dy <= 2 && fx == 9; ++dy)
            for (int dx = -2; dx <= 2 && fx == 9; ++dx) {
                if ((!dx && !dy) || (std::abs(dx) == 2 && std::abs(dy) == 2)) continue;
                bool used = false;
                for (const auto &w : c->worked) used |= w.first == dx && w.second == dy;
                if (!used) fx = dx, fy = dy;
            }
        g.toggle_tile(cid, fx, fy);
    }
    CHECK_EQ(c->specialists(), 1);
    CHECK_EQ(c->taxmen, 0);
    CHECK_EQ(c->scientists, 1);

    // Under Anarchy scientists find nothing; taxmen still collect.
    c->remove(Building::Library);
    c->remove(Building::Marketplace);
    g.civs[0].govt = Government::Anarchy;
    g.update_city(*c);
    CHECK_EQ(c->stats.sci, 0);
    CHECK(g.cycle_specialist(cid, 0));                                 // scientist -> entertainer
    CHECK(g.cycle_specialist(cid, 0));                                 // -> taxman
    CHECK_EQ(c->stats.tax, 2);
    g.civs[0].govt = Government::Monarchy;

    // A city that shrinks below 5 keeps only entertainers.
    c->size = 4;
    g.update_city(*c);
    CHECK_EQ(c->taxmen, 0);
    CHECK_EQ(c->scientists, 0);
    CHECK_EQ(c->entertainers, c->size - static_cast<int>(c->worked.size()));
}

static void test_barbarians()
{
    Game g = flat_game();
    const int bc = g.barbarian_civ();
    CHECK(bc >= 0);
    if (bc < 0) return;
    CHECK(g.at_war(0, bc));
    CHECK(g.at_war(bc, 1));
    CHECK(!g.at_war(0, 1));
    const int s0 = place(g, 0, UnitType::Settlers, 1, 20);   // coastal (ocean in column 0)
    CHECK(g.found_city(s0, "Shore"));
    // A landing from the sea, and an uprising inland, each with a leader.
    g.spawn_barbarians(true, false);
    int ships = 0, raiders = 0, leaders = 0;
    for (const Unit &u : g.units) {
        if (u.owner != bc) continue;
        if (u.type == UnitType::Leader) ++leaders;
        else if (unit_info(u.type).domain == Domain::Sea) ++ships;
        else ++raiders;
    }
    CHECK(ships >= 1);
    CHECK(raiders >= 2);
    CHECK_EQ(leaders, 1);
    g.spawn_barbarians(false, true);
    leaders = 0;
    for (const Unit &u : g.units) leaders += u.owner == bc && u.type == UnitType::Leader;
    CHECK_EQ(leaders, 2);
    // Barbarians make no peace and never count for victory.
    g.make_peace(0, bc);
    CHECK(g.at_war(0, bc));

    // Ransom: walk onto a lone leader for 100 gold.
    Game r = flat_game();
    const int rb = r.barbarian_civ();
    const int lead = place(r, rb, UnitType::Leader, 11, 10);
    const int hero = place(r, 0, UnitType::Cavalry, 10, 10);
    const int gold = r.civs[0].gold;
    r.move(hero, 1, 0);
    CHECK(r.unit(lead) == nullptr);
    CHECK_EQ(r.civs[0].gold, gold + 100);
    // With an army around him, the army fights; he is not ransomed.
    Game a = flat_game();
    const int ab = a.barbarian_civ();
    const int l2 = place(a, ab, UnitType::Leader, 11, 10);
    place(a, ab, UnitType::Legion, 11, 10);
    const int h2 = place(a, 0, UnitType::Cavalry, 10, 10);
    const int g2 = a.civs[0].gold;
    a.move(h2, 1, 0);
    CHECK_EQ(a.civs[0].gold, g2);
    CHECK(a.unit(l2) != nullptr);   // kill_stack spares leaders
}

static void test_space_race()
{
    Game g = flat_game();
    const int s0 = place(g, 0, UnitType::Settlers, 10, 10);
    const int s1 = place(g, 1, UnitType::Settlers, 30, 10);
    CHECK(g.found_city(s0, "Home"));
    CHECK(g.found_city(s1, "Rival"));
    City *home = g.city_at(10, 10);
    CHECK(home != nullptr);
    if (!home) return;
    const int structural = kSpaceBase + static_cast<int>(SpacePart::Structural);
    const int component = kSpaceBase + static_cast<int>(SpacePart::Component);
    const int module = kSpaceBase + static_cast<int>(SpacePart::Module);
    for (Advance a : {Advance::SpaceFlight, Advance::Plastics, Advance::Superconductor}) g.learn(0, a);
    // Nothing until the Apollo Program stands somewhere (anyone's will do).
    CHECK(!g.space_open());
    CHECK(!g.can_build(*home, false, structural));
    g.wonder_city[static_cast<int>(Wonder::Apollo)] = g.city_at(30, 10)->id;
    CHECK(g.space_open());
    CHECK(g.can_build(*home, false, structural));
    CHECK(g.can_build(*home, false, module));
    CHECK_EQ(building_info(structural).cost, 80);
    CHECK_EQ(building_info(component).cost, 160);
    CHECK_EQ(building_info(module).cost, 320);
    // Building parts: components alternate fuel/propulsion, modules cycle.
    home->build_unit = false;
    home->build = component;
    for (int k = 0; k < 2; ++k) {
        home->shields = 1000;
        g.end_turn();
        home = g.city_at(10, 10);
    }
    CHECK_EQ(g.civs[0].components, 2);
    SpaceStats st = g.space_stats(0);
    CHECK_EQ(st.pairs, 1);
    CHECK(!st.viable);
    g.civs[0].modules = 2;
    st = g.space_stats(0);
    CHECK_EQ(st.habitation, 1);
    CHECK_EQ(st.life_support, 1);
    CHECK_EQ(st.solar, 0);
    CHECK_EQ(st.energy_pct, 0);
    g.civs[0].modules = 3;
    st = g.space_stats(0);
    CHECK_EQ(st.success_pct, 100);
    CHECK_EQ(st.unsupported, 5);     // no frame yet
    CHECK(!g.can_launch(0));
    g.civs[0].structurals = 5;
    st = g.space_stats(0);
    CHECK(st.viable);
    CHECK(st.years > 4.0);
    CHECK(g.can_launch(0));
    // More propulsion: a faster flight.
    Game faster = g;
    faster.civs[0].components = 4;
    faster.civs[0].structurals = 7;
    CHECK(faster.space_stats(0).years < st.years);
    // Launch: no more parts; the ship arrives and wins.
    CHECK(g.launch(0));
    CHECK(!g.can_launch(0));
    CHECK(!g.can_build(*g.city_at(10, 10), false, structural));
    const int arrive = g.civs[0].arrival_turn;
    CHECK(arrive > g.turn);
    CHECK(g.arrival_year(0) >= year_for_turn(g.turn) + static_cast<int>(st.years));
    Game saved;
    CHECK(load_game(save_game(g), saved));
    CHECK_EQ(saved.civs[0].arrival_turn, arrive);
    CHECK_EQ(saved.civs[0].modules, 3);
    for (int t = 0; t < 400 && g.winner < 0; ++t) g.end_turn();
    CHECK_EQ(g.winner, 0);
    CHECK(g.space_win);

    // Losing the capital loses the ship in flight.
    Game l = saved;
    City *cap = l.city_at(10, 10);
    for (Unit &u : l.units) if (u.x == 10 && u.y == 10) u.hp = 0;
    l.units.erase(std::remove_if(l.units.begin(), l.units.end(), [](const Unit &u) { return u.x == 10 && u.y == 10; }), l.units.end());
    l.declare_war(1, 0);
    const int raider = place(l, 1, UnitType::Armor, 11, 10);
    CHECK(cap != nullptr);
    l.move(raider, -1, 0);
    CHECK(l.city_at(10, 10) && l.city_at(10, 10)->owner == 1);
    CHECK_EQ(l.civs[0].arrival_turn, -1);
    CHECK_EQ(l.civs[0].modules, 0);
}

static void test_pad_nav()
{
    // Deck, Xbox and PlayStation button names.
    const pad::Glyphs deck = pad::glyphs_for(false, true);
    CHECK(deck.a == "A" && deck.lb == "L1" && deck.view == "View" && deck.menu == "\u2630");
    const pad::Glyphs xbox = pad::glyphs_for(false, false);
    CHECK(xbox.lb == "LB" && xbox.menu == "Start");
    const pad::Glyphs ps = pad::glyphs_for(true, false, true);
    CHECK(ps.a == "\u2715" && ps.view == "Create");
    // Hold timing: one step now, the next after the delay, then on a beat.
    pad::Repeat r;
    pad::repeat_press(r, 1, 0, 1000);
    CHECK_EQ(pad::repeat_steps(r, 1000 + pad::kRepeatDelayMs - 1), 0);
    CHECK_EQ(pad::repeat_steps(r, 1000 + pad::kRepeatDelayMs), 1);
    CHECK_EQ(pad::repeat_steps(r, 1000 + pad::kRepeatDelayMs + pad::kRepeatIntervalMs), 1);
    CHECK_EQ(pad::repeat_steps(r, 1000 + pad::kRepeatDelayMs + 50 * pad::kRepeatIntervalMs), 4);   // a stall owes at most four
    pad::repeat_release(r);
    CHECK_EQ(pad::repeat_steps(r, 99999), 0);
    // The stick in eight directions, with a dead zone.
    int dx = 9, dy = 9;
    pad::stick_direction(0.1f, 0.1f, dx, dy);
    CHECK(dx == 0 && dy == 0);
    pad::stick_direction(0.9f, 0.0f, dx, dy);
    CHECK(dx == 1 && dy == 0);
    pad::stick_direction(-0.7f, -0.7f, dx, dy);
    CHECK(dx == -1 && dy == -1);
    pad::stick_direction(0.0f, 1.0f, dx, dy);
    CHECK(dx == 0 && dy == 1);
    // Focus moves between buttons the way the D-pad points.
    const std::vector<pad::Rect> b = {{10, 10, 100, 30}, {120, 10, 100, 30}, {10, 50, 100, 30}, {400, 300, 140, 38}};
    CHECK_EQ(pad::next_focus(b, 0, 1, 0), 1);
    CHECK_EQ(pad::next_focus(b, 1, -1, 0), 0);
    CHECK_EQ(pad::next_focus(b, 0, 0, 1), 2);
    CHECK_EQ(pad::next_focus(b, 0, 0, -1), -1);
    CHECK_EQ(pad::default_focus(b), 3);   // the bottom-right button
}

int main(int argc, char **argv)
{
    const std::map<std::string, std::function<void()>> tests = {
        {"calendar", test_calendar},       {"terrain_yields", test_terrain_yields}, {"tech_prereqs", test_tech_prereqs},
        {"units_table", test_units_table}, {"combat_odds", test_combat_odds},       {"city_growth", test_city_growth},
        {"settlers_work", test_settlers_work}, {"movement", test_movement},          {"save_roundtrip", test_save_roundtrip},
        {"ai_smoke", test_ai_smoke},           {"wonders", test_wonders}, {"envoys", test_envoys},
        {"railroads", test_railroads}, {"no_pollution", test_no_pollution}, {"specialists", test_specialists}, {"barbarians", test_barbarians}, {"space_race", test_space_race}, {"pad_nav", test_pad_nav},
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
