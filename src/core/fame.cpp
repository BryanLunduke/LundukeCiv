// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// The Civilization Score, retirement and the Hall of Fame (after the DOS
// game's GameEngine end-of-game reckoning and HallOfFame, by way of the
// OpenCivOne rewrite).

#include "fame.hpp"

#include <algorithm>
#include <sstream>

namespace civ {

ScoreBreakdown Game::score(int ci)
{
    ScoreBreakdown s;
    if (ci < 0 || ci >= static_cast<int>(civs.size())) return s;
    Civ &cv = civs[ci];
    // DOS counts the citizens as they would be with 40% luxuries: taxes are
    // set to 60% less science for the count and put back afterwards.
    const int keep_tax = cv.tax, keep_lux = cv.lux;
    cv.tax = std::max(0, 6 - cv.sci);
    cv.lux = 10 - cv.sci - cv.tax;
    for (const City &c : cities) {
        if (c.owner != ci) continue;
        City probe = c;   // a copy: the city itself keeps its people where they are
        probe.manual = true;
        update_city(probe);
        s.happy += probe.stats.happy;
        s.content += probe.stats.content;
        s.unhappy += probe.stats.unhappy;
        s.specialists += probe.specialists();
        s.citizens += probe.size + probe.stats.happy - probe.stats.unhappy;
    }
    cv.tax = keep_tax;
    cv.lux = keep_lux;
    for (int w = 0; w < kWonderCount; ++w) {
        const City *home = wonder_home(static_cast<Wonder>(w));
        if (home && home->owner == ci) ++s.wonders;
    }
    s.wonder_points = s.wonders * kWonderPoints;
    s.total = s.citizens + s.wonder_points;
    // The spaceship: success x colonists (in ten thousands) / 2, once landed.
    if (cv.launch_turn >= 0 || (space_win && winner == ci)) {
        const SpaceStats ss = space_stats(ci);
        const int pts = std::max(0, ss.success_pct * (ss.population / 10000) / 2);
        if (space_win && winner == ci) s.spaceship = pts;
        else s.spaceship_possible = pts;
    }
    s.total += s.spaceship;
    if (year_for_turn(turn) > 0 && peace_turns > 0) s.peace = std::min(kPeaceMax, peace_turns * kPeacePerTurn);
    s.future = cv.future * kFuturePoints;
    s.total += s.peace + s.future;
    s.total = std::max(0, s.total);
    // Conquest: (550 - turns) x 2 + 100 a rival + 400, if better.
    if (winner == ci && !space_win) {
        s.bonus = (550 - (turn - 1)) * 2 + opponents * 100 + 400;
        s.total = std::max(s.total, s.bonus);
    }
    return s;
}

long long Game::population(int ci) const
{
    long long p = 0;
    for (const City &c : cities) {
        if (c.owner == ci) p += static_cast<long long>(c.size) * (c.size + 1) / 2 * 10000;
    }
    return p;
}

void Game::retire()
{
    if (over()) return;
    retired = true;
    if (!game_ended) {
        game_ended = true;
        end_turn_number = turn;
    }
    say(human, MsgKind::Info, std::string(leader_title(human)) + " " + civs[human].leader + " retires. The " + civs[human].name +
                                  " remember a reign that lasted until " + year_text(year_for_turn(turn)) + ".");
}

FameEntry fame_entry(Game &g)
{
    FameEntry e;
    const Civ &c = g.civs[g.human];
    e.leader = c.leader;
    e.people = c.name;
    e.difficulty = static_cast<int>(g.difficulty);
    e.year = year_for_turn(g.end_turn_number > 0 ? g.end_turn_number : g.turn);
    e.population = g.population(g.human);
    e.score = g.score(g.human).total;
    return e;
}

namespace {

std::string clean(const std::string &s)
{
    std::string out;
    for (char ch : s) out += (ch == '|' || ch == '\n' || ch == '\r') ? ' ' : ch;
    return out;
}

} // namespace

std::string save_fame(const std::vector<FameEntry> &list)
{
    std::ostringstream o;
    o << "LUNDUKECIV FAME 1\n";
    for (const FameEntry &e : list) {
        o << "fame|" << clean(e.leader) << "|" << clean(e.people) << "|" << e.difficulty << "|" << e.year << "|" << e.population << "|"
          << e.score << "\n";
    }
    return o.str();
}

std::vector<FameEntry> load_fame(const std::string &text)
{
    std::vector<FameEntry> out;
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line != "LUNDUKECIV FAME 1") return out;
    while (std::getline(in, line) && static_cast<int>(out.size()) < kFameSize) {
        std::vector<std::string> f;
        std::string cur;
        for (char ch : line) {
            if (ch == '|') {
                f.push_back(cur);
                cur.clear();
            } else if (ch != '\r') {
                cur += ch;
            }
        }
        f.push_back(cur);
        if (f.size() < 7 || f[0] != "fame") continue;
        FameEntry e;
        e.leader = f[1];
        e.people = f[2];
        e.difficulty = std::clamp(std::atoi(f[3].c_str()), 0, static_cast<int>(Difficulty::Count) - 1);
        e.year = std::atoi(f[4].c_str());
        e.population = std::strtoll(f[5].c_str(), nullptr, 10);
        e.score = std::max(0, std::atoi(f[6].c_str()));
        out.push_back(e);
    }
    return out;
}

int add_fame(std::vector<FameEntry> &list, const FameEntry &e)
{
    if (e.score <= 0) return -1;
    int place = static_cast<int>(list.size());
    for (int i = 0; i < static_cast<int>(list.size()); ++i) {
        if (e.rating() > list[i].rating()) {
            place = i;
            break;
        }
    }
    if (place >= kFameSize) return -1;
    list.insert(list.begin() + place, e);
    if (static_cast<int>(list.size()) > kFameSize) list.resize(kFameSize);
    return place;
}

int famous_rank(int rating)
{
    // DOS: the last of 1..20 whose square over three is below the rating.
    int rank = -1;
    for (int i = 1; i <= 20; ++i) {
        if (i * i / 3 < rating) rank = i - 1;
    }
    return rank;
}

const char *famous_ruler(int rank)
{
    static const char *const kRulers[20] = {
        "Caligula",          "Aethelred the Unready", "King John",       "Romulus Augustulus", "Nicholas II",
        "Richard III",       "Harold Godwinson",      "Hannibal Barca",  "Robert the Bruce",   "Frederick the Great",
        "Peter the Great",   "Akbar the Great",       "Ashoka",          "Hatshepsut",         "Queen Victoria",
        "Kublai Khan",       "Alfred the Great",      "Cyrus the Great", "Marcus Aurelius",    "Mansa Musa",
    };
    return kRulers[std::clamp(rank, 0, 19)];
}

} // namespace civ
