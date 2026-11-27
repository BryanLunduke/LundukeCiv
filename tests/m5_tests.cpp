// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Milestone 5 tests: the end of the game (score, 2100 AD, retirement, the
// Hall of Fame), villages and Diplomats as in DOS, research, and the
// computer rulers' specialists, ships and aircraft. One named group per
// meson test.

#include "fame.hpp"
#include "game.hpp"

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

static int turn_for_year(int year)
{
    int t = 1;
    while (year_for_turn(t) < year) ++t;
    return t;
}

static bool said(const Game &g, const std::string &what)
{
    for (const Message &m : g.messages)
        if (m.text.find(what) != std::string::npos) return true;
    return false;
}

// ---------------------------------------------------------------- the end

static void test_score()
{
    Game g = flat_game();
    g.turn = turn_for_year(1500);
    for (int j = -2; j <= 2; ++j)
        for (int i = -2; i <= 2; ++i) g.map.at(10 + i, 10 + j).terrain = Terrain::Ocean;   // trade for luxuries
    g.map.at(10, 10).terrain = Terrain::Grassland;
    g.map.label_continents();
    City &c = town(g, 0, 10, 10, 4, "Scoretown");   // content without luxuries: no entertainers
    c.add(Building::Temple);
    const int cid = c.id;
    // Citizens are counted at 40% luxuries: with science at 60% that is the
    // city as it stands with taxes at nothing.
    g.set_rates(0, 0, 4, 6);
    g.update_city(*g.city(cid));
    const CityStats at40 = g.city(cid)->stats;
    g.set_rates(0, 4, 0, 6);
    g.update_city(*g.city(cid));
    const CityStats before = g.city(cid)->stats;
    CHECK(before.happy < at40.happy);
    ScoreBreakdown s = g.score(0);
    CHECK_EQ(s.citizens, 4 + at40.happy - at40.unhappy);
    CHECK_EQ(s.happy, at40.happy);
    CHECK(at40.happy > 0);
    // The rates and the city are as they were.
    CHECK_EQ(g.civs[0].tax, 4);
    CHECK_EQ(g.civs[0].lux, 0);
    CHECK_EQ(g.city(cid)->stats.happy, before.happy);
    CHECK_EQ(g.city(cid)->stats.tax, before.tax);
    CHECK_EQ(s.total, s.citizens);
    // Wonders 20 each, Future Techs 5 each, peace 3 a turn up to 100.
    g.wonder_city[static_cast<int>(Wonder::Pyramids)] = cid;
    g.wonder_city[static_cast<int>(Wonder::Colossus)] = cid;
    g.civs[0].future = 3;
    g.peace_turns = 10;
    s = g.score(0);
    CHECK_EQ(s.wonders, 2);
    CHECK_EQ(s.wonder_points, 40);
    CHECK_EQ(s.future, 15);
    CHECK_EQ(s.peace, 30);
    CHECK_EQ(s.total, s.citizens + 40 + 15 + 30);
    g.peace_turns = 50;
    CHECK_EQ(g.score(0).peace, 100);
    g.turn = turn_for_year(-500);   // no peace points before AD
    CHECK_EQ(g.score(0).peace, 0);
    g.turn = turn_for_year(1900);
    // A spaceship in flight is only "possible"; once it lands, success x
    // colonists (ten thousands) / 2.
    g.civs[0].structurals = 4;
    g.civs[0].components = 2;
    g.civs[0].modules = 3;
    CHECK_EQ(g.space_stats(0).success_pct, 100);
    g.civs[0].launch_turn = g.turn;
    s = g.score(0);
    CHECK_EQ(s.spaceship, 0);
    CHECK_EQ(s.spaceship_possible, 50);
    g.winner = 0;
    g.space_win = true;
    s = g.score(0);
    CHECK_EQ(s.spaceship, 50);
    CHECK_EQ(s.bonus, -1);
    // Conquest: at least (550 - turns) x 2 + 100 a rival + 400.
    g.space_win = false;
    g.civs[0].launch_turn = -1;
    g.turn = 301;
    g.opponents = 3;
    s = g.score(0);
    CHECK_EQ(s.bonus, (550 - 300) * 2 + 300 + 400);
    CHECK_EQ(s.total, s.bonus);
    // Rival peoples' fights end the peace; raiders don't count.
    Game h = flat_game();
    h.peace_turns = 9;
    const int bc = h.barbarian_civ();
    CHECK(bc >= 0);
    const int a = place(h, 0, UnitType::Legion, 10, 10);
    place(h, bc, UnitType::Militia, 11, 10);
    h.move(a, 1, 0);
    CHECK_EQ(h.peace_turns, 9);
    h.declare_war(0, 1);
    const int b = place(h, 0, UnitType::Legion, 20, 10);
    place(h, 1, UnitType::Militia, 21, 10);
    h.move(b, 1, 0);
    CHECK_EQ(h.peace_turns, 0);
}

static void test_end_2100()
{
    Game g = flat_game();
    g.civs.pop_back();   // no raiders to end the reign early
    town(g, 0, 10, 10, 3, "Home");
    town(g, 1, 30, 30, 3, "Rival");
    g.make_peace(0, 1);
    g.turn = turn_for_year(2070);
    g.peace_turns = 0;
    const int before = g.peace_turns;
    g.end_turn();
    CHECK_EQ(g.peace_turns, before + 1);   // a turn of peace in AD
    while (year_for_turn(g.turn) < 2080) g.end_turn();
    CHECK(said(g, "plans to retire in 20 years"));
    CHECK(!g.game_ended);
    while (year_for_turn(g.turn) < Game::kEndYear) g.end_turn();
    CHECK(g.game_ended);
    CHECK_EQ(year_for_turn(g.end_turn_number), 2100);
    CHECK(!g.over());   // the ruler may play on
    const int t = g.turn;
    g.end_turn();
    CHECK_EQ(g.turn, t + 1);
    CHECK_EQ(g.end_turn_number, t);
    // Retiring ends it for good.
    Game r = flat_game();
    town(r, 0, 10, 10, 3, "Home");
    r.turn = turn_for_year(1200);
    CHECK(!r.over());
    r.retire();
    CHECK(r.over());
    CHECK(r.retired && r.game_ended);
    CHECK_EQ(r.end_turn_number, r.turn);
}

static void test_hall_of_fame()
{
    std::vector<FameEntry> list;
    auto entry = [](const char *who, int level, int score) {
        FameEntry e;
        e.leader = who;
        e.people = "Lundukians";
        e.difficulty = level;
        e.score = score;
        e.year = 2100;
        e.population = 1230000;
        return e;
    };
    // Rating = score x (level + 1) / 50.
    CHECK_EQ(entry("a", 0, 500).rating(), 10);
    CHECK_EQ(entry("a", 4, 500).rating(), 50);
    CHECK_EQ(add_fame(list, entry("Chief", 0, 500)), 0);
    CHECK_EQ(add_fame(list, entry("Emperor", 4, 500)), 0);   // a better rating goes first
    CHECK_EQ(add_fame(list, entry("Prince", 2, 500)), 1);
    CHECK_EQ(add_fame(list, entry("Tie", 2, 500)), 2);       // must beat an equal one to pass it
    CHECK_EQ(add_fame(list, entry("Nobody", 2, 0)), -1);
    for (int k = 0; k < 4; ++k) add_fame(list, entry("Filler", 3, 1000));
    CHECK_EQ(static_cast<long long>(list.size()), kFameSize);
    CHECK_EQ(add_fame(list, entry("Low", 0, 10)), -1);
    CHECK(list.front().leader == "Filler");
    // Round trip through the file text.
    const std::string text = save_fame(list);
    const std::vector<FameEntry> back = load_fame(text);
    CHECK_EQ(static_cast<long long>(back.size()), static_cast<long long>(list.size()));
    for (size_t i = 0; i < back.size() && i < list.size(); ++i) {
        CHECK(back[i].leader == list[i].leader);
        CHECK_EQ(back[i].score, list[i].score);
        CHECK_EQ(back[i].difficulty, list[i].difficulty);
        CHECK_EQ(back[i].population, list[i].population);
    }
    CHECK(load_fame("garbage").empty());
    // The ruler a rating earns: the last of 1..20 with i*i/3 below it.
    CHECK_EQ(famous_rank(0), -1);
    CHECK_EQ(famous_rank(1), 0);    // 1*1/3 = 0 is below 1; 2*2/3 = 1 is not
    CHECK_EQ(famous_rank(2), 1);
    CHECK_EQ(famous_rank(200), 19);
    CHECK(std::strlen(famous_ruler(0)) > 0);
    // The human's entry for a game.
    Game g = flat_game();
    town(g, 0, 10, 10, 4, "Home");
    g.turn = turn_for_year(1800);
    g.retire();
    g.turn += 5;   // looking around afterwards changes nothing
    const FameEntry e = fame_entry(g);
    CHECK_EQ(e.year, 1800);
    CHECK(e.leader == g.civs[0].leader);
    CHECK_EQ(e.population, g.population(0));
    CHECK(e.score > 0);
}

static void test_save_v6()
{
    Game g = flat_game();
    town(g, 0, 10, 10, 4, "Home");
    g.peace_turns = 17;
    g.game_ended = true;
    g.end_turn_number = 444;
    g.opponents = 5;
    const std::string text = save_game(g);
    CHECK(text.rfind("LUNDUKECIV 6\n", 0) == 0);
    Game h;
    std::string err;
    CHECK(load_game(text, h, &err));
    CHECK_EQ(h.peace_turns, 17);
    CHECK(h.game_ended);
    CHECK(!h.retired);
    CHECK_EQ(h.end_turn_number, 444);
    CHECK_EQ(h.opponents, 5);
    // A version 5 save: no end6 line; the rivals are counted from the list.
    std::string old = text;
    old.replace(0, std::strlen("LUNDUKECIV 6"), "LUNDUKECIV 5");
    const size_t at = old.find("end6|");
    old.erase(at, old.find('\n', at) - at + 1);
    Game o;
    CHECK(load_game(old, o, &err));
    CHECK_EQ(o.peace_turns, 0);
    CHECK(!o.game_ended);
    CHECK_EQ(o.opponents, 1);
}

// ---------------------------------------------------------------- villages and Diplomats



// ---------------------------------------------------------------- research



int main(int argc, char **argv)
{
    const std::map<std::string, std::function<void()>> tests = {
        {"score", test_score}, {"end_2100", test_end_2100}, {"hall_of_fame", test_hall_of_fame}, {"save_v6", test_save_v6},
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
