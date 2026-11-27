// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#include "options.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace civ {

namespace {

struct Switch {
    const char *key;
    bool GameOptions::*field;
};

const Switch kSwitches[] = {
    {"instant_advice", &GameOptions::instant_advice}, {"autosave", &GameOptions::autosave},
    {"end_of_turn", &GameOptions::end_of_turn},       {"animations", &GameOptions::animations},
    {"enemy_moves", &GameOptions::enemy_moves},       {"event_screens", &GameOptions::event_screens},
};

std::string lower(std::string s)
{
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

std::string save_options(const GameOptions &o)
{
    std::ostringstream out;
    out << "LUNDUKECIV OPTIONS 1\n";
    for (const Switch &s : kSwitches) out << s.key << "=" << ((o.*s.field) ? 1 : 0) << "\n";
    return out.str();
}

GameOptions load_options(const std::string &text)
{
    GameOptions o;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (value != "0" && value != "1") continue;
        for (const Switch &s : kSwitches) {
            if (key == s.key) o.*s.field = value == "1";
        }
    }
    return o;
}

bool autosave_due(int turn)
{
    return turn > 1 && (turn - 1) % kAutosaveTurns == 0;
}

std::vector<int> find_cities(const Game &g, int civ, const std::string &prefix)
{
    const std::string want = lower(prefix);
    std::vector<int> found;
    for (const City &c : g.cities) {
        const bool known = c.owner == civ || ((g.map.at(c.x, c.y).explored >> civ) & 1u);
        if (!known || lower(c.name).compare(0, want.size(), want) != 0) continue;
        found.push_back(c.id);
    }
    std::sort(found.begin(), found.end(), [&](int a, int b) {
        const City *x = g.city(a), *y = g.city(b);
        const bool xm = x->owner == civ, ym = y->owner == civ;
        if (xm != ym) return xm;
        return lower(x->name) < lower(y->name);
    });
    return found;
}

} // namespace civ
