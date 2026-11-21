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
    Advance::BronzeWorking, Advance::Alphabet,       Advance::CeremonialBurial, Advance::CodeOfLaws,
    Advance::Monarchy,      Advance::Pottery,        Advance::Masonry,          Advance::Writing,
    Advance::Currency,      Advance::HorsebackRiding, Advance::IronWorking,     Advance::TheWheel,
    Advance::Mathematics,   Advance::Mysticism,      Advance::Literacy,         Advance::Philosophy,
    Advance::Trade,         Advance::Construction,   Advance::Banking,          Advance::Feudalism,
    Advance::University,    Advance::Astronomy,      Advance::Religion,         Advance::Invention,
    Advance::Gunpowder,     Advance::BridgeBuilding,
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

void Game::ai_city(City &c)
{
    const int ci = c.owner;
    bool war = civs[ci].war != 0;
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
    int own_cities = 0, own_settlers = 0, own_attackers = 0;
    for (const City &o : cities) own_cities += o.owner == ci;
    for (const Unit &u : units) {
        if (u.owner != ci) continue;
        if (unit_info(u.type).flags & kSettlers) ++own_settlers;
        if (is_military(u.type) && u.order != Order::Fortified && u.order != Order::Fortifying) ++own_attackers;
    }
    auto pick = [&](bool unit_kind, int what) {
        c.build_unit = unit_kind;
        c.build = what;
    };
    const int need_def = (war || c.size >= 5) ? 2 : 1;
    if (defenders < need_def) {
        pick(true, best_unit(false));
        return;
    }
    const int city_target = 7 + static_cast<int>(difficulty) * 2;
    if (own_cities + own_settlers < city_target && c.size >= 2 && own_settlers <= own_cities / 3 + 1 && c.stats.food_surplus >= 1) {
        pick(true, static_cast<int>(UnitType::Settlers));
        return;
    }
    if (war && own_attackers < own_cities + 1 && rng.chance(0.6)) {
        pick(true, best_unit(true));
        return;
    }
    const Building wish[] = {
        Building::Temple,     Building::Granary,    Building::Library,   Building::Marketplace, Building::CityWalls,
        Building::Courthouse, Building::Aqueduct,   Building::Colosseum, Building::University,  Building::Bank,
        Building::Cathedral,  Building::Barracks,   Building::Factory,   Building::PowerPlant,
    };
    for (Building b : wish) {
        if (!can_build(c, false, static_cast<int>(b))) continue;
        if (b == Building::Temple && c.size < 2) continue;
        if (b == Building::CityWalls && !war) continue;
        if (b == Building::Aqueduct && c.size < 8) continue;
        if (b == Building::Colosseum && c.size < 6) continue;
        if (b == Building::Cathedral && c.size < 9) continue;
        if (b == Building::Barracks) continue;
        if (b == Building::Courthouse && c.has(Building::Palace)) continue;
        if ((b == Building::Factory || b == Building::PowerPlant) && c.size < 8) continue;
        pick(false, static_cast<int>(b));
        return;
    }
    if (own_cities + own_settlers < city_target + 4 && c.size >= 3) {
        pick(true, static_cast<int>(UnitType::Settlers));
        return;
    }
    pick(true, best_unit(rng.chance(0.5)));
}

void Game::ai_diplomacy(int ci)
{
    Civ &me = civs[ci];
    auto strength = [&](int k) {
        int s = 0;
        for (const Unit &u : units) {
            if (u.owner == k && is_military(u.type)) s += unit_info(u.type).attack + unit_info(u.type).defense;
        }
        for (const City &c : cities) {
            if (c.owner == k) s += c.size;
        }
        return s;
    };
    const int mine = strength(ci);
    for (int k = 0; k < static_cast<int>(civs.size()); ++k) {
        if (k == ci || !civs[k].alive || !((me.contact >> k) & 1u)) continue;
        if (at_war(ci, k)) {
            // Tired of a war that goes nowhere.
            if (turn % 16 == 0 && rng.chance(0.15)) {
                make_peace(ci, k);
                say(ci, MsgKind::Good, "Peace with the " + civs[k].name + ".");
                say(k, MsgKind::Good, "The " + me.name + " agree to peace.");
            }
            continue;
        }
        if (turn < 50) continue;
        const int theirs = std::max(1, strength(k));
        double p = 0.01 + 0.006 * static_cast<int>(difficulty);
        if (mine > theirs * 3 / 2) p *= 3;
        if (mine < theirs) p *= 0.3;
        if (rng.chance(p)) declare_war(ci, k);
    }
}

bool Game::ai_settler(Unit &u)
{
    const int ci = u.owner;
    const int id = u.id;
    if (u.order == Order::Road || u.order == Order::Irrigate || u.order == Order::Mine || u.order == Order::Sentry) {
        if (u.order != Order::Sentry) return true;
    }
    int own_cities = 0;
    for (const City &c : cities) own_cities += c.owner == ci;
    const int city_target = 7 + static_cast<int>(difficulty) * 2 + 4;
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
    if (own_cities < city_target) {
        if (own_cities == 0 && site_ok(u.x, u.y)) {
            found_city(id);
            return true;
        }
        // Heading somewhere already?
        if (u.order == Order::Goto && u.goto_x >= 0 && site_ok(u.goto_x, u.goto_y)) {
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
    }
    // No room left: improve land around the nearest city.
    const City *home = nullptr;
    int hd = 1 << 30;
    for (const City &c : cities) {
        if (c.owner != ci) continue;
        const int d = map.distance(c.x, c.y, u.x, u.y);
        if (d < hd) {
            hd = d;
            home = &c;
        }
    }
    if (!home) {
        sentry(id);
        return true;
    }
    int tx = -1, ty = -1;
    Order job = Order::None;
    int best = -(1 << 30);
    for (const auto &o : kCrossAi) {
        const int x = map.wrap_x(home->x + o[0]), y = home->y + o[1];
        if (!map.land(x, y)) continue;
        if (owner_at(x, y) >= 0 && owner_at(x, y) != ci) continue;
        Unit probe = u;
        probe.x = x;
        probe.y = y;
        const Terrain t = map.at(x, y).terrain;
        const bool farm = t == Terrain::Grassland || t == Terrain::Plains || t == Terrain::Desert || t == Terrain::River;
        Order j = Order::None;
        int s = 0;
        if (farm && can_work(probe, Order::Irrigate)) {
            j = Order::Irrigate;
            s = 20;
        } else if (farm && can_work(probe, Order::Road)) {
            j = Order::Road;
            s = 12;
        } else if (t == Terrain::Hills && can_work(probe, Order::Mine) && !map.at(x, y).irrigation) {
            j = Order::Mine;
            s = 8;
        }
        if (j == Order::None) continue;
        // Skip squares another settler is already working.
        bool taken = false;
        for (const Unit &o2 : units) {
            if (o2.id != u.id && o2.x == x && o2.y == y && (unit_info(o2.type).flags & kSettlers)) taken = true;
        }
        if (taken) continue;
        s -= map.distance(u.x, u.y, x, y) * 2;
        if (s > best) {
            best = s;
            tx = x;
            ty = y;
            job = j;
        }
    }
    if (tx < 0) {
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

void Game::ai_unit(Unit &start)
{
    const int id = start.id;
    const int ci = start.owner;
    const UnitInfo &info = unit_info(start.type);
    if (info.flags & kSettlers) {
        ai_settler(start);
        return;
    }
    if (info.domain != Domain::Land || !is_military(start.type)) {
        // Ships, aircraft, diplomats and caravans wait at home for now.
        Unit *u = unit(id);
        if (u && u->order == Order::None) u->order = Order::Sentry;
        return;
    }
    auto best_defense_at = [&](const Unit &att, int x, int y) {
        double best = 0;
        for (int did : units_at(x, y)) {
            const Unit *d = unit(did);
            best = std::max(best, defense_strength(*d, &att));
        }
        return best;
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
        Unit *u = unit(id);
        if (!u) return;
        if (u->order == Order::Fortified || u->order == Order::Fortifying) {
            const City *c = city_at(u->x, u->y);
            // Stay unless the city has more guards than it needs.
            if (!c || garrison_need(*c) >= 0 || info.attack <= info.defense) return;
            u->order = Order::None;
        }
        if (const City *c = city_at(u->x, u->y)) {
            if (c->owner == ci && garrison_need(*c) > 0) {
                fortify(id);
                return;
            }
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
                const double a = attack_strength(*u), d = best_defense_at(*u, x, y);
                const double odds = a + d > 0 ? a / (a + d) : 0;
                if (odds > best_odds) {
                    best_odds = odds;
                    ax = i;
                    ay = j;
                }
            }
        }
        if (best_odds >= 0.45 && info.attack > 0) {
            move(id, ax, ay);
            continue;
        }
        // March on the nearest enemy city we know of.
        int tx = -1, ty = -1, td = 1 << 30;
        if (civs[ci].war && info.attack >= info.defense) {
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
                const double a = attack_strength(*u), d = best_defense_at(*u, nx, ny);
                if (a / (a + d + 1e-9) < 0.3) {
                    fortify(id); // wait for reinforcements
                    return;
                }
            }
            const MoveResult r = move(id, sx, sy);
            if (r == MoveResult::Blocked || r == MoveResult::Zoc || r == MoveResult::NeedWar) break;
            continue;
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

void Game::ai_play(int ci)
{
    Civ &c = civs[ci];
    ai_diplomacy(ci);
    if (c.research == Advance::None) choose_research_ai(ci);
    if (c.govt == Government::Despotism && has(ci, Advance::Monarchy)) revolution(ci, Government::Monarchy);
    if (c.govt != Government::Anarchy) {
        const int mx = government_info(c.govt).max_rate;
        int sci = mx, tax = 10 - mx, lux = 0;
        if (c.gold < 30) {
            sci = std::max(10 - mx, 4);
            tax = 10 - sci;
        }
        if (tax > mx) {
            tax = mx;
            lux = 10 - sci - tax;
        }
        if (c.sci != sci || c.tax != tax || c.lux != lux) set_rates(ci, tax, lux, sci);
    }
    for (City &ct : cities) {
        if (ct.owner == ci && (!can_build(ct, ct.build_unit, ct.build) || ct.founded == turn || ct.shields == 0)) ai_city(ct);
    }
    // Rush a defender into an empty city on the front.
    for (City &ct : cities) {
        if (ct.owner != ci || !c.war) continue;
        bool guarded = false;
        for (const Unit &u : units) guarded |= u.owner == ci && u.x == ct.x && u.y == ct.y && is_military(u.type);
        if (!guarded && ct.build_unit && unit_info(ct.build).defense > 0 && buy_cost(ct) <= c.gold / 2) buy(ct.id);
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
    std::vector<int> ids;
    for (const Unit &u : units) {
        if (u.owner == ci) ids.push_back(u.id);
    }
    for (int id : ids) {
        if (Unit *u = unit(id)) ai_unit(*u);
    }
}

} // namespace civ
