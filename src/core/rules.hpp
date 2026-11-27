// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

// The rules tables: terrain, units, city improvements, advances and
// governments, following the original 1991 DOS game's published numbers.

#include <cstdint>
#include <string>

namespace civ {

enum class Terrain : std::uint8_t {
    Arctic, Desert, Forest, Grassland, Hills, Jungle, Mountains, Ocean, Plains, River, Swamp, Tundra, Count
};
constexpr int kTerrainCount = static_cast<int>(Terrain::Count);

struct Yield {
    int food = 0;
    int shields = 0;
    int trade = 0;
};

struct TerrainInfo {
    const char *name;
    const char *special;   // name of the special resource found on it
    int move;              // movement points to enter
    int defense_pct;       // 100, 150, 200, 300
    Yield base;
    Yield with_special;
    int irrigate_food;     // food added by irrigation (when it does not transform)
    Terrain irrigate_to;   // == self when irrigation adds food
    int irrigate_turns;    // 0: cannot irrigate
    int mine_shields;
    Terrain mine_to;
    int mine_turns;        // 0: cannot mine
    int road_trade;        // trade added by a road
    int road_turns;
    int fortress_turns;
};

const TerrainInfo &terrain_info(Terrain t);

enum class Advance : std::uint8_t {
    AdvancedFlight, Alphabet, Astronomy, AtomicTheory, Automobile, Banking, BridgeBuilding, BronzeWorking,
    CeremonialBurial, Chemistry, Chivalry, CodeOfLaws, Combustion, Communism, Computers, Conscription,
    Construction, Corporation, Currency, Democracy, Electricity, Electronics, Engineering, Explosives,
    Feudalism, Flight, FusionPower, FutureTech, GeneticEngineering, Gunpowder, HorsebackRiding,
    Industrialization, Invention, IronWorking, LaborUnion, Literacy, Magnetism, MapMaking, Masonry,
    MassProduction, Mathematics, Medicine, Metallurgy, Monarchy, Mysticism, Navigation, NuclearFission,
    NuclearPower, Philosophy, Physics, Plastics, Pottery, Railroad, Refining, Religion,
    Republic, Robotics, Rocketry, SpaceFlight, SteamEngine, Steel, Superconductor, TheoryOfGravity,
    Trade, University, TheWheel, Writing,
    Count,
    None = 255,
};
constexpr int kAdvanceCount = static_cast<int>(Advance::Count);

struct AdvanceInfo {
    const char *name;
    Advance req1;
    Advance req2;
};
const AdvanceInfo &advance_info(Advance a);
inline const AdvanceInfo &advance_info(int a) { return advance_info(static_cast<Advance>(a)); }

enum class Domain : std::uint8_t { Land, Sea, Air };

enum UnitFlag : unsigned {
    kSettlers = 1u << 0,
    kIgnoreZoc = 1u << 1,
    kIgnoreWalls = 1u << 2,
    kSight2 = 1u << 3,
    kCoastal = 1u << 4,   // trireme: stays next to land
    kNoLandAttack = 1u << 5,
    kCarriesAir = 1u << 6,
    kNonMilitary = 1u << 7,
    kNuclear = 1u << 8,
    kNoBuild = 1u << 9,   // never built in a city (the barbarian Leader)
};

enum class UnitType : std::uint8_t {
    Settlers, Militia, Phalanx, Legion, Chariot, Cavalry, Knights, Catapult, Musketeers, Riflemen, Cannon,
    Artillery, Armor, MechInf, Diplomat, Caravan,
    Trireme, Sail, Frigate, Ironclad, Transport, Cruiser, Battleship, Carrier, Submarine,
    Fighter, Bomber, Nuclear,
    Leader,     // a barbarian chieftain: never built, ransomed when caught alone
    Count,
};
constexpr int kUnitTypeCount = static_cast<int>(UnitType::Count);

struct UnitInfo {
    const char *name;
    Domain domain;
    int attack;
    int defense;
    int moves;
    int cost;          // shields
    Advance tech;      // None: available from the start
    Advance obsolete;  // None: never
    unsigned flags;
    int capacity;      // units it can carry
    int fuel;          // air units: turns aloft
};
const UnitInfo &unit_info(UnitType u);
inline const UnitInfo &unit_info(int u) { return unit_info(static_cast<UnitType>(u)); }

enum class Building : std::uint8_t {
    Palace, Barracks, Granary, Temple, Marketplace, Library, Courthouse, CityWalls, Aqueduct, Colosseum,
    Cathedral, University, Bank, Factory, PowerPlant, HydroPlant, NuclearPlant, MfgPlant, SdiDefense,
    Count,
};
constexpr int kBuildingCount = static_cast<int>(Building::Count);

struct BuildingInfo {
    const char *name;
    int cost;
    int upkeep;
    Advance tech;
    const char *effect;
};
const BuildingInfo &building_info(Building b);

// The 21 Wonders of the World. Each is built once in the whole world; a
// city builds one as an improvement whose index is kWonderBase + wonder.
enum class Wonder : std::uint8_t {
    Colossus, Pyramids, GreatWall, HangingGardens, Lighthouse, GreatLibrary, Oracle,
    Copernicus, Michelangelo, Shakespeare, Bach, Magellan, Newton, Darwin,
    Suffrage, HooverDam, Manhattan, UnitedNations, Apollo, Seti, CureForCancer,
    Count,
};
constexpr int kWonderCount = static_cast<int>(Wonder::Count);
constexpr int kWonderBase = 64;
inline bool is_wonder_build(int what) { return what >= kWonderBase && what < kWonderBase + kWonderCount; }

struct WonderInfo {
    const char *name;
    int cost;
    Advance tech;
    Advance obsolete;   // None: never
    const char *effect;
};
const WonderInfo &wonder_info(Wonder w);
inline const WonderInfo &wonder_info(int w) { return wonder_info(static_cast<Wonder>(w)); }
// "the Pyramids", "Copernicus' Observatory": the name as used in a sentence.
std::string wonder_title(int w);

// Spaceship parts (the space race): built like improvements once the
// Apollo Program stands anywhere, with index kSpaceBase + part.
enum class SpacePart : std::uint8_t { Structural, Component, Module, Count };
constexpr int kSpacePartCount = static_cast<int>(SpacePart::Count);
constexpr int kSpaceBase = 96;
inline bool is_space_build(int what) { return what >= kSpaceBase && what < kSpaceBase + kSpacePartCount; }
constexpr int kMaxStructurals = 39;
constexpr int kMaxComponents = 16;   // fuel and propulsion, in turn
constexpr int kMaxModules = 12;      // habitation, life support, solar panels, in turn

// Buildings by index; wonder indices (kWonderBase + w) give the wonder's
// name, cost and effect with no upkeep.
const BuildingInfo &building_info(int b);

enum class Government : std::uint8_t { Anarchy, Despotism, Monarchy, Communism, Republic, Democracy, Count };
struct GovernmentInfo {
    const char *name;
    Advance tech;
    int max_rate;          // tenths for any one of tax/lux/sci
    int settler_food;
    int free_units;        // -1: city size
};
const GovernmentInfo &government_info(Government g);

// Corruption, exactly as DOS Civilization 475.05 works it out (see README):
//   corruption = trade * distance * 3 / (20 * government + 80)
// with government numbered Anarchy 0, Despotism 1, Monarchy 2, Communism 3,
// Republic 4 (Democracy has none), integer division, then halved (rounded
// down) by a Courthouse. `distance` is to the Palace (dos_distance, 0 in the
// capital, 32 with no Palace); under Communism it is always 10.
constexpr int kNoPalaceDistance = 32;
constexpr int kCommunismDistance = 10;
int dos_corruption(int trade, int distance, Government g, bool courthouse);

enum class Difficulty : std::uint8_t { Chieftain, Warlord, Prince, King, Emperor, Count };
const char *difficulty_name(Difficulty d);
int content_base(Difficulty d);

// Calendar: turn 1 is 4000 BC, 20 years a turn to 1000 AD, then 10, 5, 2
// and 1 (there is no year 0).
int year_for_turn(int turn);
std::string year_text(int year);

} // namespace civ
