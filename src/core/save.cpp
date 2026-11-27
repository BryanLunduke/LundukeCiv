// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Saved games are plain text: a version line, the settings, the map as one
// row of hex words per line, then one line per civ, city and unit with
// fields split by '|'. Easy to diff and to read when something goes wrong.

#include "game.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace civ {

namespace {

constexpr const char *kMagic = "LUNDUKECIV 3";      // written
constexpr const char *kMagic2 = "LUNDUKECIV 2";     // still read: milestone 2 saves
constexpr const char *kMagic1 = "LUNDUKECIV 1";     // still read: milestone 1 saves

// Saves before version 3 had pollution: Mass Transit (bit 17) and the
// Recycling Center (bit 19) are gone, so the improvements after them move
// down; polluted squares (tile bit 19), the "climate" line and the
// clean-up order are simply dropped.
constexpr int kOldMassTransit = 17;
constexpr int kOldRecycling = 19;
constexpr int kOldBuildingCount = 21;

std::uint32_t buildings_from_v2(std::uint32_t old)
{
    std::uint32_t out = 0;
    int to = 0;
    for (int b = 0; b < kOldBuildingCount; ++b) {
        if (b == kOldMassTransit || b == kOldRecycling) continue;
        if ((old >> b) & 1u) out |= 1u << to;
        ++to;
    }
    return out;
}

// An improvement index from a version 2 save; -1 for one that is gone.
int building_from_v2(int old)
{
    if (old < 0 || old >= kOldBuildingCount) return old;   // wonders, spaceship parts
    if (old == kOldMassTransit || old == kOldRecycling) return -1;
    return old - (old > kOldMassTransit ? 1 : 0) - (old > kOldRecycling ? 1 : 0);
}

std::string clean(const std::string &s)
{
    std::string out;
    for (char ch : s) out += (ch == '|' || ch == '\n' || ch == '\r') ? ' ' : ch;
    return out;
}

std::vector<std::string> split(const std::string &line)
{
    std::vector<std::string> out;
    std::string cur;
    for (char ch : line) {
        if (ch == '|') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += ch;
        }
    }
    out.push_back(cur);
    return out;
}

long long num(const std::vector<std::string> &f, size_t i)
{
    return i < f.size() ? std::strtoll(f[i].c_str(), nullptr, 10) : 0;
}

unsigned tile_word(const Tile &t)
{
    return static_cast<unsigned>(t.terrain) | (t.special << 4) | (t.hut << 5) | (t.road << 6) | (t.irrigation << 7) |
           (t.mine << 8) | (t.fortress << 9) | (static_cast<unsigned>(t.explored) << 10) | (t.railroad << 18);
}

} // namespace

std::string save_game(const Game &g)
{
    std::ostringstream o;
    o << kMagic << "\n";
    o << "game|" << g.turn << "|" << static_cast<int>(g.difficulty) << "|" << g.human << "|" << g.next_unit_id << "|"
      << g.next_city_id << "|" << g.winner << "|" << (g.human_dead ? 1 : 0) << "|" << (g.all_ai ? 1 : 0) << "|" << g.seed
      << "|" << g.rng.state << "|" << g.tech_catchup << "\n";
    o << "map|" << g.map.w << "|" << g.map.h << "\n";
    char buf[16];
    for (int y = 0; y < g.map.h; ++y) {
        std::string row;
        for (int x = 0; x < g.map.w; ++x) {
            std::snprintf(buf, sizeof buf, "%05x", tile_word(g.map.at(x, y)));
            row += buf;
        }
        o << row << "\n";
    }
    for (const Civ &c : g.civs) {
        std::string techs;
        for (bool b : c.techs) techs += b ? '1' : '0';
        o << "civ|" << clean(c.name) << "|" << clean(c.leader) << "|" << c.color << "|" << c.alive << "|" << c.ai << "|"
          << c.gold << "|" << c.tax << "|" << c.lux << "|" << c.sci << "|" << static_cast<int>(c.govt) << "|"
          << static_cast<int>(c.target) << "|" << c.anarchy << "|" << techs << "|" << static_cast<int>(c.research) << "|"
          << c.bulbs << "|" << c.future << "|" << c.name_next << "|" << static_cast<int>(c.war) << "|"
          << static_cast<int>(c.contact) << "|" << static_cast<int>(c.embassy) << "|" << (c.barbarian ? 1 : 0) << "|" << c.structurals << "|"
          << c.components << "|" << c.modules << "|" << c.launch_turn << "|" << c.arrival_turn << "\n";
    }
    for (const City &c : g.cities) {
        std::string worked;
        for (const auto &w : c.worked) worked += std::to_string(w.first) + "," + std::to_string(w.second) + ";";
        o << "city|" << c.id << "|" << clean(c.name) << "|" << c.owner << "|" << c.x << "|" << c.y << "|" << c.size << "|"
          << c.food << "|" << c.shields << "|" << c.build_unit << "|" << c.build << "|" << c.buildings << "|"
          << c.entertainers << "|" << c.manual << "|" << c.founded << "|" << c.bought << "|" << worked << "|";
        for (size_t r = 0; r < c.routes.size(); ++r) o << (r ? "," : "") << c.routes[r];
        o << "|" << static_cast<int>(c.stolen) << "|" << c.taxmen << "|" << c.scientists << "\n";
    }
    for (const Unit &u : g.units) {
        o << "unit|" << u.id << "|" << static_cast<int>(u.type) << "|" << u.owner << "|" << u.x << "|" << u.y << "|"
          << u.moves << "|" << u.veteran << "|" << static_cast<int>(u.order) << "|" << u.work << "|" << u.home << "|"
          << u.goto_x << "|" << u.goto_y << "|" << u.fuel << "|" << u.carried_by << "|" << u.done << "|" << u.hp << "\n";
    }
    o << "wonders";
    for (int w = 0; w < kWonderCount; ++w) o << "|" << g.wonder_city[w];
    o << "\n";
    o << "wstart";
    for (std::uint32_t b : g.wonder_started) o << "|" << b;
    o << "\n";
    o << "space|" << (g.space_win ? 1 : 0) << "\n";
    o << "end\n";
    return o.str();
}

bool load_game(const std::string &text, Game &out, std::string *error)
{
    auto fail = [&](const std::string &why) {
        if (error) *error = why;
        return false;
    };
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || (line != kMagic && line != kMagic2 && line != kMagic1)) return fail("Not a Lunduke Civ saved game.");
    const int version = line == kMagic ? 3 : (line == kMagic2 ? 2 : 1);
    Game g;
    bool ended = false;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const std::vector<std::string> f = split(line);
        const std::string &k = f[0];
        if (k == "game") {
            g.turn = static_cast<int>(num(f, 1));
            g.difficulty = static_cast<Difficulty>(num(f, 2));
            g.human = static_cast<int>(num(f, 3));
            g.next_unit_id = static_cast<int>(num(f, 4));
            g.next_city_id = static_cast<int>(num(f, 5));
            g.winner = static_cast<int>(num(f, 6));
            g.human_dead = num(f, 7) != 0;
            g.all_ai = num(f, 8) != 0;
            g.seed = f.size() > 9 ? std::strtoull(f[9].c_str(), nullptr, 10) : 1;
            g.rng.state = f.size() > 10 ? std::strtoull(f[10].c_str(), nullptr, 10) : 1;
            if (g.rng.state == 0) g.rng.state = 1;
            g.tech_catchup = std::clamp(static_cast<int>(num(f, 11)), 0, 6);
        } else if (k == "map") {
            if (num(f, 1) != kMapW || num(f, 2) != kMapH) return fail("Unsupported map size.");
            for (int y = 0; y < kMapH; ++y) {
                if (!std::getline(in, line) || static_cast<int>(line.size()) < kMapW * 5) return fail("Map data is short.");
                for (int x = 0; x < kMapW; ++x) {
                    const unsigned v = static_cast<unsigned>(std::strtoul(line.substr(static_cast<size_t>(x) * 5, 5).c_str(), nullptr, 16));
                    Tile &t = g.map.at(x, y);
                    const unsigned ter = v & 15u;
                    if (ter >= static_cast<unsigned>(kTerrainCount)) return fail("Bad terrain in map.");
                    t.terrain = static_cast<Terrain>(ter);
                    t.special = (v >> 4) & 1u;
                    t.hut = (v >> 5) & 1u;
                    t.road = (v >> 6) & 1u;
                    t.irrigation = (v >> 7) & 1u;
                    t.mine = (v >> 8) & 1u;
                    t.fortress = (v >> 9) & 1u;
                    t.explored = static_cast<std::uint8_t>((v >> 10) & 0xFFu);
                    t.railroad = (v >> 18) & 1u;
                }
            }
            g.map.label_continents();
        } else if (k == "civ") {
            Civ c;
            c.name = f.size() > 1 ? f[1] : "";
            c.leader = f.size() > 2 ? f[2] : "";
            c.color = static_cast<int>(num(f, 3));
            c.alive = num(f, 4) != 0;
            c.ai = num(f, 5) != 0;
            c.gold = static_cast<int>(num(f, 6));
            c.tax = static_cast<int>(num(f, 7));
            c.lux = static_cast<int>(num(f, 8));
            c.sci = static_cast<int>(num(f, 9));
            c.govt = static_cast<Government>(num(f, 10));
            c.target = static_cast<Government>(num(f, 11));
            c.anarchy = static_cast<int>(num(f, 12));
            const std::string techs = f.size() > 13 ? f[13] : "";
            for (int i = 0; i < kAdvanceCount && i < static_cast<int>(techs.size()); ++i) c.techs[i] = techs[i] == '1';
            c.research = static_cast<Advance>(num(f, 14));
            c.bulbs = static_cast<int>(num(f, 15));
            c.future = static_cast<int>(num(f, 16));
            c.name_next = static_cast<int>(num(f, 17));
            c.war = static_cast<std::uint8_t>(num(f, 18));
            c.contact = static_cast<std::uint8_t>(num(f, 19));
            c.embassy = static_cast<std::uint8_t>(num(f, 20));
            c.barbarian = num(f, 21) != 0;
            if (f.size() > 26) {
                c.structurals = static_cast<int>(num(f, 22));
                c.components = static_cast<int>(num(f, 23));
                c.modules = static_cast<int>(num(f, 24));
                c.launch_turn = static_cast<int>(num(f, 25));
                c.arrival_turn = static_cast<int>(num(f, 26));
            }
            g.civs.push_back(c);
        } else if (k == "city") {
            City c;
            c.id = static_cast<int>(num(f, 1));
            c.name = f.size() > 2 ? f[2] : "";
            c.owner = static_cast<int>(num(f, 3));
            c.x = static_cast<int>(num(f, 4));
            c.y = static_cast<int>(num(f, 5));
            c.size = static_cast<int>(num(f, 6));
            c.food = static_cast<int>(num(f, 7));
            c.shields = static_cast<int>(num(f, 8));
            c.build_unit = num(f, 9) != 0;
            c.build = static_cast<int>(num(f, 10));
            c.buildings = static_cast<std::uint32_t>(num(f, 11));
            if (version < 3) {
                c.buildings = buildings_from_v2(c.buildings);
                if (!c.build_unit) {
                    c.build = building_from_v2(c.build);
                    if (c.build < 0) {
                        // Was building Mass Transit or a Recycling Center: the
                        // shields go to a defender instead.
                        c.build_unit = true;
                        c.build = static_cast<int>(UnitType::Militia);
                    }
                }
            }
            c.entertainers = static_cast<int>(num(f, 12));
            c.manual = num(f, 13) != 0;
            c.founded = static_cast<int>(num(f, 14));
            c.bought = num(f, 15) != 0;
            std::istringstream ws(f.size() > 16 ? f[16] : "");
            std::string pair;
            while (std::getline(ws, pair, ';')) {
                const size_t comma = pair.find(',');
                if (comma == std::string::npos) continue;
                c.worked.push_back({std::atoi(pair.substr(0, comma).c_str()), std::atoi(pair.substr(comma + 1).c_str())});
            }
            if (f.size() > 17) {
                std::istringstream rs(f[17]);
                std::string id;
                while (std::getline(rs, id, ',')) {
                    if (!id.empty() && static_cast<int>(c.routes.size()) < kMaxRoutes) c.routes.push_back(std::atoi(id.c_str()));
                }
            }
            c.stolen = static_cast<std::uint8_t>(num(f, 18));
            c.taxmen = std::max(0, static_cast<int>(num(f, 19)));
            c.scientists = std::max(0, static_cast<int>(num(f, 20)));
            g.cities.push_back(c);
        } else if (k == "unit") {
            Unit u;
            u.id = static_cast<int>(num(f, 1));
            const int t = static_cast<int>(num(f, 2));
            if (t < 0 || t >= kUnitTypeCount) return fail("Bad unit type.");
            u.type = static_cast<UnitType>(t);
            u.owner = static_cast<int>(num(f, 3));
            u.x = static_cast<int>(num(f, 4));
            u.y = static_cast<int>(num(f, 5));
            u.moves = static_cast<int>(num(f, 6));
            u.veteran = num(f, 7) != 0;
            const int order = static_cast<int>(num(f, 8));
            u.order = order >= 0 && order <= static_cast<int>(Order::Railroad) ? static_cast<Order>(order) : Order::None;
            u.work = static_cast<int>(num(f, 9));
            u.home = static_cast<int>(num(f, 10));
            u.goto_x = static_cast<int>(num(f, 11));
            u.goto_y = static_cast<int>(num(f, 12));
            u.fuel = static_cast<int>(num(f, 13));
            u.carried_by = static_cast<int>(num(f, 14));
            u.done = num(f, 15) != 0;
            u.hp = f.size() > 16 ? std::clamp(static_cast<int>(num(f, 16)), 1, kUnitHp) : kUnitHp;
            g.units.push_back(u);
        } else if (k == "wonders") {
            for (int w = 0; w < kWonderCount && w + 1 < static_cast<int>(f.size()); ++w) g.wonder_city[w] = static_cast<int>(num(f, w + 1));
        } else if (k == "space") {
            g.space_win = num(f, 1) != 0;
        } else if (k == "climate") {
            // Global warming from a version 2 save: no longer part of the game.
        } else if (k == "wstart") {
            for (int c = 0; c < kMaxCivs && c + 1 < static_cast<int>(f.size()); ++c) g.wonder_started[c] = static_cast<std::uint32_t>(num(f, c + 1));
        } else if (k == "end") {
            ended = true;
            break;
        }
    }
    if (!ended) return fail("The saved game is cut short.");
    if (g.civs.empty()) return fail("No civilizations in the saved game.");
    for (const City &c : g.cities) {
        if (c.owner < 0 || c.owner >= static_cast<int>(g.civs.size())) return fail("City with a bad owner.");
    }
    for (const Unit &u : g.units) {
        if (u.owner < 0 || u.owner >= static_cast<int>(g.civs.size())) return fail("Unit with a bad owner.");
    }
    // Saves from before barbarians: add them.
    if (g.barbarian_civ() < 0 && static_cast<int>(g.civs.size()) < kMaxCivs) {
        Civ b;
        b.name = "Barbarians";
        b.leader = "Chieftain";
        b.color = 8;
        b.barbarian = true;
        b.gold = 0;
        g.civs.push_back(b);
    }
    for (City &c : g.cities) g.update_city(c);
    g.refresh_visibility();
    out = std::move(g);
    return true;
}

} // namespace civ
