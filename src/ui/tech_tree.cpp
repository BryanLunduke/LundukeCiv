// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#include "tech_tree.hpp"

#include "draw.hpp"

#include <string>
#include <vector>

namespace ui {
namespace {

using civ::Advance;
using civ::Building;
using civ::Game;
using civ::Government;
using civ::UnitType;
using civ::Wonder;
using civ::advance_info;
using civ::building_info;
using civ::government_info;
using civ::kAdvanceCount;
using civ::kBuildingCount;
using civ::kUnitTypeCount;
using civ::kWonderCount;
using civ::unit_info;
using civ::wonder_info;
using civ::wonder_title;

const char *kCategoryByAdvance[kAdvanceCount] = {
    /* AdvancedFlight */ "Exploration",
    /* Alphabet */ "Science",
    /* Astronomy */ "Exploration",
    /* AtomicTheory */ "Science",
    /* Automobile */ "Exploration",
    /* Banking */ "Economy",
    /* BridgeBuilding */ "Economy",
    /* BronzeWorking */ "Military",
    /* CeremonialBurial */ "Civic",
    /* Chemistry */ "Science",
    /* Chivalry */ "Military",
    /* CodeOfLaws */ "Civic",
    /* Combustion */ "Exploration",
    /* Communism */ "Civic",
    /* Computers */ "Science",
    /* Conscription */ "Civic",
    /* Construction */ "Civic",
    /* Corporation */ "Economy",
    /* Currency */ "Economy",
    /* Democracy */ "Civic",
    /* Electricity */ "Science",
    /* Electronics */ "Science",
    /* Engineering */ "Economy",
    /* Explosives */ "Military",
    /* Feudalism */ "Military",
    /* Flight */ "Exploration",
    /* FusionPower */ "Science",
    /* FutureTech */ "Science",
    /* GeneticEngineering */ "Science",
    /* Gunpowder */ "Military",
    /* HorsebackRiding */ "Military",
    /* Industrialization */ "Economy",
    /* Invention */ "Science",
    /* IronWorking */ "Military",
    /* LaborUnion */ "Civic",
    /* Literacy */ "Science",
    /* Magnetism */ "Exploration",
    /* MapMaking */ "Exploration",
    /* Masonry */ "Civic",
    /* MassProduction */ "Economy",
    /* Mathematics */ "Science",
    /* Medicine */ "Science",
    /* Metallurgy */ "Military",
    /* Monarchy */ "Civic",
    /* Mysticism */ "Civic",
    /* Navigation */ "Exploration",
    /* NuclearFission */ "Science",
    /* NuclearPower */ "Science",
    /* Philosophy */ "Science",
    /* Physics */ "Science",
    /* Plastics */ "Economy",
    /* Pottery */ "Civic",
    /* Railroad */ "Exploration",
    /* Refining */ "Economy",
    /* Religion */ "Civic",
    /* Republic */ "Civic",
    /* Robotics */ "Military",
    /* Rocketry */ "Exploration",
    /* SpaceFlight */ "Exploration",
    /* SteamEngine */ "Exploration",
    /* Steel */ "Military",
    /* Superconductor */ "Science",
    /* TheoryOfGravity */ "Science",
    /* Trade */ "Economy",
    /* University */ "Science",
    /* TheWheel */ "Military",
    /* Writing */ "Science",
};

std::string join(const std::vector<std::string> &parts, const char *sep)
{
    std::string s;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) s += sep;
        s += parts[i];
    }
    return s;
}

std::string requires_text(Advance a)
{
    const auto &info = advance_info(a);
    if (info.req1 == Advance::None && info.req2 == Advance::None) return "Requires: nothing (a starting advance)";
    if (info.req2 == Advance::None) return std::string("Requires: ") + advance_info(info.req1).name;
    if (info.req1 == Advance::None) return std::string("Requires: ") + advance_info(info.req2).name;
    return std::string("Requires: ") + advance_info(info.req1).name + " and " + advance_info(info.req2).name;
}

std::string allows_text(Advance a)
{
    std::vector<std::string> out;
    for (int u = 0; u < kUnitTypeCount; ++u) {
        if (unit_info(u).tech == a) out.push_back(unit_info(u).name);
    }
    for (int b = 0; b < kBuildingCount; ++b) {
        if (building_info(b).tech == a) out.push_back(building_info(b).name);
    }
    for (int gv = 0; gv < static_cast<int>(Government::Count); ++gv) {
        if (government_info(static_cast<Government>(gv)).tech == a) {
            out.push_back(government_info(static_cast<Government>(gv)).name);
        }
    }
    for (int w = 0; w < kWonderCount; ++w) {
        if (wonder_info(w).tech == a) out.push_back(wonder_title(w));
    }
    if (out.empty()) return {};
    return "Allows: " + join(out, ", ");
}

std::string leads_text(Advance a)
{
    std::vector<std::string> out;
    for (int i = 0; i < kAdvanceCount; ++i) {
        const auto &info = advance_info(i);
        if (info.req1 == a || info.req2 == a) out.push_back(info.name);
    }
    if (out.empty()) return {};
    return "Leads to: " + join(out, ", ");
}

std::string turns_text(int t)
{
    if (t < 0) return "stalled";
    return std::to_string(t) + (t == 1 ? " turn" : " turns");
}

} // namespace

const char *advance_category(Advance a)
{
    const int i = static_cast<int>(a);
    if (i < 0 || i >= kAdvanceCount) return "Science";
    return kCategoryByAdvance[i];
}

TreeViewModel make_advance_tree_model(const Game &g, int civ, int goal_id)
{
    TreeViewModel m;
    m.goal = goal_id;
    m.researching = (g.civs[civ].research == Advance::None) ? -1 : static_cast<int>(g.civs[civ].research);

    m.nodes.reserve(kAdvanceCount);
    std::vector<int> known;
    for (int i = 0; i < kAdvanceCount; ++i) {
        TreeNode n;
        n.id = i;
        const Advance a = static_cast<Advance>(i);
        const auto &info = advance_info(a);
        n.name = info.name;
        n.category = advance_category(a);
        if (info.req1 != Advance::None) n.prereqs.push_back(static_cast<int>(info.req1));
        if (info.req2 != Advance::None) n.prereqs.push_back(static_cast<int>(info.req2));
        m.nodes.push_back(std::move(n));
        if (g.has(civ, a)) known.push_back(i);
    }

    const int cost = g.research_cost(civ);
    const int bulbs = g.civs[civ].bulbs;
    auto sci_per_turn = [&]() {
        int sci = 0;
        for (const auto &c : g.cities) {
            if (c.owner == civ && !c.stats.disorder) sci += c.stats.sci;
        }
        return sci;
    };
    const int sci = sci_per_turn();
    auto turns_for = [&](int need) {
        if (need <= 0) return 1;
        if (sci <= 0) return -1;
        return (need + sci - 1) / sci;
    };

    for (const TreeNode &n : m.nodes) {
        const Advance a = static_cast<Advance>(n.id);
        const bool is_res = g.civs[civ].research == a;
        const bool is_known = g.has(civ, a) && !is_res;
        const bool prereqs = tree_prereqs_met(n, known);
        TreeNodeInfo inf;
        inf.state = classify_node(is_known, is_res, prereqs);
        inf.requires_text = requires_text(a);
        const std::string al = allows_text(a);
        const std::string ld = leads_text(a);
        inf.allows_text = al;
        inf.leads_text = ld;
        inf.category = advance_category(a);
        if (is_res) {
            inf.cost_text = std::to_string(bulbs) + " / " + std::to_string(cost) + " bulbs · " + turns_text(turns_for(cost - bulbs));
        } else if (!is_known) {
            // Civ I costs the next discovery, not a particular advance. Locked
            // nodes still show that figure so the tooltip always has a cost.
            inf.cost_text = std::to_string(cost) + " bulbs to discover next";
            if (inf.state == TreeNodeState::Available)
                inf.cost_text = std::to_string(cost) + " bulbs · " + turns_text(turns_for(cost)) + " if started now";
        }
        m.info[n.id] = std::move(inf);
    }

    const int known_n = g.techs_known(civ);
    m.title_line = "The " + g.civs[civ].name + " know " + std::to_string(known_n) + " of " +
                   std::to_string(kAdvanceCount) + " advances. The next one costs " + std::to_string(cost) +
                   " bulbs.";
    if (g.civs[civ].research != Advance::None) {
        m.research_name = advance_info(g.civs[civ].research).name;
        m.research_right = std::to_string(bulbs) + " / " + std::to_string(cost) + "  ·  " + turns_text(turns_for(cost - bulbs));
        m.research_frac = cost > 0 ? static_cast<double>(bulbs) / cost : 0;
    }
    return m;
}

} // namespace ui
