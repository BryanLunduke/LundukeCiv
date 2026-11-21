// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#include "rules.hpp"

namespace civ {

namespace {
using T = Terrain;
// name, special, move, defense, base, special, irr food, irr to, irr turns, mine shields, mine to, mine turns,
// road trade, road turns, fortress turns
const TerrainInfo kTerrain[kTerrainCount] = {
    {"Arctic", "Seals", 2, 100, {0, 0, 0}, {2, 0, 0}, 0, T::Arctic, 0, 0, T::Arctic, 0, 0, 4, 6},
    {"Desert", "Oasis", 1, 100, {0, 1, 0}, {3, 1, 0}, 1, T::Desert, 5, 1, T::Desert, 5, 1, 2, 5},
    {"Forest", "Game", 2, 150, {1, 2, 0}, {3, 2, 0}, 0, T::Plains, 5, 0, T::Forest, 0, 0, 4, 6},
    {"Grassland", "Shield", 1, 100, {2, 0, 0}, {2, 1, 0}, 1, T::Grassland, 5, 0, T::Forest, 10, 1, 2, 5},
    {"Hills", "Coal", 2, 200, {1, 0, 0}, {1, 2, 0}, 1, T::Hills, 10, 3, T::Hills, 10, 0, 4, 6},
    {"Jungle", "Gems", 2, 150, {1, 0, 0}, {1, 0, 4}, 0, T::Grassland, 15, 0, T::Forest, 15, 0, 4, 6},
    {"Mountains", "Gold", 3, 300, {0, 1, 0}, {0, 1, 6}, 0, T::Mountains, 0, 1, T::Mountains, 10, 0, 6, 7},
    {"Ocean", "Fish", 1, 100, {1, 0, 2}, {3, 0, 2}, 0, T::Ocean, 0, 0, T::Ocean, 0, 0, 0, 0},
    {"Plains", "Horses", 1, 100, {1, 1, 0}, {1, 3, 0}, 1, T::Plains, 5, 0, T::Forest, 15, 1, 2, 5},
    {"River", "Shield", 1, 150, {2, 0, 1}, {2, 1, 1}, 1, T::River, 5, 0, T::River, 0, 0, 2, 5},
    {"Swamp", "Oil", 2, 150, {1, 0, 0}, {1, 4, 0}, 0, T::Grassland, 15, 0, T::Forest, 15, 0, 4, 6},
    {"Tundra", "Game", 1, 100, {1, 0, 0}, {3, 0, 0}, 0, T::Tundra, 0, 0, T::Tundra, 0, 0, 2, 5},
};

using A = Advance;
const AdvanceInfo kAdvances[kAdvanceCount] = {
    {"Advanced Flight", A::Flight, A::Electricity},
    {"Alphabet", A::None, A::None},
    {"Astronomy", A::Mysticism, A::Mathematics},
    {"Atomic Theory", A::TheoryOfGravity, A::Physics},
    {"Automobile", A::Combustion, A::Steel},
    {"Banking", A::Republic, A::Trade},
    {"Bridge Building", A::IronWorking, A::Construction},
    {"Bronze Working", A::None, A::None},
    {"Ceremonial Burial", A::None, A::None},
    {"Chemistry", A::Medicine, A::University},
    {"Chivalry", A::HorsebackRiding, A::Feudalism},
    {"Code of Laws", A::Alphabet, A::None},
    {"Combustion", A::Refining, A::Explosives},
    {"Communism", A::Philosophy, A::Industrialization},
    {"Computers", A::Electronics, A::Mathematics},
    {"Conscription", A::Republic, A::Explosives},
    {"Construction", A::Currency, A::Masonry},
    {"Corporation", A::Banking, A::Industrialization},
    {"Currency", A::BronzeWorking, A::None},
    {"Democracy", A::Philosophy, A::Literacy},
    {"Electricity", A::Metallurgy, A::Magnetism},
    {"Electronics", A::Engineering, A::Electricity},
    {"Engineering", A::TheWheel, A::Construction},
    {"Explosives", A::Gunpowder, A::Chemistry},
    {"Feudalism", A::Monarchy, A::Masonry},
    {"Flight", A::Combustion, A::Physics},
    {"Fusion Power", A::NuclearPower, A::Superconductor},
    {"Future Tech", A::FusionPower, A::None},
    {"Genetic Engineering", A::Corporation, A::Medicine},
    {"Gunpowder", A::IronWorking, A::Invention},
    {"Horseback Riding", A::None, A::None},
    {"Industrialization", A::Banking, A::Railroad},
    {"Invention", A::Engineering, A::Literacy},
    {"Iron Working", A::BronzeWorking, A::None},
    {"Labor Union", A::MassProduction, A::Communism},
    {"Literacy", A::CodeOfLaws, A::Writing},
    {"Magnetism", A::Navigation, A::Physics},
    {"Map Making", A::Alphabet, A::None},
    {"Masonry", A::None, A::None},
    {"Mass Production", A::Corporation, A::Automobile},
    {"Mathematics", A::Alphabet, A::Masonry},
    {"Medicine", A::Philosophy, A::Trade},
    {"Metallurgy", A::Gunpowder, A::University},
    {"Monarchy", A::CodeOfLaws, A::CeremonialBurial},
    {"Mysticism", A::CeremonialBurial, A::None},
    {"Navigation", A::Astronomy, A::MapMaking},
    {"Nuclear Fission", A::MassProduction, A::AtomicTheory},
    {"Nuclear Power", A::Electronics, A::NuclearFission},
    {"Philosophy", A::Mysticism, A::Literacy},
    {"Physics", A::Navigation, A::Mathematics},
    {"Plastics", A::Refining, A::SpaceFlight},
    {"Pottery", A::None, A::None},
    {"Railroad", A::BridgeBuilding, A::SteamEngine},
    {"Recycling", A::MassProduction, A::Democracy},
    {"Refining", A::Corporation, A::Chemistry},
    {"Religion", A::Philosophy, A::Writing},
    {"The Republic", A::CodeOfLaws, A::Literacy},
    {"Robotics", A::Computers, A::Plastics},
    {"Rocketry", A::Electronics, A::AdvancedFlight},
    {"Space Flight", A::Computers, A::Rocketry},
    {"Steam Engine", A::Invention, A::Physics},
    {"Steel", A::Metallurgy, A::Industrialization},
    {"Superconductor", A::MassProduction, A::Plastics},
    {"Theory of Gravity", A::Astronomy, A::University},
    {"Trade", A::CodeOfLaws, A::Currency},
    {"University", A::Philosophy, A::Mathematics},
    {"The Wheel", A::None, A::None},
    {"Writing", A::Alphabet, A::None},
};

using D = Domain;
// name, domain, att, def, moves, cost, tech, obsolete, flags, capacity, fuel
const UnitInfo kUnits[kUnitTypeCount] = {
    {"Settlers", D::Land, 0, 1, 1, 40, A::None, A::None, kSettlers | kNonMilitary, 0, 0},
    {"Militia", D::Land, 1, 1, 1, 10, A::None, A::Gunpowder, 0, 0, 0},
    {"Phalanx", D::Land, 1, 2, 1, 20, A::BronzeWorking, A::Gunpowder, 0, 0, 0},
    {"Legion", D::Land, 3, 1, 1, 20, A::IronWorking, A::Conscription, 0, 0, 0},
    {"Chariot", D::Land, 4, 1, 2, 40, A::TheWheel, A::Chivalry, 0, 0, 0},
    {"Cavalry", D::Land, 2, 1, 2, 20, A::HorsebackRiding, A::Conscription, 0, 0, 0},
    {"Knights", D::Land, 4, 2, 2, 40, A::Chivalry, A::Automobile, 0, 0, 0},
    {"Catapult", D::Land, 6, 1, 1, 40, A::Mathematics, A::Metallurgy, 0, 0, 0},
    {"Musketeers", D::Land, 2, 3, 1, 30, A::Gunpowder, A::Conscription, 0, 0, 0},
    {"Riflemen", D::Land, 3, 5, 1, 30, A::Conscription, A::None, 0, 0, 0},
    {"Cannon", D::Land, 8, 1, 1, 40, A::Metallurgy, A::Robotics, 0, 0, 0},
    {"Artillery", D::Land, 12, 2, 2, 60, A::Robotics, A::None, kIgnoreWalls, 0, 0},
    {"Armor", D::Land, 10, 5, 3, 80, A::Automobile, A::None, 0, 0, 0},
    {"Mech. Inf.", D::Land, 6, 6, 3, 50, A::LaborUnion, A::None, 0, 0, 0},
    {"Diplomat", D::Land, 0, 0, 2, 30, A::Writing, A::None, kIgnoreZoc | kNonMilitary, 0, 0},
    {"Caravan", D::Land, 0, 1, 1, 50, A::Trade, A::None, kIgnoreZoc | kNonMilitary, 0, 0},
    {"Trireme", D::Sea, 1, 0, 3, 40, A::MapMaking, A::Navigation, kCoastal, 2, 0},
    {"Sail", D::Sea, 1, 1, 3, 40, A::Navigation, A::Magnetism, 0, 3, 0},
    {"Frigate", D::Sea, 2, 2, 3, 40, A::Magnetism, A::Industrialization, 0, 4, 0},
    {"Ironclad", D::Sea, 4, 4, 4, 60, A::SteamEngine, A::Combustion, 0, 0, 0},
    {"Transport", D::Sea, 0, 3, 4, 50, A::Industrialization, A::None, kNonMilitary, 8, 0},
    {"Cruiser", D::Sea, 6, 6, 6, 80, A::Combustion, A::None, kSight2, 0, 0},
    {"Battleship", D::Sea, 18, 12, 4, 160, A::Steel, A::None, kSight2, 0, 0},
    {"Carrier", D::Sea, 1, 12, 5, 160, A::AdvancedFlight, A::None, kSight2 | kCarriesAir, 8, 0},
    {"Submarine", D::Sea, 8, 2, 3, 50, A::MassProduction, A::None, kSight2 | kNoLandAttack, 0, 0},
    {"Fighter", D::Air, 4, 2, 10, 60, A::Flight, A::None, 0, 0, 1},
    {"Bomber", D::Air, 12, 1, 8, 120, A::AdvancedFlight, A::None, kIgnoreWalls | kSight2, 0, 2},
    {"Nuclear", D::Air, 99, 0, 16, 160, A::Rocketry, A::None, kNuclear | kIgnoreZoc, 0, 1},
};

const BuildingInfo kBuildings[kBuildingCount] = {
    {"Palace", 200, 0, A::Masonry, "Capital: less corruption in cities close to it."},
    {"Barracks", 40, 1, A::None, "New units are veterans."},
    {"Granary", 60, 1, A::Pottery, "Keeps half the food when the city grows."},
    {"Temple", 40, 1, A::CeremonialBurial, "1 unhappy citizen content (2 with Mysticism)."},
    {"Marketplace", 80, 1, A::Currency, "Tax and luxuries +50%."},
    {"Library", 80, 1, A::Writing, "Science +50%."},
    {"Courthouse", 80, 1, A::CodeOfLaws, "Corruption -50%."},
    {"City Walls", 120, 2, A::Masonry, "Defense x3 against land attack; no population loss."},
    {"Aqueduct", 120, 2, A::Construction, "Lets the city grow past size 10."},
    {"Colosseum", 100, 4, A::Construction, "3 unhappy citizens content."},
    {"Cathedral", 160, 3, A::Religion, "4 unhappy citizens content."},
    {"University", 160, 3, A::University, "Science +50% (needs a Library)."},
    {"Bank", 120, 3, A::Banking, "Tax and luxuries +50% (needs a Marketplace)."},
    {"Factory", 200, 4, A::Industrialization, "Shields +50%."},
    {"Power Plant", 160, 4, A::Refining, "Shields +50% (needs a Factory)."},
    {"Hydro Plant", 240, 4, A::Electronics, "Shields +50% (needs a Factory)."},
    {"Nuclear Plant", 160, 2, A::NuclearPower, "Shields +50% (needs a Factory)."},
    {"Mass Transit", 160, 4, A::MassProduction, "No pollution from population."},
    {"Mfg. Plant", 320, 6, A::Robotics, "Shields +100% (needs a Factory)."},
    {"Recycling Center", 200, 2, A::Recycling, "Industrial pollution -66%."},
    {"SDI Defense", 200, 4, A::Superconductor, "Protects the city from nuclear attack."},
};

const GovernmentInfo kGovernments[static_cast<int>(Government::Count)] = {
    {"Anarchy", A::None, 0, 40, 1, -1},
    {"Despotism", A::None, 6, 40, 1, -1},
    {"Monarchy", A::Monarchy, 7, 25, 1, 3},
    {"Communism", A::Communism, 8, 0, 1, 3},
    {"Republic", A::Republic, 8, 15, 2, 0},
    {"Democracy", A::Democracy, 9, 0, 2, 0},
};
} // namespace

const TerrainInfo &terrain_info(Terrain t) { return kTerrain[static_cast<int>(t)]; }
const AdvanceInfo &advance_info(Advance a) { return kAdvances[static_cast<int>(a)]; }
const UnitInfo &unit_info(UnitType u) { return kUnits[static_cast<int>(u)]; }
const BuildingInfo &building_info(Building b) { return kBuildings[static_cast<int>(b)]; }
const GovernmentInfo &government_info(Government g) { return kGovernments[static_cast<int>(g)]; }

const char *difficulty_name(Difficulty d)
{
    static const char *n[] = {"Chieftain", "Warlord", "Prince", "King", "Emperor"};
    return n[static_cast<int>(d)];
}

int content_base(Difficulty d)
{
    static const int n[] = {6, 5, 4, 3, 2};
    return n[static_cast<int>(d)];
}

int year_for_turn(int turn)
{
    int year = -4000;
    for (int t = 1; t < turn; ++t) {
        if (year < 1000) {
            if (year == -20) {
                year = 1;
            } else if (year == 1) {
                year = 20;
            } else {
                year += 20;
            }
        } else if (year < 1500) {
            year += 10;
        } else if (year < 1750) {
            year += 5;
        } else if (year < 1850) {
            year += 2;
        } else {
            year += 1;
        }
    }
    return year;
}

std::string year_text(int year)
{
    return year < 0 ? std::to_string(-year) + " BC" : std::to_string(year) + " AD";
}

} // namespace civ
