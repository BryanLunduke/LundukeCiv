// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#include "databank.hpp"

#include "game.hpp"

#include <algorithm>

namespace civ {

namespace {

std::string list(const std::vector<std::string> &v)
{
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) s += (i ? ", " : "") + v[i];
    return s.empty() ? "nothing" : s;
}

std::string adv(Advance a) { return a == Advance::None ? "nothing (known from the start)" : advance_info(a).name; }

std::string yield(const Yield &y)
{
    return std::to_string(y.food) + " food, " + std::to_string(y.shields) + " shields, " + std::to_string(y.trade) + " trade";
}

const char *domain_name(Domain d) { return d == Domain::Land ? "Land" : (d == Domain::Sea ? "Sea" : "Air"); }

// What a building guards against (the DOS disasters).
const char *guards(Building b)
{
    switch (b) {
    case Building::Aqueduct: return "Prevents plague (until Medicine) and fire.";
    case Building::CityWalls: return "Prevents floods in cities beside a river.";
    case Building::Temple: return "Prevents volcanic eruptions near mountains.";
    case Building::Granary: return "Prevents famine.";
    case Building::Barracks: return "Prevents pirate raids on coastal cities.";
    default: return nullptr;
    }
}

} // namespace

const char *databank_topic_name(DatabankTopic t)
{
    static const char *names[] = {"Units", "Advances", "Improvements", "Wonders", "Governments", "Terrain"};
    return names[static_cast<int>(t)];
}

std::vector<DatabankEntry> databank(DatabankTopic topic)
{
    std::vector<DatabankEntry> out;
    switch (topic) {
    case DatabankTopic::Units:
        for (int u = 0; u < kUnitTypeCount; ++u) {
            const UnitInfo &i = unit_info(u);
            if (i.flags & kNoBuild) continue;
            DatabankEntry e;
            e.name = i.name;
            e.facts.push_back(std::string("Type: ") + domain_name(i.domain) + " unit");
            e.facts.push_back("Attack " + std::to_string(i.attack) + ", defense " + std::to_string(i.defense) + ", moves " +
                              std::to_string(i.moves));
            e.facts.push_back("Cost: " + std::to_string(i.cost) + " shields");
            e.facts.push_back("Requires: " + adv(i.tech));
            if (i.obsolete != Advance::None) e.facts.push_back(std::string("Obsolete with: ") + advance_info(i.obsolete).name);
            if (i.capacity) e.facts.push_back("Carries " + std::to_string(i.capacity) + ((i.flags & kCarriesAir) ? " air units" : " land units"));
            if (i.fuel) e.facts.push_back("Must return to a city or Carrier every " + std::to_string(i.fuel) + (i.fuel == 1 ? " turn" : " turns"));
            if (i.flags & kSettlers) e.facts.push_back("Founds and joins cities; builds roads, railroads, irrigation, mines and fortresses");
            if (i.flags & kIgnoreZoc) e.facts.push_back("Ignores zones of control");
            if (i.flags & kIgnoreWalls) e.facts.push_back("Ignores City Walls");
            if (i.flags & kSight2) e.facts.push_back("Sees two squares away");
            if (i.flags & kNoLandAttack) e.facts.push_back("Cannot attack land squares");
            if (i.flags & kCoastal) e.facts.push_back("May leave the coast, but a Trireme that ends its turn with no land beside it is lost half the time");
            if (i.flags & kNuclear)
                e.facts.push_back("Destroys every unit on and around its target; cities there lose half their people. "
                                  "SDI Defense within 3 squares shoots it down");
            if (i.domain == Domain::Air && i.attack > 0) e.facts.push_back("In flight it can be attacked only by Fighters");
            if (u == static_cast<int>(UnitType::Fighter)) e.facts.push_back("The only unit that can attack aircraft in flight");
            if (u == static_cast<int>(UnitType::Diplomat)) e.facts.push_back("Establishes embassies, investigates cities, steals advances, sabotages, incites revolt, bribes units");
            if (u == static_cast<int>(UnitType::Caravan)) e.facts.push_back("Helps build Wonders; opens trade routes worth gold and science");
            if (i.domain == Domain::Land && u != static_cast<int>(UnitType::Diplomat) && u != static_cast<int>(UnitType::Caravan))
                e.facts.push_back("Can pillage improvements outside cities (Shift+P)");
            out.push_back(e);
        }
        break;
    case DatabankTopic::Advances:
        for (int a = 0; a < kAdvanceCount; ++a) {
            const AdvanceInfo &i = advance_info(a);
            DatabankEntry e;
            e.name = i.name;
            std::vector<std::string> req;
            if (i.req1 != Advance::None) req.push_back(advance_info(i.req1).name);
            if (i.req2 != Advance::None) req.push_back(advance_info(i.req2).name);
            e.facts.push_back("Requires: " + list(req));
            std::vector<std::string> leads, allows;
            for (int b = 0; b < kAdvanceCount; ++b) {
                const AdvanceInfo &o = advance_info(b);
                if (o.req1 == static_cast<Advance>(a) || o.req2 == static_cast<Advance>(a)) leads.push_back(o.name);
            }
            for (int u = 0; u < kUnitTypeCount; ++u) {
                if (unit_info(u).tech == static_cast<Advance>(a) && !(unit_info(u).flags & kNoBuild)) allows.push_back(unit_info(u).name);
            }
            for (int b = 0; b < kBuildingCount; ++b) {
                if (building_info(b).tech == static_cast<Advance>(a)) allows.push_back(building_info(b).name);
            }
            for (int w = 0; w < kWonderCount; ++w) {
                if (wonder_info(w).tech == static_cast<Advance>(a)) allows.push_back(wonder_info(w).name);
            }
            for (int gv = 0; gv < static_cast<int>(Government::Count); ++gv) {
                const GovernmentInfo &gi = government_info(static_cast<Government>(gv));
                if (gi.tech == static_cast<Advance>(a)) allows.push_back(gi.name);
            }
            e.facts.push_back("Leads to: " + list(leads));
            e.facts.push_back("Allows: " + list(allows));
            std::vector<std::string> ends;
            for (int u = 0; u < kUnitTypeCount; ++u) {
                if (unit_info(u).obsolete == static_cast<Advance>(a)) ends.push_back(unit_info(u).name);
            }
            for (int w = 0; w < kWonderCount; ++w) {
                if (wonder_info(w).obsolete == static_cast<Advance>(a)) ends.push_back(wonder_info(w).name);
            }
            if (a == static_cast<int>(Advance::Gunpowder) || a == static_cast<int>(Advance::Combustion)) ends.push_back("existing Barracks");
            if (!ends.empty()) e.facts.push_back("Makes obsolete: " + list(ends));
            if (a == static_cast<int>(Advance::Medicine)) e.facts.push_back("Ends plague in cities without an Aqueduct");
            if (a == static_cast<int>(Advance::FutureTech)) e.facts.push_back("May be studied again and again; each adds to the score");
            out.push_back(e);
        }
        break;
    case DatabankTopic::Improvements:
        for (int b = 1; b < kBuildingCount; ++b) {
            const BuildingInfo &i = building_info(b);
            DatabankEntry e;
            e.name = i.name;
            e.facts.push_back("Cost: " + std::to_string(i.cost) + " shields; sells for " + std::to_string(i.cost) + " gold (one sale per city a turn)");
            if (b == static_cast<int>(Building::Barracks))
                e.facts.push_back("Upkeep: 1 gold, +1 from Prince level, +1 on Emperor, +1 each once Gunpowder and Combustion are known");
            else
                e.facts.push_back("Upkeep: " + std::to_string(i.upkeep) + " gold a turn");
            e.facts.push_back("Requires: " + adv(i.tech));
            e.facts.push_back(std::string("Effect: ") + i.effect);
            if (const char *gd = guards(static_cast<Building>(b))) e.facts.push_back(gd);
            if (b == static_cast<int>(Building::SdiDefense)) e.facts.push_back("Shoots down any enemy Nuclear attack aimed within 3 squares");
            out.push_back(e);
        }
        {
            const BuildingInfo &p = building_info(0);
            DatabankEntry e;
            e.name = p.name;
            e.facts.push_back("Cost: " + std::to_string(p.cost) + " shields; cannot be sold");
            e.facts.push_back(std::string("Effect: ") + p.effect);
            e.facts.push_back("Losing the capital of a large people can bring civil war");
            out.push_back(e);
        }
        break;
    case DatabankTopic::Wonders:
        for (int w = 0; w < kWonderCount; ++w) {
            const WonderInfo &i = wonder_info(w);
            DatabankEntry e;
            e.name = i.name;
            e.facts.push_back("Cost: " + std::to_string(i.cost) + " shields; one in the whole world");
            e.facts.push_back("Requires: " + adv(i.tech));
            if (i.obsolete != Advance::None) e.facts.push_back(std::string("Obsolete with: ") + advance_info(i.obsolete).name);
            e.facts.push_back(std::string("Effect: ") + i.effect);
            if (w == static_cast<int>(Wonder::Pyramids)) e.facts.push_back("Anarchy lasts a single turn");
            if (w == static_cast<int>(Wonder::GreatWall) || w == static_cast<int>(Wonder::UnitedNations))
                e.facts.push_back("Foreign rulers do not turn hostile toward its owner");
            out.push_back(e);
        }
        break;
    case DatabankTopic::Governments:
        for (int gv = 0; gv < static_cast<int>(Government::Count); ++gv) {
            const Government g = static_cast<Government>(gv);
            const GovernmentInfo &i = government_info(g);
            DatabankEntry e;
            e.name = i.name;
            e.facts.push_back("Requires: " + adv(i.tech));
            e.facts.push_back(i.max_rate ? "Highest rate: " + std::to_string(i.max_rate * 10) + "%" : std::string("No taxes or science: all trade is luxury"));
            e.facts.push_back("Settlers eat " + std::to_string(i.settler_food) + " food");
            e.facts.push_back(i.free_units < 0 ? std::string("Units free of shield upkeep: as many as the city's size")
                                               : "Units free of shield upkeep: " + std::to_string(i.free_units));
            const int step3 = (gv / 2 + 2) * ((14 - 2 * 2) / 2);
            e.facts.push_back("Empire size: unhappiness grows every " + std::to_string(step3) + " cities (Prince level)");
            switch (g) {
            case Government::Anarchy:
                e.facts.push_back("Lasts until the next turn divisible by 4 (1 to 4 turns; 1 with the Pyramids)");
                e.facts.push_back("-1 on squares yielding 3 or more; no celebrations");
                break;
            case Government::Despotism:
                e.facts.push_back("-1 on squares yielding 3 or more (not while celebrating)");
                e.facts.push_back("Up to 3 units in a city keep order");
                break;
            case Government::Monarchy:
            case Government::Communism:
                e.facts.push_back("Up to 3 units in a city keep order");
                e.facts.push_back("While celebrating: +1 trade on squares with trade");
                if (g == Government::Communism) e.facts.push_back("Corruption is the same in every city, whatever the distance to the Palace");
                break;
            case Government::Republic:
            case Government::Democracy:
                e.facts.push_back("+1 trade on squares with trade");
                e.facts.push_back(g == Government::Republic ? "1 unhappy citizen for each attacking unit away from home"
                                                            : "2 unhappy citizens for each attacking unit away from home");
                e.facts.push_back("While celebrating, cities grow by 1 each turn they have spare food");
                e.facts.push_back("Senate: accepts every cease-fire and peace offered; forbids breaking a treaty");
                if (g == Government::Democracy) {
                    e.facts.push_back("No corruption");
                    e.facts.push_back("A city in disorder two turns running brings the government down into Anarchy");
                }
                break;
            default: break;
            }
            out.push_back(e);
        }
        break;
    case DatabankTopic::Terrain:
        for (int t = 0; t < kTerrainCount; ++t) {
            const TerrainInfo &i = terrain_info(static_cast<Terrain>(t));
            DatabankEntry e;
            e.name = i.name;
            e.facts.push_back("Movement cost: " + std::to_string(i.move) + "; defense +" + std::to_string(i.defense_pct - 100) + "%");
            e.facts.push_back("Yields: " + yield(i.base));
            if (i.special && *i.special) e.facts.push_back(std::string("With ") + i.special + ": " + yield(i.with_special));
            if (i.irrigate_turns) {
                if (i.irrigate_to == static_cast<Terrain>(t))
                    e.facts.push_back("Irrigation: +" + std::to_string(i.irrigate_food) + " food (" + std::to_string(i.irrigate_turns) + " turns)");
                else
                    e.facts.push_back(std::string("Irrigation turns it into ") + terrain_info(i.irrigate_to).name + " (" +
                                      std::to_string(i.irrigate_turns) + " turns)");
            }
            if (i.mine_turns) {
                if (i.mine_to == static_cast<Terrain>(t))
                    e.facts.push_back("Mine: +" + std::to_string(i.mine_shields) + " shields (" + std::to_string(i.mine_turns) + " turns)");
                else
                    e.facts.push_back(std::string("Mining turns it into ") + terrain_info(i.mine_to).name + " (" + std::to_string(i.mine_turns) +
                                      " turns)");
            }
            if (i.road_turns) e.facts.push_back("Road: " + std::to_string(i.road_turns) + " turns" + (i.road_trade ? ", +1 trade" : ""));
            if (t == static_cast<int>(Terrain::Hills)) e.facts.push_back("A city beside hills may suffer earthquakes");
            if (t == static_cast<int>(Terrain::River)) e.facts.push_back("A city beside a river may suffer floods");
            if (t == static_cast<int>(Terrain::Mountains)) e.facts.push_back("A city beside mountains may suffer volcanic eruptions");
            if (t == static_cast<int>(Terrain::Ocean)) e.facts.push_back("A coastal city without Barracks may be raided by pirates");
            out.push_back(e);
        }
        break;
    default: break;
    }
    return out;
}

} // namespace civ
