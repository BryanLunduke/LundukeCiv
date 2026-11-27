// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>

namespace civ {

namespace {

const CivPreset kPresets[] = {
    {"Sumerians", "Gilgamesh", 1, {"Ur", "Uruk", "Eridu", "Lagash", "Nippur", "Kish", "Umma", "Larsa", "Isin", "Adab",
                                   "Shuruppak", "Sippar", "Akshak", "Girsu", "Bad-tibira", "Zabalam"}},
    {"Egyptians", "Ramesses", 2, {"Thebes", "Memphis", "Heliopolis", "Abydos", "Elephantine", "Hermopolis", "Edfu",
                                  "Giza", "Avaris", "Bubastis", "Tanis", "Sais", "Akhetaten", "Buto", "Coptos", "Hierakonpolis"}},
    {"Greeks", "Pericles", 3, {"Athens", "Sparta", "Corinth", "Argos", "Thebes", "Delphi", "Megara", "Miletus",
                               "Ephesus", "Rhodes", "Syracuse", "Olympia", "Knossos", "Mycenae", "Chalcis", "Eretria"}},
    {"Carthaginians", "Hannibal", 4, {"Carthage", "Utica", "Hippo", "Hadrumetum", "Lixus", "Gades", "Motya",
                                      "Panormus", "Leptis", "Thapsus", "Kerkouane", "Sabratha", "Tharros", "Nora", "Sulci", "Ebusus"}},
    {"Persians", "Cyrus", 5, {"Persepolis", "Pasargadae", "Susa", "Ecbatana", "Babylon", "Sardis", "Bactra",
                              "Gordion", "Tarsus", "Arbela", "Rhagae", "Hecatompylos", "Kerman", "Gabae", "Maracanda", "Zadracarta"}},
    {"Norse", "Harald", 6, {"Trondheim", "Uppsala", "Hedeby", "Birka", "Kaupang", "Roskilde", "Ribe", "Sigtuna",
                            "Jelling", "Lund", "Bergen", "Oslo", "Aarhus", "Viborg", "Tunsberg", "Skiringssal"}},
    {"Mayans", "Pacal", 7, {"Palenque", "Tikal", "Calakmul", "Copan", "Uxmal", "Chichen", "Caracol", "Yaxchilan",
                            "Bonampak", "Quirigua", "Coba", "Mayapan", "Piedras", "Dzibanche", "Naranjo", "Altun Ha"}},
};

const char *kPlayerCities[] = {
    "Lundukia", "Amberford", "Tuxhaven", "Penhallow", "Greywater", "Stonebridge", "Kestrel", "Marrowdale",
    "Fennick", "Oakhollow", "Brightmoor", "Wyndham", "Larkspur", "Copperhill", "Ravensby", "Silverlake",
    "Thornbury", "Ashcombe", "Glenmarch", "Highwold", "Saltmere", "Dunmoor", "Elderfield", "Bramblewick",
};

const int kCross[20][2] = {
    {-1, -2}, {0, -2}, {1, -2}, {-2, -1}, {-1, -1}, {0, -1}, {1, -1}, {2, -1}, {-2, 0}, {-1, 0},
    {1, 0},   {2, 0},  {-2, 1}, {-1, 1},  {0, 1},   {1, 1},  {2, 1},  {-1, 2}, {0, 2},  {1, 2},
};

bool military(UnitType t)
{
    return !(unit_info(t).flags & kNonMilitary) && unit_info(t).attack + unit_info(t).defense > 0;
}

} // namespace

const CivPreset &civ_preset(int i) { return kPresets[i % civ_preset_count()]; }
int civ_preset_count() { return static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0])); }
const char *player_city_name(int i) { return kPlayerCities[i % static_cast<int>(sizeof(kPlayerCities) / sizeof(kPlayerCities[0]))]; }

// ---------------------------------------------------------------- setup

Game Game::create(const GameSettings &s)
{
    Game g;
    g.seed = s.seed;
    g.rng.reseed(s.seed ^ 0xC1F1u);
    g.difficulty = s.difficulty;
    g.all_ai = s.all_ai;
    MapSettings ms;
    ms.seed = s.seed;
    ms.land_pct = s.land_pct;
    g.map = generate_map(ms);
    const int n = std::clamp(s.civs, 2, kMaxCivs);
    g.civs.resize(n);
    // The player is civ 0; computer players take presets in a shuffled order.
    std::vector<int> order;
    for (int i = 0; i < civ_preset_count(); ++i) {
        order.push_back(i);
    }
    for (int i = static_cast<int>(order.size()) - 1; i > 0; --i) {
        std::swap(order[i], order[g.rng.below(i + 1)]);
    }
    for (int i = 0; i < n; ++i) {
        Civ &c = g.civs[i];
        if (i == 0) {
            c.name = s.player_name;
            c.leader = s.player_leader;
            c.color = s.player_color;
            c.ai = s.all_ai;
        } else {
            int p = order[(i - 1) % order.size()];
            c.name = kPresets[p].name;
            c.leader = kPresets[p].leader;
            c.color = kPresets[p].color == s.player_color ? 0 : kPresets[p].color;
            c.ai = true;
            c.name_next = (p + 1) * 100; // remembers the preset
        }
        c.gold = 50;
    }
    g.human = 0;
    // Start positions: good land, spread apart, each on a big enough continent.
    std::vector<int> conts(256, 0);
    for (const Tile &t : g.map.tiles) {
        conts[t.continent]++;
    }
    std::vector<std::pair<int, int>> starts;
    for (int i = 0; i < n; ++i) {
        int best = -1, bx = 0, by = 0;
        for (int tries = 0; tries < 3000; ++tries) {
            const int x = g.rng.below(g.map.w), y = 4 + g.rng.below(g.map.h - 8);
            const Tile &t = g.map.at(x, y);
            if (t.terrain != Terrain::Grassland && t.terrain != Terrain::Plains && t.terrain != Terrain::River) {
                continue;
            }
            if (conts[t.continent] < 40 || t.hut) {
                continue;
            }
            int score = 0;
            for (const auto &o : kCross) {
                const Yield yv = g.tile_yield(x + o[0], y + o[1], 0, false);
                if (g.map.valid_y(y + o[1])) {
                    score += yv.food * 3 + yv.shields * 2 + yv.trade;
                }
            }
            int closest = 99;
            for (const auto &p : starts) {
                closest = std::min(closest, g.map.distance(x, y, p.first, p.second));
            }
            if (closest < 12) {
                score -= (12 - closest) * 20;
            }
            if (score > best) {
                best = score;
                bx = x;
                by = y;
            }
        }
        starts.push_back({bx, by});
        g.reveal(i, bx, by, 2);
        g.add_unit(i, UnitType::Settlers, bx, by, -1);
        g.add_unit(i, UnitType::Militia, bx, by, -1);
        if (i == 0 && (s.difficulty == Difficulty::Chieftain || s.difficulty == Difficulty::Warlord)) {
            g.add_unit(i, UnitType::Settlers, bx, by, -1);
        }
    }
    // The barbarians: an extra people with no land, at war with everyone.
    if (n < kMaxCivs) {
        Civ b;
        b.name = "Barbarians";
        b.leader = "Chieftain";
        b.color = 8;
        b.ai = true;
        b.barbarian = true;
        b.gold = 0;
        g.civs.push_back(b);   // at_war() is always true against them
    }
    g.refresh_visibility();
    g.say(0, MsgKind::Info, "Your people, the " + g.civs[0].name + ", await your command, " + g.civs[0].leader + ".");
    return g;
}

// ---------------------------------------------------------------- lookups

Unit *Game::unit(int id)
{
    for (Unit &u : units) {
        if (u.id == id) return &u;
    }
    return nullptr;
}
const Unit *Game::unit(int id) const
{
    for (const Unit &u : units) {
        if (u.id == id) return &u;
    }
    return nullptr;
}
City *Game::city(int id)
{
    for (City &c : cities) {
        if (c.id == id) return &c;
    }
    return nullptr;
}
const City *Game::city(int id) const
{
    for (const City &c : cities) {
        if (c.id == id) return &c;
    }
    return nullptr;
}
City *Game::city_at(int x, int y)
{
    x = map.wrap_x(x);
    for (City &c : cities) {
        if (c.x == x && c.y == y) return &c;
    }
    return nullptr;
}
const City *Game::city_at(int x, int y) const
{
    x = map.wrap_x(x);
    for (const City &c : cities) {
        if (c.x == x && c.y == y) return &c;
    }
    return nullptr;
}
std::vector<int> Game::units_at(int x, int y) const
{
    x = map.wrap_x(x);
    std::vector<int> out;
    for (const Unit &u : units) {
        if (u.x == x && u.y == y) out.push_back(u.id);
    }
    return out;
}
int Game::owner_at(int x, int y) const
{
    x = map.wrap_x(x);
    for (const Unit &u : units) {
        if (u.x == x && u.y == y) return u.owner;
    }
    if (const City *c = city_at(x, y)) return c->owner;
    return -1;
}
bool Game::enemy_at(int civ, int x, int y) const
{
    const int o = owner_at(x, y);
    return o >= 0 && o != civ;
}
City *Game::capital(int civ)
{
    for (City &c : cities) {
        if (c.owner == civ && c.has(Building::Palace)) return &c;
    }
    return nullptr;
}
int Game::cargo_count(int ship_id) const
{
    int n = 0;
    for (const Unit &u : units) {
        if (u.carried_by == ship_id) ++n;
    }
    return n;
}

void Game::say(int civ, MsgKind kind, const std::string &text, int x, int y)
{
    if (text.size() >= 2 && text.compare(text.size() - 2, 2, "..") == 0 && (text.size() < 3 || text[text.size() - 3] != '.')) {
        // "builds Mech. Inf.." -- one full stop is enough.
        say(civ, kind, text.substr(0, text.size() - 1), x, y);
        return;
    }
    Message m;
    m.civ = civ;
    m.kind = kind;
    m.text = text;
    m.x = x;
    m.y = y;
    m.turn = turn;
    messages.push_back(m);
    if (messages.size() > 400) {
        messages.erase(messages.begin(), messages.begin() + 100);
    }
}

// ---------------------------------------------------------------- wonders

const City *Game::wonder_home(Wonder w) const
{
    const int id = wonder_city[static_cast<int>(w)];
    return id >= 0 ? city(id) : nullptr;
}

int Game::wonder_owner(Wonder w) const
{
    const City *c = wonder_home(w);
    return c ? c->owner : -1;
}

bool Game::wonder_obsolete(Wonder w) const
{
    const int o = wonder_owner(w);
    const Advance a = wonder_info(w).obsolete;
    return o >= 0 && a != Advance::None && has(o, a);
}

bool Game::wonder_active(int civ, Wonder w) const
{
    return civ >= 0 && wonder_owner(w) == civ && !wonder_obsolete(w);
}

bool Game::wonder_in(const City &c, Wonder w) const
{
    return wonder_city[static_cast<int>(w)] == c.id && !wonder_obsolete(w);
}

int Game::full_moves(const Unit &u) const
{
    int m = unit_info(u.type).moves;
    if (unit_info(u.type).domain == Domain::Sea) {
        if (wonder_active(u.owner, Wonder::Lighthouse)) ++m;
        if (wonder_active(u.owner, Wonder::Magellan)) ++m;
    }
    return m * 3;
}

bool Game::hoover_here(const City &c) const
{
    if (!wonder_active(c.owner, Wonder::HooverDam)) return false;
    const City *home = wonder_home(Wonder::HooverDam);
    return home && map.at(home->x, home->y).continent == map.at(c.x, c.y).continent;
}

int Game::wonders_in(const City &c) const
{
    int n = 0;
    for (int w = 0; w < kWonderCount; ++w) n += wonder_city[w] == c.id;
    return n;
}

void Game::announce_wonder_start(const City &c)
{
    if (c.build_unit || !is_wonder_build(c.build)) return;
    const int w = c.build - kWonderBase;
    std::uint32_t &bits = wonder_started[static_cast<size_t>(c.owner)];
    if ((bits >> w) & 1u) return;
    bits |= 1u << w;
    Message m;
    m.civ = -1;
    m.kind = MsgKind::Info;
    m.text = "The " + civs[c.owner].name + " begin work on " + wonder_title(w) + " in " + c.name + ".";
    m.x = c.x;
    m.y = c.y;
    m.turn = turn;
    messages.push_back(m);
}

void Game::complete_wonder(City &c, Wonder w)
{
    const int wi = static_cast<int>(w);
    wonder_city[wi] = c.id;
    Message m;
    m.civ = -1;
    m.kind = MsgKind::Wonder;
    m.wonder = wi;
    m.text = "The " + civs[c.owner].name + " complete " + wonder_title(wi) + " in " + c.name + "!";
    m.x = c.x;
    m.y = c.y;
    m.turn = turn;
    messages.push_back(m);
    Civ &civ = civs[c.owner];
    switch (w) {
    case Wonder::Darwin: {
        // The advance under study, and one more.
        for (int k = 0; k < 2; ++k) {
            if (civ.research == Advance::None) {
                if (civ.ai) choose_research_ai(c.owner);
                else {
                    const auto opts = researchable(c.owner);
                    if (!opts.empty()) civ.research = opts.front();
                }
            }
            if (civ.research == Advance::None) break;
            const Advance a = civ.research;
            learn(c.owner, a);
            say(c.owner, MsgKind::Tech, std::string("Darwin's Voyage: your scientists discover ") + advance_info(a).name + "!");
            if (civ.ai) choose_research_ai(c.owner);
        }
        break;
    }
    case Wonder::Apollo:
        for (const City &o : cities) reveal(c.owner, o.x, o.y, 1);
        break;
    default: break;
    }
    // Everyone else building it must switch: to another wonder if one is open.
    for (City &o : cities) {
        if (o.id == c.id || o.build_unit || o.build != kWonderBase + wi) continue;
        int alt = -1;
        for (int k = 0; k < kWonderCount; ++k) {
            if (can_build(o, false, kWonderBase + k)) {
                alt = kWonderBase + k;
                break;
            }
        }
        if (alt >= 0) {
            o.build = alt;
            say(o.owner, MsgKind::Bad, o.name + " can no longer build " + wonder_title(wi) + "; work turns to " + wonder_title(alt - kWonderBase) + ".",
                o.x, o.y);
            announce_wonder_start(o);
        } else {
            say(o.owner, MsgKind::Bad, o.name + " can no longer build " + wonder_title(wi) + ".", o.x, o.y);
            if (civs[o.owner].ai) ai_city(o);
        }
    }
}

std::string Game::city_display_name(int id) const
{
    const City *c = city(id);
    return c ? c->name : std::string("?");
}

std::string Game::unit_name(const Unit &u) const
{
    std::string adj = civs[u.owner].name;
    return (u.veteran ? "Veteran " : "") + adj + " " + unit_info(u.type).name;
}

// ---------------------------------------------------------------- knowledge

int Game::techs_known(int civ) const
{
    int n = 0;
    for (bool b : civs[civ].techs) n += b ? 1 : 0;
    return n + civs[civ].future;
}

bool Game::can_research(int civ, Advance a) const
{
    if (a == Advance::None) return false;
    const AdvanceInfo &i = advance_info(a);
    if (a != Advance::FutureTech && has(civ, a)) return false;
    return has(civ, i.req1) && has(civ, i.req2);
}

std::vector<Advance> Game::researchable(int civ) const
{
    std::vector<Advance> out;
    for (int i = 0; i < kAdvanceCount; ++i) {
        if (can_research(civ, static_cast<Advance>(i))) out.push_back(static_cast<Advance>(i));
    }
    return out;
}

int Game::research_cost(int civ) const
{
    // Linear in advances known, steeper on harder levels (see README).
    const int mult = 6 + 2 * static_cast<int>(difficulty);
    return std::max(mult, (techs_known(civ) + 1) * mult);
}

void Game::learn(int civ, Advance a)
{
    if (a == Advance::FutureTech) {
        civs[civ].future++;
    } else {
        civs[civ].techs[static_cast<int>(a)] = true;
    }
    if (civs[civ].research == a) {
        civs[civ].research = Advance::None;
    }
}

bool Game::unit_available(int civ, UnitType u) const
{
    const UnitInfo &i = unit_info(u);
    if (i.flags & kNoBuild) return false;
    if (!has(civ, i.tech)) return false;
    if (i.obsolete != Advance::None && has(civ, i.obsolete)) return false;
    if ((i.flags & kNuclear) && !wonder_built(Wonder::Manhattan)) return false; // needs the Manhattan Project somewhere
    return true;
}

bool Game::can_build(const City &c, bool is_unit, int what) const
{
    if (is_unit) {
        if (what < 0 || what >= kUnitTypeCount) return false;
        const UnitInfo &i = unit_info(what);
        if (!unit_available(c.owner, static_cast<UnitType>(what))) return false;
        if (i.domain == Domain::Sea && !map.coastal(c.x, c.y)) return false;
        return true;
    }
    if (is_wonder_build(what)) {
        const Wonder w = static_cast<Wonder>(what - kWonderBase);
        const WonderInfo &wi = wonder_info(w);
        if (wonder_built(w) || !has(c.owner, wi.tech)) return false;
        return wi.obsolete == Advance::None || !has(c.owner, wi.obsolete);
    }
    if (is_space_build(what)) {
        const Civ &cv = civs[c.owner];
        if (!space_open() || cv.launch_turn >= 0 || !has(c.owner, building_info(what).tech)) return false;
        switch (static_cast<SpacePart>(what - kSpaceBase)) {
        case SpacePart::Structural: return cv.structurals < kMaxStructurals;
        case SpacePart::Component: return cv.components < kMaxComponents;
        case SpacePart::Module: return cv.modules < kMaxModules;
        default: return false;
        }
    }
    if (what < 0 || what >= kBuildingCount) return false;
    const Building b = static_cast<Building>(what);
    if (c.has(b) || !has(c.owner, building_info(b).tech)) return false;
    switch (b) {
    case Building::University: return c.has(Building::Library);
    case Building::Bank: return c.has(Building::Marketplace);
    case Building::PowerPlant:
    case Building::HydroPlant:
    case Building::NuclearPlant:
    case Building::MfgPlant: return c.has(Building::Factory);
    case Building::Barracks: return true;
    default: return true;
    }
}

// ---------------------------------------------------------------- yields

Yield Game::tile_yield(int x, int y, int civ, bool centre) const
{
    if (!map.valid_y(y)) return {};
    const Tile &t = map.at(x, y);
    const TerrainInfo &ti = terrain_info(t.terrain);
    Yield v = t.special ? ti.with_special : ti.base;
    const bool can_irr = ti.irrigate_turns > 0 && ti.irrigate_to == t.terrain;
    if ((t.irrigation || centre) && can_irr) v.food += ti.irrigate_food;
    if (t.mine) v.shields += ti.mine_shields;
    if (t.road || centre) v.trade += ti.road_trade;
    const Government g = civ >= 0 && civ < static_cast<int>(civs.size()) ? civs[civ].govt : Government::Despotism;
    if (g == Government::Despotism || g == Government::Anarchy) {
        if (v.food >= 3) v.food -= 1;
        if (v.shields >= 3) v.shields -= 1;
        if (v.trade >= 3) v.trade -= 1;
    } else if (g == Government::Republic || g == Government::Democracy) {
        if (v.trade >= 1) v.trade += 1;
    }
    if (centre) v.shields = std::max(1, v.shields);
    if (t.railroad) v.shields += v.shields / 2;
    return v;
}

bool Game::tile_free_for(const City &c, int x, int y) const
{
    if (!map.valid_y(y)) return false;
    x = map.wrap_x(x);
    if (city_at(x, y)) return false;
    for (const Unit &u : units) {
        if (u.x == x && u.y == y && u.owner != c.owner) return false;
    }
    for (const City &o : cities) {
        if (o.id == c.id) continue;
        for (const auto &w : o.worked) {
            if (map.wrap_x(o.x + w.first) == x && o.y + w.second == y) return false;
        }
    }
    return true;
}

void Game::arrange_workers(City &c)
{
    c.worked.clear();
    struct Cand {
        int dx, dy;
        Yield y;
    };
    std::vector<Cand> cands;
    for (const auto &o : kCross) {
        const int x = c.x + o[0], y = c.y + o[1];
        if (!tile_free_for(c, x, y)) continue;
        cands.push_back({o[0], o[1], tile_yield(x, y, c.owner, false)});
    }
    int workers = std::max(0, c.size - c.specialists());
    const Yield centre = tile_yield(c.x, c.y, c.owner, true);
    int food = centre.food;
    const int need = c.size * 2 + 2; // eaters plus room to grow
    for (int k = 0; k < workers && !cands.empty(); ++k) {
        const bool hungry = food < need;
        auto score = [&](const Cand &a) {
            return hungry ? a.y.food * 6 + a.y.shields * 3 + a.y.trade * 2 : a.y.food * 3 + a.y.shields * 3 + a.y.trade * 2;
        };
        auto best = std::max_element(cands.begin(), cands.end(), [&](const Cand &a, const Cand &b) { return score(a) < score(b); });
        c.worked.push_back({best->dx, best->dy});
        food += best->y.food;
        cands.erase(best);
    }
    // Not enough land: the rest entertain.
    settle_specialists(c);
}

void Game::settle_specialists(City &c) const
{
    const int total = std::max(0, c.size - static_cast<int>(c.worked.size()));
    if (c.size < kSpecialistSize) c.taxmen = c.scientists = 0;
    c.taxmen = std::clamp(c.taxmen, 0, total);
    c.scientists = std::clamp(c.scientists, 0, total - c.taxmen);
    c.entertainers = total - c.taxmen - c.scientists;
}

void Game::update_city(City &c)
{
    if (!c.manual) {
        c.entertainers = c.taxmen = c.scientists = 0;
        arrange_workers(c);
    } else {
        // Drop tiles that became unavailable.
        std::vector<std::pair<int, int>> keep;
        for (const auto &w : c.worked) {
            if (tile_free_for(c, c.x + w.first, c.y + w.second)) keep.push_back(w);
        }
        // A city that shrank has fewer workers to place.
        if (static_cast<int>(keep.size()) > c.size) keep.resize(static_cast<size_t>(std::max(0, c.size)));
        c.worked = keep;
        settle_specialists(c);
    }
    const Civ &civ = civs[c.owner];
    for (int pass = 0; pass < 12; ++pass) {
        CityStats s;
        const bool colossus = wonder_in(c, Wonder::Colossus);
        Yield raw = tile_yield(c.x, c.y, c.owner, true);
        if (colossus && raw.trade > 0) raw.trade++;
        for (const auto &w : c.worked) {
            const Yield y = tile_yield(c.x + w.first, c.y + w.second, c.owner, false);
            raw.food += y.food;
            raw.shields += y.shields;
            raw.trade += y.trade + (colossus && y.trade > 0 ? 1 : 0);
        }
        s.base_trade = raw.trade;
        // Trade routes: a share of both cities' trade.
        c.routes.erase(std::remove_if(c.routes.begin(), c.routes.end(), [&](int id) { return city(id) == nullptr; }), c.routes.end());
        for (int id : c.routes) {
            const City *o = city(id);
            CityStats mine;
            mine.base_trade = raw.trade;
            City probe;
            probe.x = c.x;
            probe.y = c.y;
            probe.owner = c.owner;
            probe.stats = mine;
            s.route_trade += route_value(probe, *o);
        }
        raw.trade += s.route_trade;
        s.raw = raw;
        // Unit support.
        const GovernmentInfo &gi = government_info(civ.govt);
        int supported = 0;
        for (const Unit &u : units) {
            if (u.home != c.id) continue;
            if (unit_info(u.type).flags & kSettlers) s.food_upkeep += gi.settler_food;
            ++supported;
        }
        const int free_units = gi.free_units < 0 ? c.size : gi.free_units;
        s.shield_upkeep = std::max(0, supported - free_units);
        s.food_surplus = raw.food - c.size * 2 - s.food_upkeep;
        int shields = raw.shields;
        if (c.has(Building::Factory)) {
            int pct = 150;
            if (c.has(Building::PowerPlant) || c.has(Building::HydroPlant) || c.has(Building::NuclearPlant) || hoover_here(c)) pct += 50;
            if (c.has(Building::MfgPlant)) pct += 100;
            shields = shields * pct / 100;
        }
        s.shield_surplus = shields - s.shield_upkeep;
        // Corruption grows with distance from the Palace.
        int dist = 20;
        for (const City &o : cities) {
            if (o.owner == c.owner && o.has(Building::Palace)) dist = map.distance(c.x, c.y, o.x, o.y);
        }
        int corr_pct = civ.govt == Government::Communism ? 20 : std::min(60, gi.corruption_pct * dist / 10);
        if (civ.govt == Government::Democracy) corr_pct = 0;
        if (c.has(Building::Courthouse)) corr_pct /= 2;
        if (c.has(Building::Palace)) corr_pct = 0;
        s.corruption = raw.trade * corr_pct / 100;
        const int trade = raw.trade - s.corruption;
        int sci = (trade * civ.sci + 5) / 10;
        int lux = std::min(trade - sci, (trade * civ.lux + 5) / 10);
        int tax = trade - sci - lux;
        if (civ.govt == Government::Anarchy) {
            lux += sci + tax;
            sci = tax = 0;
        }
        // Specialists, before the Marketplace, Bank, Library and University
        // multiply them (DOS order). Under Anarchy scientists find nothing.
        lux += c.entertainers * 2;
        tax += c.taxmen * 2;
        if (civ.govt != Government::Anarchy) sci += c.scientists * 2;
        if (c.has(Building::Marketplace)) {
            tax = tax * 3 / 2;
            lux = lux * 3 / 2;
        }
        if (c.has(Building::Bank)) {
            tax = tax * 3 / 2;
            lux = lux * 3 / 2;
        }
        // Libraries and Universities: +50% each, +83% with Isaac Newton's College.
        const int lib_pct = wonder_active(c.owner, Wonder::Newton) ? 183 : 150;
        if (c.has(Building::Library)) sci = sci * lib_pct / 100;
        if (c.has(Building::University)) sci = sci * lib_pct / 100;
        if (wonder_in(c, Wonder::Copernicus)) sci *= 2;
        if (wonder_active(c.owner, Wonder::Seti)) sci = sci * 3 / 2;
        s.tax = tax;
        s.lux = lux;
        s.sci = sci;
        // Happiness.
        const int workers = c.size - c.specialists();
        int content = std::min(workers, content_base(difficulty));
        int unhappy = workers - content;
        int happy = 0;
        int l = lux;
        while (l >= 2) {
            if (content > 0) {
                --content;
                ++happy;
            } else if (unhappy > 0) {
                --unhappy;
                ++content;
            } else {
                break;
            }
            l -= 2;
        }
        auto calm = [&](int n) {
            const int k = std::min(n, unhappy);
            unhappy -= k;
            content += k;
        };
        if (c.has(Building::Temple)) {
            const int t = has(c.owner, Advance::Mysticism) ? 2 : 1;
            calm(wonder_active(c.owner, Wonder::Oracle) ? t * 2 : t);
        }
        if (c.has(Building::Colosseum)) calm(3);
        if (c.has(Building::Cathedral)) calm(wonder_active(c.owner, Wonder::Michelangelo) ? 6 : 4);
        if (wonder_active(c.owner, Wonder::Bach)) {
            const City *home = wonder_home(Wonder::Bach);
            if (home && map.at(home->x, home->y).continent == map.at(c.x, c.y).continent) calm(2);
        }
        // Hanging Gardens and the Cure for Cancer: one more happy citizen.
        auto cheer = [&]() {
            if (content > 0) {
                --content;
                ++happy;
            } else if (unhappy > 0) {
                --unhappy;
                ++content;
            }
        };
        if (wonder_active(c.owner, Wonder::HangingGardens)) cheer();
        if (wonder_active(c.owner, Wonder::CureForCancer)) cheer();
        if (civ.govt == Government::Despotism || civ.govt == Government::Monarchy || civ.govt == Government::Communism ||
            civ.govt == Government::Anarchy) {
            int troops = 0;
            for (const Unit &u : units) {
                if (u.owner == c.owner && u.x == c.x && u.y == c.y && military(u.type)) ++troops;
            }
            calm(std::min(3, troops));
        }
        if (civ.govt == Government::Republic || civ.govt == Government::Democracy) {
            // Troops away from home make the people unhappy: one each in a
            // Republic, two in a Democracy (less with Women's Suffrage).
            int per = civ.govt == Government::Democracy ? 2 : 1;
            if (wonder_active(c.owner, Wonder::Suffrage)) --per;
            int away = 0;
            for (const Unit &u : units) {
                if (u.home != c.id || !military(u.type) || unit_info(u.type).attack == 0) continue;
                const City *in = city_at(u.x, u.y);
                if (in && in->owner == c.owner) continue;
                if (map.at(u.x, u.y).fortress && map.distance(u.x, u.y, c.x, c.y) <= 3) continue;
                ++away;
            }
            for (int k = 0; k < away * per; ++k) {
                if (content > 0) {
                    --content;
                    ++unhappy;
                } else if (happy > 0) {
                    --happy;
                    ++content;
                }
            }
        }
        if (wonder_in(c, Wonder::Shakespeare)) calm(unhappy);
        s.happy = happy;
        s.content = content;
        s.unhappy = unhappy;
        s.disorder = unhappy > happy;
        s.food_box = food_box(c);
        c.stats = s;
        // Computer players and auto-arranged cities keep order with entertainers.
        if (!s.disorder || c.manual || c.worked.empty()) break;
        // Retire the poorest worker.
        auto worst = std::min_element(c.worked.begin(), c.worked.end(), [&](const auto &a, const auto &b) {
            const Yield ya = tile_yield(c.x + a.first, c.y + a.second, c.owner, false);
            const Yield yb = tile_yield(c.x + b.first, c.y + b.second, c.owner, false);
            return ya.food * 3 + ya.shields * 2 + ya.trade < yb.food * 3 + yb.shields * 2 + yb.trade;
        });
        c.worked.erase(worst);
        settle_specialists(c);
    }
}

int Game::ai_cost_percent(int civ) const
{
    if (civ < 0 || civ >= static_cast<int>(civs.size()) || !civs[civ].ai) return 100;
    static const int pct[] = {110, 90, 75, 65, 55};
    return pct[static_cast<int>(difficulty)];
}

int Game::production_cost(const City &c) const
{
    const int base = c.build_unit ? unit_info(c.build).cost : building_info(c.build).cost;
    return std::max(1, base * ai_cost_percent(c.owner) / 100);
}

int Game::buy_cost(const City &c) const
{
    const int left = std::max(0, production_cost(c) - c.shields);
    if (left == 0) return 0;
    int cost = 2 * left + left * left / 20;
    if (c.shields == 0) cost *= 2;
    if (c.build_unit) cost = cost * 2; // units cost more to rush
    return cost;
}

// ---------------------------------------------------------------- space race

bool Game::space_open() const { return wonder_built(Wonder::Apollo); }

SpaceStats Game::space_stats(int ci) const
{
    SpaceStats s;
    const Civ &c = civs[ci];
    s.fuel = (c.components + 1) / 2;
    s.propulsion = c.components / 2;
    s.pairs = std::min(s.fuel, s.propulsion);
    s.habitation = (c.modules + 2) / 3;
    s.life_support = (c.modules + 1) / 3;
    s.solar = c.modules / 3;
    s.unsupported = std::max(0, c.components + c.modules - c.structurals);
    s.population = s.habitation * 10000;
    s.life_pct = s.habitation ? std::min(100, s.life_support * 100 / s.habitation) : 0;
    const int need = s.habitation + s.life_support;
    s.energy_pct = need ? std::min(100, s.solar * 2 * 100 / need) : 0;
    s.success_pct = s.life_pct * s.energy_pct / 100;
    s.mass = 400 * c.structurals + 800 * c.components + 1600 * c.modules;
    const int units = c.structurals + 2 * c.components + 4 * c.modules;
    s.years = s.pairs ? 4.0 + 0.6 * units / s.pairs : 0;
    s.viable = s.pairs >= 1 && s.habitation >= 1 && s.life_support >= 1 && s.solar >= 1 && s.unsupported == 0;
    return s;
}

bool Game::can_launch(int ci) const
{
    const Civ &c = civs[ci];
    bool palace = false;
    for (const City &o : cities) palace |= o.owner == ci && o.has(Building::Palace);
    return c.alive && c.launch_turn < 0 && space_open() && palace && space_stats(ci).viable;
}

bool Game::launch(int ci)
{
    if (!can_launch(ci)) return false;
    Civ &c = civs[ci];
    const SpaceStats s = space_stats(ci);
    const int target = year_for_turn(turn) + static_cast<int>(std::ceil(s.years));
    int t = turn + 1;
    while (year_for_turn(t) < target) ++t;
    c.launch_turn = turn;
    c.arrival_turn = t;
    const City *cap = capital(ci);
    const std::string news = "The " + c.name + " launch a spaceship from " + (cap ? cap->name : std::string("their capital")) +
                             "! It will reach Alpha Centauri in " + year_text(year_for_turn(t)) + ".";
    for (int o = 0; o < static_cast<int>(civs.size()); ++o) {
        if (civs[o].alive && !civs[o].barbarian) say(o, o == ci ? MsgKind::Good : MsgKind::Bad, news, cap ? cap->x : -1, cap ? cap->y : -1);
    }
    return true;
}

int Game::arrival_year(int ci) const
{
    return civs[ci].arrival_turn >= 0 ? year_for_turn(civs[ci].arrival_turn) : -1;
}

void Game::lose_spaceship(int ci, bool in_flight)
{
    Civ &c = civs[ci];
    c.structurals = c.components = c.modules = 0;
    c.launch_turn = c.arrival_turn = -1;
    say(-1, MsgKind::Bad, in_flight ? "With their capital lost, the " + c.name + "' spaceship is lost in space."
                                    : "With their capital lost, the " + c.name + "' spaceship is destroyed on the pad.");
}

void Game::space_arrivals()
{
    if (winner >= 0) return;
    for (int ci = 0; ci < static_cast<int>(civs.size()); ++ci) {
        Civ &c = civs[ci];
        if (c.arrival_turn < 0 || turn < c.arrival_turn || !c.alive) continue;
        const SpaceStats s = space_stats(ci);
        if (rng.below(100) < s.success_pct) {
            winner = ci;
            space_win = true;
            say(-1, MsgKind::Good, "The " + c.name + "' colonists reach Alpha Centauri! A new world begins.");
            return;
        }
        say(-1, MsgKind::Bad, "The " + c.name + "' spaceship is lost on the way to Alpha Centauri.");
        c.structurals = c.components = c.modules = 0;
        c.launch_turn = c.arrival_turn = -1;
    }
}

// ---------------------------------------------------------------- barbarians

int Game::barbarian_civ() const
{
    for (int i = 0; i < static_cast<int>(civs.size()); ++i) {
        if (civs[i].barbarian) return i;
    }
    return -1;
}

int Game::ransom(int unit_id, int tx, int ty)
{
    const Unit *u = unit(unit_id);
    if (!u || city_at(tx, ty)) return 0;
    const std::vector<int> here = units_at(tx, ty);
    if (here.empty()) return 0;
    for (int id : here) {
        const Unit *v = unit(id);
        if (!v || v->type != UnitType::Leader || v->owner == u->owner) return 0;
    }
    const int me = u->owner;
    const int gold = 100 * static_cast<int>(here.size());
    for (int id : here) kill_unit(id);
    civs[me].gold += gold;
    say(me, MsgKind::Good, "You capture a barbarian leader! His people pay " + std::to_string(gold) + " gold in ransom.", tx, ty);
    return gold;
}

namespace {

std::vector<UnitType> barbarian_army(int era, int difficulty, Rng &rng)
{
    static const std::vector<UnitType> eras[] = {
        {UnitType::Legion, UnitType::Legion, UnitType::Militia},
        {UnitType::Legion, UnitType::Knights, UnitType::Catapult},
        {UnitType::Knights, UnitType::Knights, UnitType::Catapult},
        {UnitType::Musketeers, UnitType::Knights, UnitType::Cannon},
        {UnitType::Riflemen, UnitType::Cavalry, UnitType::Cannon},
        {UnitType::Riflemen, UnitType::Artillery, UnitType::Armor},
    };
    const std::vector<UnitType> &pool = eras[std::clamp(era, 0, 5)];
    std::vector<UnitType> out;
    const int n = 2 + (difficulty >= 3 ? 1 : 0) + rng.below(2);
    for (int i = 0; i < n; ++i) out.push_back(pool[static_cast<size_t>(i) % pool.size()]);
    return out;
}

} // namespace

bool Game::barbarian_landing(int bc, UnitType ship, const std::vector<UnitType> &army)
{
    std::vector<int> targets;
    for (const City &c : cities) {
        if (!civs[c.owner].barbarian && map.coastal(c.x, c.y)) targets.push_back(c.id);
    }
    if (targets.empty()) return false;
    for (int attempt = 0; attempt < 6; ++attempt) {
        const City *c = city(targets[rng.below(static_cast<int>(targets.size()))]);
        std::vector<std::array<int, 4>> spots;   // land x, y, sea x, y
        for (int j = -3; j <= 3; ++j) {
            for (int i = -3; i <= 3; ++i) {
                const int x = map.wrap_x(c->x + i), y = c->y + j;
                const int d = map.distance(x, y, c->x, c->y);
                if (d < 2 || !map.land(x, y) || city_at(x, y) || owner_at(x, y) >= 0) continue;
                for (int b = -1; b <= 1; ++b) {
                    for (int a = -1; a <= 1; ++a) {
                        const int sx = map.wrap_x(x + a), sy = y + b;
                        if (map.ocean(sx, sy) && owner_at(sx, sy) < 0) spots.push_back({x, y, sx, sy});
                    }
                }
            }
        }
        if (spots.empty()) continue;
        const auto sp = spots[rng.below(static_cast<int>(spots.size()))];
        const int sid = add_unit(bc, ship, sp[2], sp[3], -1);
        if (Unit *s = unit(sid)) {
            s->order = Order::Sentry;
            s->work = 0;
        }
        for (UnitType t : army) {
            const int id = add_unit(bc, t, sp[0], sp[1], -1);
            if (Unit *u = unit(id)) u->moves = 0;
        }
        const int lid = add_unit(bc, UnitType::Leader, sp[0], sp[1], -1);
        if (Unit *l = unit(lid)) l->moves = 0;
        say(c->owner, MsgKind::Bad, "Barbarians from the sea land near " + c->name + "!", sp[0], sp[1]);
        return true;
    }
    return false;
}

bool Game::barbarian_uprising(int bc, const std::vector<UnitType> &army)
{
    for (int tries = 0; tries < 400; ++tries) {
        const int x = rng.below(map.w), y = 2 + rng.below(map.h - 4);
        if (!map.land(x, y) || city_at(x, y) || owner_at(x, y) >= 0) continue;
        const City *closest = nullptr;
        int best = 99;
        for (const City &c : cities) {
            if (civs[c.owner].barbarian || map.at(c.x, c.y).continent != map.at(x, y).continent) continue;
            const int d = map.distance(x, y, c.x, c.y);
            if (d < best) {
                best = d;
                closest = &c;
            }
        }
        if (!closest || best < 4 || best > 8) continue;
        for (UnitType t : army) {
            const int id = add_unit(bc, t, x, y, -1);
            if (Unit *u = unit(id)) u->moves = 0;
        }
        const int lid = add_unit(bc, UnitType::Leader, x, y, -1);
        if (Unit *l = unit(lid)) l->moves = 0;
        say(closest->owner, MsgKind::Bad, "A barbarian uprising near " + closest->name + "!", x, y);
        return true;
    }
    return false;
}

void Game::spawn_barbarians(bool force_sea, bool force_land)
{
    const int bc = barbarian_civ();
    if (bc < 0) return;
    const bool forced = force_sea || force_land;
    if (!forced) {
        if (turn < 50) return;
        const double p = 0.015 + 0.005 * static_cast<int>(difficulty);
        if (!rng.chance(p)) return;
    }
    int known = 0;
    for (int c = 0; c < static_cast<int>(civs.size()); ++c) {
        if (!civs[c].barbarian && civs[c].alive) known = std::max(known, techs_known(c));
    }
    const int era = known / 12;
    const std::vector<UnitType> army = barbarian_army(era, static_cast<int>(difficulty), rng);
    const UnitType ship = era <= 1 ? UnitType::Trireme : (era <= 3 ? UnitType::Sail : UnitType::Frigate);
    const bool sea = force_sea || (!force_land && rng.chance(0.5));
    if (sea && barbarian_landing(bc, ship, army)) return;
    if (!force_sea) barbarian_uprising(bc, army);
}

void Game::ai_barbarians(int bc)
{
    std::vector<int> ids;
    for (const Unit &u : units) {
        if (u.owner == bc) ids.push_back(u.id);
    }
    for (City &c : cities) {
        if (c.owner == bc && (c.shields == 0 || !can_build(c, c.build_unit, c.build))) ai_city(c);
    }
    for (int id : ids) {
        Unit *u = unit(id);
        if (!u) continue;
        const UnitInfo &info = unit_info(u->type);
        if (info.domain == Domain::Sea) {
            // The longships sail away once their raiders are ashore.
            if (++u->work >= 3) kill_unit(id);
            continue;
        }
        if (u->type == UnitType::Leader) {
            // Keep close to the strongest band.
            const Unit *band = nullptr;
            int bd = 99;
            for (const Unit &o : units) {
                if (o.owner != bc || o.id == id || !military(o.type)) continue;
                const int d = map.distance(o.x, o.y, u->x, u->y);
                if (d < bd) {
                    bd = d;
                    band = &o;
                }
            }
            if (!band && !city_at(u->x, u->y)) {
                // Alone, with no band to lead: he slips away after a while.
                if (++u->work >= 5) kill_unit(id);
                continue;
            }
            if (band && bd > 0) {
                int sx = 0, sy = 0;
                const int bx = band->x, by = band->y;
                if (next_step(*u, bx, by, sx, sy, 800)) move(id, sx, sy);
            }
            continue;
        }
        // A barbarian city keeps one guard.
        if (const City *home = city_at(u->x, u->y)) {
            int guards = 0;
            for (const Unit &o : units) guards += o.owner == bc && o.x == home->x && o.y == home->y && military(o.type);
            if (guards <= 1) {
                u->order = Order::Fortified;
                continue;
            }
        }
        // March on the nearest city of anyone else on this landmass.
        const int cont = map.at(u->x, u->y).continent;
        const City *target = nullptr;
        int best = 17;
        for (const City &c : cities) {
            if (c.owner == bc || map.at(c.x, c.y).continent != cont) continue;
            const int d = map.distance(u->x, u->y, c.x, c.y);
            if (d < best) {
                best = d;
                target = &c;
            }
        }
        const int tx = target ? target->x : -1, ty = target ? target->y : -1;
        for (int guard = 0; guard < 6; ++guard) {
            u = unit(id);
            if (!u || u->moves <= 0) break;
            u->order = Order::None;
            // Fight anything weak enough next to us; take empty cities.
            int ax = 0, ay = 0;
            double odds = 0;
            for (int j = -1; j <= 1; ++j) {
                for (int i = -1; i <= 1; ++i) {
                    if (!i && !j) continue;
                    const int x = map.wrap_x(u->x + i), y = u->y + j;
                    if (!map.land(x, y)) continue;
                    const int o = owner_at(x, y);
                    if (o < 0 || o == bc) continue;
                    double p = 1.0;
                    for (int did : units_at(x, y)) {
                        const Unit *d = unit(did);
                        p = std::min(p, combat_win_chance(attack_strength(*u), defense_strength(*d, u), u->hp, d->hp));
                    }
                    if (p > odds) {
                        odds = p;
                        ax = i;
                        ay = j;
                    }
                }
            }
            if (odds >= 0.3 && info.attack > 0) {
                move(id, ax, ay);
                continue;
            }
            if (tx < 0) break;
            int sx = 0, sy = 0;
            if (!next_step(*u, tx, ty, sx, sy, 2000)) break;
            const MoveResult r = move(id, sx, sy);
            if (r == MoveResult::Blocked || r == MoveResult::Zoc || r == MoveResult::Failed || r == MoveResult::NoMoves) break;
        }
    }
}

// ---------------------------------------------------------------- diplomats

const char *diplo_action_name(DiploAction a)
{
    static const char *n[] = {"Establish Embassy", "Investigate City", "Steal Technology", "Industrial Sabotage",
                              "Incite a Revolt", "Bribe Unit"};
    return n[static_cast<int>(a)];
}

int Game::incite_cost(const City &c, int civ) const
{
    (void)civ;
    if (c.has(Building::Palace)) return -1;   // a capital never revolts
    // Civ-style: the owner's treasury plus a sum for the city, cheaper far
    // from their capital and in disorder.
    int dist = 32;
    for (const City &o : cities) {
        if (o.owner == c.owner && o.has(Building::Palace)) dist = map.distance(c.x, c.y, o.x, o.y);
    }
    int cost = (civs[c.owner].gold + 1000) * c.size / (dist + 3);
    for (const Unit &u : units) {
        if (u.x == c.x && u.y == c.y) cost += unit_info(u.type).cost;
    }
    if (c.stats.disorder) cost /= 2;
    return std::max(50, cost);
}

int Game::bribe_cost(const Unit &u, int civ) const
{
    (void)civ;
    int dist = 32;
    for (const City &o : cities) {
        if (o.owner == u.owner && o.has(Building::Palace)) dist = map.distance(u.x, u.y, o.x, o.y);
    }
    return std::max(20, (civs[u.owner].gold + 1000) / (dist + 2) * unit_info(u.type).cost / 10);
}

bool Game::diplomat_can(int unit_id, int tx, int ty, DiploAction a) const
{
    const Unit *u = unit(unit_id);
    if (!u || u->type != UnitType::Diplomat) return false;
    tx = map.wrap_x(tx);
    if (!map.valid_y(ty) || map.distance(u->x, u->y, tx, ty) > 1) return false;
    const City *c = city_at(tx, ty);
    const int me = u->owner;
    if (a == DiploAction::Bribe) {
        if (c) return false;
        const std::vector<int> here = units_at(tx, ty);
        if (here.size() != 1) return false;
        const Unit *v = unit(here.front());
        return v && v->owner != me && civs[me].gold >= bribe_cost(*v, me);
    }
    if (!c || c->owner == me) return false;
    switch (a) {
    case DiploAction::Embassy: return !((civs[me].embassy >> c->owner) & 1u);
    case DiploAction::Investigate: return true;
    case DiploAction::StealTech: {
        if ((c->stolen >> me) & 1u) return false;
        for (int t = 0; t < kAdvanceCount; ++t) {
            if (civs[c->owner].techs[t] && !civs[me].techs[t]) return true;
        }
        return false;
    }
    case DiploAction::Sabotage: return true;
    case DiploAction::Incite: {
        const int cost = incite_cost(*c, me);
        return cost > 0 && civs[me].gold >= cost;
    }
    default: return false;
    }
}

std::string Game::diplomat_act(int unit_id, int tx, int ty, DiploAction a)
{
    if (!diplomat_can(unit_id, tx, ty, a)) return std::string();
    Unit *u = unit(unit_id);
    const int me = u->owner;
    tx = map.wrap_x(tx);
    City *c = city_at(tx, ty);
    const int them = c ? c->owner : owner_at(tx, ty);
    civs[me].contact |= static_cast<std::uint8_t>(1u << them);
    civs[them].contact |= static_cast<std::uint8_t>(1u << me);
    // Hostile acts against a people at peace are an incident: war.
    if (diplomat_hostile(a) && !at_war(me, them)) declare_war(me, them);
    std::string out;
    switch (a) {
    case DiploAction::Embassy:
        civs[me].embassy |= static_cast<std::uint8_t>(1u << them);
        out = "Embassy established with the " + civs[them].name + " in " + c->name + ".";
        break;
    case DiploAction::Investigate:
        out = "Your Diplomat reports on " + c->name + ".";
        break;
    case DiploAction::StealTech: {
        std::vector<int> opts;
        for (int t = 0; t < kAdvanceCount; ++t) {
            if (civs[them].techs[t] && !civs[me].techs[t]) opts.push_back(t);
        }
        const int t = opts[rng.below(static_cast<int>(opts.size()))];
        learn(me, static_cast<Advance>(t));
        c->stolen |= static_cast<std::uint8_t>(1u << me);
        out = std::string("Your Diplomat steals the secret of ") + advance_info(t).name + " from " + c->name + "!";
        say(them, MsgKind::Bad, "The " + civs[me].name + " steal " + advance_info(t).name + " from " + c->name + ".", c->x, c->y);
        break;
    }
    case DiploAction::Sabotage: {
        std::vector<int> blds;
        for (int b = 1; b < kBuildingCount; ++b) {
            if (c->has(static_cast<Building>(b))) blds.push_back(b);
        }
        if (blds.empty() || rng.chance(0.4)) {
            out = "Your Diplomat sabotages production in " + c->name + ": " + std::to_string(c->shields) + " shields lost.";
            c->shields = 0;
        } else {
            const int b = blds[rng.below(static_cast<int>(blds.size()))];
            c->remove(static_cast<Building>(b));
            out = std::string("Your Diplomat destroys the ") + building_info(b).name + " in " + c->name + ".";
        }
        say(them, MsgKind::Bad, "Saboteurs from the " + civs[me].name + " strike " + c->name + ".", c->x, c->y);
        update_city(*c);
        break;
    }
    case DiploAction::Incite: {
        const int cost = incite_cost(*c, me);
        civs[me].gold -= cost;
        const int old = c->owner;
        c->owner = me;
        c->manual = false;
        c->build_unit = true;
        c->build = static_cast<int>(UnitType::Militia);
        // Its garrison joins the revolt; units it supported elsewhere disband.
        std::vector<int> drop;
        for (Unit &v : units) {
            if (v.x == c->x && v.y == c->y && v.owner == old) {
                v.owner = me;
                v.home = c->id;
            } else if (v.home == c->id && v.owner == old) {
                drop.push_back(v.id);
            }
        }
        for (int id : drop) kill_unit(id);
        out = c->name + " revolts and joins the " + civs[me].name + " for " + std::to_string(cost) + " gold!";
        say(old, MsgKind::Bad, c->name + " has been incited to revolt by the " + civs[me].name + "!", c->x, c->y);
        update_city(*c);
        reveal(me, c->x, c->y, 2);
        break;
    }
    case DiploAction::Bribe: {
        Unit *v = unit(units_at(tx, ty).front());
        const int cost = bribe_cost(*v, me);
        civs[me].gold -= cost;
        out = std::string("The ") + unit_info(v->type).name + " of the " + civs[v->owner].name + " is bribed to join you for " +
              std::to_string(cost) + " gold.";
        say(v->owner, MsgKind::Bad, std::string("Your ") + unit_info(v->type).name + " is bribed by the " + civs[me].name + ".", tx, ty);
        v->owner = me;
        v->home = -1;
        v->order = Order::None;
        v->moves = 0;
        break;
    }
    default: break;
    }
    say(me, a == DiploAction::Investigate ? MsgKind::Info : MsgKind::Good, out, tx, ty);
    kill_unit(unit_id);
    if (a == DiploAction::Incite) check_elimination();
    return out;
}

// ---------------------------------------------------------------- caravans

bool Game::caravan_can_help(const Unit &u, const City &c) const
{
    return u.type == UnitType::Caravan && c.owner == u.owner && !c.build_unit && is_wonder_build(c.build) &&
           map.distance(u.x, u.y, c.x, c.y) <= 1;
}

bool Game::caravan_can_trade(const Unit &u, const City &c) const
{
    if (u.type != UnitType::Caravan || map.distance(u.x, u.y, c.x, c.y) > 1) return false;
    const City *home = city(u.home);
    if (!home || home->id == c.id) return false;
    if (map.distance(home->x, home->y, c.x, c.y) < 9) return false;
    if (static_cast<int>(home->routes.size()) >= kMaxRoutes) return false;
    return std::find(home->routes.begin(), home->routes.end(), c.id) == home->routes.end();
}

int Game::route_value(const City &a, const City &b) const
{
    // Civ-style: a share of both cities' trade; more for foreign partners
    // and across the sea.
    int v = (a.stats.base_trade + b.stats.base_trade + 4) / 8;
    if (a.owner != b.owner) v = v * 3 / 2;
    if (map.at(a.x, a.y).continent != map.at(b.x, b.y).continent) v = v * 3 / 2;
    return std::max(1, v);
}

int Game::trade_bonus(const Unit &u, const City &c) const
{
    const City *home = city(u.home);
    if (!home) return 0;
    const int d = map.distance(home->x, home->y, c.x, c.y);
    int bonus = (d + 10) * (home->stats.base_trade + c.stats.base_trade) / 24;
    if (home->owner != c.owner) bonus *= 2;
    return std::max(10, bonus);
}

bool Game::caravan_help(int unit_id, int city_id)
{
    Unit *u = unit(unit_id);
    City *c = city(city_id);
    if (!u || !c || !caravan_can_help(*u, *c)) return false;
    c->shields += unit_info(UnitType::Caravan).cost;
    say(u->owner, MsgKind::City, "Your Caravan helps build " + wonder_title(c->build - kWonderBase) + " in " + c->name + ".", c->x, c->y);
    kill_unit(unit_id);
    return true;
}

int Game::caravan_trade(int unit_id, int city_id)
{
    Unit *u = unit(unit_id);
    City *c = city(city_id);
    if (!u || !c || !caravan_can_trade(*u, *c)) return -1;
    City *home = city(u->home);
    const int bonus = trade_bonus(*u, *c);
    const int me = u->owner;
    civs[me].gold += bonus;
    civs[me].bulbs += bonus;
    home->routes.push_back(c->id);
    if (static_cast<int>(c->routes.size()) < kMaxRoutes && std::find(c->routes.begin(), c->routes.end(), home->id) == c->routes.end())
        c->routes.push_back(home->id);
    if (c->owner != me) {
        civs[me].contact |= static_cast<std::uint8_t>(1u << c->owner);
        civs[c->owner].contact |= static_cast<std::uint8_t>(1u << me);
    }
    say(me, MsgKind::Good,
        "Trade route opened between " + home->name + " and " + c->name + ": " + std::to_string(bonus) + " gold and " + std::to_string(bonus) +
            " bulbs, and more trade every turn.",
        c->x, c->y);
    kill_unit(unit_id);
    update_city(*home);
    update_city(*c);
    return bonus;
}

// ---------------------------------------------------------------- combat

double Game::attack_strength(const Unit &a) const
{
    double s = unit_info(a.type).attack;
    if (a.veteran) s *= 1.5;
    return s;
}

double Game::defense_strength(const Unit &d, const Unit *attacker) const
{
    const UnitInfo &ui = unit_info(d.type);
    double s = ui.defense;
    const Tile &t = map.at(d.x, d.y);
    if (ui.domain == Domain::Land) s = s * terrain_info(t.terrain).defense_pct / 100.0;
    const City *c = city_at(d.x, d.y);
    const bool land_attack = attacker && unit_info(attacker->type).domain == Domain::Land;
    const bool walls = c && c->has(Building::CityWalls) && land_attack && !(unit_info(attacker->type).flags & kIgnoreWalls);
    if (walls) {
        s *= 3.0;
    } else if (t.fortress && !c) {
        s *= 2.0;
    } else if (ui.domain == Domain::Land && (d.order == Order::Fortified || c)) {
        s *= 1.5;
    }
    if (d.veteran) s *= 1.5;
    return s;
}

double combat_win_chance(double att, double def, int att_hp, int def_hp)
{
    if (def_hp <= 0) return 1.0;
    if (att_hp <= 0) return 0.0;
    if (att + def <= 0) return 0.5;
    const double p = att / (att + def);
    // win[a][d]: the attacker wins from a hit points against d.
    std::vector<double> row(static_cast<size_t>(def_hp) + 1, 0.0), prev(static_cast<size_t>(def_hp) + 1, 0.0);
    for (int a = 1; a <= att_hp; ++a) {
        row[0] = 1.0;
        for (int d = 1; d <= def_hp; ++d) row[d] = p * row[d - 1] + (1 - p) * prev[d];
        prev = row;
    }
    return prev[def_hp];
}

MoveResult Game::attack(int unit_id, int tx, int ty)
{
    Unit *a = unit(unit_id);
    if (!a) return MoveResult::Failed;
    tx = map.wrap_x(tx);
    std::vector<int> here = units_at(tx, ty);
    if (here.empty()) return MoveResult::Failed;
    // The defender most likely to hold stands up.
    const double att = attack_strength(*a);
    int best = -1;
    double best_hold = -1;
    for (int id : here) {
        const Unit *d = unit(id);
        if (d->carried_by >= 0 && map.ocean(tx, ty)) continue;
        const double hold = 1.0 - combat_win_chance(att, defense_strength(*d, a), a->hp, d->hp);
        if (hold > best_hold) {
            best_hold = hold;
            best = id;
        }
    }
    if (best < 0) best = here.front();
    Unit *d = unit(best);
    const int att_owner = a->owner, def_owner = d->owner;
    if (!at_war(att_owner, def_owner)) declare_war(att_owner, def_owner);
    CombatResult r;
    r.happened = true;
    r.att_type = a->type;
    r.def_type = d->type;
    r.att_owner = att_owner;
    r.def_owner = def_owner;
    r.att = att;
    r.def = defense_strength(*d, a);
    r.round_odds = r.att + r.def > 0 ? r.att / (r.att + r.def) : 1.0;
    r.att_hp0 = a->hp;
    r.def_hp0 = d->hp;
    r.odds = combat_win_chance(r.att, r.def, a->hp, d->hp);
    r.x = tx;
    r.y = ty;
    r.turn = turn;
    // Fight it out round by round.
    int ahp = a->hp, dhp = d->hp;
    while (ahp > 0 && dhp > 0) {
        if (rng.unit() < r.round_odds) {
            --dhp;
            r.rounds.push_back(1);
        } else {
            --ahp;
            r.rounds.push_back(0);
        }
    }
    r.att_hp = ahp;
    r.def_hp = dhp;
    r.attacker_won = dhp <= 0;
    a->moves = std::max(0, a->moves - 3);
    if (a->order == Order::Fortified || a->order == Order::Fortifying) a->order = Order::None;
    const std::string an = unit_name(*a), dn = unit_name(*d);
    City *c = city_at(tx, ty);
    if (r.attacker_won) {
        a->hp = ahp;
        if (!a->veteran && rng.chance(0.5)) a->veteran = true;
        const bool single = c || map.at(tx, ty).fortress;
        if (single) {
            kill_unit(d->id);
            r.killed = 1;
            if (c && !c->has(Building::CityWalls) && c->size > 1) {
                c->size--;
                r.city_shrank = true;
            }
        } else {
            r.killed = static_cast<int>(here.size());
            kill_stack(tx, ty);
        }
        say(att_owner, MsgKind::Combat, "Your " + std::string(unit_info(r.att_type).name) + " defeated the " + dn + ".", tx, ty);
        say(def_owner, MsgKind::Combat, "Your " + std::string(unit_info(r.def_type).name) + " was destroyed by the " + an + ".", tx, ty);
    } else {
        d->hp = dhp;
        if (!d->veteran && rng.chance(0.5)) d->veteran = true;
        kill_unit(unit_id);
        r.killed = 1;
        say(att_owner, MsgKind::Combat, "Your " + std::string(unit_info(r.att_type).name) + " was destroyed attacking the " + dn + ".", tx, ty);
        say(def_owner, MsgKind::Combat, "Your " + std::string(unit_info(r.def_type).name) + " held off the " + an + ".", tx, ty);
    }
    last_combat = r;
    check_elimination();
    return MoveResult::Attacked;
}

// ---------------------------------------------------------------- movement

bool Game::can_enter(const Unit &u, int tx, int ty) const
{
    if (!map.valid_y(ty)) return false;
    const UnitInfo &i = unit_info(u.type);
    const bool ocean = map.ocean(tx, ty);
    const City *c = city_at(tx, ty);
    switch (i.domain) {
    case Domain::Land:
        if (!ocean) return true;
        // A friendly ship with room.
        for (const Unit &s : units) {
            if (s.x == map.wrap_x(tx) && s.y == ty && s.owner == u.owner && unit_info(s.type).domain == Domain::Sea &&
                !(unit_info(s.type).flags & kCarriesAir) && cargo_count(s.id) < unit_info(s.type).capacity) {
                return true;
            }
        }
        return false;
    case Domain::Sea:
        if (!ocean) return c && c->owner == u.owner;
        if ((i.flags & kCoastal) && !map.next_to_land(tx, ty)) return false;
        return true;
    case Domain::Air: return true;
    }
    return false;
}

int Game::move_cost(const Unit &u, int fx, int fy, int tx, int ty) const
{
    const UnitInfo &i = unit_info(u.type);
    if (i.domain != Domain::Land) return 3;
    const Tile &a = map.at(fx, fy);
    const Tile &b = map.at(tx, ty);
    const bool road_a = a.road || city_at(fx, fy);
    const bool road_b = b.road || city_at(tx, ty);
    if (a.railroad && b.railroad) return 0;   // railroads: free movement
    if (road_a && road_b) return 1;
    if (b.terrain == Terrain::Ocean) return 3;
    return terrain_info(b.terrain).move * 3;
}

bool Game::zoc_blocks(const Unit &u, int tx, int ty) const
{
    const UnitInfo &i = unit_info(u.type);
    if (i.domain != Domain::Land || (i.flags & kIgnoreZoc)) return false;
    if (map.ocean(tx, ty)) return false;
    // Moving into a square with our own unit or city is always allowed.
    const int o = owner_at(tx, ty);
    if (o == u.owner) return false;
    auto enemy_near = [&](int x, int y) {
        for (const Unit &e : units) {
            if (e.owner == u.owner || unit_info(e.type).domain != Domain::Land) continue;
            if (map.distance(x, y, e.x, e.y) == 1) return true;
        }
        return false;
    };
    return enemy_near(u.x, u.y) && enemy_near(tx, ty);
}

MoveResult Game::move(int unit_id, int dx, int dy)
{
    Unit *u = unit(unit_id);
    if (!u) return MoveResult::Failed;
    if (u->moves <= 0) return MoveResult::NoMoves;
    const int tx = map.wrap_x(u->x + dx), ty = u->y + dy;
    if (!map.valid_y(ty) || (dx == 0 && dy == 0)) return MoveResult::Blocked;
    const UnitInfo &i = unit_info(u->type);
    // A barbarian Leader caught alone is ransomed.
    if (i.domain == Domain::Land && military(u->type) && ransom(unit_id, tx, ty) > 0) {
        u = unit(unit_id);
        if (!u) return MoveResult::Failed;
    }
    // Foreign units or city in the way.
    const int other = owner_at(tx, ty);
    if (other >= 0 && other != u->owner) {
        civs[u->owner].contact |= static_cast<std::uint8_t>(1u << other);
        civs[other].contact |= static_cast<std::uint8_t>(1u << u->owner);
        // Diplomats and Caravans deal with what they meet.
        if (u->type == UnitType::Diplomat && (city_at(tx, ty) || units_at(tx, ty).size() == 1)) return MoveResult::Diplomat;
        if (u->type == UnitType::Caravan && city_at(tx, ty)) return MoveResult::Caravan;
        if (!at_war(u->owner, other)) return MoveResult::NeedWar;
        const bool has_units = !units_at(tx, ty).empty();
        if (has_units) {
            if (i.attack <= 0) return MoveResult::Blocked;
            if (i.domain == Domain::Land && map.ocean(tx, ty)) return MoveResult::Blocked;
            if ((i.flags & kNoLandAttack) && map.land(tx, ty)) return MoveResult::Blocked;
            return attack(unit_id, tx, ty);
        }
        // An empty enemy city: land troops take it.
        City *c = city_at(tx, ty);
        if (c && i.domain == Domain::Land && military(u->type)) {
            const int cost = move_cost(*u, u->x, u->y, tx, ty);
            (void)cost;
            u->x = tx;
            u->y = ty;
            u->moves = 0;
            capture_city(*c, u->owner);
            return MoveResult::Captured;
        }
        return MoveResult::Blocked;
    }
    if (!can_enter(*u, tx, ty)) return MoveResult::Blocked;
    if (zoc_blocks(*u, tx, ty)) return MoveResult::Zoc;
    const int cost = move_cost(*u, u->x, u->y, tx, ty);
    if (u->moves < cost && u->moves < full_moves(*u)) {
        // Not enough movement left: a chance to make it anyway.
        if (rng.below(cost) >= u->moves) {
            u->moves = 0;
            return MoveResult::Failed;
        }
    }
    u->moves = std::max(0, u->moves - cost);
    const int fx = u->x, fy = u->y;
    u->x = tx;
    u->y = ty;
    if (u->order != Order::Goto) u->order = Order::None;
    u->work = 0;
    MoveResult result = MoveResult::Moved;
    // Boarding and landing.
    if (i.domain == Domain::Land) {
        if (map.ocean(tx, ty)) {
            for (Unit &s : units) {
                if (s.x == tx && s.y == ty && s.owner == u->owner && unit_info(s.type).domain == Domain::Sea &&
                    cargo_count(s.id) < unit_info(s.type).capacity) {
                    u->carried_by = s.id;
                    u->order = Order::Sentry;
                    result = MoveResult::Boarded;
                    break;
                }
            }
        } else {
            u->carried_by = -1;
        }
    }
    // Ships take their cargo along.
    if (i.domain == Domain::Sea) {
        for (Unit &c : units) {
            if (c.carried_by == u->id) {
                c.x = tx;
                c.y = ty;
            }
        }
    }
    (void)fx;
    (void)fy;
    reveal(u->owner, tx, ty, (i.flags & kSight2) ? 2 : 1);
    if (map.at(tx, ty).hut && i.domain == Domain::Land) enter_hut(*u);
    if (u->type == UnitType::Caravan) {
        const City *c = city_at(tx, ty);
        if (c && c->owner == u->owner && (caravan_can_help(*u, *c) || caravan_can_trade(*u, *c))) result = MoveResult::Caravan;
    }
    return result;
}

void Game::enter_hut(Unit &u)
{
    Tile &t = map.at(u.x, u.y);
    t.hut = false;
    const int roll = rng.below(100);
    const int owner = u.owner;
    if (roll < 35) {
        civs[owner].gold += 50;
        say(owner, MsgKind::Good, "You find 50 gold in the ruins of an old village.", u.x, u.y);
    } else if (roll < 60) {
        std::vector<Advance> opts = researchable(owner);
        opts.erase(std::remove(opts.begin(), opts.end(), Advance::FutureTech), opts.end());
        if (!opts.empty()) {
            const Advance a = opts[rng.below(static_cast<int>(opts.size()))];
            learn(owner, a);
            say(owner, MsgKind::Tech, std::string("Wise elders of the village teach you ") + advance_info(a).name + ".", u.x, u.y);
        } else {
            civs[owner].gold += 25;
        }
    } else if (roll < 85) {
        const int id = add_unit(owner, UnitType::Legion, u.x, u.y, -1);
        if (Unit *m = unit(id)) m->moves = 0;
        say(owner, MsgKind::Good, "A band of mercenaries joins you: a Legion.", u.x, u.y);
    } else {
        say(owner, MsgKind::Info, "The village is long abandoned.", u.x, u.y);
    }
}

void Game::capture_city(City &c, int new_owner)
{
    const int old = c.owner;
    int total = 0;
    for (const City &o : cities) {
        if (o.owner == old) total += o.size;
    }
    const int plunder = total > 0 ? civs[old].gold * c.size / total : 0;
    civs[old].gold -= plunder;
    civs[new_owner].gold += plunder;
    if (civs[new_owner].barbarian && c.size <= 2 && !c.has(Building::Palace) && wonders_in(c) == 0) {
        // Raiders sack a small town and burn it rather than hold it.
        const int id = c.id;
        const std::string name = c.name;
        const int cx = c.x, cy = c.y;
        std::vector<int> drop;
        for (const Unit &u : units) {
            if (u.home == id) drop.push_back(u.id);
        }
        for (int uid : drop) kill_unit(uid);
        say(old, MsgKind::Bad, "Barbarians sack " + name + " and burn it to the ground! They carry off " + std::to_string(plunder) + " gold.", cx, cy);
        cities.erase(std::remove_if(cities.begin(), cities.end(), [id](const City &o) { return o.id == id; }), cities.end());
        return;
    }
    if (c.has(Building::Palace) && (civs[old].structurals || civs[old].components || civs[old].modules)) {
        // The capital falls: the spaceship is lost, on the pad or in flight.
        lose_spaceship(old, civs[old].launch_turn >= 0);
    }
    c.remove(Building::Palace);
    c.owner = new_owner;
    c.manual = false;
    c.shields = 0;
    c.build_unit = true;
    c.build = static_cast<int>(UnitType::Militia);
    if (c.size > 1) c.size--;
    // Units the city supported for the old owner are disbanded.
    std::vector<int> drop;
    for (const Unit &u : units) {
        if (u.home == c.id && u.owner == old) drop.push_back(u.id);
    }
    for (int id : drop) kill_unit(id);
    say(new_owner, MsgKind::Good, "You capture " + c.name + "! Plunder: " + std::to_string(plunder) + " gold.", c.x, c.y);
    say(old, MsgKind::Bad, c.name + " has fallen to the " + civs[new_owner].name + ".", c.x, c.y);
    update_city(c);
    reveal(new_owner, c.x, c.y, 2);
    check_elimination();
}

void Game::set_goto(int unit_id, int x, int y)
{
    if (Unit *u = unit(unit_id)) {
        u->order = Order::Goto;
        u->goto_x = map.wrap_x(x);
        u->goto_y = y;
    }
}

bool Game::next_step(const Unit &u, int tx, int ty, int &sdx, int &sdy, int limit) const
{
    tx = map.wrap_x(tx);
    if (u.x == tx && u.y == ty) return false;
    const int W = map.w, H = map.h;
    std::vector<int> cost(static_cast<size_t>(W) * H, 1 << 29);
    std::vector<int> from(static_cast<size_t>(W) * H, -1);
    using Node = std::pair<int, int>; // f, index
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    const int start = u.y * W + u.x, goal = ty * W + tx;
    cost[start] = 0;
    open.push({0, start});
    const UnitInfo &ui = unit_info(u.type);
    int expanded = 0;
    while (!open.empty() && expanded < limit) {
        const int p = open.top().second;
        open.pop();
        if (p == goal) break;
        ++expanded;
        const int px = p % W, py = p / W;
        for (int j = -1; j <= 1; ++j) {
            for (int i = -1; i <= 1; ++i) {
                if (!i && !j) continue;
                const int nx = map.wrap_x(px + i), ny = py + j;
                if (!map.valid_y(ny)) continue;
                const int np = ny * W + nx;
                bool ok;
                if (np == goal) {
                    ok = ui.domain == Domain::Air || (ui.domain == Domain::Land ? map.land(nx, ny) : (map.ocean(nx, ny) || city_at(nx, ny)));
                } else {
                    if (ui.domain == Domain::Land) {
                        ok = map.land(nx, ny);
                    } else if (ui.domain == Domain::Sea) {
                        const City *cc = city_at(nx, ny);
                        ok = (map.ocean(nx, ny) && (!(ui.flags & kCoastal) || map.next_to_land(nx, ny))) || (cc && cc->owner == u.owner);
                    } else {
                        ok = true;
                    }
                    const int o = owner_at(nx, ny);
                    if (o >= 0 && o != u.owner) ok = false;
                }
                if (!ok) continue;
                Unit probe = u;
                const int c = cost[p] + move_cost(probe, px, py, nx, ny);
                if (c < cost[np]) {
                    cost[np] = c;
                    from[np] = p;
                    const int h = map.distance(nx, ny, tx, ty);
                    open.push({c + h, np});
                }
            }
        }
    }
    if (from[goal] < 0) return false;
    int p = goal;
    while (from[p] != start) p = from[p];
    sdx = map.dx(u.x, p % W);
    sdy = p / W - u.y;
    return true;
}

bool Game::step_goto(int unit_id)
{
    Unit *u = unit(unit_id);
    if (!u || u->order != Order::Goto) return false;
    if (u->x == u->goto_x && u->y == u->goto_y) {
        u->order = Order::None;
        return false;
    }
    int sx = 0, sy = 0;
    if (!next_step(*u, u->goto_x, u->goto_y, sx, sy)) {
        u->order = Order::None;
        return false;
    }
    const int bx = u->x, by = u->y;
    const MoveResult r = move(unit_id, sx, sy);
    u = unit(unit_id);
    if (!u) return false;
    const bool moved = u->x != bx || u->y != by;
    if (r != MoveResult::Moved && r != MoveResult::Boarded && !(r == MoveResult::Caravan && moved)) {
        if (r != MoveResult::Failed) u->order = Order::None;
        return false;
    }
    if (u->x == u->goto_x && u->y == u->goto_y) u->order = Order::None;
    return true;
}

// ---------------------------------------------------------------- unit orders

int Game::add_unit(int owner, UnitType t, int x, int y, int home)
{
    Unit u;
    u.id = next_unit_id++;
    u.type = t;
    u.owner = owner;
    u.x = map.wrap_x(x);
    u.y = y;
    u.home = home;
    u.moves = unit_info(t).moves * 3;
    u.fuel = unit_info(t).fuel;
    units.push_back(u);
    reveal(owner, u.x, y, (unit_info(t).flags & kSight2) ? 2 : 1);
    return u.id;
}

void Game::kill_unit(int id)
{
    for (size_t i = 0; i < units.size(); ++i) {
        if (units[i].id == id) {
            const int ship = units[i].id;
            units.erase(units.begin() + static_cast<long>(i));
            // Cargo goes down with a ship at sea.
            std::vector<int> cargo;
            for (const Unit &c : units) {
                if (c.carried_by == ship) cargo.push_back(c.id);
            }
            for (int c : cargo) {
                Unit *cu = unit(c);
                if (cu && map.ocean(cu->x, cu->y)) {
                    kill_unit(c);
                } else if (cu) {
                    cu->carried_by = -1;
                }
            }
            return;
        }
    }
}

void Game::kill_stack(int x, int y)
{
    // A barbarian Leader slips away from the slaughter, to be caught alone.
    for (int id : units_at(x, y)) {
        const Unit *u = unit(id);
        if (u && u->type == UnitType::Leader) continue;
        kill_unit(id);
    }
}

bool Game::can_work(const Unit &u, Order job) const
{
    if (!(unit_info(u.type).flags & kSettlers)) return false;
    const Tile &t = map.at(u.x, u.y);
    const TerrainInfo &ti = terrain_info(t.terrain);
    if (t.terrain == Terrain::Ocean) return false;
    switch (job) {
    case Order::Road:
        if (t.road || city_at(u.x, u.y)) return false;
        if (t.terrain == Terrain::River && !has(u.owner, Advance::BridgeBuilding)) return false;
        return true;
    case Order::Irrigate: {
        if (ti.irrigate_turns == 0 || t.irrigation || city_at(u.x, u.y)) return false;
        if (ti.irrigate_to != t.terrain) return true; // changes the terrain, no water needed
        if (t.terrain == Terrain::River) return true;
        const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto &d : d4) {
            const int nx = u.x + d[0], ny = u.y + d[1];
            if (!map.valid_y(ny)) continue;
            const Tile &n = map.at(nx, ny);
            if (n.terrain == Terrain::Ocean || n.terrain == Terrain::River || n.irrigation) return true;
            if (city_at(nx, ny)) return true;
        }
        return false;
    }
    case Order::Mine:
        if (ti.mine_turns == 0 || t.mine || city_at(u.x, u.y)) return false;
        return true;
    case Order::Fortress: return has(u.owner, Advance::Construction) && !t.fortress && !city_at(u.x, u.y);
    case Order::Railroad: return has(u.owner, Advance::Railroad) && t.road && !t.railroad && !city_at(u.x, u.y);
    default: return false;
    }
}

bool Game::start_work(int unit_id, Order job)
{
    Unit *u = unit(unit_id);
    if (!u || !can_work(*u, job)) return false;
    u->order = job;
    u->work = 0;
    u->moves = 0;
    return true;
}

void Game::fortify(int unit_id)
{
    if (Unit *u = unit(unit_id)) {
        if (unit_info(u->type).domain != Domain::Land || (unit_info(u->type).flags & kSettlers)) {
            u->order = Order::Sentry;
        } else {
            u->order = Order::Fortifying;
        }
        u->moves = 0;
    }
}

void Game::sentry(int unit_id)
{
    if (Unit *u = unit(unit_id)) {
        u->order = Order::Sentry;
    }
}

void Game::disband(int unit_id)
{
    kill_unit(unit_id);
}

std::string Game::next_city_name(int civ)
{
    Civ &c = civs[civ];
    std::string name;
    if (civ == 0 || c.name_next < 100) {
        // The player's civ (or a preset-less one) uses the player list.
        const int n = c.name_next % 100;
        name = player_city_name(n);
        if (n >= 24) name += " " + std::to_string(n / 24 + 1);
        c.name_next++;
    } else {
        const int preset = c.name_next / 100 - 1;
        const int n = c.name_next % 100;
        name = kPresets[preset].cities[n % 16];
        if (n >= 16) name = "New " + name;
        c.name_next++;
    }
    return name;
}

std::string Game::suggest_city_name(int civ) const
{
    Game &self = const_cast<Game &>(*this);
    const int keep = civs[civ].name_next;
    std::string n = self.next_city_name(civ);
    self.civs[civ].name_next = keep;
    return n;
}

bool Game::found_city(int unit_id, const std::string &name)
{
    Unit *u = unit(unit_id);
    if (!u || !(unit_info(u->type).flags & kSettlers)) return false;
    if (!map.land(u->x, u->y) || city_at(u->x, u->y)) return false;
    for (const City &c : cities) {
        if (map.distance(c.x, c.y, u->x, u->y) <= 1) return false;
    }
    City c;
    c.id = next_city_id++;
    c.owner = u->owner;
    c.x = u->x;
    c.y = u->y;
    c.name = name.empty() ? next_city_name(u->owner) : name;
    c.founded = turn;
    c.size = 1;
    bool first = true;
    for (const City &o : cities) {
        if (o.owner == u->owner) first = false;
    }
    if (first) c.add(Building::Palace);
    Tile &t = map.at(c.x, c.y);
    t.road = true;
    t.hut = false;
    const int owner = u->owner;
    // Default production: a defender first.
    c.build_unit = true;
    c.build = static_cast<int>(unit_available(owner, UnitType::Phalanx) ? UnitType::Phalanx : UnitType::Militia);
    kill_unit(unit_id);
    cities.push_back(c);
    City &nc = cities.back();
    // Units standing in the city call it home if they have none.
    for (Unit &o : units) {
        if (o.owner == owner && o.home < 0) o.home = nc.id;
    }
    update_city(nc);
    reveal(owner, nc.x, nc.y, 2);
    say(owner, MsgKind::City, "The " + civs[owner].name + " found " + nc.name + ".", nc.x, nc.y);
    return true;
}

bool Game::join_city(int unit_id)
{
    Unit *u = unit(unit_id);
    if (!u || !(unit_info(u->type).flags & kSettlers)) return false;
    City *c = city_at(u->x, u->y);
    if (!c || c->owner != u->owner || c->size >= 10) return false;
    c->size++;
    kill_unit(unit_id);
    update_city(*c);
    return true;
}

void Game::declare_war(int a, int b)
{
    if (a == b || is_barbarian(a) || is_barbarian(b)) return;
    civs[a].war |= static_cast<std::uint8_t>(1u << b);
    civs[b].war |= static_cast<std::uint8_t>(1u << a);
    say(a, MsgKind::Bad, "You are at war with the " + civs[b].name + ".");
    say(b, MsgKind::Bad, "The " + civs[a].name + " declare war on you!");
}

void Game::make_peace(int a, int b)
{
    if (is_barbarian(a) || is_barbarian(b)) return;   // no peace with raiders
    civs[a].war &= static_cast<std::uint8_t>(~(1u << b));
    civs[b].war &= static_cast<std::uint8_t>(~(1u << a));
}

void Game::set_rates(int civ, int tax, int lux, int sci)
{
    Civ &c = civs[civ];
    const int mx = std::max(1, government_info(c.govt).max_rate);
    tax = std::clamp(tax, 0, 10);
    lux = std::clamp(lux, 0, 10);
    sci = std::clamp(sci, 0, 10);
    if (tax + lux + sci != 10) sci = std::max(0, 10 - tax - lux);
    if (c.govt != Government::Anarchy && (tax > mx || lux > mx || sci > mx)) return;
    c.tax = tax;
    c.lux = lux;
    c.sci = sci;
    for (City &ct : cities) {
        if (ct.owner == civ) update_city(ct);
    }
}

void Game::revolution(int civ, Government g)
{
    Civ &c = civs[civ];
    const bool pyramids = wonder_active(civ, Wonder::Pyramids);
    if (!pyramids && !has(civ, government_info(g).tech)) return;
    c.target = g;
    c.govt = Government::Anarchy;
    c.anarchy = pyramids ? 1 : 2;
    say(civ, MsgKind::Bad, "Revolution! The " + c.name + " fall into Anarchy.");
    for (City &ct : cities) {
        if (ct.owner == civ) update_city(ct);
    }
}

bool Game::buy(int city_id)
{
    City *c = city(city_id);
    if (!c || c->bought) return false;
    const int cost = buy_cost(*c);
    if (cost <= 0 || civs[c->owner].gold < cost) return false;
    civs[c->owner].gold -= cost;
    c->shields = production_cost(*c);
    c->bought = true;
    return true;
}

void Game::set_production(int city_id, bool is_unit, int what)
{
    City *c = city(city_id);
    if (!c || !can_build(*c, is_unit, what)) return;
    // Switching between units and buildings loses half the shields.
    if (c->build_unit != is_unit) c->shields /= 2;
    c->build_unit = is_unit;
    c->build = what;
    announce_wonder_start(*c);
}

void Game::toggle_tile(int city_id, int dx, int dy)
{
    City *c = city(city_id);
    if (!c) return;
    if (dx == 0 && dy == 0) {
        // Clicking the centre hands the city back to automatic placement.
        c->manual = false;
        update_city(*c);
        return;
    }
    if (std::max(std::abs(dx), std::abs(dy)) > 2 || (std::abs(dx) == 2 && std::abs(dy) == 2)) return;
    c->manual = true;
    for (size_t i = 0; i < c->worked.size(); ++i) {
        if (c->worked[i].first == dx && c->worked[i].second == dy) {
            c->worked.erase(c->worked.begin() + static_cast<long>(i));
            update_city(*c);   // the citizen becomes an entertainer
            return;
        }
    }
    if (!tile_free_for(*c, c->x + dx, c->y + dy)) return;
    if (c->specialists() > 0) {
        // An entertainer goes to work first, then a taxman, then a scientist.
        if (c->entertainers > 0) c->entertainers--;
        else if (c->taxmen > 0) c->taxmen--;
        else c->scientists--;
    } else if (!c->worked.empty()) {
        c->worked.erase(c->worked.begin()); // move the first worker here
    } else {
        return;
    }
    c->worked.push_back({dx, dy});
    update_city(*c);
}

bool Game::cycle_specialist(int city_id, int index)
{
    City *c = city(city_id);
    if (!c || index < 0 || index >= c->specialists()) return false;
    if (c->size < kSpecialistSize) return false;
    c->manual = true;
    if (index < c->entertainers) {
        c->entertainers--;
        c->taxmen++;
    } else if (index < c->entertainers + c->taxmen) {
        c->taxmen--;
        c->scientists++;
    } else {
        c->scientists--;
        c->entertainers++;
    }
    update_city(*c);
    return true;
}

// ---------------------------------------------------------------- turn

void Game::reveal(int civ, int x, int y, int r)
{
    for (int j = -r; j <= r; ++j) {
        for (int i = -r; i <= r; ++i) {
            if (!map.valid_y(y + j)) continue;
            Tile &t = map.at(x + i, y + j);
            t.explored |= static_cast<std::uint8_t>(1u << civ);
            t.visible |= static_cast<std::uint8_t>(1u << civ);
        }
    }
}

void Game::refresh_visibility()
{
    for (Tile &t : map.tiles) t.visible = 0;
    for (const Unit &u : units) reveal(u.owner, u.x, u.y, (unit_info(u.type).flags & kSight2) ? 2 : 1);
    for (const City &c : cities) reveal(c.owner, c.x, c.y, 2);
    // Contact: civs that can see one another's units or cities have met.
    for (const Unit &u : units) {
        const Tile &t = map.at(u.x, u.y);
        for (int c = 0; c < static_cast<int>(civs.size()); ++c) {
            if (c != u.owner && ((t.visible >> c) & 1u)) {
                civs[c].contact |= static_cast<std::uint8_t>(1u << u.owner);
                civs[u.owner].contact |= static_cast<std::uint8_t>(1u << c);
            }
        }
    }
}

int Game::next_unit_for(int civ, int after_id) const
{
    std::vector<int> ids;
    for (const Unit &u : units) {
        if (u.owner != civ || u.moves <= 0 || u.done) continue;
        if (u.order != Order::None) continue;
        ids.push_back(u.id);
    }
    if (ids.empty()) return -1;
    std::sort(ids.begin(), ids.end());
    for (int id : ids) {
        if (id > after_id) return id;
    }
    return ids.front();
}

void Game::begin_human_phase()
{
    std::vector<int> ids;
    for (const Unit &u : units) {
        if (u.owner == human && u.order == Order::Goto) ids.push_back(u.id);
    }
    for (int id : ids) {
        for (int guard = 0; guard < 30; ++guard) {
            Unit *u = unit(id);
            if (!u || u->moves <= 0 || u->order != Order::Goto) break;
            if (!step_goto(id)) break;
        }
    }
    // Sentries wake when an enemy comes next to them.
    for (Unit &u : units) {
        if (u.owner != human || u.order != Order::Sentry || u.carried_by >= 0) continue;
        for (const Unit &e : units) {
            if (e.owner != human && map.distance(u.x, u.y, e.x, e.y) <= 1) {
                u.order = Order::None;
                break;
            }
        }
    }
}

void Game::check_elimination()
{
    for (int c = 0; c < static_cast<int>(civs.size()); ++c) {
        if (!civs[c].alive || civs[c].barbarian) continue;
        bool has_city = false, has_settlers = false;
        for (const City &ct : cities) has_city |= ct.owner == c;
        for (const Unit &u : units) has_settlers |= (u.owner == c && (unit_info(u.type).flags & kSettlers));
        if (!has_city && !has_settlers) {
            civs[c].alive = false;
            std::vector<int> drop;
            for (const Unit &u : units) {
                if (u.owner == c) drop.push_back(u.id);
            }
            for (int id : drop) kill_unit(id);
            say(-1, MsgKind::Bad, "The " + civs[c].name + " have been destroyed.");
            if (c == human && !all_ai) human_dead = true;
        }
    }
    int alive = 0, last = -1;
    for (int c = 0; c < static_cast<int>(civs.size()); ++c) {
        if (civs[c].alive && !civs[c].barbarian) {
            ++alive;
            last = c;
        }
    }
    if (alive == 1 && winner < 0) {
        winner = last;
        say(-1, MsgKind::Good, "The " + civs[last].name + " have conquered the world!");
    }
}

void Game::process_city(City &c, bool &learned_any)
{
    (void)learned_any;
    update_city(c);
    Civ &civ = civs[c.owner];
    const CityStats s = c.stats;
    if (s.disorder) {
        say(c.owner, MsgKind::Bad, "Civil disorder in " + c.name + "!", c.x, c.y);
    }
    // Food.
    c.food += s.food_surplus;
    if (c.food >= s.food_box) {
        if (c.size < size_cap(c)) {
            c.size++;
            c.food = c.has(Building::Granary) ? food_box(c) / 2 : 0;
            say(c.owner, MsgKind::City, c.name + " grows to size " + std::to_string(c.size) + ".", c.x, c.y);
        } else {
            c.food = s.food_box;
            if (c.size == 10) say(c.owner, MsgKind::Info, c.name + " needs an Aqueduct to grow.", c.x, c.y);
        }
    } else if (c.food < 0) {
        // Famine: settlers starve first, then the city shrinks.
        int victim = -1;
        for (const Unit &u : units) {
            if (u.home == c.id && (unit_info(u.type).flags & kSettlers)) victim = u.id;
        }
        if (victim >= 0) {
            kill_unit(victim);
            say(c.owner, MsgKind::Bad, "Famine in " + c.name + ": Settlers starve.", c.x, c.y);
        } else if (c.size > 1) {
            c.size--;
            say(c.owner, MsgKind::Bad, "Famine in " + c.name + "! The city shrinks.", c.x, c.y);
        }
        c.food = 0;
    }
    // Shields.
    if (!s.disorder) {
        c.shields += s.shield_surplus;
    }
    if (s.shield_surplus < 0) {
        // Can't support everyone: the farthest unit is disbanded.
        int victim = -1, far_d = -1;
        for (const Unit &u : units) {
            if (u.home != c.id) continue;
            const int d = map.distance(u.x, u.y, c.x, c.y);
            if (d > far_d) {
                far_d = d;
                victim = u.id;
            }
        }
        if (victim >= 0) {
            kill_unit(victim);
            say(c.owner, MsgKind::Bad, c.name + " can't support a unit; it is disbanded.", c.x, c.y);
        }
        c.shields = std::max(0, c.shields);
    }
    c.bought = false;
    if (has(c.owner, Advance::Railroad)) {
        Tile &ct = map.at(c.x, c.y);
        ct.road = true;
        ct.railroad = true;
    }
    if (!can_build(c, c.build_unit, c.build)) {
        c.build_unit = true;
        c.build = static_cast<int>(unit_available(c.owner, UnitType::Phalanx) ? UnitType::Phalanx : UnitType::Militia);
        if (!unit_available(c.owner, static_cast<UnitType>(c.build))) {
            for (int u = kUnitTypeCount - 1; u >= 0; --u) {
                if (can_build(c, true, u) && unit_info(u).domain == Domain::Land && military(static_cast<UnitType>(u))) {
                    c.build = u;
                    break;
                }
            }
        }
    }
    const int cost = production_cost(c);
    if (civ.barbarian && c.build_unit && c.shields >= cost) {
        // Raiders hold what they took but do not breed armies: a few
        // troops per city at most.
        int raiders = 0, held = 0;
        for (const Unit &u : units) raiders += u.owner == c.owner && military(u.type);
        for (const City &o : cities) held += o.owner == c.owner;
        if (raiders >= std::min(held + 2, 10)) c.shields = cost - 1;
    }
    if (c.shields >= cost) {
        if (c.build_unit) {
            const UnitType t = static_cast<UnitType>(c.build);
            const bool settlers = unit_info(t).flags & kSettlers;
            if (settlers && c.size <= 1) {
                c.shields = cost; // waits for the city to grow
            } else {
                if (settlers) c.size--;
                const int id = add_unit(c.owner, t, c.x, c.y, c.id);
                if (Unit *u = unit(id)) u->veteran = c.has(Building::Barracks);
                c.shields -= cost;
                say(c.owner, MsgKind::City, c.name + " builds " + unit_info(t).name + ".", c.x, c.y);
            }
        } else if (is_space_build(c.build)) {
            c.shields -= cost;
            const SpacePart part = static_cast<SpacePart>(c.build - kSpaceBase);
            std::string what = building_info(c.build).name;
            if (part == SpacePart::Structural) {
                ++civ.structurals;
            } else if (part == SpacePart::Component) {
                what = civ.components % 2 == 0 ? "SS Component (fuel)" : "SS Component (propulsion)";
                ++civ.components;
            } else {
                static const char *kinds[] = {"habitation", "life support", "solar panels"};
                what = std::string("SS Module (") + kinds[civ.modules % 3] + ")";
                ++civ.modules;
            }
            say(c.owner, MsgKind::City, c.name + " builds " + what + ".", c.x, c.y);
            if (!can_build(c, false, c.build)) {
                // That part is full: on to the next one, or back to troops.
                int next = -1;
                for (int k = 0; k < kSpacePartCount && next < 0; ++k) {
                    if (can_build(c, false, kSpaceBase + k)) next = kSpaceBase + k;
                }
                if (next >= 0) {
                    c.build = next;
                } else {
                    c.build_unit = true;
                    c.build = static_cast<int>(unit_available(c.owner, UnitType::Musketeers) ? UnitType::Musketeers : UnitType::Phalanx);
                }
            }
        } else if (is_wonder_build(c.build)) {
            c.shields -= cost;
            complete_wonder(c, static_cast<Wonder>(c.build - kWonderBase));
            if (!civ.ai) {
                c.build_unit = true;
                c.build = static_cast<int>(unit_available(c.owner, UnitType::Phalanx) ? UnitType::Phalanx : UnitType::Militia);
            }
        } else {
            const Building b = static_cast<Building>(c.build);
            if (b == Building::Palace) {
                for (City &o : cities) {
                    if (o.owner == c.owner) o.remove(Building::Palace);
                }
            }
            c.add(b);
            c.shields -= cost;
            say(c.owner, MsgKind::City, c.name + " completes the " + building_info(b).name + ".", c.x, c.y);
            if (!civ.ai) {
                // Keep the city busy: a defender unless one is already there.
                c.build_unit = true;
                c.build = static_cast<int>(unit_available(c.owner, UnitType::Phalanx) ? UnitType::Phalanx : UnitType::Militia);
                for (int u = 0; u < kUnitTypeCount; ++u) {
                    if (can_build(c, true, u) && unit_info(u).domain == Domain::Land && unit_info(u).defense > unit_info(c.build).defense)
                        c.build = u;
                }
            }
        }
        if (civ.ai) ai_city(c);
    }
    // Trade.
    if (!s.disorder) {
        civ.gold += s.tax;
        civ.bulbs += s.sci;
    }
}

void Game::start_turn_for(int ci)
{
    Civ &civ = civs[ci];
    if (!civ.alive) return;
    if (civ.govt == Government::Anarchy) {
        if (--civ.anarchy <= 0) {
            civ.govt = civ.target == Government::Anarchy ? Government::Despotism : civ.target;
            const int mx = government_info(civ.govt).max_rate;
            civ.sci = std::min(civ.sci, mx);
            civ.tax = std::min(10 - civ.sci, mx);
            civ.lux = 10 - civ.sci - civ.tax;
            say(ci, MsgKind::Good, std::string("The new government is ") + government_info(civ.govt).name + ".");
        }
    }
    bool learned = false;
    std::vector<int> ids;
    for (const City &c : cities) {
        if (c.owner == ci) ids.push_back(c.id);
    }
    for (int id : ids) {
        if (City *c = city(id)) process_city(*c, learned);
    }
    // Upkeep of buildings, paid in gold.
    int upkeep = 0;
    for (const City &c : cities) {
        if (c.owner != ci) continue;
        for (int b = 0; b < kBuildingCount; ++b) {
            if (c.has(static_cast<Building>(b))) upkeep += building_info(b).upkeep;
        }
    }
    civ.gold -= upkeep;
    while (civ.gold < 0) {
        // Sell the cheapest building somewhere.
        City *sell_city = nullptr;
        int sell_b = -1, sell_cost = 1 << 30;
        for (City &c : cities) {
            if (c.owner != ci) continue;
            for (int b = 1; b < kBuildingCount; ++b) {
                if (c.has(static_cast<Building>(b)) && building_info(b).cost < sell_cost) {
                    sell_cost = building_info(b).cost;
                    sell_b = b;
                    sell_city = &c;
                }
            }
        }
        if (!sell_city) {
            civ.gold = 0;
            break;
        }
        sell_city->remove(static_cast<Building>(sell_b));
        civ.gold += sell_cost;
        say(ci, MsgKind::Bad, std::string("Short of gold: the ") + building_info(sell_b).name + " in " + sell_city->name + " is sold.",
            sell_city->x, sell_city->y);
    }
    if (civ.barbarian) {
        // Raiders know whatever the world knows, for their cities' builds.
        for (int a = 0; a < kAdvanceCount; ++a) {
            bool any = false;
            for (const Civ &o : civs) any |= !o.barbarian && o.alive && o.techs[a];
            civ.techs[a] = any;
        }
        civ.research = Advance::None;
        civ.bulbs = 0;
    }
    // The Great Library: an advance two other civilizations know.
    if (wonder_active(ci, Wonder::GreatLibrary)) {
        for (int a = 0; a < kAdvanceCount; ++a) {
            if (civ.techs[a] || a == static_cast<int>(Advance::FutureTech)) continue;
            int knowers = 0;
            for (int k = 0; k < static_cast<int>(civs.size()); ++k) {
                if (k != ci && civs[k].alive && !civs[k].barbarian && civs[k].techs[a]) ++knowers;
            }
            if (knowers >= 2) {
                learn(ci, static_cast<Advance>(a));
                say(ci, MsgKind::Tech, std::string("The Great Library gives you ") + advance_info(a).name + ".");
                break;
            }
        }
    }
    // Research.
    if (civ.research == Advance::None && civ.ai && !civ.barbarian) choose_research_ai(ci);
    while (civ.research != Advance::None && civ.bulbs >= research_cost(ci)) {
        civ.bulbs -= research_cost(ci);
        const Advance a = civ.research;
        learn(ci, a);
        say(ci, MsgKind::Tech, std::string("Your wise men discover ") + advance_info(a).name + "!");
        if (civ.ai) choose_research_ai(ci);
    }
    // Units: new movement, terrain work, fortifying, fuel.
    std::vector<int> uids;
    for (const Unit &u : units) {
        if (u.owner == ci) uids.push_back(u.id);
    }
    for (int id : uids) {
        Unit *u = unit(id);
        if (!u) continue;
        u->moves = full_moves(*u);
        u->done = false;
        // Wounds heal: fully in a city of ours, faster when resting.
        if (u->hp < kUnitHp) {
            const City *hc = city_at(u->x, u->y);
            if (hc && hc->owner == ci) {
                u->hp = kUnitHp;
            } else {
                const bool resting = u->order == Order::Fortified || u->order == Order::Fortifying || u->order == Order::Sentry;
                u->hp = std::min(kUnitHp, u->hp + (resting ? 3 : 1));
            }
        }
        if (u->order == Order::Fortifying) u->order = Order::Fortified;
        if (u->order == Order::Road || u->order == Order::Irrigate || u->order == Order::Mine || u->order == Order::Fortress ||
            u->order == Order::Railroad) {
            u->moves = 0;
            Tile &t = map.at(u->x, u->y);
            const TerrainInfo &ti = terrain_info(t.terrain);
            u->work++;
            int need = 0;
            switch (u->order) {
            case Order::Road: need = ti.road_turns; break;
            case Order::Irrigate: need = ti.irrigate_turns; break;
            case Order::Mine: need = ti.mine_turns; break;
            case Order::Railroad: need = ti.road_turns * 2; break;
            default: need = ti.fortress_turns; break;
            }
            // Settlers working the same job on the square help each other.
            int crew = 0;
            for (const Unit &o : units) {
                if (o.x == u->x && o.y == u->y && o.order == u->order && o.owner == u->owner) ++crew;
            }
            if (u->work * std::max(1, crew) >= need) {
                switch (u->order) {
                case Order::Road: t.road = true; break;
                case Order::Irrigate:
                    if (ti.irrigate_to != t.terrain) {
                        t.terrain = ti.irrigate_to;
                        t.mine = false;
                    } else {
                        t.irrigation = true;
                        t.mine = false;
                    }
                    break;
                case Order::Mine:
                    if (ti.mine_to != t.terrain) {
                        t.terrain = ti.mine_to;
                        t.irrigation = false;
                    } else {
                        t.mine = true;
                        t.irrigation = false;
                    }
                    break;
                case Order::Railroad: t.railroad = true; break;
                default: t.fortress = true; break;
                }
                for (Unit &o : units) {
                    if (o.x == u->x && o.y == u->y && o.owner == u->owner && o.order == u->order) {
                        o.order = Order::None;
                        o.work = 0;
                    }
                }
            }
        }
        if (unit_info(u->type).domain == Domain::Air) {
            const City *c = city_at(u->x, u->y);
            bool safe = c && c->owner == ci;
            for (const Unit &s : units) {
                if (s.x == u->x && s.y == u->y && (unit_info(s.type).flags & kCarriesAir) && s.owner == ci) safe = true;
            }
            if (safe) {
                u->fuel = unit_info(u->type).fuel;
            } else if (--u->fuel < 0) {
                say(ci, MsgKind::Bad, std::string("Your ") + unit_info(u->type).name + " ran out of fuel and was lost.", u->x, u->y);
                kill_unit(id);
            }
        }
    }
}

void Game::new_turn()
{
    ++turn;
    for (int c = 0; c < static_cast<int>(civs.size()); ++c) start_turn_for(c);
    spawn_barbarians();
    space_arrivals();
    check_elimination();
    refresh_visibility();
}

void Game::end_turn()
{
    for (int c = 0; c < static_cast<int>(civs.size()); ++c) {
        if (!civs[c].alive || !civs[c].ai) continue;
        if (civs[c].barbarian) ai_barbarians(c);
        else ai_play(c);
    }
    new_turn();
    if (!civs[human].ai) begin_human_phase();
}

} // namespace civ
