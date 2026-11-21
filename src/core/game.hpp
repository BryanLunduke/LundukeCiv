// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

// Game state and rules: civilizations, units, cities, the turn, combat,
// research and the computer players. No windowing or drawing here, so the
// rules build into the unit tests on their own.

#include "map.hpp"
#include "rng.hpp"
#include "rules.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace civ {

constexpr int kMaxCivs = 8;
constexpr int kUnitHp = 10;   // hit points of a healthy unit; each lost combat round costs one

enum class Order : std::uint8_t { None, Fortifying, Fortified, Sentry, Goto, Road, Irrigate, Mine, Fortress };

struct Unit {
    int id = 0;
    UnitType type = UnitType::Militia;
    int owner = 0;
    int x = 0;
    int y = 0;
    int moves = 0;          // in thirds of a movement point
    bool veteran = false;
    Order order = Order::None;
    int work = 0;           // turns spent on the current terrain job
    int home = -1;          // supporting city id, -1 none
    int goto_x = -1;
    int goto_y = -1;
    int fuel = 0;           // air units: turns left aloft
    int carried_by = -1;    // ship id when aboard
    bool done = false;      // skipped for this turn
    int hp = kUnitHp;       // damage from combat heals over a few turns
};

struct CityStats {
    Yield raw;              // from worked tiles and the centre
    int food_upkeep = 0;
    int shield_upkeep = 0;
    int food_surplus = 0;
    int shield_surplus = 0;
    int corruption = 0;
    int tax = 0;
    int lux = 0;
    int sci = 0;
    int happy = 0;
    int content = 0;
    int unhappy = 0;
    int food_box = 0;
    bool disorder = false;
};

struct City {
    int id = 0;
    std::string name;
    int owner = 0;
    int x = 0;
    int y = 0;
    int size = 1;
    int food = 0;
    int shields = 0;
    bool build_unit = true;
    int build = static_cast<int>(UnitType::Militia);
    std::uint32_t buildings = 0;
    std::vector<std::pair<int, int>> worked; // offsets from the centre
    int entertainers = 0;
    bool manual = false;    // the player placed workers by hand
    int founded = 1;
    bool bought = false;
    CityStats stats;

    bool has(Building b) const { return (buildings >> static_cast<int>(b)) & 1u; }
    void add(Building b) { buildings |= 1u << static_cast<int>(b); }
    void remove(Building b) { buildings &= ~(1u << static_cast<int>(b)); }
};

struct Civ {
    std::string name;       // plural people name, "Lundukians"
    std::string leader;
    int color = 0;
    bool alive = true;
    bool ai = true;
    int gold = 50;
    int tax = 4;            // tenths
    int lux = 0;
    int sci = 6;
    Government govt = Government::Despotism;
    Government target = Government::Despotism;
    int anarchy = 0;
    std::array<bool, kAdvanceCount> techs{};
    Advance research = Advance::None;
    int bulbs = 0;
    int future = 0;
    int name_next = 0;
    std::uint8_t war = 0;       // bit per civ
    std::uint8_t contact = 0;   // bit per civ
};

enum class MsgKind : std::uint8_t { Info, Good, Bad, Combat, Tech, City, Wonder };

struct Message {
    int civ = -1;          // -1 everyone
    MsgKind kind = MsgKind::Info;
    std::string text;
    int wonder = -1;       // MsgKind::Wonder: which one
    int x = -1;
    int y = -1;
    int turn = 0;
};

struct CombatResult {
    bool happened = false;
    UnitType att_type = UnitType::Militia;
    UnitType def_type = UnitType::Militia;
    int att_owner = 0;
    int def_owner = 0;
    double att = 0;
    double def = 0;
    double odds = 0;        // attacker's chance to win the whole fight
    double round_odds = 0;  // attacker's chance to win any one round
    int att_hp0 = 0;        // hit points going in
    int def_hp0 = 0;
    int att_hp = 0;         // hit points left
    int def_hp = 0;
    std::vector<std::uint8_t> rounds; // 1: the attacker won that round
    bool attacker_won = false;
    int x = 0;
    int y = 0;
    int killed = 0;         // units lost by the loser's side
    bool city_shrank = false;
    int turn = 0;
};

enum class MoveResult : std::uint8_t {
    Moved, Attacked, Captured, Boarded, Blocked, NoMoves, NeedWar, Zoc, Failed, Destroyed
};

// Combat is fought in rounds: each round the attacker wins with chance
// att / (att + def) and the loser of the round loses one hit point; the
// first side out of hit points loses. This is the chance the attacker wins.
double combat_win_chance(double att, double def, int att_hp, int def_hp);

struct GameSettings {
    std::uint64_t seed = 1;
    Difficulty difficulty = Difficulty::Prince;
    int civs = 5;              // including the player
    std::string player_name = "Lundukians";
    std::string player_leader = "Bryan";
    int player_color = 0;
    int land_pct = 32;
    bool all_ai = false;       // autoplay: the player's civ is run by the computer too
};

// Civ names offered for the computer players: peoples of history.
struct CivPreset {
    const char *name;
    const char *leader;
    int color;
    const char *cities[16];
};
const CivPreset &civ_preset(int i);
int civ_preset_count();
const char *player_city_name(int i);

class Game {
public:
    Map map;
    std::vector<Civ> civs;
    std::vector<Unit> units;
    std::vector<City> cities;
    int turn = 1;
    Difficulty difficulty = Difficulty::Prince;
    int human = 0;
    Rng rng;
    int next_unit_id = 1;
    int next_city_id = 1;
    std::vector<Message> messages;
    CombatResult last_combat;
    int winner = -1;           // civ that won by conquest
    bool human_dead = false;
    bool all_ai = false;
    std::uint64_t seed = 1;
    // Wonders of the World: the city id holding each, -1 not built yet,
    // -2 lost with its city.
    std::array<int, kWonderCount> wonder_city{};
    std::array<std::uint32_t, kMaxCivs> wonder_started{};  // announced "is building" bits

    Game() { wonder_city.fill(-1); }

    // --- setup
    static Game create(const GameSettings &s);

    // --- lookups
    Unit *unit(int id);
    const Unit *unit(int id) const;
    City *city(int id);
    const City *city(int id) const;
    City *city_at(int x, int y);
    const City *city_at(int x, int y) const;
    std::vector<int> units_at(int x, int y) const;  // ids, ships first
    bool enemy_at(int civ, int x, int y) const;      // a foreign unit or city
    int owner_at(int x, int y) const;                // -1 empty
    City *capital(int civ);

    // --- knowledge
    bool has(int civ, Advance a) const { return a == Advance::None || civs[civ].techs[static_cast<int>(a)]; }
    int techs_known(int civ) const;
    bool can_research(int civ, Advance a) const;
    std::vector<Advance> researchable(int civ) const;
    int research_cost(int civ) const;
    void learn(int civ, Advance a);
    bool can_build(const City &c, bool unit, int what) const;
    bool unit_available(int civ, UnitType u) const;

    // --- wonders
    int wonder_owner(Wonder w) const;                  // civ holding it, -1 none
    bool wonder_built(Wonder w) const { return wonder_city[static_cast<int>(w)] != -1; }
    bool wonder_obsolete(Wonder w) const;              // its owner knows the advance that ends it
    bool wonder_active(int civ, Wonder w) const;       // civ holds it and it still works
    bool wonder_in(const City &c, Wonder w) const;     // in this city and still working
    const City *wonder_home(Wonder w) const;
    int wonders_in(const City &c) const;               // count, for the city screen

    // --- rules
    Yield tile_yield(int x, int y, int civ, bool centre) const;
    bool tile_free_for(const City &c, int x, int y) const;
    void update_city(City &c);            // recompute stats (and arrange workers unless manual)
    void arrange_workers(City &c);
    // Computer rulers get Civ-style handicaps: cheaper growth and builds on
    // harder levels, dearer on Chieftain (percent, 100 for humans).
    int ai_cost_percent(int civ) const;
    int food_box(const City &c) const { return std::max(4, (c.size + 1) * ai_cost_percent(c.owner) / 10); }
    int size_cap(const City &c) const { return c.has(Building::Aqueduct) ? 99 : 10; }
    int buy_cost(const City &c) const;
    int production_cost(const City &c) const;
    double defense_strength(const Unit &d, const Unit *attacker) const;
    double attack_strength(const Unit &a) const;
    int move_cost(const Unit &u, int fx, int fy, int tx, int ty) const;
    bool can_enter(const Unit &u, int tx, int ty) const;   // terrain/domain only
    bool zoc_blocks(const Unit &u, int tx, int ty) const;
    int full_moves(const Unit &u) const;
    bool hoover_here(const City &c) const;   // Hoover Dam powers this city
    int cargo_count(int ship_id) const;

    // --- actions
    MoveResult move(int unit_id, int dx, int dy);
    MoveResult attack(int unit_id, int tx, int ty);
    bool found_city(int unit_id, const std::string &name = std::string());
    bool join_city(int unit_id);
    bool start_work(int unit_id, Order job);
    bool can_work(const Unit &u, Order job) const;
    void fortify(int unit_id);
    void sentry(int unit_id);
    void disband(int unit_id);
    void set_goto(int unit_id, int x, int y);
    bool step_goto(int unit_id);          // one move toward the goto target
    void declare_war(int a, int b);
    void make_peace(int a, int b);
    bool at_war(int a, int b) const { return a != b && ((civs[a].war >> b) & 1u); }
    void set_rates(int civ, int tax, int lux, int sci);
    void revolution(int civ, Government g);
    bool buy(int city_id);
    void set_production(int city_id, bool unit, int what);
    void toggle_tile(int city_id, int dx, int dy);

    // --- turn
    void begin_human_phase();             // goto orders, sentries waking
    void end_turn();                      // computer players, then a new turn
    int next_unit_for(int civ, int after_id) const;
    void refresh_visibility();
    void check_elimination();

    // --- AI
    void ai_play(int civ);

    void say(int civ, MsgKind kind, const std::string &text, int x = -1, int y = -1);
    std::string city_display_name(int id) const;
    std::string unit_name(const Unit &u) const;
    std::string suggest_city_name(int civ) const;   // the name found_city would pick

    // Pathfinding for land/sea units; returns false if no route.
    bool next_step(const Unit &u, int tx, int ty, int &sdx, int &sdy, int limit = 6000) const;

private:
    int add_unit(int owner, UnitType t, int x, int y, int home);
    void kill_unit(int id);
    void kill_stack(int x, int y);
    void process_city(City &c, bool &learned_any);
    void start_turn_for(int civ);
    void new_turn();
    void reveal(int civ, int x, int y, int r);
    void enter_hut(Unit &u);
    void capture_city(City &c, int new_owner);
    std::string next_city_name(int civ);
    void choose_research_ai(int civ);
    void complete_wonder(City &c, Wonder w);
    void announce_wonder_start(const City &c);
    void ai_city(City &c);
    void ai_unit(Unit &u);
    void ai_diplomacy(int civ);
    bool ai_settler(Unit &u);
};

// Save and load as plain text. Version line first.
std::string save_game(const Game &g);
bool load_game(const std::string &text, Game &g, std::string *error = nullptr);

} // namespace civ
