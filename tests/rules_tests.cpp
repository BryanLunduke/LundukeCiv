// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Rules tests: run one named group per meson test.

#include "game.hpp"

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
    CHECK_EQ(kUnitTypeCount, 28);
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

int main(int argc, char **argv)
{
    const std::map<std::string, std::function<void()>> tests = {
        {"calendar", test_calendar},       {"terrain_yields", test_terrain_yields}, {"tech_prereqs", test_tech_prereqs},
        {"units_table", test_units_table}, {"combat_odds", test_combat_odds},       {"city_growth", test_city_growth},
        {"settlers_work", test_settlers_work}, {"movement", test_movement},          {"save_roundtrip", test_save_roundtrip},
        {"ai_smoke", test_ai_smoke},
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
