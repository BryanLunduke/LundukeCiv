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

static void test_huts()
{
    Game g = flat_game();
    g.turn = turn_for_year(-2000);
    // Roll 0: 4-5 squares from the nearest city, a free city; further, 50
    // gold; closer, mercenaries.
    town(g, 1, 20, 20, 3, "Them");
    CHECK(g.hut_outcome(0, 24, 20, 0) == HutOutcome::FreeCity);
    CHECK(g.hut_outcome(0, 25, 20, 0) == HutOutcome::FreeCity);
    CHECK(g.hut_outcome(0, 26, 20, 0) == HutOutcome::Gold);
    CHECK(g.hut_outcome(0, 23, 20, 0) == HutOutcome::Mercenaries);
    // Roll 1: an advance before 1000 AD, gold after (and on the first turn).
    CHECK(g.hut_outcome(0, 30, 20, 1) == HutOutcome::Advance);
    g.turn = turn_for_year(1010);
    CHECK(g.hut_outcome(0, 30, 20, 1) == HutOutcome::Gold);
    g.turn = 1;
    CHECK(g.hut_outcome(0, 30, 20, 1) == HutOutcome::Gold);
    g.turn = turn_for_year(-2000);
    CHECK(g.hut_outcome(0, 30, 20, 2) == HutOutcome::Gold);
    // Roll 3: with no city of our own, or close to one, mercenaries;
    // otherwise barbarians.
    CHECK(g.hut_outcome(0, 30, 20, 3) == HutOutcome::Mercenaries);
    town(g, 0, 40, 20, 3, "Ours");
    CHECK(g.hut_outcome(0, 30, 20, 3) == HutOutcome::Barbarians);
    CHECK(g.hut_outcome(0, 22, 20, 3) == HutOutcome::Mercenaries);

    // Entering villages, many times over: every outcome is one of the five
    // and never an empty village.
    int seen[5] = {};
    for (int k = 0; k < 120; ++k) {
        Game h = flat_game();
        h.rng.state = 1000 + static_cast<unsigned>(k) * 7919u;
        h.turn = turn_for_year(-2000);
        town(h, 0, 10, 10, 3, "Home");
        const int dist = 2 + k % 6;   // 2, 3: mercenaries likely; 4, 5: a free city possible; 6, 7: not
        h.map.at(10 + dist, 10).hut = true;
        const int u = place(h, 0, UnitType::Legion, 9 + dist, 10);
        const int gold = h.civs[0].gold, techs = h.techs_known(0), units = static_cast<int>(h.units.size());
        h.move(u, 1, 0);
        CHECK(!h.map.at(10 + dist, 10).hut);
        CHECK(!said(h, "abandoned"));
        const City *fc = h.city_at(10 + dist, 10);
        if (fc) {
            ++seen[static_cast<int>(HutOutcome::FreeCity)];
            CHECK(dist < 6);
            CHECK_EQ(fc->owner, 0);
            CHECK_EQ(fc->size, 1);
        } else if (h.civs[0].gold == gold + 50) {
            ++seen[static_cast<int>(HutOutcome::Gold)];
        } else if (h.techs_known(0) == techs + 1) {
            ++seen[static_cast<int>(HutOutcome::Advance)];
        } else {
            int raiders = 0, mercs = 0;
            for (const Unit &v : h.units) {
                raiders += h.civs[v.owner].barbarian;
                mercs += v.owner == 0 && v.id != u && (v.type == UnitType::Legion || v.type == UnitType::Cavalry);
            }
            if (raiders) {
                ++seen[static_cast<int>(HutOutcome::Barbarians)];
                CHECK_EQ(raiders, 3);   // one city: every third square of eight
            } else {
                CHECK_EQ(mercs, 1);
                CHECK_EQ(static_cast<long long>(h.units.size()), units + 1);
                ++seen[static_cast<int>(HutOutcome::Mercenaries)];
            }
        }
    }
    for (int k = 0; k < 5; ++k) {
        if (seen[k] == 0) std::fprintf(stderr, "hut outcome %d never seen\n", k);
        CHECK(seen[k] > 0);
    }
    // The horde grows with the cities: 4 against two, 8 against three.
    for (int cities = 2; cities <= 3; ++cities) {
        Game h = flat_game();
        h.turn = turn_for_year(-2000);
        for (int k = 0; k < cities; ++k) town(h, 0, 10, 10 + k * 4, 3, "Home");
        bool done = false;
        for (unsigned seed = 1; seed < 400 && !done; ++seed) {
            Game t = h;
            t.rng.state = seed;
            t.map.at(30, 30).hut = true;
            const int u = place(t, 0, UnitType::Legion, 29, 30);
            t.move(u, 1, 0);
            int raiders = 0;
            for (const Unit &v : t.units) raiders += t.civs[v.owner].barbarian;
            if (raiders) {
                CHECK_EQ(raiders, cities == 2 ? 4 : 8);
                done = true;
            }
        }
        CHECK(done);
    }
    // Aircraft fly over a village and find nothing in it.
    Game a = flat_game();
    a.map.at(21, 20).hut = true;
    const int f = place(a, 0, UnitType::Fighter, 20, 20);
    const int gold = a.civs[0].gold;
    a.move(f, 1, 0);
    CHECK(a.map.at(21, 20).hut);
    CHECK_EQ(a.civs[0].gold, gold);
}

static void test_diplomats()
{
    Game g = flat_game();
    town(g, 1, 20, 20, 5, "Capital");   // their first city: the Palace
    City &other = town(g, 1, 26, 20, 5, "Other");
    const int oid = other.id;
    g.learn(1, Advance::Writing);
    // Meet with King: the Diplomat lives and its turn ends; no war.
    int d = place(g, 0, UnitType::Diplomat, 21, 20);
    CHECK(g.diplomat_can(d, 20, 20, DiploAction::MeetKing));
    CHECK(g.diplomat_act(d, 20, 20, DiploAction::MeetKing).empty());
    CHECK(g.unit(d) != nullptr);
    CHECK_EQ(g.unit(d)->moves, 0);
    CHECK(!g.at_war(0, 1));
    CHECK(std::string(diplo_action_name(DiploAction::MeetKing)) == "Meet with King");
    // Not with the raiders.
    const int bc = g.barbarian_civ();
    town(g, bc, 40, 20, 3, "Camp");
    const int d2 = place(g, 0, UnitType::Diplomat, 41, 20);
    CHECK(!g.diplomat_can(d2, 40, 20, DiploAction::MeetKing));
    // Every other city action spends it.
    const DiploAction spent[] = {DiploAction::Embassy, DiploAction::Investigate, DiploAction::StealTech, DiploAction::Sabotage};
    for (DiploAction a : spent) {
        const int k = place(g, 0, UnitType::Diplomat, 27, 20);
        CHECK(g.diplomat_can(k, 26, 20, a));
        CHECK(!g.diplomat_act(k, 26, 20, a).empty());
        CHECK(g.unit(k) == nullptr);
    }
    // Nothing left to steal here: the attempt is refused and costs nothing.
    const int k2 = place(g, 0, UnitType::Diplomat, 27, 20);
    CHECK(!g.diplomat_can(k2, 26, 20, DiploAction::StealTech));
    CHECK(g.diplomat_act(k2, 26, 20, DiploAction::StealTech).empty());
    CHECK(g.unit(k2) != nullptr);
    // Incite spends it.
    g.civs[0].gold = 5000;
    CHECK(g.diplomat_can(k2, 26, 20, DiploAction::Incite));
    CHECK(!g.diplomat_act(k2, 26, 20, DiploAction::Incite).empty());
    CHECK(g.unit(k2) == nullptr);
    CHECK_EQ(g.city(oid)->owner, 0);
    // Bribing a unit leaves the Diplomat standing, its turn over.
    const int v = place(g, 1, UnitType::Legion, 31, 30);
    const int k3 = place(g, 0, UnitType::Diplomat, 30, 30);
    CHECK(g.diplomat_can(k3, 31, 30, DiploAction::Bribe));
    CHECK(!g.diplomat_act(k3, 31, 30, DiploAction::Bribe).empty());
    CHECK(g.unit(k3) != nullptr);
    CHECK_EQ(g.unit(k3)->moves, 0);
    CHECK_EQ(g.unit(v)->owner, 0);
    CHECK(Game::diplomat_spent(DiploAction::Embassy));
    CHECK(!Game::diplomat_spent(DiploAction::MeetKing));
    CHECK(!Game::diplomat_spent(DiploAction::Bribe));
}

// ---------------------------------------------------------------- research

static void test_science_carryover()
{
    Game g = flat_game();
    g.civs[0].ai = false;
    g.civs[1].ai = true;
    City &c = town(g, 0, 10, 10, 6, "Home");
    (void)c;
    // Surplus bulbs are lost when an advance is found.
    g.civs[0].research = Advance::Alphabet;
    const int cost = g.research_cost(0);
    g.civs[0].bulbs = cost + 7;
    g.end_turn();
    CHECK(g.has(0, Advance::Alphabet));
    CHECK(g.civs[0].bulbs < cost);   // only this turn's science, not the 7 over
    // The choice stands once bulbs are gathered (DOS has no switching).
    g.civs[0].research = Advance::Pottery;
    g.civs[0].bulbs = 3;
    CHECK(!g.can_change_research(0));
    CHECK(!g.set_research(0, Advance::BronzeWorking));
    CHECK(g.civs[0].research == Advance::Pottery);
    g.civs[0].bulbs = 0;
    CHECK(g.set_research(0, Advance::BronzeWorking));
    CHECK(g.civs[0].research == Advance::BronzeWorking);
    CHECK(!g.set_research(0, Advance::Monarchy));   // not open yet
    // A human whose subject arrives another way loses the bulbs toward it.
    g.civs[0].bulbs = 9;
    g.learn(0, Advance::BronzeWorking);
    CHECK(g.civs[0].research == Advance::None);
    CHECK_EQ(g.civs[0].bulbs, 0);
    CHECK(g.can_change_research(0));
    // Learning something else keeps them.
    g.civs[0].research = Advance::Pottery;
    g.civs[0].bulbs = 9;
    g.learn(0, Advance::Masonry);
    CHECK_EQ(g.civs[0].bulbs, 9);
    // A computer ruler keeps them (DOS).
    g.civs[1].research = Advance::Pottery;
    g.civs[1].bulbs = 9;
    g.learn(1, Advance::Pottery);
    CHECK_EQ(g.civs[1].bulbs, 9);
}

static void test_ai_specialists()
{
    // A computer city with more people than good land: the spare ones
    // become scientists (taxmen when the treasury is low), and it stays in
    // order.
    auto setup = [](int gold) {
        Game g = flat_game(Terrain::Tundra);
        g.civs[1].ai = true;
        g.civs[1].gold = gold;
        g.civs[1].research = Advance::Alphabet;
        City &c = town(g, 1, 20, 20, 8, "Crowded");
        c.add(Building::Temple);
        // Only the centre and a few squares are workable: the rest is ocean.
        for (int j = -2; j <= 2; ++j)
            for (int i = -2; i <= 2; ++i)
                if (std::abs(i) + std::abs(j) > 1) g.map.at(20 + i, 20 + j).terrain = Terrain::Ocean;
        g.map.label_continents();
        g.update_city(c);
        return g;
    };
    Game rich = setup(500);
    const City &r = *rich.city_at(20, 20);
    CHECK(r.specialists() > 0);
    CHECK(r.scientists > 0);
    CHECK_EQ(r.taxmen, 0);
    CHECK(!r.stats.disorder);
    Game poor = setup(0);
    const City &p = *poor.city_at(20, 20);
    CHECK(p.taxmen > 0);
    CHECK_EQ(p.scientists, 0);
    CHECK(!p.stats.disorder);
    // Not below size 5, and never for a human ruler.
    Game small = setup(500);
    City &s = *small.city_at(20, 20);
    s.size = 4;
    small.update_city(s);
    CHECK_EQ(s.taxmen + s.scientists, 0);
    Game human = setup(500);
    human.civs[1].ai = false;
    City &h = *human.city_at(20, 20);
    human.update_city(h);
    CHECK_EQ(h.taxmen + h.scientists, 0);
    CHECK(h.entertainers > 0);
}

int main(int argc, char **argv)
{
    const std::map<std::string, std::function<void()>> tests = {
        {"score", test_score}, {"end_2100", test_end_2100}, {"hall_of_fame", test_hall_of_fame}, {"save_v6", test_save_v6},
        {"huts", test_huts}, {"diplomats", test_diplomats}, {"science_carryover", test_science_carryover},
        {"ai_specialists", test_ai_specialists},
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
