// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.
//
// Treaties, audiences with foreign leaders, the Senate and civil war.
//
// The computer rulers' reasoning follows the quantities of DOS
// Civilization's MeetWithKing (as decompiled in OpenCivOne): fear of the
// other side's defences where both stand, a demand built from where our
// attack beats their defence, hostility when that pays, and tribute of
// demand x (level + 1) / 32 fifty-gold pieces (at most 20). The order of
// the decisions and the cease-fire (a Civ 2 idea) are ours.

#include "game.hpp"

#include <algorithm>
#include <map>

namespace civ {

namespace {

bool fighting(const UnitInfo &i) { return !(i.flags & kNonMilitary) && i.attack + i.defense > 0; }

std::uint8_t bit(int k) { return static_cast<std::uint8_t>(1u << k); }

} // namespace

const char *treaty_name(Treaty t)
{
    switch (t) {
    case Treaty::War: return "War";
    case Treaty::Ceasefire: return "Cease-fire";
    case Treaty::Peace: return "Peace";
    default: return "No treaty";
    }
}

bool talk_has_choice(const Talk &t)
{
    if (t.forced) return false;
    switch (t.kind) {
    case TalkKind::Tribute:
    case TalkKind::TechDemand:
    case TalkKind::Ceasefire:
    case TalkKind::Peace:
    case TalkKind::TechForPeace:
    case TalkKind::Embassy: return true;
    default: return false;
    }
}

// ---------------------------------------------------------------- treaties

Treaty Game::treaty(int a, int b) const
{
    if (a == b) return Treaty::Peace;
    if (at_war(a, b)) return Treaty::War;
    if ((civs[a].peace >> b) & 1u) return Treaty::Peace;
    if ((civs[a].ceasefire >> b) & 1u) return Treaty::Ceasefire;
    return Treaty::None;
}

bool Game::senate(int civ) const
{
    const Government g = civs[civ].govt;
    return g == Government::Republic || g == Government::Democracy;
}

void Game::declare_war(int a, int b)
{
    if (a == b || is_barbarian(a) || is_barbarian(b)) return;
    if (has_treaty(a, b)) civs[b].vendetta |= bit(a);   // they broke their word
    civs[a].war |= bit(b);
    civs[b].war |= bit(a);
    civs[a].peace &= static_cast<std::uint8_t>(~bit(b));
    civs[b].peace &= static_cast<std::uint8_t>(~bit(a));
    civs[a].ceasefire &= static_cast<std::uint8_t>(~bit(b));
    civs[b].ceasefire &= static_cast<std::uint8_t>(~bit(a));
    say(a, MsgKind::Bad, "You are at war with the " + civs[b].name + ".");
    say(b, MsgKind::Bad, "The " + civs[a].name + " declare war on you!");
}

void Game::make_ceasefire(int a, int b)
{
    if (is_barbarian(a) || is_barbarian(b)) return;
    civs[a].war &= static_cast<std::uint8_t>(~bit(b));
    civs[b].war &= static_cast<std::uint8_t>(~bit(a));
    civs[a].peace &= static_cast<std::uint8_t>(~bit(b));
    civs[b].peace &= static_cast<std::uint8_t>(~bit(a));
    civs[a].ceasefire |= bit(b);
    civs[b].ceasefire |= bit(a);
    civs[a].ceasefire_end[b] = civs[b].ceasefire_end[a] = turn + kCeasefireTurns;
}

void Game::make_peace(int a, int b)
{
    if (is_barbarian(a) || is_barbarian(b)) return;   // no peace with raiders
    civs[a].war &= static_cast<std::uint8_t>(~bit(b));
    civs[b].war &= static_cast<std::uint8_t>(~bit(a));
    civs[a].ceasefire &= static_cast<std::uint8_t>(~bit(b));
    civs[b].ceasefire &= static_cast<std::uint8_t>(~bit(a));
    civs[a].peace |= bit(b);
    civs[b].peace |= bit(a);
}

void Game::ceasefires_end()
{
    for (int a = 0; a < static_cast<int>(civs.size()); ++a) {
        for (int b = a + 1; b < static_cast<int>(civs.size()); ++b) {
            if (treaty(a, b) != Treaty::Ceasefire || turn < civs[a].ceasefire_end[b]) continue;
            civs[a].ceasefire &= static_cast<std::uint8_t>(~bit(b));
            civs[b].ceasefire &= static_cast<std::uint8_t>(~bit(a));
            civs[a].war |= bit(b);
            civs[b].war |= bit(a);
            for (int k : {a, b}) {
                const int o = k == a ? b : a;
                if (!civs[k].ai) {
                    Talk t;
                    t.from = o;
                    t.to = k;
                    t.kind = TalkKind::CeasefireOver;
                    t.text = "The cease-fire with the " + civs[o].name + " has run out. We are at war again.";
                    talks.push_back(t);
                }
                say(k, MsgKind::Bad, "The cease-fire with the " + civs[o].name + " is over: war resumes.");
            }
        }
    }
}

// ---------------------------------------------------------------- reckoning

int Game::civ_score(int k) const
{
    // DOS ranking: coins / 32 + city sizes x 8 + advances + shields in units.
    int s = std::max(0, civs[k].gold) / 32 + techs_known(k);
    for (const City &c : cities) {
        if (c.owner == k) s += c.size * 8;
    }
    for (const Unit &u : units) {
        if (u.owner == k) s += unit_info(u.type).cost / 10;
    }
    return s;
}

int Game::civ_rank(int k) const
{
    // Ranks 7, 6, ... from the strongest down.
    const int mine = civ_score(k);
    int above = 0;
    for (int o = 0; o < static_cast<int>(civs.size()); ++o) {
        if (o == k || !civs[o].alive || civs[o].barbarian) continue;
        const int s = civ_score(o);
        if (s > mine || (s == mine && o < k)) ++above;
    }
    return std::max(0, 7 - above);
}

Attitude Game::attitude(int ai, int other) const
{
    Attitude a;
    if (ai == other || is_barbarian(ai) || is_barbarian(other)) return a;
    struct Side {
        int att = 0, def = 0, cities = 0;
    };
    std::map<int, Side> mine, theirs;
    int ai_mil = 0, other_mil = 0;
    bool other_nukes = false, ai_nukes = false;
    for (const Unit &u : units) {
        if (u.owner != ai && u.owner != other) continue;
        const UnitInfo &i = unit_info(u.type);
        if (!fighting(i)) continue;
        const int cont = map.at(u.x, u.y).continent;
        Side &s = (u.owner == ai ? mine : theirs)[cont];
        s.att += i.attack;
        s.def += i.defense;
        (u.owner == ai ? ai_mil : other_mil) += i.attack + i.defense;
        if (i.flags & kNuclear) (u.owner == ai ? ai_nukes : other_nukes) = true;
    }
    for (const City &c : cities) {
        if (c.owner == ai) mine[map.at(c.x, c.y).continent].cities++;
        if (c.owner == other) theirs[map.at(c.x, c.y).continent].cities++;
    }
    const int level = static_cast<int>(difficulty);
    for (const auto &kv : mine) {
        auto it = theirs.find(kv.first);
        if (it == theirs.end()) continue;
        const Side &m = kv.second, &t = it->second;
        if (m.cities > 0 && t.def > m.att) {
            a.fear += t.def * (difficulty == Difficulty::Chieftain ? 4 : 2) / (m.att + 1);
        } else if (m.att > 0 && (t.att > 0 || t.cities > 0)) {   // DOS: both armed; we add their open cities
            a.hostile = true;
        }
        if (t.cities > 0) a.demand += std::max(0, m.att - t.def);
    }
    int other_cities = 0;
    for (const City &c : cities) other_cities += c.owner == other;
    const bool grudge = (civs[ai].vendetta >> other) & 1u;
    // The strongest civilization late in the game draws everyone's anger.
    if ((civ_rank(other) == 7 && other_cities > 4 && !ai_nukes && turn > 200) || (grudge && 3 * ai_mil > other_mil)) {
        a.hostile = true;
        a.demand = std::max(a.demand, 10 * civ_rank(ai));
        a.fear = 0;
    }
    if (other_nukes && !ai_nukes) a.fear += 8;
    // Leanings of the people (Civ 1's "aggressive" and "friendly").
    if (civs[ai].tribe >= 0) {
        const int mood = tribe_info(civs[ai].tribe).mood;
        if (mood > 0) a.demand = a.demand * 3 / 2;
        if (mood < 0) a.demand /= 2;
    }
    if (a.fear > 0) a.hostile = false;
    if (wonder_active(other, Wonder::GreatWall) || wonder_active(other, Wonder::UnitedNations)) a.hostile = false;
    if (grudge) a.demand *= 2;
    if (a.demand == 0 || 3 * ai_mil < other_mil) a.hostile = false;
    const int pieces = std::clamp(a.demand * (level + 1) / 32, 0, 20);
    a.tribute = std::min(pieces, std::max(0, civs[other].gold) / 50) * 50;
    return a;
}

// ---------------------------------------------------------------- audiences

std::string Game::leader_greeting(int k) const
{
    return std::string(leader_title(k)) + " " + civs[k].leader + " of the " + civs[k].name;
}

static Advance tech_gap(const Game &g, int has, int lacks)
{
    // The most advanced thing `has` knows that `lacks` does not.
    return g.capture_tech(lacks, has);
}

bool Game::settle(const Talk &t, bool accept, std::string *what)
{
    const int a = t.from, b = t.to;   // a asks, b answers
    std::string out;
    switch (t.kind) {
    case TalkKind::Tribute:
        if (accept && civs[b].gold >= t.gold) {
            civs[b].gold -= t.gold;
            civs[a].gold += t.gold;
            out = "The " + civs[b].name + " pay " + std::to_string(t.gold) + " gold to the " + civs[a].name + ".";
        } else {
            accept = false;
            declare_war(a, b);
            out = "The " + civs[a].name + " declare war!";
        }
        break;
    case TalkKind::TechDemand:
        if (accept && t.tech != Advance::None) {
            learn(a, t.tech, "A treaty brings you the secret of");
            out = "The " + civs[a].name + " learn " + advance_info(t.tech).name + ".";
        } else {
            accept = false;
            declare_war(a, b);
            out = "The " + civs[a].name + " declare war!";
        }
        break;
    case TalkKind::Ceasefire:
        if (accept) {
            make_ceasefire(a, b);
            out = "A cease-fire between the " + civs[a].name + " and the " + civs[b].name + ".";
        }
        break;
    case TalkKind::Peace:
        if (accept) {
            make_peace(a, b);
            out = "Peace between the " + civs[a].name + " and the " + civs[b].name + ".";
        }
        break;
    case TalkKind::TechForPeace:
        if (accept) {
            if (t.tech != Advance::None) learn(b, t.tech, "A treaty brings you the secret of");
            make_peace(a, b);
            out = "Peace between the " + civs[a].name + " and the " + civs[b].name + "; the " + civs[b].name + " learn " +
                  advance_info(t.tech).name + ".";
        }
        break;
    case TalkKind::Embassy:
        if (accept) {
            civs[a].embassy |= bit(b);
            civs[b].embassy |= bit(a);
            out = "The " + civs[a].name + " and the " + civs[b].name + " exchange embassies.";
        }
        break;
    case TalkKind::WarDeclared:
    case TalkKind::TreatyCancelled:
        if (!at_war(a, b)) declare_war(a, b);
        break;
    default: break;
    }
    if (what) *what = out;
    return accept;
}

void Game::deliver(Talk t)
{
    civs[t.from].last_talk[t.to] = turn;
    civs[t.to].last_talk[t.from] = turn;
    const bool offer = t.kind == TalkKind::Ceasefire || t.kind == TalkKind::Peace || t.kind == TalkKind::TechForPeace;
    if (!civs[t.to].ai) {
        if (offer && senate(t.to)) {
            // The Senate will hear of nothing but yes.
            t.forced = true;
            t.text += " Your Senate accepts the offer for you.";
            settle(t, true, nullptr);
        } else if (t.kind == TalkKind::WarDeclared || t.kind == TalkKind::TreatyCancelled) {
            settle(t, true, nullptr);
        }
        talks.push_back(t);
        return;
    }
    // Between computer rulers: the other side's own reckoning decides.
    const Attitude them = attitude(t.to, t.from);
    bool yes = false;
    switch (t.kind) {
    case TalkKind::Tribute:
    case TalkKind::TechDemand: yes = them.fear > 0 || !them.hostile; break;
    case TalkKind::Ceasefire:
    case TalkKind::Peace:
    case TalkKind::TechForPeace: yes = senate(t.to) || !them.hostile; break;
    case TalkKind::Embassy: yes = !them.hostile; break;
    default: yes = true; break;
    }
    std::string what;
    settle(t, yes, &what);
    if (!what.empty()) {
        for (int k = 0; k < static_cast<int>(civs.size()); ++k) {
            if (!civs[k].ai && k != t.from && k != t.to && (((civs[k].embassy >> t.from) & 1u) || ((civs[k].embassy >> t.to) & 1u)))
                say(k, MsgKind::Info, what);
        }
    }
}

void Game::meet(int ai, int k)
{
    Civ &me = civs[ai];
    const int last = me.last_talk[k];
    const bool first = last == 0;
    if (!first && turn - last < 8) return;
    const Attitude at = attitude(ai, k);
    const Treaty tr = treaty(ai, k);
    Talk t;
    t.from = ai;
    t.to = k;
    const std::string who = civs[k].ai ? std::string("the ") + civs[k].name : std::string("you");
    const std::string hail = first ? "Greetings. We are the " + me.name + ", and I am " + me.leader + ". " : std::string();
    const Advance theirs = tech_gap(*this, k, ai);
    const Advance ours = tech_gap(*this, ai, k);
    switch (tr) {
    case Treaty::War:
        if (!at.hostile && (at.fear > 0 || turn - last >= 16)) {
            t.kind = TalkKind::Ceasefire;
            t.text = hail + "This war profits no one. Let our armies rest: will " + who + " agree to a cease-fire?";
        } else {
            if (!first) return;
            t.kind = TalkKind::Greeting;
            t.text = hail + "Our quarrel will be settled on the field.";
        }
        break;
    case Treaty::Ceasefire:
        if (at.hostile) return;
        t.kind = TalkKind::Peace;
        t.text = hail + "The guns are quiet. Shall we make it a lasting peace?";
        break;
    case Treaty::None:
        if (at.hostile) {
            if (at.tribute > 0) {
                t.kind = TalkKind::Tribute;
                t.gold = at.tribute;
                t.text = hail + "Your cities lie open to our armies. " + std::to_string(at.tribute) +
                         " gold would persuade us to look elsewhere.";
            } else if (theirs != Advance::None && rng.below(2) == 0) {
                t.kind = TalkKind::TechDemand;
                t.tech = theirs;
                t.text = hail + "Your scholars know " + advance_info(theirs).name + ". Share it with us, or we will come and take it.";
            } else {
                t.kind = TalkKind::WarDeclared;
                t.text = hail + "You are weak and we are many. Prepare for war!";
            }
        } else if (at.fear > 0 && ours != Advance::None) {
            t.kind = TalkKind::TechForPeace;
            t.tech = ours;
            t.text = hail + "Let there be peace between us. As a token, our scholars will teach yours " + advance_info(ours).name + ".";
        } else if (first || rng.below(2) == 0) {
            t.kind = TalkKind::Peace;
            t.text = hail + "We seek only friendship. Will " + who + " sign a treaty of peace?";
        } else if (!((me.embassy >> k) & 1u)) {
            t.kind = TalkKind::Embassy;
            t.text = hail + "Let us exchange embassies, so that each may know the other better.";
        } else {
            return;
        }
        break;
    case Treaty::Peace:
        if (at.hostile && !senate(ai) && (((me.vendetta >> k) & 1u) || at.demand > 20) && rng.below(4) == 0) {
            t.kind = TalkKind::TreatyCancelled;
            t.text = "Our patience is at an end. The " + me.name + " tear up the treaty: this means war!";
        } else if (!((me.embassy >> k) & 1u) || !((civs[k].embassy >> ai) & 1u)) {
            t.kind = TalkKind::Embassy;
            t.text = hail + "Friends should not be strangers. Let us exchange embassies.";
        } else if (first) {
            t.kind = TalkKind::Greeting;
            t.text = hail + "May our peace long endure.";
        } else {
            return;
        }
        break;
    }
    deliver(t);
}

void Game::ai_diplomacy(int ci)
{
    if (civs[ci].barbarian || !civs[ci].alive) return;
    for (int k = 0; k < static_cast<int>(civs.size()); ++k) {
        if (k == ci || !civs[k].alive || civs[k].barbarian || !((civs[ci].contact >> k) & 1u)) continue;
        // Between two computer rulers the lower index speaks, once.
        if (civs[k].ai && k < ci) continue;
        meet(ci, k);
    }
}

std::string Game::answer_talk(bool accept)
{
    if (talks.empty()) return std::string();
    const Talk t = talks.front();
    talks.erase(talks.begin());
    if (!talk_has_choice(t)) return std::string();
    std::string what;
    const bool yes = settle(t, accept, &what);
    if (!yes && (t.kind == TalkKind::Ceasefire || t.kind == TalkKind::Peace || t.kind == TalkKind::TechForPeace || t.kind == TalkKind::Embassy))
        what = "You send the " + civs[t.from].name + " envoy away.";
    if (!what.empty()) say(t.to, yes ? MsgKind::Good : MsgKind::Bad, what);
    return what;
}

std::string Game::propose(int from, int to, Proposal p, bool *accepted)
{
    bool yes = false;
    std::string out;
    const Attitude at = attitude(to, from);
    const std::string them = leader_greeting(to);
    if (accepted) *accepted = false;
    if (from == to || is_barbarian(to) || !civs[to].alive) return std::string();
    if (civs[from].last_talk[to] == turn && p != Proposal::CancelTreaty) {
        return them + ": \"We have spoken already this year. Come back another time.\"";
    }
    civs[from].last_talk[to] = civs[to].last_talk[from] = turn;
    switch (p) {
    case Proposal::Ceasefire:
        if (!at_war(from, to)) return them + ": \"We are not at war.\"";
        yes = !at.hostile || at.fear > 0 || senate(to);
        if (yes) make_ceasefire(from, to);
        out = them + (yes ? ": \"Very well. Our armies will stand down.\"" : ": \"You will not escape so easily.\"");
        break;
    case Proposal::Peace:
        if (treaty(from, to) == Treaty::Peace) return them + ": \"We are already at peace.\"";
        if (at_war(from, to)) return them + ": \"First let the fighting stop.\"";
        yes = !at.hostile || senate(to);
        if (yes) make_peace(from, to);
        out = them + (yes ? ": \"Let it be peace, then.\"" : ": \"Peace? We think not.\"");
        break;
    case Proposal::Embassy:
        yes = !at.hostile;
        if (yes) {
            civs[from].embassy |= bit(to);
            civs[to].embassy |= bit(from);
        }
        out = them + (yes ? ": \"Our envoys will set out at once.\"" : ": \"We have no wish to see your spies among us.\"");
        break;
    case Proposal::CancelTreaty:
        if (!has_treaty(from, to)) return them + ": \"There is no treaty between us.\"";
        if (senate(from)) return "The Senate refuses to break the treaty with the " + civs[to].name + ".";
        declare_war(from, to);
        yes = true;
        out = them + ": \"So be it. The treaty is ended, and you will regret it.\"";
        break;
    }
    if (accepted) *accepted = yes;
    return out;
}

// ---------------------------------------------------------------- civil war

int Game::civil_war(int ci, int cap_x, int cap_y)
{
    Civ &old = civs[ci];
    if (!old.ai || old.barbarian) return -1;
    std::vector<int> mine;
    int total = 0;
    for (const City &c : cities) {
        if (c.owner == ci) {
            mine.push_back(c.id);
            total += c.size;
        }
    }
    if (static_cast<int>(mine.size()) + 1 <= 4) return -1;   // more than four, counting the capital just lost
    if (human >= 0 && human < static_cast<int>(civs.size()) && civ_score(ci) <= civ_score(human)) return -1;
    // A free place: a dead people's, or a new one.
    int slot = -1;
    for (int k = 0; k < static_cast<int>(civs.size()); ++k) {
        if (!civs[k].alive && !civs[k].barbarian && k != human) slot = k;
    }
    if (slot < 0 && static_cast<int>(civs.size()) < kMaxCivs) {
        civs.emplace_back();
        slot = static_cast<int>(civs.size()) - 1;
    }
    if (slot < 0) return -1;
    Civ &fresh = civs[slot];
    Civ &was = civs[ci];   // re-read: civs may have grown
    const Civ before = fresh;
    fresh = Civ();
    if (was.tribe >= 0) {
        set_tribe(slot, tribe_pair(was.tribe));
    } else {
        fresh.name = "Rebel " + was.name;
        fresh.leader = "General";
    }
    // A colour of its own if the pair's is taken.
    std::uint32_t used = 0;
    for (int k = 0; k < static_cast<int>(civs.size()); ++k) {
        if (k != slot && civs[k].alive) used |= 1u << civs[k].color;
    }
    if ((used >> fresh.color) & 1u) {
        for (int c = 0; c < 8; ++c) {
            if (!((used >> c) & 1u)) {
                fresh.color = c;
                break;
            }
        }
    }
    (void)before;
    fresh.ai = true;
    fresh.alive = true;
    fresh.techs = was.techs;
    fresh.govt = was.govt;
    fresh.target = was.target;
    fresh.tax = was.tax;
    fresh.lux = was.lux;
    fresh.sci = was.sci;
    fresh.research = was.research;
    fresh.gold = was.gold / 2;
    was.gold -= fresh.gold;
    fresh.contact = static_cast<std::uint8_t>(was.contact | bit(ci));
    was.contact |= bit(slot);
    fresh.vendetta = bit(ci);
    was.vendetta |= bit(slot);
    for (int k = 0; k < static_cast<int>(civs.size()); ++k) {
        civs[k].war &= static_cast<std::uint8_t>(~bit(slot));
        civs[k].peace &= static_cast<std::uint8_t>(~bit(slot));
        civs[k].ceasefire &= static_cast<std::uint8_t>(~bit(slot));
        civs[k].embassy &= static_cast<std::uint8_t>(~bit(slot));
        civs[k].vendetta &= static_cast<std::uint8_t>(k == ci ? 0xFF : ~bit(slot));
        civs[k].last_talk[slot] = 0;
        if ((fresh.contact >> k) & 1u) civs[k].contact |= bit(slot);
    }
    fresh.war = bit(ci);
    civs[ci].war |= bit(slot);
    // Which cities go: those on other continents when the people there
    // outnumber (twice over) the home continent's but are not more than
    // it; otherwise the farthest from the lost capital until the rebels
    // hold a third of the people.
    const int home = map.at(cap_x, cap_y).continent;
    int home_pop = 0, other_pop = 0;
    for (int id : mine) {
        const City &c = *city(id);
        (map.at(c.x, c.y).continent == home ? home_pop : other_pop) += c.size;
    }
    std::vector<int> going;
    if (other_pop * 2 > home_pop && other_pop <= home_pop) {
        for (int id : mine) {
            const City &c = *city(id);
            if (map.at(c.x, c.y).continent != home) going.push_back(id);
        }
    } else {
        std::vector<int> order = mine;
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            const City &x = *city(a), &y = *city(b);
            const int da = map.distance(x.x, x.y, cap_x, cap_y), db = map.distance(y.x, y.y, cap_x, cap_y);
            return da != db ? da > db : a < b;
        });
        int pop = 0;
        for (int id : order) {
            if (pop * 3 >= total || going.size() + 1 >= order.size()) break;
            going.push_back(id);
            pop += city(id)->size;
        }
    }
    if (going.empty()) {
        civs[slot].alive = false;
        return -1;
    }
    int far_id = -1, far_d = -1;
    for (int id : going) {
        City &c = *city(id);
        c.owner = slot;
        c.manual = false;
        c.celebrating = false;
        const int d = map.distance(c.x, c.y, cap_x, cap_y);
        if (d > far_d) {
            far_d = d;
            far_id = id;
        }
    }
    // Units in a city side with it; in the field, a unit sides with its
    // home city -- but a square keeps to one side (the one most of its
    // units are for, the old ruler on a tie), and cargo stays with its ship.
    std::map<std::pair<int, int>, std::pair<int, int>> votes;   // square: (old, rebels)
    for (const Unit &u : units) {
        if (u.owner != ci || city_at(u.x, u.y) || u.carried_by >= 0) continue;
        const City *home_city = city(u.home);
        auto &v = votes[{u.x, u.y}];
        if (home_city && home_city->owner == slot) ++v.second;
        else ++v.first;
    }
    for (Unit &u : units) {
        if (u.owner != ci || u.carried_by >= 0) continue;
        if (const City *here = city_at(u.x, u.y)) {
            if (here->owner == slot) u.owner = slot;
        } else {
            const auto &v = votes[{u.x, u.y}];
            if (v.second > v.first) u.owner = slot;
        }
    }
    for (Unit &u : units) {
        if (u.owner != ci || u.carried_by < 0) continue;
        if (const Unit *ship = unit(u.carried_by)) u.owner = ship->owner;
    }
    for (Unit &u : units) {
        if (u.owner != ci && u.owner != slot) continue;
        // Support stays with one's own side.
        const City *h = city(u.home);
        const City *here = city_at(u.x, u.y);
        if (h && h->owner != u.owner) u.home = here && here->owner == u.owner ? here->id : -1;
    }
    if (City *p = city(far_id)) p->add(Building::Palace);
    for (City &c : cities) {
        if (c.owner == slot || c.owner == ci) update_city(c);
    }
    ++civil_wars;
    const std::string msg = "Civil war! The " + civs[ci].name + " are torn apart: the " + civs[slot].name + " under " + civs[slot].leader +
                            " rise in " + std::to_string(going.size()) + (going.size() == 1 ? " city." : " cities.");
    say(-1, MsgKind::Bad, msg, city(far_id)->x, city(far_id)->y);
    refresh_visibility();
    return slot;
}

} // namespace civ
