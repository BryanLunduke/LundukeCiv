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
#include "tribes.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace civ {

constexpr int kMaxCivs = 8;
constexpr int kUnitHp = 10;
constexpr int kCeasefireTurns = 16;   // hit points of a healthy unit; each lost combat round costs one

enum class Order : std::uint8_t { None, Fortifying, Fortified, Sentry, Goto, Road, Irrigate, Mine, Fortress, Railroad };

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
    // Computer rulers: what the unit is about (AiPlan). A ship keeps its
    // destination in goto_x, goto_y.
    std::uint8_t plan = 0;
};

enum AiPlan : std::uint8_t {
    kPlanNone = 0,
    kPlanPassenger = 1,   // waits, sentried in a port, for a ship overseas
    kPlanFerry = 2,       // a ship carrying passengers to goto_x, goto_y
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
    int palace_distance = 0; // for corruption (DOS distance; 10 under Communism)
    int base_trade = 0;     // trade from the land, before trade routes
    int route_trade = 0;    // added by trade routes
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
    // Specialists: citizens not working the land. As in the DOS game each
    // makes 2 of something: an entertainer luxuries, a taxman gold, a
    // scientist bulbs. Taxmen and scientists need a city of size 5 or more.
    int entertainers = 0;
    int taxmen = 0;
    int scientists = 0;
    int specialists() const { return entertainers + taxmen + scientists; }
    bool manual = false;    // the player placed workers by hand
    int founded = 1;
    bool bought = false;
    std::vector<int> routes;     // trade route partner city ids (up to 3)
    std::uint8_t stolen = 0;     // civs whose diplomats already stole an advance here
    // "We Love the King Day": a celebration that started last turn (more
    // than half the people happy, none unhappy, size 3 or more).
    bool celebrating = false;
    int disorder_turns = 0;      // turns in a row in civil disorder
    bool sold = false;           // a building was sold here this turn (one a turn)
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
    std::uint8_t embassy = 0;   // bit per civ we have an embassy with
    // Treaties (bit per civ, kept the same on both sides): a cease-fire
    // runs out after kCeasefireTurns; a peace treaty lasts until broken.
    std::uint8_t ceasefire = 0;
    std::uint8_t peace = 0;
    std::uint8_t vendetta = 0;  // grudges: civs that broke a treaty with us or nuked us
    std::array<int, kMaxCivs> ceasefire_end{};
    std::array<int, kMaxCivs> last_talk{};   // turn of the last audience with each civ (0 never)
    int tribe = -1;             // Civ 1 tribe (tribe_info), -1 a people of the player's own making
    bool barbarian = false;     // the raiders: always at war, never a rival for victory
    // The spaceship.
    int structurals = 0;
    int components = 0;
    int modules = 0;
    int launch_turn = -1;       // -1 not launched
    int arrival_turn = -1;
};

// What a civ's spaceship adds up to.
struct SpaceStats {
    int fuel = 0, propulsion = 0, pairs = 0;
    int habitation = 0, life_support = 0, solar = 0;
    int unsupported = 0;        // components and modules no structural holds
    int population = 0;         // colonists
    int life_pct = 0;           // life support for the colonists
    int energy_pct = 0;         // power for habitation and life support
    int success_pct = 0;
    int mass = 0;               // tons
    double years = 0;           // flight time
    bool viable = false;        // enough of everything to launch
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

// Moments the human player sees a full screen for (event screens). The
// window code takes them from Game::events and clears the list; they are
// not saved.
enum class EventKind : std::uint8_t {
    Advance,      // arg: the advance learned
    Government,   // arg: the new government
    CityTaken,    // city: the city's id, arg: the old owner (civ is the new one)
    CityLost,     // city: the city's id, arg: the new owner
};

struct GameEvent {
    EventKind kind = EventKind::Advance;
    int civ = -1;       // whose event
    int arg = -1;
    int city = -1;
    std::string name;   // the city's name (it may be gone by the time it shows)
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
    Moved, Attacked, Captured, Boarded, Blocked, NoMoves, NeedWar, Zoc, Failed, Destroyed,
    FighterOnly,   // only Fighters may attack aircraft outside a city
    Nuked,         // a Nuclear unit went off (or was shot down by SDI Defense)
    Senate,        // the Senate will not let a Republic or Democracy break a treaty
    Diplomat,   // a Diplomat reached a foreign city or unit: choose an action
    Caravan,    // a Caravan reached a city: help a wonder or open a trade route
};

// What a village of a minor tribe holds (DOS).
enum class HutOutcome : std::uint8_t { Gold, Advance, Mercenaries, Barbarians, FreeCity };

// What a Diplomat may do. As in DOS, a Diplomat is spent on every action
// in a city -- except Meet with King, which only ends its turn, and an
// attempt that comes to nothing (no advance to steal). Bribing a unit
// leaves the Diplomat in the field.
enum class DiploAction : std::uint8_t { Embassy, Investigate, StealTech, Sabotage, Incite, MeetKing, Bribe, Count };
const char *diplo_action_name(DiploAction a);
constexpr int kMaxRoutes = 3;

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
    int player_color = 1;      // a people of the player's own making (tribe -1)
    int player_tribe = -1;     // or one of the fourteen (tribe_info)
    int land_pct = 32;
    // Customize World (see MapSettings), or the Earth.
    bool earth = false;
    int temperature = 1;
    int climate = 1;
    int age = 1;
    bool all_ai = false;       // autoplay: the player's civ is run by the computer too
};

const char *player_city_name(int i);

// Relations between two peoples. Everyone starts with no treaty; a war ends
// in a cease-fire (kCeasefireTurns, then war again) and a cease-fire may
// become a peace treaty. A Republic or Democracy's Senate holds to them.
enum class Treaty : std::uint8_t { War, None, Ceasefire, Peace };
const char *treaty_name(Treaty t);

// An audience with a foreign leader. The computer rulers' come to the human
// player through Game::talks, to be answered with answer_talk.
enum class TalkKind : std::uint8_t {
    Greeting,        // first meeting, nothing asked
    Tribute,         // pay `gold` or there is war
    TechDemand,      // give `tech` or there is war
    Ceasefire,       // offer to stop a war
    Peace,           // offer a peace treaty
    TechForPeace,    // `tech` given for a peace treaty
    Embassy,         // exchange embassies
    WarDeclared,     // news: they attack
    TreatyCancelled, // news: they tore up a treaty
    CeasefireOver,   // news: the cease-fire has run out
    Count
};
struct Talk {
    int from = -1;
    int to = -1;
    TalkKind kind = TalkKind::Greeting;
    int gold = 0;
    Advance tech = Advance::None;
    bool forced = false;   // the Senate accepted it for us: OK only
    std::string text;      // what the leader says
};
bool talk_has_choice(const Talk &t);   // accept / refuse, or just OK

// What one ruler thinks of another (after DOS MeetWithKing): fear of the
// other's defences, a demand built from where our attack beats them, and
// hostility when that pays.
struct Attitude {
    int fear = 0;
    int demand = 0;
    bool hostile = false;
    int tribute = 0;       // gold it would ask, in 50s
};

// The Civilization Score (DOS): every citizen counts, twice when happy and
// not at all when unhappy (worked out as if luxuries were at 40%); 20 for
// each Wonder of the World; 3 for each turn of world peace in AD, at most
// 100; 5 for each Future Technology; and the colonists' share when the
// spaceship lands. A conquest scores at least the conqueror's bonus.
struct ScoreBreakdown {
    int citizens = 0;       // size + happy - unhappy, summed over the cities
    int happy = 0, content = 0, unhappy = 0, specialists = 0;
    int wonders = 0;        // how many
    int wonder_points = 0;  // 20 each
    int peace = 0;
    int future = 0;
    int spaceship = 0;      // points the colonists bring (0 unless landed)
    int spaceship_possible = 0;  // what a ship in flight would bring
    int bonus = -1;         // conquest bonus, -1 none
    int total = 0;
};

// A proposal the player makes in Foreign Affairs.
enum class Proposal : std::uint8_t { Ceasefire, Peace, Embassy, CancelTreaty };

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
    std::vector<GameEvent> events;   // for the human player only; see EventKind
    CombatResult last_combat;
    int winner = -1;           // civ that won by conquest or by reaching Alpha Centauri
    int civil_wars = 0;
    bool space_win = false;    // the winner's colonists reached Alpha Centauri
    bool human_dead = false;
    bool all_ai = false;
    std::uint64_t seed = 1;
    // Wonders of the World: the city id holding each, -1 not built yet,
    // -2 lost with its city.
    std::array<int, kWonderCount> wonder_city{};
    std::array<std::uint32_t, kMaxCivs> wonder_started{};  // announced "is building" bits
    // The DOS game's catch-up step in research cost (0..6), worked out
    // every 50 turns once the calendar reaches AD: the most advances any
    // civilization knows, less the turn / 9.
    int tech_catchup = 0;
    // --- the end of the game (DOS)
    // Turns (the human's, in AD) since two peoples last fought: world peace
    // for the Civilization Score.
    int peace_turns = 0;
    bool game_ended = false;   // 2100 AD came or the ruler retired: the score is final
    bool retired = false;
    int end_turn_number = 0;   // the turn the game ended on (0 not yet)
    int opponents = 0;         // computer rivals at the start, for the conquest bonus

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
    // Distance from this city to its owner's Palace as corruption counts it:
    // 0 in the capital, kNoPalaceDistance at most (or with no Palace), and
    // kCommunismDistance everywhere under Communism.
    int palace_distance(const City &c) const;

    // --- knowledge
    bool has(int civ, Advance a) const { return a == Advance::None || civs[civ].techs[static_cast<int>(a)]; }
    int techs_known(int civ) const;
    bool can_research(int civ, Advance a) const;
    std::vector<Advance> researchable(int civ) const;
    // Research cost as in DOS Civilization 475.05 (see README):
    //   ratio = (human: 2 x level + 6, computer: 14 - level) + tech_catchup,
    //           for a human at least 11 - advances known;
    //   an advance is found when bulbs / m > advances known x ratio, with
    //   m = 1 before 1 AD and 2 after; the bulbs then start again from 0.
    // research_cost is the bulbs that takes: m x (known x ratio + 1), but
    // never fewer than kMinResearchCost (the FAQ's "at least 10").
    int research_ratio(int civ) const;
    int research_cost(int civ) const;
    static constexpr int kMinResearchCost = 10;   // no advance costs fewer bulbs
    void learn(int civ, Advance a);
    // The DOS game asks what to study only when an advance is found: once
    // bulbs are gathered toward one, the choice stands until it is found.
    // Surplus bulbs are lost when an advance is found (research starts
    // again from 0), and a human ruler loses the bulbs toward an advance
    // that arrives some other way (learn).
    bool can_change_research(int civ) const;
    bool set_research(int civ, Advance a);   // false: not open to them, or the choice already stands
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
    // `celebrating`: the city working the square holds a "We Love the King
    // Day" (DOS: no Despotism penalty; Monarchy and Communism get the
    // Republic's +1 trade).
    Yield tile_yield(int x, int y, int civ, bool centre, bool celebrating = false) const;
    bool tile_free_for(const City &c, int x, int y) const;
    void update_city(City &c);            // recompute stats (and arrange workers unless manual)
    void arrange_workers(City &c);
    // Everyone not on a worked square is a specialist: keep the taxmen and
    // scientists that still fit (none below size 5), the rest entertain.
    void settle_specialists(City &c) const;
    // Computer rulers get Civ-style handicaps: cheaper growth and builds on
    // harder levels, dearer on Chieftain (percent, 100 for humans).
    int ai_cost_percent(int civ) const;
    int food_box(const City &c) const { return std::max(4, (c.size + 1) * ai_cost_percent(c.owner) / 10); }
    int size_cap(const City &c) const { return c.has(Building::Aqueduct) ? 99 : 10; }
    // DOS buy price with r the shields still needed: units 2r + r*r/20,
    // improvements 2r, Wonders and spaceship parts 4r; twice that when the
    // box is empty.
    int buy_cost(const City &c) const;
    int production_cost(const City &c) const;
    // Empire size (DOS, human rulers only): with N our cities and
    //   r = (government / 2 + 2) x ((14 - 2 x level) / 2)
    // a city gets ((id mod r) + N - r) / r more unhappy citizens (never < 0).
    int empire_size_step(int civ) const;
    int empire_unhappy(const City &c) const;
    // A city celebrates "We Love the King Day" (DOS): size 3 or more, no
    // unhappy citizens, at least half (rounded up) happy, not in Anarchy.
    bool celebration_due(const City &c) const;
    // Gold upkeep of one building in a city (Barracks: 1, +1 from Prince,
    // +1 on Emperor, +1 each with Gunpowder and Combustion, as in DOS).
    int building_upkeep(int civ, int b) const;
    // One building a turn may be sold, for its cost in shields as gold.
    bool can_sell(const City &c, int b) const;
    bool sell_building(int city_id, int b);
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
    void declare_war(int a, int b);       // breaks any treaty: a grudge on b's side
    void make_peace(int a, int b);        // a peace treaty
    void make_ceasefire(int a, int b);
    Treaty treaty(int a, int b) const;
    bool has_treaty(int a, int b) const { return treaty(a, b) == Treaty::Ceasefire || treaty(a, b) == Treaty::Peace; }
    // A Republic or Democracy has a Senate: it accepts every cease-fire and
    // peace offered, and will not break a treaty.
    bool senate(int civ) const;
    bool senate_blocks(int a, int b) const { return senate(a) && has_treaty(a, b); }

    // --- the Civilization Score, the end in 2100 AD and retirement
    static constexpr int kEndYear = 2100;          // DOS: 2100 less 20 a level; here 2100 on every level
    static constexpr int kRetireWarningYears = 20; // "plans retirement in 20 years"
    static constexpr int kWonderPoints = 20;
    static constexpr int kFuturePoints = 5;
    static constexpr int kPeacePerTurn = 3;
    static constexpr int kPeaceMax = 100;
    ScoreBreakdown score(int civ);                 // works the cities out at 40% luxuries, then puts them back
    long long population(int civ) const;          // as the DOS game counts people
    void retire();                                 // the ruler steps down: the game ends now
    // No more turns: retired, conquered, destroyed or flown to the stars.
    // After 2100 AD the ruler may keep playing; the Hall of Fame keeps the
    // score the game ended with.
    bool over() const { return retired || winner >= 0 || human_dead; }

    // --- diplomacy (DOS MeetWithKing, approximated: see README)
    Attitude attitude(int ai, int other) const;
    int civ_score(int civ) const;          // DOS ranking points
    int civ_rank(int civ) const;           // 7 strongest, down to 0
    std::vector<Talk> talks;               // waiting for the human player, oldest first
    // The human player's answer to talks.front(): true accepts (pays,
    // gives, signs). Removes it and returns what happens.
    std::string answer_talk(bool accept);
    // The player asks a computer ruler; `accepted` reports the answer.
    std::string propose(int from, int to, Proposal p, bool *accepted = nullptr);
    std::string leader_greeting(int civ) const;
    // Civil war (DOS): a computer people with more than four cities that
    // loses its capital while stronger than the human player splits in
    // two. Returns the new civ index, -1 if none.
    int civil_war(int civ, int cap_x, int cap_y);
    bool at_war(int a, int b) const
    {
        return a != b && (civs[a].barbarian || civs[b].barbarian || ((civs[a].war >> b) & 1u));
    }
    void set_rates(int civ, int tax, int lux, int sci);
    // --- orders from the DOS Orders menu
    bool can_pillage(const Unit &u) const;
    bool pillage(int unit_id);               // mine/irrigation, else railroad, else road
    bool set_home_city(int unit_id);         // the city the unit stands in supports it
    int unload(int ship_id);                 // wakes the cargo; returns how many
    // --- nuclear attack (DOS): every unit on the target and the eight
    // squares around it dies, every city there loses half its people. A
    // city with SDI Defense within 3 squares of the target shoots it down.
    const City *sdi_cover(int attacker, int tx, int ty) const;
    MoveResult nuke(int unit_id, int tx, int ty);
    // --- natural disasters (DOS, after turn 50: two random city slots of
    // 128 each turn; size 5+, owner with more than one city).
    enum class Disaster : std::uint8_t { Earthquake, Plague, Flood, Volcano, Famine, Fire, Pirates, Count };
    bool disaster(City &c, Disaster d);      // true if it struck
    void natural_disasters();
    // Triremes of a human ruler that end the turn with no land next to them
    // are lost half the time (DOS).
    void triremes_at_sea(int civ);
    // The advance a conqueror takes from a captured city: the most advanced
    // one the loser knows and the winner does not (None if none).
    Advance capture_tech(int winner, int loser) const;
    void revolution(int civ, Government g);
    int anarchy_turns(int civ) const;            // DOS: to the next turn divisible by 4
    const char *leader_title(int civ) const;     // "King", "Consul"... by government
    bool buy(int city_id);
    void set_production(int city_id, bool unit, int what);
    void toggle_tile(int city_id, int dx, int dy);
    // Clicking specialist `index` (entertainers first, then taxmen, then
    // scientists) turns an entertainer into a taxman, a taxman into a
    // scientist and a scientist back into an entertainer. False, and no
    // change, in a city smaller than kSpecialistSize.
    static constexpr int kSpecialistSize = 5;
    bool cycle_specialist(int city_id, int index);

    // --- diplomats and caravans
    bool diplomat_can(int unit_id, int tx, int ty, DiploAction a) const;
    bool diplomat_hostile(DiploAction a) const
    {
        return a != DiploAction::Embassy && a != DiploAction::Investigate && a != DiploAction::MeetKing;
    }
    static bool diplomat_spent(DiploAction a) { return a != DiploAction::MeetKing && a != DiploAction::Bribe; }
    int incite_cost(const City &c, int civ) const;   // gold, -1 if it cannot be incited
    int bribe_cost(const Unit &u, int civ) const;
    // Carries out the action (see DiploAction for when the Diplomat is
    // spent). Returns what happened. Meet with King returns an empty string:
    // the audience itself is the window code's (Proposal, propose).
    std::string diplomat_act(int unit_id, int tx, int ty, DiploAction a);
    bool caravan_can_help(const Unit &u, const City &c) const;
    bool caravan_can_trade(const Unit &u, const City &c) const;
    int route_value(const City &a, const City &b) const;     // trade per turn added to a
    int trade_bonus(const Unit &u, const City &c) const;     // one-time gold and bulbs
    bool caravan_help(int unit_id, int city_id);
    int caravan_trade(int unit_id, int city_id);              // returns the bonus, -1 on failure

    // --- barbarians
    int barbarian_civ() const;                      // index, -1 none
    bool is_barbarian(int civ) const { return civ >= 0 && civ < static_cast<int>(civs.size()) && civs[civ].barbarian; }
    void spawn_barbarians(bool force_sea = false, bool force_land = false);
    int ransom(int unit_id, int tx, int ty);        // capture lone barbarian leaders: gold won
    // --- villages: what roll (0..3) gives for a unit of `owner` at x, y
    HutOutcome hut_outcome(int owner, int x, int y, int roll) const;

    // --- space race
    bool space_open() const;                        // the Apollo Program stands somewhere
    SpaceStats space_stats(int civ) const;
    bool can_launch(int civ) const;
    bool launch(int civ);
    int arrival_year(int civ) const;                // -1 if not in flight

    // --- turn
    void begin_human_phase();             // goto orders, sentries waking
    void end_turn();                      // computer players, then a new turn
    int next_unit_for(int civ, int after_id) const;
    void refresh_visibility();
    void check_elimination();

    // --- AI
    void ai_play(int civ);
    // Computer rulers overseas (ai_navy.cpp): passengers waiting for a ship
    // (at x, y, or anywhere with x < 0), a free site on another landmass,
    // and the ship or aircraft a city should build (-1 none).
    int ai_passengers(int civ, int x = -1, int y = -1) const;
    bool ai_overseas_site(int civ, int fx, int fy, int &sx, int &sy) const;
    int ai_navy_build(const City &c) const;
    // No free city site left on any landmass where the civ has a city.
    bool ai_boxed_in(int civ) const;

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
    void compute_city_stats(City &c);    // c.stats from its workers and specialists as they are
    void ai_specialists(City &c);
    void check_research(int civ);         // the advance under study is found
    void update_tech_catchup();
    void start_turn_for(int civ);
    void new_turn();
    void reveal(int civ, int x, int y, int r);
    void enter_hut(int unit_id);
    int make_city(int owner, int x, int y);   // a city with no Settlers (a hut's advanced tribe); its id
    void capture_city(City &c, int new_owner);
    std::string next_city_name(int civ);
    void set_tribe(int civ, int tribe);   // name, leader, colour and cities of a Civ 1 people
    void choose_research_ai(int civ);
    void complete_wonder(City &c, Wonder w);
    void announce_wonder_start(const City &c);
    void ai_city(City &c);
    void ai_unit(Unit &u);
    void ai_diplomacy(int civ);
    void meet(int ai, int other);          // one audience, if one is due
    void deliver(Talk t);                  // to the human's queue, or settled at once between computers
    bool settle(const Talk &t, bool accept, std::string *what);
    void ceasefires_end();
    bool ai_settler(Unit &u);
    void ai_envoy(Unit &u);    // Diplomats and Caravans
    // --- the computer rulers at sea and in the air (ai_navy.cpp)
    void ai_ship(int unit_id);
    void ai_ferry(int unit_id);
    void ai_sail_home(int unit_id);
    void ai_explore_sea(int unit_id);
    void ai_air(int unit_id);
    void ai_disembark(int unit_id);
    bool ai_seek_ship(int unit_id);
    bool ai_landing(const Unit &ship, int tx, int ty, int &ox, int &oy) const;
    const City *ai_overseas_enemy(int ci, int fx, int fy) const;
    bool ai_has_ferry(int ci) const;
    void ai_barbarians(int civ);
    bool barbarian_landing(int bc, UnitType ship, const std::vector<UnitType> &army);
    bool barbarian_uprising(int bc, const std::vector<UnitType> &army);
    void space_arrivals();
    void lose_spaceship(int civ, bool in_flight);
};

// Save and load as plain text. Version line first.
std::string save_game(const Game &g);
bool load_game(const std::string &text, Game &g, std::string *error = nullptr);

} // namespace civ
