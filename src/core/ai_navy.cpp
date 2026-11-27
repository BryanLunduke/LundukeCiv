// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// The computer players at sea and in the air, kept to simple rules like the
// rest of the AI:
//  - Settlers with no room left at home, and attackers with no enemy city
//    on their landmass while at war overseas, walk to a port and wait there
//    (sentried, kPlanPassenger) for a ship.
//  - A ship that can carry units waits in port until enough passengers are
//    aboard (one Settlers, or three troops, or a few turns), sails to a
//    coast next to a free site or an enemy city, and puts them ashore. With
//    nobody to carry it fetches waiting passengers, or explores.
//  - Warships attack what they can beat at sea and on the coast, hunt the
//    nearest enemy ship or coastal city they know of, and rest in port.
//  - Fighters and Bombers strike what they can reach and still get home,
//    and are always back in a city (or on a Carrier) at the end of the turn.

#include "game.hpp"

#include <algorithm>
#include <cstdlib>
#include <queue>

namespace civ {

namespace {

bool ferry_type(UnitType t)
{
    const UnitInfo &i = unit_info(t);
    return i.domain == Domain::Sea && i.capacity > 0 && !(i.flags & kCarriesAir);
}

bool warship_type(UnitType t)
{
    const UnitInfo &i = unit_info(t);
    // Frigates fight as well as carry.
    return i.domain == Domain::Sea && !(i.flags & kCarriesAir) && (i.capacity == 0 ? i.attack > 0 : i.attack >= 2);
}

} // namespace

int Game::ai_passengers(int ci, int x, int y) const
{
    int n = 0;
    for (const Unit &u : units) {
        if (u.owner != ci || u.plan != kPlanPassenger || u.carried_by >= 0) continue;
        if (x >= 0 && (u.x != x || u.y != y)) continue;
        ++n;
    }
    return n;
}

bool Game::ai_has_ferry(int ci) const
{
    for (const Unit &u : units) {
        if (u.owner == ci && ferry_type(u.type)) return true;
    }
    for (int t = 0; t < kUnitTypeCount; ++t) {
        if (ferry_type(static_cast<UnitType>(t)) && unit_available(ci, static_cast<UnitType>(t))) return true;
    }
    return false;
}

// A good free site on another landmass that we know of, nearest first.
bool Game::ai_overseas_site(int ci, int fx, int fy, int &sx, int &sy) const
{
    const int home = map.at(fx, fy).continent;
    int best = 1 << 30;
    sx = sy = -1;
    for (int y = 2; y < map.h - 2; ++y) {
        for (int x = 0; x < map.w; ++x) {
            const Tile &t = map.at(x, y);
            if (!map.land(x, y) || t.continent == home || !((t.explored >> ci) & 1u) || t.terrain == Terrain::Arctic) continue;
            if (!map.coastal(x, y) && map.distance(x, y, fx, fy) > 0) {
                // Inland is fine too, but a coastal site is where we land.
            }
            bool clear = true;
            for (const City &c : cities) {
                if (map.distance(c.x, c.y, x, y) < 4) {
                    clear = false;
                    break;
                }
            }
            if (!clear) continue;
            const int o = owner_at(x, y);
            if (o >= 0 && o != ci) continue;
            const Yield yv = tile_yield(x, y, ci, true);
            const int score = map.distance(fx, fy, x, y) * 4 - (yv.food * 3 + yv.shields * 2 + yv.trade) * 2 - (map.coastal(x, y) ? 6 : 0);
            if (score < best) {
                best = score;
                sx = x;
                sy = y;
            }
        }
    }
    return sx >= 0;
}

// "Free" means within Settlers' reach (nine squares) of one of our cities.
bool Game::ai_boxed_in(int ci) const
{
    std::vector<char> mine;
    for (const City &c : cities) {
        if (c.owner != ci) continue;
        const int k = map.at(c.x, c.y).continent;
        if (k >= static_cast<int>(mine.size())) mine.resize(static_cast<size_t>(k) + 1, 0);
        if (k >= 0) mine[static_cast<size_t>(k)] = 1;
    }
    if (mine.empty()) return false;
    for (int y = 2; y < map.h - 2; ++y) {
        for (int x = 0; x < map.w; ++x) {
            if (!map.land(x, y) || map.at(x, y).terrain == Terrain::Arctic) continue;
            const int k = map.at(x, y).continent;
            if (k < 0 || k >= static_cast<int>(mine.size()) || !mine[static_cast<size_t>(k)]) continue;
            bool clear = true, reach = false;
            for (const City &c : cities) {
                const int d = map.distance(c.x, c.y, x, y);
                if (d < 4) {
                    clear = false;
                    break;
                }
                reach |= c.owner == ci && d <= 9;
            }
            if (clear && reach) return false;
        }
    }
    return true;
}

// The nearest city we are at war with (raiders aside) that we know of on
// another landmass.
const City *Game::ai_overseas_enemy(int ci, int fx, int fy) const
{
    const int home = map.at(fx, fy).continent;
    const City *best = nullptr;
    int bd = 1 << 30;
    for (const City &c : cities) {
        if (c.owner == ci || !at_war(ci, c.owner) || civs[c.owner].barbarian) continue;
        if (map.at(c.x, c.y).continent == home || !((map.at(c.x, c.y).explored >> ci) & 1u)) continue;
        const int d = map.distance(fx, fy, c.x, c.y);
        if (d < bd) {
            bd = d;
            best = &c;
        }
    }
    return best;
}

// An ocean square on the coast nearest the land square tx, ty (up to four
// squares away, on the same landmass), nearest the ship and reachable by
// it. False if none.
bool Game::ai_landing(const Unit &ship, int tx, int ty, int &ox, int &oy) const
{
    struct Cand {
        int d, x, y;
    };
    std::vector<Cand> cands;
    // The nearest ring of coast around the target that has any.
    const int cont = map.land(tx, ty) ? map.at(tx, ty).continent : -1;
    for (int r = 0; r <= 4 && cands.empty(); ++r) {
        for (int j = -r; j <= r; ++j) {
            for (int i = -r; i <= r; ++i) {
                if (std::max(std::abs(i), std::abs(j)) != r) continue;
                const int lx = map.wrap_x(tx + i), ly = ty + j;
                if (!map.valid_y(ly) || !map.land(lx, ly) || map.at(lx, ly).continent != cont) continue;
                const City *lc = city_at(lx, ly);
                if (lc && lc->owner != ship.owner && (i || j)) continue;
                const int lo = owner_at(lx, ly);
                if (lo >= 0 && lo != ship.owner && (i || j)) continue;
                for (int b = -1; b <= 1; ++b) {
                    for (int a = -1; a <= 1; ++a) {
                        const int sx = map.wrap_x(lx + a), sy = ly + b;
                        if (!map.ocean(sx, sy)) continue;
                        const int so = owner_at(sx, sy);
                        if (so >= 0 && so != ship.owner) continue;
                        if ((unit_info(ship.type).flags & kCoastal) && !map.next_to_land(sx, sy)) continue;
                        cands.push_back({map.distance(ship.x, ship.y, sx, sy), sx, sy});
                    }
                }
            }
        }
    }
    std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) { return a.d < b.d; });
    int tries = 0;
    for (const Cand &c : cands) {
        if (c.d == 0) {
            ox = c.x;
            oy = c.y;
            return true;
        }
        int dx = 0, dy = 0;
        if (next_step(ship, c.x, c.y, dx, dy, 4000)) {
            ox = c.x;
            oy = c.y;
            return true;
        }
        if (++tries >= 4) break;
    }
    return false;
}

// Walk to the nearest port of ours on this landmass and wait there for a
// ship. False if there is no such port.
bool Game::ai_seek_ship(int unit_id)
{
    Unit *u = unit(unit_id);
    if (!u) return false;
    const int ci = u->owner;
    const int cont = map.at(u->x, u->y).continent;
    const City *port = nullptr;
    int bd = 1 << 30;
    for (const City &c : cities) {
        if (c.owner != ci || map.at(c.x, c.y).continent != cont || !map.coastal(c.x, c.y)) continue;
        const int d = map.distance(u->x, u->y, c.x, c.y) * 4 - ai_passengers(ci, c.x, c.y);
        if (d < bd) {
            bd = d;
            port = &c;
        }
    }
    if (!port) return false;
    const int px = port->x, py = port->y;
    u->plan = kPlanPassenger;
    for (int guard = 0; guard < 12; ++guard) {
        u = unit(unit_id);
        if (!u) return true;
        if (u->x == px && u->y == py) {
            u->order = Order::Sentry;
            u->work = 0;
            return true;
        }
        if (u->moves <= 0) return true;
        int dx = 0, dy = 0;
        if (!next_step(*u, px, py, dx, dy, 3000)) {
            u->plan = kPlanNone;
            return false;
        }
        const MoveResult r = move(unit_id, dx, dy);
        if (r != MoveResult::Moved) return true;
    }
    return true;
}

// A unit the ship has woken next to the shore steps onto the land: into an
// enemy city it can take, at an enemy it can beat, or onto the free square
// nearest where it is going.
void Game::ai_disembark(int unit_id)
{
    Unit *u = unit(unit_id);
    if (!u || u->carried_by < 0 || u->moves <= 0) return;
    const int ci = u->owner;
    const bool settler = unit_info(u->type).flags & kSettlers;
    // Where to: an enemy city for troops, a free site for Settlers.
    int gx = -1, gy = -1;
    if (settler) {
        int best = -(1 << 30);
        for (int j = -6; j <= 6; ++j) {
            for (int i = -6; i <= 6; ++i) {
                const int x = map.wrap_x(u->x + i), y = u->y + j;
                if (!map.land(x, y) || y < 2 || y >= map.h - 2) continue;
                bool clear = true;
                for (const City &c : cities) clear = clear && map.distance(c.x, c.y, x, y) >= 4;
                if (!clear) continue;
                const Yield yv = tile_yield(x, y, ci, true);
                const int s = yv.food * 3 + yv.shields * 2 + yv.trade - map.distance(u->x, u->y, x, y) * 3;
                if (s > best) {
                    best = s;
                    gx = x;
                    gy = y;
                }
            }
        }
    } else {
        int bd = 1 << 30;
        for (const City &c : cities) {
            if (c.owner == ci || !at_war(ci, c.owner)) continue;
            const int d = map.distance(u->x, u->y, c.x, c.y);
            if (d < bd) {
                bd = d;
                gx = c.x;
                gy = c.y;
            }
        }
    }
    int bx = 0, by = 0, best = 1 << 30;
    bool attack_it = false;
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            if (!i && !j) continue;
            const int x = map.wrap_x(u->x + i), y = u->y + j;
            if (!map.land(x, y)) continue;
            const int o = owner_at(x, y);
            if (o >= 0 && o != ci) {
                if (settler || !at_war(ci, o) || unit_info(u->type).attack <= 0) continue;
                // An empty enemy city is taken; defended squares are fought
                // only with good odds.
                double worst = 1.0;
                for (int did : units_at(x, y)) {
                    const Unit *d = unit(did);
                    worst = std::min(worst, combat_win_chance(attack_strength(*u), defense_strength(*d, u), u->hp, d->hp));
                }
                if (worst < 0.55) continue;
                bx = i;
                by = j;
                attack_it = true;
                best = -1;
                continue;
            }
            if (attack_it) continue;
            const int d = gx >= 0 ? map.distance(x, y, gx, gy) : 0;
            if (d < best) {
                best = d;
                bx = i;
                by = j;
            }
        }
    }
    if (best == (1 << 30)) return;   // nowhere to land: stay aboard
    u->order = Order::None;
    u->plan = kPlanNone;
    move(unit_id, bx, by);
}

void Game::ai_ship(int unit_id)
{
    Unit *u = unit(unit_id);
    if (!u) return;
    const int ci = u->owner;
    const UnitInfo &info = unit_info(u->type);
    if (const City *port = city_at(u->x, u->y); port && port->owner == ci && u->plan != kPlanFerry) {
        // In port and not on a voyage: anyone still aboard goes ashore, and
        // units resting here that are not waiting for a ship are woken so
        // they don't board by mistake when we sail.
        for (Unit &o : units) {
            if (o.owner != ci || o.x != u->x || o.y != u->y || unit_info(o.type).domain != Domain::Land) continue;
            if (o.carried_by == unit_id) {
                o.carried_by = -1;
                o.plan = kPlanNone;
                o.order = Order::None;
            } else if (o.carried_by < 0 && o.order == Order::Sentry && o.plan != kPlanPassenger) {
                o.order = Order::None;
            }
        }
    }
    if (ferry_type(u->type)) {
        ai_ferry(unit_id);
        return;
    }
    if (!warship_type(u->type)) {
        // Carriers stay in port as floating airfields.
        if (!city_at(u->x, u->y)) ai_sail_home(unit_id);
        else u->order = Order::Sentry;
        return;
    }
    // Badly hurt: home to mend.
    if (u->hp <= kUnitHp / 2) {
        if (!city_at(u->x, u->y)) ai_sail_home(unit_id);
        return;
    }
    bool war = false;
    for (int k = 0; k < static_cast<int>(civs.size()); ++k) war |= k != ci && !civs[k].barbarian && civs[k].alive && at_war(ci, k);
    for (int guard = 0; guard < 12; ++guard) {
        u = unit(unit_id);
        if (!u || u->moves <= 0) return;
        // Strike what we can beat next to us: ships at sea, and troops on
        // the coast for ships that may bombard.
        int ax = 0, ay = 0;
        double best_odds = 0;
        for (int j = -1; j <= 1; ++j) {
            for (int i = -1; i <= 1; ++i) {
                if (!i && !j) continue;
                const int x = map.wrap_x(u->x + i), y = u->y + j;
                if (!map.valid_y(y)) continue;
                const int o = owner_at(x, y);
                if (o < 0 || o == ci || !at_war(ci, o) || units_at(x, y).empty()) continue;
                if (map.land(x, y) && (info.flags & kNoLandAttack)) continue;
                double worst = 1.0;
                bool air_only = !city_at(x, y);
                for (int did : units_at(x, y)) {
                    const Unit *d = unit(did);
                    air_only = air_only && unit_info(d->type).domain == Domain::Air;
                    worst = std::min(worst, combat_win_chance(attack_strength(*u), defense_strength(*d, u), u->hp, d->hp));
                }
                if (air_only) continue;
                if (worst > best_odds) {
                    best_odds = worst;
                    ax = i;
                    ay = j;
                }
            }
        }
        if (best_odds >= 0.6) {
            move(unit_id, ax, ay);
            continue;
        }
        if (!war) break;
        // Hunt: the nearest enemy ship we can see, else an enemy port.
        int tx = -1, ty = -1, td = 16;
        for (const Unit &e : units) {
            if (e.owner == ci || !at_war(ci, e.owner) || civs[e.owner].barbarian || !map.ocean(e.x, e.y)) continue;
            if (!((map.at(e.x, e.y).explored >> ci) & 1u)) continue;
            const int d = map.distance(u->x, u->y, e.x, e.y);
            if (d < td) {
                td = d;
                tx = e.x;
                ty = e.y;
            }
        }
        if (tx < 0 && !(info.flags & kNoLandAttack)) {
            for (const City &c : cities) {
                if (c.owner == ci || !at_war(ci, c.owner) || civs[c.owner].barbarian || !map.coastal(c.x, c.y)) continue;
                if (!((map.at(c.x, c.y).explored >> ci) & 1u)) continue;
                const int d = map.distance(u->x, u->y, c.x, c.y);
                if (d < td + 8) {
                    td = d - 8;
                    tx = c.x;
                    ty = c.y;
                }
            }
        }
        if (tx < 0) break;
        if (map.distance(u->x, u->y, tx, ty) <= 1) break;   // next to it but no good odds: wait
        int ox = -1, oy = -1;
        if (map.ocean(tx, ty)) {
            ox = tx;
            oy = ty;
        } else if (!ai_landing(*u, tx, ty, ox, oy)) {
            break;
        }
        int dx = 0, dy = 0;
        if (!next_step(*u, ox, oy, dx, dy, 4000)) break;
        const int nx = map.wrap_x(u->x + dx), ny = u->y + dy;
        const int o = owner_at(nx, ny);
        if (o >= 0 && o != ci) break;   // the way is held: the strike check above decides next turn
        if (move(unit_id, dx, dy) != MoveResult::Moved) break;
    }
    u = unit(unit_id);
    if (u && u->moves > 0 && !war && !city_at(u->x, u->y)) ai_sail_home(unit_id);
}

void Game::ai_sail_home(int unit_id)
{
    Unit *u = unit(unit_id);
    if (!u) return;
    const int ci = u->owner;
    const City *port = nullptr;
    int bd = 1 << 30;
    for (const City &c : cities) {
        if (c.owner != ci || !map.coastal(c.x, c.y)) continue;
        const int d = map.distance(u->x, u->y, c.x, c.y) - ai_passengers(ci, c.x, c.y) * 6;
        if (d < bd) {
            bd = d;
            port = &c;
        }
    }
    if (!port) return;
    const int px = port->x, py = port->y;
    for (int guard = 0; guard < 12; ++guard) {
        u = unit(unit_id);
        if (!u || u->moves <= 0) return;
        if (u->x == px && u->y == py) {
            u->order = Order::Sentry;
            return;
        }
        int dx = 0, dy = 0;
        if (!next_step(*u, px, py, dx, dy, 4000)) return;
        if (move(unit_id, dx, dy) != MoveResult::Moved) return;
    }
}

void Game::ai_ferry(int unit_id)
{
    Unit *u = unit(unit_id);
    if (!u) return;
    const int ci = u->owner;
    const UnitInfo &info = unit_info(u->type);
    if (cargo_count(unit_id) == 0 && u->plan == kPlanFerry) u->plan = kPlanNone;
    const City *here = city_at(u->x, u->y);
    if (u->plan != kPlanFerry && here && here->owner == ci) {
        const int waiting = ai_passengers(ci, u->x, u->y);
        if (waiting == 0) {
            // Fetch passengers waiting elsewhere, or look around.
            if (ai_passengers(ci) > 0) {
                const int hx = u->x, hy = u->y;
                ai_sail_home(unit_id);
                u = unit(unit_id);
                if (u && u->x == hx && u->y == hy) u->order = Order::Sentry;
                return;
            }
            ai_explore_sea(unit_id);
            return;
        }
        bool settlers = false;
        for (const Unit &p : units) {
            if (p.owner == ci && p.x == u->x && p.y == u->y && p.plan == kPlanPassenger && (unit_info(p.type).flags & kSettlers)) settlers = true;
        }
        const int need = settlers ? 1 : std::min(3, info.capacity);
        if (waiting < need && ++u->work < 4) {
            u->order = Order::Sentry;
            return;
        }
        u->work = 0;
        int tx = -1, ty = -1;
        if (settlers) {
            if (!ai_overseas_site(ci, u->x, u->y, tx, ty)) tx = -1;
        } else if (const City *e = ai_overseas_enemy(ci, u->x, u->y)) {
            tx = e->x;
            ty = e->y;
        }
        int ox = -1, oy = -1;
        if (tx < 0 || !ai_landing(*u, tx, ty, ox, oy)) {
            // Nowhere to go after all: the passengers go about their business.
            for (Unit &p : units) {
                if (p.owner == ci && p.x == u->x && p.y == u->y && p.plan == kPlanPassenger) {
                    p.plan = kPlanNone;
                    p.order = Order::None;
                }
            }
            u->order = Order::Sentry;
            return;
        }
        // Make sure the passengers are the ones who board.
        for (Unit &p : units) {
            if (p.owner == ci && p.x == u->x && p.y == u->y && p.carried_by < 0 && unit_info(p.type).domain == Domain::Land &&
                p.order == Order::Sentry && p.plan != kPlanPassenger)
                p.order = Order::Fortified;
        }
        u->plan = kPlanFerry;
        u->goto_x = ox;
        u->goto_y = oy;
        u->order = Order::None;
    }
    if (u->plan != kPlanFerry) {
        if (here && here->owner == ci) {
            u->order = Order::Sentry;
        } else if (cargo_count(unit_id) == 0) {
            // Out exploring: carry on unless passengers wait for us.
            if (ai_passengers(ci) > 0) ai_sail_home(unit_id);
            else ai_explore_sea(unit_id);
        }
        return;
    }
    // Under way.
    for (int guard = 0; guard < 12; ++guard) {
        u = unit(unit_id);
        if (!u) return;
        const bool there = map.distance(u->x, u->y, u->goto_x, u->goto_y) == 0 ||
                           (map.distance(u->x, u->y, u->goto_x, u->goto_y) <= 1 && map.next_to_land(u->x, u->y) && map.ocean(u->x, u->y));
        if (there && map.ocean(u->x, u->y)) {
            std::vector<int> cargo;
            for (Unit &c : units) {
                if (c.carried_by == unit_id) {
                    c.order = Order::None;
                    c.plan = kPlanNone;
                    c.done = false;
                    cargo.push_back(c.id);
                }
            }
            for (int cid : cargo) ai_disembark(cid);
            u = unit(unit_id);
            if (u && cargo_count(unit_id) == 0) {
                u->plan = kPlanNone;
                u->order = Order::None;
            }
            return;
        }
        if (u->moves <= 0) return;
        int dx = 0, dy = 0;
        if (!next_step(*u, u->goto_x, u->goto_y, dx, dy, 5000)) {
            u->plan = kPlanNone;
            return;
        }
        const int nx = map.wrap_x(u->x + dx), ny = u->y + dy;
        const int o = owner_at(nx, ny);
        if (o >= 0 && o != ci) return;   // someone in the way: wait a turn
        if (move(unit_id, dx, dy) != MoveResult::Moved) return;
    }
}

void Game::ai_explore_sea(int unit_id)
{
    Unit *u = unit(unit_id);
    if (!u) return;
    const int ci = u->owner;
    const bool coastal_only = unit_info(u->type).flags & kCoastal;
    for (int guard = 0; guard < 12; ++guard) {
        u = unit(unit_id);
        if (!u || u->moves <= 0) return;
        // Breadth first over the sea to the nearest square next to the unknown.
        const int W = map.w;
        std::vector<char> seen(static_cast<size_t>(W) * map.h, 0);
        std::queue<std::pair<int, int>> q;
        q.push({u->x, u->y});
        seen[static_cast<size_t>(u->y) * W + u->x] = 1;
        int fx = -1, fy = -1, steps = 0;
        while (!q.empty() && steps < 2500) {
            auto [x, y] = q.front();
            q.pop();
            ++steps;
            bool frontier = false;
            for (int j = -1; j <= 1 && !frontier; ++j)
                for (int i = -1; i <= 1; ++i)
                    if (map.valid_y(y + j) && !((map.at(x + i, y + j).explored >> ci) & 1u)) frontier = true;
            if (frontier && !(x == u->x && y == u->y)) {
                fx = x;
                fy = y;
                break;
            }
            for (int j = -1; j <= 1; ++j) {
                for (int i = -1; i <= 1; ++i) {
                    const int nx = map.wrap_x(x + i), ny = y + j;
                    if (!map.ocean(nx, ny) || seen[static_cast<size_t>(ny) * W + nx]) continue;
                    if (coastal_only && !map.next_to_land(nx, ny)) continue;
                    const int o = owner_at(nx, ny);
                    if (o >= 0 && o != ci) continue;
                    seen[static_cast<size_t>(ny) * W + nx] = 1;
                    q.push({nx, ny});
                }
            }
        }
        if (fx < 0) {
            ai_sail_home(unit_id);
            return;
        }
        int dx = 0, dy = 0;
        if (!next_step(*u, fx, fy, dx, dy, 4000)) return;
        if (move(unit_id, dx, dy) != MoveResult::Moved) return;
    }
}

void Game::ai_air(int unit_id)
{
    Unit *u = unit(unit_id);
    if (!u) return;
    const int ci = u->owner;
    const UnitInfo &info = unit_info(u->type);
    if (info.flags & kNuclear) {
        u->order = Order::Sentry;   // the computer rulers never start a nuclear war
        return;
    }
    auto base_at = [&](int x, int y) {
        const City *c = city_at(x, y);
        if (c && c->owner == ci) return true;
        for (const Unit &s : units)
            if (s.owner == ci && s.x == x && s.y == y && (unit_info(s.type).flags & kCarriesAir)) return true;
        return false;
    };
    auto nearest_base = [&](int x, int y, int &bx, int &by) {
        int bd = 1 << 30;
        for (const City &c : cities) {
            if (c.owner != ci) continue;
            const int d = map.distance(x, y, c.x, c.y);
            if (d < bd) {
                bd = d;
                bx = c.x;
                by = c.y;
            }
        }
        return bd;
    };
    // Strike: something within half our range (there and back), that we
    // can beat. Only Fighters may attack aircraft away from a city.
    const int reach = info.moves / 2;
    if (base_at(u->x, u->y) && u->moves >= info.moves * 3) {
        int tx = -1, ty = -1;
        double best = 0.6;
        for (const Unit &e : units) {
            if (e.owner == ci || !at_war(ci, e.owner)) continue;
            if (map.distance(u->x, u->y, e.x, e.y) > reach || !((map.at(e.x, e.y).explored >> ci) & 1u)) continue;
            const bool in_city = city_at(e.x, e.y) != nullptr;
            bool air_only = !in_city;
            double worst = 1.0;
            for (int did : units_at(e.x, e.y)) {
                const Unit *d = unit(did);
                air_only = air_only && unit_info(d->type).domain == Domain::Air;
                worst = std::min(worst, combat_win_chance(attack_strength(*u), defense_strength(*d, u), u->hp, d->hp));
            }
            if (air_only && u->type != UnitType::Fighter) continue;
            // Fighters go for aircraft first.
            const double want = worst + (air_only ? 0.2 : 0.0);
            if (want > best) {
                best = want;
                tx = e.x;
                ty = e.y;
            }
        }
        if (tx >= 0) {
            for (int guard = 0; guard < 16; ++guard) {
                u = unit(unit_id);
                if (!u || u->moves <= 0) break;
                if (map.distance(u->x, u->y, tx, ty) <= 1) {
                    move(unit_id, map.dx(u->x, tx), ty - u->y);
                    break;
                }
                int dx = 0, dy = 0;
                if (!next_step(*u, tx, ty, dx, dy, 3000)) break;
                if (move(unit_id, dx, dy) != MoveResult::Moved) break;
            }
        }
    }
    // Home before the fuel runs out.
    u = unit(unit_id);
    if (!u) return;
    if (base_at(u->x, u->y)) {
        u->order = Order::Sentry;
        return;
    }
    int bx = -1, by = -1;
    nearest_base(u->x, u->y, bx, by);
    if (bx < 0) return;
    for (int guard = 0; guard < 16; ++guard) {
        u = unit(unit_id);
        if (!u || u->moves <= 0) return;
        if (u->x == bx && u->y == by) {
            u->order = Order::Sentry;
            return;
        }
        int dx = 0, dy = 0;
        if (!next_step(*u, bx, by, dx, dy, 3000)) return;
        if (move(unit_id, dx, dy) != MoveResult::Moved) return;
    }
}

int Game::ai_navy_build(const City &c) const
{
    // Ships and aircraft a computer city should build now, -1 none.
    const int ci = c.owner;
    int own_cities = 0;
    for (const City &o : cities) own_cities += o.owner == ci;
    int ferries = 0, room = 0, warships = 0, air = 0;
    for (const Unit &u : units) {
        if (u.owner != ci) continue;
        if (ferry_type(u.type)) {
            ++ferries;
            room += unit_info(u.type).capacity;
        }
        warships += warship_type(u.type);
        air += unit_info(u.type).domain == Domain::Air && !(unit_info(u.type).flags & kNuclear);
    }
    const bool port = map.coastal(c.x, c.y);
    auto best = [&](bool (*kind)(UnitType), bool by_room) {
        int pick = -1, score = -1;
        for (int t = 0; t < kUnitTypeCount; ++t) {
            if (!kind(static_cast<UnitType>(t)) || !can_build(c, true, t)) continue;
            const UnitInfo &i = unit_info(t);
            const int s = by_room ? i.capacity * 10 + i.moves * 3 + i.defense : i.attack * 4 + i.defense * 2 + i.moves;
            if (s > score) {
                score = s;
                pick = t;
            }
        }
        return pick;
    };
    const int waiting = ai_passengers(ci);
    // A ship for the passengers waiting in our ports.
    if (port && waiting > 0 && (ferries == 0 || (waiting > room && ferries < 1 + own_cities / 6))) {
        bool on_order = false;
        for (const City &o : cities) on_order |= o.owner == ci && o.id != c.id && o.build_unit && ferry_type(static_cast<UnitType>(o.build));
        if (!on_order) {
            const int t = best(ferry_type, true);
            if (t >= 0) return t;
        }
    }
    bool war = false;
    for (int k = 0; k < static_cast<int>(civs.size()); ++k) war |= k != ci && !civs[k].barbarian && civs[k].alive && at_war(ci, k);
    if (!war) {
        // One ship to look around once the homeland fills up.
        if (port && ferries == 0 && c.size >= 3 && ai_boxed_in(ci)) {
            const int t = best(ferry_type, true);
            if (t >= 0) return t;
        }
        return -1;
    }
    if (port && warships < (own_cities + 2) / 3 && c.size >= 4) {
        const int t = best(warship_type, false);
        if (t >= 0) return t;
    }
    if (air < (own_cities + 3) / 4 && c.size >= 5) {
        int pick = -1, score = -1;
        for (int t = 0; t < kUnitTypeCount; ++t) {
            const UnitInfo &i = unit_info(t);
            if (i.domain != Domain::Air || (i.flags & kNuclear) || !can_build(c, true, t)) continue;
            const int s = i.attack * 3 + i.moves;
            if (s > score) {
                score = s;
                pick = t;
            }
        }
        if (pick >= 0) return pick;
    }
    return -1;
}

} // namespace civ
