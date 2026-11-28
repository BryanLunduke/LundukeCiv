// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// The computer players: simple, readable rules rather than anything clever.
// They expand with Settlers, keep a defender in every city, research toward
// Monarchy and the classic city improvements, and march on the nearest
// enemy city once at war.

#include "game.hpp"

#include <algorithm>
#include <queue>

namespace civ {

namespace {

const Advance kResearchOrder[] = {
    Advance::Alphabet,      Advance::CeremonialBurial, Advance::BronzeWorking, Advance::CodeOfLaws,
    Advance::Monarchy,      Advance::Pottery,          Advance::Writing,       Advance::Currency,
    Advance::Masonry,       Advance::Mysticism,        Advance::Mathematics,   Advance::Literacy,
    Advance::TheWheel,      Advance::IronWorking,      Advance::Trade,         Advance::Philosophy,
    Advance::Construction,  Advance::HorsebackRiding,  Advance::Astronomy,     Advance::Republic,
    Advance::Banking,       Advance::University,       Advance::Religion,      Advance::Feudalism,
    Advance::BridgeBuilding, Advance::Invention,       Advance::Gunpowder,     Advance::Navigation,
    Advance::Physics,       Advance::Medicine,         Advance::TheoryOfGravity, Advance::Chivalry,
};

bool is_military(UnitType t)
{
    const UnitInfo &i = unit_info(t);
    return !(i.flags & kNonMilitary) && i.attack + i.defense > 0;
}

const int kCrossAi[20][2] = {
    {-1, -2}, {0, -2}, {1, -2}, {-2, -1}, {-1, -1}, {0, -1}, {1, -1}, {2, -1}, {-2, 0}, {-1, 0},
    {1, 0},   {2, 0},  {-2, 1}, {-1, 1},  {0, 1},   {1, 1},  {2, 1},  {-1, 2}, {0, 2},  {1, 2},
};

} // namespace

void Game::choose_research_ai(int ci)
{
    Civ &c = civs[ci];
    // Boxed in on our landmass: ships first, to settle overseas - Map
    // Making, then the road to Navigation for ships that cross the ocean.
    if (!has(ci, Advance::Navigation) && ai_boxed_in(ci)) {
        const Advance sea_path[] = {Advance::MapMaking, Advance::Navigation, Advance::Astronomy, Advance::Mathematics,
                                    Advance::Mysticism, Advance::Alphabet, Advance::Masonry, Advance::CeremonialBurial};
        for (Advance a : sea_path) {
            if (can_research(ci, a)) {
                c.research = a;
                return;
            }
        }
    }
    for (Advance a : kResearchOrder) {
        if (can_research(ci, a)) {
            c.research = a;
            return;
        }
    }
    std::vector<Advance> opts = researchable(ci);
    if (opts.empty()) {
        c.research = Advance::None;
        return;
    }
    // Prefer anything that is not Future Tech.
    std::vector<Advance> real;
    for (Advance a : opts) {
        if (a != Advance::FutureTech) real.push_back(a);
    }
    if (!real.empty()) opts = real;
    c.research = opts[rng.below(static_cast<int>(opts.size()))];
}

int Game::ai_space_part(int ci, int city_id, bool frame_only) const
{
    // What the computer's spaceship needs next, counting parts finished and
    // parts other cities are building: modules and components up to a
    // modest target, and always as many structurals as they need.
    const Civ &cv = civs[ci];
    const int want_modules = difficulty >= Difficulty::King ? 6 : 3;
    const int want_components = difficulty >= Difficulty::King ? 6 : 4;
    int st = cv.structurals, co = cv.components, mo = cv.modules, building = 0;
    for (const City &o : cities) {
        if (o.owner != ci || o.id == city_id || o.build_unit || !is_space_build(o.build)) continue;
        ++building;
        const SpacePart p = static_cast<SpacePart>(o.build - kSpaceBase);
        if (p == SpacePart::Structural) ++st;
        else if (p == SpacePart::Component) ++co;
        else ++mo;
    }
    int own = 0;
    for (const City &o : cities) own += o.owner == ci;
    if (building >= 1 + own / 4) return -1;
    const int frame = std::max(co, want_components) + std::max(mo, want_modules);
    if (st < co + mo + 1 && st < frame) return static_cast<int>(SpacePart::Structural);
    if (frame_only) return st < frame ? static_cast<int>(SpacePart::Structural) : -1;
    if (mo < want_modules) return static_cast<int>(SpacePart::Module);
    if (co < want_components) return static_cast<int>(SpacePart::Component);
    if (st < frame) return static_cast<int>(SpacePart::Structural);
    return -1;
}

void Game::ai_city(City &c)
{
    const int ci = c.owner;
    if (civs[ci].barbarian) {
        // Raiders build raiders.
        int best = -1, score = -1;
        for (int u = 0; u < kUnitTypeCount; ++u) {
            if (!can_build(c, true, u) || unit_info(u).domain != Domain::Land || !is_military(static_cast<UnitType>(u))) continue;
            const int sc = unit_info(u).attack * 4 + unit_info(u).moves;
            if (sc > score) {
                score = sc;
                best = u;
            }
        }
        c.build_unit = true;
        c.build = best >= 0 ? best : static_cast<int>(UnitType::Militia);
        return;
    }
    auto best_unit = [&](bool attack_role) {
        int best = -1, score = -1;
        for (int u = 0; u < kUnitTypeCount; ++u) {
            if (!can_build(c, true, u)) continue;
            const UnitInfo &i = unit_info(u);
            if (i.domain != Domain::Land || !is_military(static_cast<UnitType>(u))) continue;
            const int s = attack_role ? i.attack * 4 + i.moves : i.defense * 4 + (i.attack > i.defense ? 0 : 1);
            if (s > score) {
                score = s;
                best = u;
            }
        }
        return best;
    };
    int defenders = 0;
    for (const Unit &u : units) {
        if (u.owner == ci && u.x == c.x && u.y == c.y && is_military(u.type) && unit_info(u.type).domain == Domain::Land) ++defenders;
    }
    int own_cities = 0, own_settlers = 0, own_attackers = 0, own_military = 0, workers = 0;
    for (const City &o : cities) own_cities += o.owner == ci;
    for (const Unit &u : units) {
        if (u.owner != ci) continue;
        if (unit_info(u.type).flags & kSettlers) {
            ++own_settlers;
            if (u.order == Order::Road || u.order == Order::Irrigate || u.order == Order::Mine) ++workers;
        }
        if (is_military(u.type)) {
            ++own_military;
            // Attackers: units stronger in attack than defence (the rest are
            // garrison troops, as in DOS).
            if (unit_info(u.type).domain == Domain::Land && unit_info(u.type).attack > unit_info(u.type).defense) ++own_attackers;
        }
    }
    auto pick = [&](bool unit_kind, int what) {
        c.build_unit = unit_kind;
        c.build = what;
        announce_wonder_start(c);
    };
    const CityStats &st = c.stats;
    // A front: an enemy we are at war with has a city we know of on this
    // landmass, or enemy troops are close.
    bool front = false;     // the enemy is close to this city
    bool war_here = false;  // an enemy city we know of shares our landmass
    bool raiders_here = false;  // a barbarian-held city within reach
    const int cont = map.at(c.x, c.y).continent;
    for (const City &o : cities) {
        if (!at_war(ci, o.owner) || map.at(o.x, o.y).continent != cont || !((map.at(o.x, o.y).explored >> ci) & 1u)) continue;
        war_here = true;
        if (civs[o.owner].barbarian && map.distance(o.x, o.y, c.x, c.y) <= 12) raiders_here = true;
        if (map.distance(o.x, o.y, c.x, c.y) <= 8) front = true;
    }
    for (const Unit &e : units) {
        if (at_war(ci, e.owner) && map.distance(e.x, e.y, c.x, c.y) <= 4) front = true;
    }
    // 1. A garrison: one defender in peace, two on a front or in a big city.
    const int need_def = (front || c.size >= 7) ? 2 : 1;
    if (defenders < need_def) {
        pick(true, best_unit(false));
        return;
    }
    // 2. Expansion while there is room: young cities wait until size 2
    // (the first few) or 3, and only a few settlers walk at once.
    const int city_target = 8 + static_cast<int>(difficulty) * 2;
    const int min_size = own_cities < 4 ? 2 : 3;
    const int workers_wanted = own_cities >= 3 ? std::max(1, (own_cities + 1) / 3) : 0;
    const int expanders = own_settlers - std::min(own_settlers, workers_wanted);
    if (own_cities + expanders < city_target && c.size >= min_size && expanders <= own_cities / 3 + 1 &&
        st.food_surplus >= 1 && !(front && own_attackers < own_cities / 2)) {
        pick(true, static_cast<int>(UnitType::Settlers));
        return;
    }
    // 3. War on this landmass: some attackers.
    // A city held by barbarians close by is worth taking back.
    if (war_here && own_attackers < own_cities / 2 + 1 + (raiders_here ? 2 : 0) && (raiders_here || rng.chance(0.5))) {
        const int att = best_unit(true);
        if (att >= 0 && unit_info(att).attack > unit_info(att).defense) {   // a real attacker, not more guards
            pick(true, att);
            return;
        }
    }
    // 3b. Ships for passengers waiting in port, warships and aircraft in
    // wartime, one ship to look around once the homeland is full.
    {
        const int naval = ai_navy_build(c);
        if (naval >= 0 && (ai_passengers(ci) > 0 || rng.chance(0.35))) {
            pick(true, naval);
            return;
        }
    }
    // 4. Improvements, most useful first for this city.
    const int corruption = st.corruption;
    struct Want {
        Building b;
        bool ok;
    };
    const Want wants[] = {
        {Building::Temple, c.size >= 2 || st.unhappy > 0},
        {Building::Granary, c.size >= 3 && st.food_surplus >= 1},
        {Building::Library, st.raw.trade >= 3},
        {Building::Marketplace, st.raw.trade >= 4 || st.lux > 0},
        {Building::Courthouse, corruption >= 2 && !c.has(Building::Palace)},
        {Building::Aqueduct, c.size >= 8},
        {Building::Colosseum, c.size >= 6 && (st.unhappy > 0 || c.entertainers > 0)},
        {Building::CityWalls, front && c.size >= 4},
        {Building::University, st.raw.trade >= 6},
        {Building::Bank, st.raw.trade >= 8},
        {Building::Cathedral, c.size >= 9 && (st.unhappy > 0 || c.entertainers > 1)},
        {Building::Factory, c.size >= 7 && st.raw.shields >= 8},
        {Building::PowerPlant, c.size >= 8},
    };
    // Core buildings still wanted here come before wonders, caravans and
    // spaceship parts most of the time.
    bool pending = false;
    for (const Want &w : wants) pending |= w.ok && can_build(c, false, static_cast<int>(w.b));
    const bool spare = !pending || rng.chance(0.25);
    // Envoys: a Diplomat for embassies (and theft in wartime), Caravans to
    // speed up our wonders or open trade routes.
    {
        int diplomats = 0, caravans = 0;
        for (const Unit &u : units) {
            if (u.owner != ci) continue;
            diplomats += u.type == UnitType::Diplomat;
            caravans += u.type == UnitType::Caravan;
        }
        bool embassy_wanted = false;
        for (const City &o : cities) {
            if (o.owner != ci && !civs[o.owner].barbarian && map.at(o.x, o.y).continent == cont && ((map.at(o.x, o.y).explored >> ci) & 1u) &&
                (!((civs[ci].embassy >> o.owner) & 1u) || (at_war(ci, o.owner) && !((o.stolen >> ci) & 1u))))
                embassy_wanted = true;
        }
        if (embassy_wanted && diplomats == 0 && own_cities >= 3 && c.size >= 3 && can_build(c, true, static_cast<int>(UnitType::Diplomat)) &&
            rng.chance(0.2)) {
            pick(true, static_cast<int>(UnitType::Diplomat));
            return;
        }
        bool wonder_near = false;
        for (const City &o : cities) {
            if (o.owner == ci && o.id != c.id && !o.build_unit && is_wonder_build(o.build) && map.at(o.x, o.y).continent == cont &&
                map.distance(o.x, o.y, c.x, c.y) <= 12)
                wonder_near = true;
        }
        if (can_build(c, true, static_cast<int>(UnitType::Caravan)) && caravans < 1 + own_cities / 4 && c.size >= 4 &&
            st.shield_surplus >= 3 && !front && spare && ((wonder_near && rng.chance(0.3)) || (static_cast<int>(c.routes.size()) < kMaxRoutes && st.raw.trade >= 5 && rng.chance(0.08)))) {
            pick(true, static_cast<int>(UnitType::Caravan));
            return;
        }
    }
    // The space race: once the Apollo Program stands, big productive cities
    // build a modest, complete spaceship and the capital launches it. Parts
    // already under way elsewhere count, so the ship comes out balanced:
    // frame enough to hold every component and module.
    if (space_open() && c.size >= 6 && st.shield_surplus >= 6 && !front && spare && civs[ci].launch_turn < 0) {
        const int part = ai_space_part(ci, c.id);
        if (part >= 0 && can_build(c, false, kSpaceBase + part)) {
            pick(false, kSpaceBase + part);
            return;
        }
        // A part we can't make yet: a frame section, if more are wanted.
        if (part >= 0 && part != static_cast<int>(SpacePart::Structural) &&
            ai_space_part(ci, c.id, true) == static_cast<int>(SpacePart::Structural) &&
            can_build(c, false, kSpaceBase + static_cast<int>(SpacePart::Structural))) {
            pick(false, kSpaceBase + static_cast<int>(SpacePart::Structural));
            return;
        }
    }
    // A productive city with the basics takes on a Wonder of the World now
    // and then, one or two at a time per civilization.
    if (c.size >= 5 && st.shield_surplus >= 4 && c.has(Building::Temple) && !front && spare) {
        int in_progress = 0;
        std::uint32_t taken = 0;
        for (const City &o : cities) {
            if (o.owner == ci && o.id != c.id && !o.build_unit && is_wonder_build(o.build)) {
                ++in_progress;
                taken |= 1u << (o.build - kWonderBase);
            }
        }
        if (in_progress < 1 + own_cities / 8 && rng.chance(0.5)) {
            std::vector<int> open;
            for (int w = 0; w < kWonderCount; ++w) {
                if (!((taken >> w) & 1u) && can_build(c, false, kWonderBase + w)) open.push_back(w);
            }
            if (!open.empty()) {
                pick(false, kWonderBase + open[rng.below(static_cast<int>(open.size()))]);
                return;
            }
        }
    }
    // A building is only worth its upkeep if the treasury can carry it;
    // a Marketplace pays its own way in a city with trade.
    const int margin = ai_gold_margin(ci);
    auto affordable = [&](Building b) {
        const int up = building_upkeep(ci, static_cast<int>(b));
        if (up <= 0 || up <= margin) return true;
        return (b == Building::Marketplace || b == Building::Bank) && st.raw.trade >= 6;
    };
    for (const Want &w : wants) {
        if (!w.ok || !can_build(c, false, static_cast<int>(w.b)) || !affordable(w.b)) continue;
        pick(false, static_cast<int>(w.b));
        return;
    }
    // 5. Workers to irrigate and mine: about one for every three cities.
    if (own_cities >= 3 && own_settlers - workers < 2 && workers < (own_cities + 2) / 3 && c.size >= 4 &&
        st.food_surplus >= 2) {
        pick(true, static_cast<int>(UnitType::Settlers));
        return;
    }
    // 6. Anything else still worth building, then a modest army.
    for (int b = 1; b < kBuildingCount; ++b) {
        const Building bb = static_cast<Building>(b);
        if (bb == Building::Palace || bb == Building::SdiDefense || bb == Building::Barracks) continue;
        if (bb == Building::CityWalls && !front) continue;
        if (can_build(c, false, b) && building_info(b).upkeep <= (c.size >= 4 ? 2 : 1) && affordable(bb)) {
            pick(false, b);
            return;
        }
    }
    if (own_military < own_cities + 1) {
        pick(true, best_unit(front && rng.chance(0.5)));
        return;
    }
    // Nothing needed: a worker for later.
    if (c.size >= 2 && own_settlers <= own_cities / 2) {
        pick(true, static_cast<int>(UnitType::Settlers));
        return;
    }
    // Rather than an ever larger standing army: Caravans, which help build
    // wonders or open trade routes, or a wonder of our own.
    int caravans = 0;
    for (const Unit &u : units) caravans += u.owner == ci && u.type == UnitType::Caravan;
    if (can_build(c, true, static_cast<int>(UnitType::Caravan)) && caravans < 2 + own_cities / 3) {
        pick(true, static_cast<int>(UnitType::Caravan));
        return;
    }
    for (int w = 0; w < kWonderCount; ++w) {
        if (can_build(c, false, kWonderBase + w)) {
            pick(false, kWonderBase + w);
            return;
        }
    }
    if (own_military < 2 * own_cities + 2) {
        pick(true, best_unit(war_here && rng.chance(0.5)));
        return;
    }
    // Truly nothing: a worker to improve the land, or a defender in a
    // city too small to spare the people.
    pick(true, c.size >= 3 ? static_cast<int>(UnitType::Settlers) : best_unit(false));
}

// ai_diplomacy: see diplomacy.cpp.

bool Game::ai_settler(Unit &u)
{
    const int ci = u.owner;
    const int id = u.id;
    if (u.order == Order::Road || u.order == Order::Irrigate || u.order == Order::Mine) return true;
    if (u.carried_by >= 0) {
        if (u.order != Order::Sentry) ai_disembark(id);
        return true;
    }
    if (u.plan == kPlanPassenger) {
        const City *port = city_at(u.x, u.y);
        int sx = -1, sy = -1;
        if (port && port->owner == ci && ++u.work <= 20 && ai_overseas_site(ci, u.x, u.y, sx, sy)) {
            u.order = Order::Sentry;
            return true;
        }
        u.plan = kPlanNone;
        u.work = 0;
        u.order = Order::None;
    }
    int own_cities = 0;
    for (const City &c : cities) own_cities += c.owner == ci;
    int improving = 0;
    for (const Unit &o : units) {
        if (o.owner == ci && o.id != id && (unit_info(o.type).flags & kSettlers) &&
            (o.order == Order::Road || o.order == Order::Irrigate || o.order == Order::Mine)) {
            ++improving;
        }
    }
    const int city_target = 8 + static_cast<int>(difficulty) * 2 + 4;
    const int want_workers = own_cities >= 3 ? std::max(1, (own_cities + 1) / 3) : 0;
    auto site_ok = [&](int x, int y) {
        if (!map.land(x, y) || !map.valid_y(y) || y < 2 || y >= map.h - 2) return false;
        if (map.at(x, y).terrain == Terrain::Arctic) return false;
        for (const City &c : cities) {
            if (map.distance(c.x, c.y, x, y) < 4) return false;
        }
        const int o = owner_at(x, y);
        return o < 0 || o == ci;
    };
    auto site_score = [&](int x, int y) {
        int s = 0;
        for (const auto &o : kCrossAi) {
            if (!map.valid_y(y + o[1])) continue;
            const Yield v = tile_yield(x + o[0], y + o[1], ci, false);
            s += v.food * 3 + v.shields * 2 + v.trade;
        }
        const Yield cy = tile_yield(x, y, ci, true);
        s += cy.food * 4 + cy.shields * 3 + cy.trade * 2;
        if (map.coastal(x, y)) s += 6;
        if (map.at(x, y).terrain == Terrain::River) s += 6;
        return s;
    };
    const bool heading_to_site = u.order == Order::Goto && u.goto_x >= 0 && site_ok(u.goto_x, u.goto_y);
    const bool be_worker = !heading_to_site && own_cities >= 1 && improving < want_workers;
    if (own_cities < city_target && !be_worker) {
        if (own_cities == 0 && site_ok(u.x, u.y)) {
            found_city(id);
            return true;
        }
        if (heading_to_site) {
            for (int g = 0; g < 8; ++g) {
                Unit *p = unit(id);
                if (!p || p->moves <= 0 || p->order != Order::Goto) break;
                if (!step_goto(id)) break;
            }
            Unit *p = unit(id);
            if (p && p->x == p->goto_x && p->y == p->goto_y && site_ok(p->x, p->y)) {
                found_city(id);
            } else if (p && p->order != Order::Goto && site_ok(p->x, p->y)) {
                found_city(id);
            }
            return true;
        }
        int best = -(1 << 30), bx = -1, by = -1;
        const int cont = map.at(u.x, u.y).continent;
        for (int dy = -9; dy <= 9; ++dy) {
            for (int dx = -9; dx <= 9; ++dx) {
                const int x = map.wrap_x(u.x + dx), y = u.y + dy;
                if (!map.valid_y(y) || !site_ok(x, y) || map.at(x, y).continent != cont) continue;
                const int s = site_score(x, y) - map.distance(u.x, u.y, x, y) * 4;
                if (s > best) {
                    best = s;
                    bx = x;
                    by = y;
                }
            }
        }
        if (bx >= 0) {
            if (bx == u.x && by == u.y) {
                found_city(id);
                return true;
            }
            set_goto(id, bx, by);
            for (int g = 0; g < 8; ++g) {
                Unit *p = unit(id);
                if (!p || p->moves <= 0 || p->order != Order::Goto) break;
                if (!step_goto(id)) break;
            }
            Unit *p = unit(id);
            if (p && p->x == bx && p->y == by && site_ok(bx, by)) found_city(id);
            return true;
        }
        // No room left on this landmass: a site we know of overseas, if
        // we have (or can build) a ship and not too many already wait.
        int sx = -1, sy = -1;
        if (own_cities >= 1 && ai_has_ferry(ci) && ai_passengers(ci) < 2 && ai_overseas_site(ci, u.x, u.y, sx, sy)) {
            if (ai_seek_ship(id)) return true;
        }
    }
    // Improve land: the job that adds the most to a square one of our
    // cities works (or could work), close by.
    int tx = -1, ty = -1;
    Order job = Order::None;
    int best = 0;
    for (const City &c : cities) {
        if (c.owner != ci || map.distance(c.x, c.y, u.x, u.y) > 10) continue;
        for (const auto &o : kCrossAi) {
            const int x = map.wrap_x(c.x + o[0]), y = c.y + o[1];
            if (!map.land(x, y)) continue;
            if (owner_at(x, y) >= 0 && owner_at(x, y) != ci) continue;
            bool worked = false;
            for (const auto &w : c.worked) worked |= w.first == o[0] && w.second == o[1];
            bool taken = false;
            for (const Unit &o2 : units) {
                if (o2.id != u.id && o2.x == x && o2.y == y && (unit_info(o2.type).flags & kSettlers)) taken = true;
            }
            if (taken) continue;
            Unit probe = u;
            probe.x = x;
            probe.y = y;
            const Yield before = tile_yield(x, y, ci, false);
            const Order jobs[] = {Order::Irrigate, Order::Mine, Order::Road};
            for (Order j : jobs) {
                if (!can_work(probe, j)) continue;
                Tile &t = map.at(x, y);
                const Tile keep = t;
                const TerrainInfo &ti = terrain_info(t.terrain);
                int turns = 0;
                if (j == Order::Irrigate) {
                    // Only plain irrigation; changing forest or jungle is slow.
                    if (ti.irrigate_to != t.terrain) continue;
                    t.irrigation = true;
                    t.mine = false;
                    turns = ti.irrigate_turns;
                } else if (j == Order::Mine) {
                    if (ti.mine_to != t.terrain) continue;
                    t.mine = true;
                    t.irrigation = false;
                    turns = ti.mine_turns;
                } else {
                    t.road = true;
                    turns = ti.road_turns;
                }
                const Yield after = tile_yield(x, y, ci, false);
                t = keep;
                int gain = (after.food - before.food) * 30 + (after.shields - before.shields) * 22 + (after.trade - before.trade) * 18;
                if (gain <= 0) continue;
                if (worked) gain *= 2;
                const int s = gain / std::max(1, turns) - map.distance(u.x, u.y, x, y) * 3;
                if (s > best) {
                    best = s;
                    tx = x;
                    ty = y;
                    job = j;
                }
            }
        }
    }
    if (tx < 0) {
        if (own_cities < city_target && be_worker) {
            // Nothing worth doing nearby: found a city after all.
            Unit *p = unit(id);
            if (p && site_ok(p->x, p->y)) {
                found_city(id);
                return true;
            }
        }
        if (join_city(id)) return true;
        sentry(id);
        return true;
    }
    if (u.x != tx || u.y != ty) {
        set_goto(id, tx, ty);
        for (int g = 0; g < 8; ++g) {
            Unit *p = unit(id);
            if (!p || p->moves <= 0 || p->order != Order::Goto) break;
            if (!step_goto(id)) break;
        }
    }
    Unit *p = unit(id);
    if (p && p->x == tx && p->y == ty && p->moves > 0) {
        p->order = Order::None;
        start_work(id, job);
    }
    return true;
}

void Game::ai_envoy(Unit &start)
{
    const int id = start.id;
    const int ci = start.owner;
    const bool diplomat = start.type == UnitType::Diplomat;
    const int cont = map.at(start.x, start.y).continent;
    // Pick a city to visit on this landmass.
    const City *target = nullptr;
    int best = 1 << 30;
    for (const City &c : cities) {
        if (map.at(c.x, c.y).continent != cont) continue;
        const int d = map.distance(start.x, start.y, c.x, c.y);
        int score = d;
        if (civs[c.owner].barbarian) continue;
        if (diplomat) {
            if (c.owner == ci || !((map.at(c.x, c.y).explored >> ci) & 1u)) continue;
            const bool embassy = (civs[ci].embassy >> c.owner) & 1u;
            const bool steal = at_war(ci, c.owner) && !((c.stolen >> ci) & 1u);
            if (embassy && !steal) continue;
        } else {
            if (c.owner == ci && !c.build_unit && is_wonder_build(c.build)) {
                score = d - 100;   // a wonder of ours comes first
            } else {
                const City *home = city(start.home);
                if (!home || home->id == c.id || map.distance(home->x, home->y, c.x, c.y) < 9) continue;
                if (static_cast<int>(home->routes.size()) >= kMaxRoutes) continue;
                if (std::find(home->routes.begin(), home->routes.end(), c.id) != home->routes.end()) continue;
                if (c.owner != ci && (at_war(ci, c.owner) || !((map.at(c.x, c.y).explored >> ci) & 1u))) continue;
                score = d - c.size * 2;
            }
        }
        if (score < best) {
            best = score;
            target = &c;
        }
    }
    if (!target) {
        Unit *u = unit(id);
        if (u && u->order == Order::None) u->order = Order::Sentry;
        return;
    }
    const int tx = target->x, ty = target->y, tid = target->id;
    for (int guard = 0; guard < 8; ++guard) {
        Unit *u = unit(id);
        if (!u || u->moves <= 0) return;
        u->order = Order::None;
        if (map.distance(u->x, u->y, tx, ty) <= 1) {
            const City *c = city(tid);
            if (!c) return;
            if (diplomat) {
                if (!((civs[ci].embassy >> c->owner) & 1u)) diplomat_act(id, tx, ty, DiploAction::Embassy);
                else if (diplomat_can(id, tx, ty, DiploAction::StealTech)) diplomat_act(id, tx, ty, DiploAction::StealTech);
                else diplomat_act(id, tx, ty, DiploAction::Sabotage);
                return;
            }
            if (caravan_can_help(*u, *c)) {
                caravan_help(id, tid);
                return;
            }
            if (caravan_can_trade(*u, *c)) {
                caravan_trade(id, tid);
                return;
            }
            if (c->owner == ci && (u->x != tx || u->y != ty)) {
                move(id, tx - u->x, ty - u->y);
                continue;
            }
            u->order = Order::Sentry;
            return;
        }
        int sx = 0, sy = 0;
        if (!next_step(*u, tx, ty, sx, sy)) {
            u->order = Order::Sentry;
            return;
        }
        const MoveResult r = move(id, sx, sy);
        if (r != MoveResult::Moved && r != MoveResult::Caravan) return;
    }
}

void Game::ai_unit(Unit &start)
{
    const int id = start.id;
    const int ci = start.owner;
    const UnitInfo &info = unit_info(start.type);
    if (start.carried_by >= 0 && info.domain == Domain::Land) {
        // Aboard: wait while the ship sails; step ashore once woken.
        if (start.order != Order::Sentry) ai_disembark(id);
        return;
    }
    if (info.flags & kSettlers) {
        ai_settler(start);
        return;
    }
    if (start.type == UnitType::Diplomat || start.type == UnitType::Caravan) {
        ai_envoy(start);
        return;
    }
    if (info.domain == Domain::Sea) {
        ai_ship(id);
        return;
    }
    if (info.domain == Domain::Air) {
        ai_air(id);
        return;
    }
    if (!is_military(start.type)) {
        Unit *u = unit(id);
        if (u && u->order == Order::None) u->order = Order::Sentry;
        return;
    }
    // Chance to beat the best defender on a square.
    auto win_chance_at = [&](const Unit &att, int x, int y) {
        double worst = 1.0;
        for (int did : units_at(x, y)) {
            const Unit *d = unit(did);
            worst = std::min(worst, combat_win_chance(attack_strength(att), defense_strength(*d, &att), att.hp, d->hp));
        }
        return worst;
    };
    // A city of ours that needs a garrison?
    auto garrison_need = [&](const City &c) {
        int n = 0;
        for (const Unit &o : units) {
            if (o.owner == ci && o.x == c.x && o.y == c.y && is_military(o.type) && unit_info(o.type).domain == Domain::Land) ++n;
        }
        return ((civs[ci].war || c.size >= 5) ? 2 : 1) - n;
    };
    {
        // Waiting in port for a ship (a few turns at most), unless the city
        // itself needs the guard.
        Unit *u = unit(id);
        if (!u) return;
        if (u->plan == kPlanPassenger) {
            const City *c = city_at(u->x, u->y);
            if (c && c->owner == ci && garrison_need(*c) <= 0 && ++u->work <= 20 && ai_overseas_enemy(ci, u->x, u->y)) {
                u->order = Order::Sentry;
                return;
            }
            u->plan = kPlanNone;
            u->work = 0;
            u->order = Order::None;
        }
    }
    {
        Unit *u = unit(id);
        if (!u) return;
        if (u->order == Order::Fortified || u->order == Order::Fortifying) {
            const City *c = city_at(u->x, u->y);
            if (!c) {
                // Dug in out in the field to heal or to wait for help: an
                // attacker takes up the march again once mostly healed.
                if (info.attack <= info.defense || u->hp < kUnitHp * 3 / 4 || !rng.chance(0.25)) return;
            } else if (garrison_need(*c) >= 0 || info.attack <= info.defense) {
                return;   // stay unless the city has more guards than it needs
            }
            u->order = Order::None;
        }
        if (const City *c = city_at(u->x, u->y)) {
            if (c->owner == ci && garrison_need(*c) > 0) {
                fortify(id);
                return;
            }
        }
    }
    {
        // Badly hurt: rest where it stands (healing is faster fortified,
        // complete in a city).
        Unit *u = unit(id);
        if (u && u->hp <= kUnitHp / 2 && !city_at(u->x, u->y)) {
            fortify(id);
            return;
        }
    }
    for (int guard = 0; guard < 12; ++guard) {
        Unit *u = unit(id);
        if (!u || u->moves <= 0) return;
        // Attack a weak neighbour.
        int ax = 0, ay = 0;
        double best_odds = 0;
        for (int j = -1; j <= 1; ++j) {
            for (int i = -1; i <= 1; ++i) {
                if (!i && !j) continue;
                const int x = map.wrap_x(u->x + i), y = u->y + j;
                if (!map.land(x, y)) continue;
                const int o = owner_at(x, y);
                if (o < 0 || o == ci || !at_war(ci, o) || units_at(x, y).empty()) continue;
                const double odds = win_chance_at(*u, x, y);
                if (odds > best_odds) {
                    best_odds = odds;
                    ax = i;
                    ay = j;
                }
            }
        }
        if (best_odds >= 0.55 && info.attack > 0) {
            move(id, ax, ay);
            continue;
        }
        // March on the nearest enemy city we know of. Units as strong in
        // defence as in attack (Militia, Musketeers, Mech. Inf.) are
        // defenders, as in the DOS game, and stay home.
        int tx = -1, ty = -1, td = 1 << 30;
        if (info.attack > info.defense) {
            const int cont = map.at(u->x, u->y).continent;
            for (const City &c : cities) {
                if (!at_war(ci, c.owner)) continue;
                if (!((map.at(c.x, c.y).explored >> ci) & 1u) || map.at(c.x, c.y).continent != cont) continue;
                const int d = map.distance(u->x, u->y, c.x, c.y);
                if (d < td) {
                    td = d;
                    tx = c.x;
                    ty = c.y;
                }
            }
        }
        if (tx >= 0) {
            int sx = 0, sy = 0;
            if (!next_step(*u, tx, ty, sx, sy, 3000)) break;
            const int nx = map.wrap_x(u->x + sx), ny = u->y + sy;
            const int o = owner_at(nx, ny);
            if (o >= 0 && o != ci && !units_at(nx, ny).empty()) {
                if (win_chance_at(*u, nx, ny) < 0.25) {
                    fortify(id); // wait for reinforcements
                    return;
                }
            }
            const MoveResult r = move(id, sx, sy);
            if (r == MoveResult::Blocked || r == MoveResult::Zoc || r == MoveResult::NeedWar) break;
            continue;
        }
        // At war with nobody on this landmass but someone overseas: off to
        // a port to wait for a ship.
        if (info.attack > info.defense && ai_overseas_enemy(ci, u->x, u->y) && ai_has_ferry(ci) && ai_passengers(ci) < 6) {
            if (ai_seek_ship(id)) return;
        }
        // Explore: walk to the nearest land square next to the unknown.
        {
            const int W = map.w;
            std::vector<int> seen(static_cast<size_t>(W) * map.h, 0);
            std::queue<std::pair<int, int>> q;
            q.push({u->x, u->y});
            seen[static_cast<size_t>(u->y) * W + u->x] = 1;
            int fx = -1, fy = -1, steps = 0;
            while (!q.empty() && steps < 1500) {
                auto [x, y] = q.front();
                q.pop();
                ++steps;
                bool frontier = false;
                for (int j = -1; j <= 1 && !frontier; ++j) {
                    for (int i = -1; i <= 1; ++i) {
                        if (map.valid_y(y + j) && !((map.at(x + i, y + j).explored >> ci) & 1u)) {
                            frontier = true;
                            break;
                        }
                    }
                }
                if (frontier && !(x == u->x && y == u->y)) {
                    fx = x;
                    fy = y;
                    break;
                }
                for (int j = -1; j <= 1; ++j) {
                    for (int i = -1; i <= 1; ++i) {
                        const int nx = map.wrap_x(x + i), ny = y + j;
                        if (!map.land(nx, ny) || seen[static_cast<size_t>(ny) * W + nx]) continue;
                        const int o = owner_at(nx, ny);
                        if (o >= 0 && o != ci) continue;
                        seen[static_cast<size_t>(ny) * W + nx] = 1;
                        q.push({nx, ny});
                    }
                }
            }
            if (fx >= 0) {
                int sx = 0, sy = 0;
                if (!next_step(*u, fx, fy, sx, sy, 2000)) break;
                const MoveResult r = move(id, sx, sy);
                if (r == MoveResult::Blocked || r == MoveResult::Zoc || r == MoveResult::NeedWar) break;
                continue;
            }
        }
        // Nothing to do: go home to the city that most needs a guard.
        {
            const City *home = nullptr;
            int score = 1 << 30;
            for (const City &c : cities) {
                if (c.owner != ci) continue;
                const int s = map.distance(u->x, u->y, c.x, c.y) - garrison_need(c) * 10;
                if (s < score) {
                    score = s;
                    home = &c;
                }
            }
            if (!home) {
                fortify(id);
                return;
            }
            if (home->x == u->x && home->y == u->y) {
                fortify(id);
                return;
            }
            int sx = 0, sy = 0;
            if (!next_step(*u, home->x, home->y, sx, sy, 3000)) {
                fortify(id);
                return;
            }
            const MoveResult r = move(id, sx, sy);
            if (r != MoveResult::Moved) {
                fortify(id);
                return;
            }
        }
    }
}

Government Game::ai_government(int ci) const
{
    // DOS (AIEngine, CityWorker): the Republic when the trade it brings
    // outweighs the discontent of troops in the field -- each weighs less
    // for a civilized people and where a Marketplace calms the city -- and
    // not while at war with a people that is ahead in science; Communism for
    // a big empire; otherwise Monarchy.
    const Civ &c = civs[ci];
    const int ideology = c.tribe >= 0 ? tribe_info(c.tribe).ideology : 0;
    int own = 0, score = 0;
    for (const City &ct : cities) {
        if (ct.owner != ci) continue;
        ++own;
        for (const auto &w : ct.worked) score += tile_yield(ct.x + w.first, ct.y + w.second, ci, false).trade > 0;
    }
    for (const Unit &u : units) {
        if (u.owner != ci || !is_military(u.type) || unit_info(u.type).attack == 0) continue;
        const City *in = city_at(u.x, u.y);
        if (in && in->owner == ci) continue;
        const City *home = city(u.home);
        score -= (home && home->has(Building::Marketplace) ? 4 : 6) - 2 * ideology;
    }
    for (int o = 0; o < static_cast<int>(civs.size()); ++o) {
        if (o != ci && civs[o].alive && !civs[o].barbarian && at_war(ci, o) && techs_known(o) > techs_known(ci)) score = -999;
    }
    // Once a Republic, it takes clear trouble to give it up.
    if (c.govt == Government::Republic) score += 8 + own;
    if (has(ci, Advance::Republic) && score >= 0) return Government::Republic;
    if (has(ci, Advance::Communism) && own > 10) return Government::Communism;
    if (has(ci, Advance::Monarchy)) return Government::Monarchy;
    return Government::Despotism;
}

int Game::ai_gold_margin(int ci) const
{
    // Gold a turn left over with taxes as high as the government allows,
    // after paying for buildings (an estimate from the cities' current tax).
    const Civ &c = civs[ci];
    const int mx = government_info(c.govt).max_rate;
    int income = 0, upkeep = 0;
    for (const City &ct : cities) {
        if (ct.owner != ci) continue;
        income += ct.stats.tax;
        for (int b = 0; b < kBuildingCount; ++b) {
            if (ct.has(static_cast<Building>(b))) upkeep += building_upkeep(ci, b);
        }
        if (!ct.build_unit && ct.build >= 0 && ct.build < kBuildingCount) upkeep += building_upkeep(ci, ct.build);
    }
    if (c.tax > 0) income = income * mx / c.tax;
    return income - upkeep;
}

void Game::ai_rates(int ci)
{
    // As much science as the treasury allows: the lowest tax that pays for
    // the buildings (and puts something by while the treasury is thin).
    Civ &c = civs[ci];
    const int mx = government_info(c.govt).max_rate;
    int lux = 0, idle = 0, citizens = 0, own = 0, upkeep = 0;
    for (const City &ct : cities) {
        if (ct.owner != ci) continue;
        ++own;
        idle += ct.entertainers;
        citizens += ct.size;
        for (int b = 0; b < kBuildingCount; ++b) {
            if (ct.has(static_cast<Building>(b))) upkeep += building_upkeep(ci, b);
        }
    }
    // Some luxuries when cities have to keep many entertainers.
    if (citizens > 0 && idle * 6 > citizens) lux = std::min(3, mx);
    const int reserve = 40 + 5 * own;
    const int need = upkeep + (c.gold < reserve ? std::max(2, own / 2) : 0);
    const int lowest = std::max(0, 10 - mx - lux);
    for (int tax = lowest; tax <= mx; ++tax) {
        const int sci = 10 - tax - lux;
        if (sci < 0) break;
        if (sci > mx) continue;
        if (c.tax != tax || c.lux != lux || c.sci != sci) set_rates(ci, tax, lux, sci);
        int income = 0;
        for (const City &ct : cities) income += ct.owner == ci ? ct.stats.tax : 0;
        if (income >= need) return;
    }
}

void Game::ai_play(int ci)
{
    Civ &c = civs[ci];
    ai_diplomacy(ci);
    if (c.research == Advance::None) choose_research_ai(ci);
    if (can_launch(ci)) {
        // Launch a complete ship, or a viable one once a rival is on its way.
        const SpaceStats ss = space_stats(ci);
        bool rival = false;
        for (const Civ &o : civs) rival |= &o != &c && o.launch_turn >= 0;
        if ((ss.success_pct >= 100 && ss.pairs >= 2) || rival) launch(ci);
    }
    // DOS (AIEngine): every eighth turn the ruler weighs its government; a
    // Despot moves to Monarchy as soon as it can.
    if (c.govt != Government::Anarchy) {
        const Government want = ai_government(ci);
        if (want != c.govt && (((ci + turn) & 7) == 0 || c.govt == Government::Despotism)) revolution(ci, want);
    }
    if (c.govt != Government::Anarchy) ai_rates(ci);
    for (City &ct : cities) {
        if (ct.owner == ci && (!can_build(ct, ct.build_unit, ct.build) || ct.founded == turn || ct.shields == 0)) ai_city(ct);
    }
    // Rush a defender into a weakly held city with enemies (or raiders) close.
    for (City &ct : cities) {
        if (ct.owner != ci) continue;
        int guards = 0, threats = 0;
        for (const Unit &u : units) {
            guards += u.owner == ci && u.x == ct.x && u.y == ct.y && is_military(u.type);
            threats += at_war(ci, u.owner) && is_military(u.type) && unit_info(u.type).attack > 0 && map.distance(u.x, u.y, ct.x, ct.y) <= 2;
        }
        if (threats == 0 || guards >= 2 || (guards == 1 && threats < 2)) continue;
        if (!ct.build_unit || unit_info(ct.build).defense < 2) {
            int best = -1;
            for (int u = 0; u < kUnitTypeCount; ++u) {
                if (can_build(ct, true, u) && unit_info(u).domain == Domain::Land && is_military(static_cast<UnitType>(u)) &&
                    (best < 0 || unit_info(u).defense > unit_info(best).defense))
                    best = u;
            }
            if (best >= 0 && (ct.build_unit || ct.shields < 10)) {
                ct.build_unit = true;
                ct.build = best;
            }
        }
        if (ct.build_unit && unit_info(ct.build).defense > 0 && buy_cost(ct) <= c.gold) buy(ct.id);
    }
    // Spend a fat treasury: rush the cheapest job that is half done.
    for (int guard = 0; guard < 4 && c.gold > 120; ++guard) {
        City *best = nullptr;
        int best_cost = 1 << 30;
        for (City &ct : cities) {
            if (ct.owner != ci || ct.bought) continue;
            const int cost = buy_cost(ct);
            if (cost > 0 && cost < best_cost && ct.shields * 2 >= production_cost(ct)) {
                best_cost = cost;
                best = &ct;
            }
        }
        if (!best || best_cost > c.gold - 80) break;
        buy(best->id);
    }
    // An army far larger than the cities need is disbanded a unit at a
    // time, the weakest of an overfull garrison first, to save its upkeep.
    {
        int own = 0, mil = 0;
        for (const City &ct : cities) own += ct.owner == ci;
        for (const Unit &u : units) mil += u.owner == ci && is_military(u.type);
        for (const City &ct : cities) {
            if (mil <= 3 * own + 4) break;
            if (ct.owner != ci) continue;
            int weakest = -1, guards = 0;
            for (const Unit &u : units) {
                if (u.owner != ci || u.x != ct.x || u.y != ct.y || !is_military(u.type) || unit_info(u.type).domain != Domain::Land) continue;
                ++guards;
                if (unit_info(u.type).attack > unit_info(u.type).defense) continue;   // attackers resting here march again
                if (weakest < 0 || unit_info(u.type).defense < unit_info(unit(weakest)->type).defense) weakest = u.id;
            }
            if (guards > (ct.size >= 7 ? 3 : 2) && weakest >= 0) {
                disband(weakest);
                --mil;
            }
        }
    }
    std::vector<int> ids;
    for (const Unit &u : units) {
        if (u.owner == ci) ids.push_back(u.id);
    }
    for (int id : ids) {
        if (Unit *u = unit(id)) ai_unit(*u);
    }
}

} // namespace civ
