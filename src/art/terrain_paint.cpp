// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Terrain painter. Each pixel is picked from a small per-terrain ramp, so
// the whole map stays inside a limited palette. Coasts are found per pixel
// from a smoothed land field, then given beach, foam and shallows by
// distance. Features (trees, hills, peaks, reeds) are stamped tile by tile
// from top to bottom so lower tiles overlap the ones above, as on a board.

#include "terrain_paint.hpp"

#include <algorithm>
#include <cmath>
#include <deque>

namespace art {

namespace {

constexpr Argb kOutline = rgb(0x1A1620);

struct Ramp {
    Argb dark, mid, light, accent;
};

Ramp ground_ramp(Terrain t)
{
    switch (t) {
    case Terrain::Grassland: return {rgb(0x3C7E2A), rgb(0x4E9A34), rgb(0x66B044), rgb(0x2F6A22)};
    case Terrain::Plains: return {rgb(0x828A38), rgb(0x9CA446), rgb(0xBAB85C), rgb(0x6E7430)};
    case Terrain::Desert: return {rgb(0xC09E5C), rgb(0xDCC07A), rgb(0xEED79A), rgb(0xA8864A)};
    case Terrain::Tundra: return {rgb(0x6E6E58), rgb(0x8A8A6C), rgb(0xA6A88A), rgb(0x6C7C52)};
    case Terrain::Arctic: return {rgb(0xA8BCD4), rgb(0xDCE6F0), rgb(0xF8FCFF), rgb(0x8CA4C4)};
    case Terrain::Forest: return {rgb(0x2E6624), rgb(0x3C7A2C), rgb(0x4E8E36), rgb(0x24521C)};
    case Terrain::Jungle: return {rgb(0x24601E), rgb(0x2E7826), rgb(0x46982E), rgb(0x1C4C18)};
    case Terrain::Swamp: return {rgb(0x4A5C36), rgb(0x5C7044), rgb(0x728652), rgb(0x3C4C2E)};
    case Terrain::Hills: return {rgb(0x6E7E38), rgb(0x8A9C48), rgb(0xA6B65E), rgb(0x56642C)};
    case Terrain::Mountains: return {rgb(0x625C4C), rgb(0x7A7460), rgb(0x948E78), rgb(0x4E4A3E)};
    default: return {rgb(0x1E4A8C), rgb(0x2A5CA0), rgb(0x4A7EC0), rgb(0x163A70)};
    }
}

Argb ground_px(Terrain t, int gx, int gy)
{
    const Ramp r = ground_ramp(t);
    const double n = hashf(gx, gy, 101);
    const double m = hashf(gx / 3, gy / 3, 102);
    switch (t) {
    case Terrain::Grassland: {
        // Short "v" tufts.
        const int cx = gx % 8, cy = gy % 7;
        const bool tuft = hashf(gx / 8, gy / 7, 103) < 0.55 && ((cy == 3 && (cx == 2 || cx == 4)) || (cy == 4 && cx == 3));
        if (tuft) return r.accent;
        if (n < 0.10) return r.dark;
        if (n > 0.93 || (m > 0.8 && n > 0.6)) return r.light;
        return r.mid;
    }
    case Terrain::Plains: {
        const int cx = gx % 6, cy = gy % 6;
        const bool blade = hashf(gx / 6, gy / 6, 104) < 0.6 && cx == 2 && (cy == 2 || cy == 3);
        if (blade) return r.accent;
        if (blade == false && cx == 3 && cy == 1 && hashf(gx / 6, gy / 6, 104) < 0.6) return r.light;
        if (n < 0.08) return r.dark;
        if (n > 0.92) return r.light;
        return r.mid;
    }
    case Terrain::Desert: {
        // Wind ripples: shallow arcs.
        const double wave = std::sin(gx * 0.35 + std::sin(gy * 0.21) * 2.0) * 2.0;
        const int band = static_cast<int>(std::floor((gy + wave) / 6.0));
        const double inband = (gy + wave) - band * 6.0;
        if (inband < 1.0 && hashf(gx / 5, band, 105) < 0.75) return r.dark;
        if (inband >= 1.0 && inband < 2.0 && hashf(gx / 5, band, 105) < 0.75) return r.light;
        if (n < 0.05) return r.accent;
        return r.mid;
    }
    case Terrain::Tundra: {
        if (hashf(gx / 4, gy / 3, 106) < 0.28 && n < 0.6) return r.accent; // moss
        if (n > 0.97) return rgb(0xE8EEF0);                                // frost
        if (n < 0.12) return r.dark;
        if (n > 0.88) return r.light;
        return r.mid;
    }
    case Terrain::Arctic: {
        const double drift = std::sin(gx * 0.25 + gy * 0.12) + std::sin(gy * 0.4 - gx * 0.07);
        if (drift > 1.55) return r.dark;
        if (drift > 1.15) return r.accent;
        if (n > 0.9) return r.light;
        return r.mid;
    }
    case Terrain::Swamp:
    case Terrain::Jungle:
    case Terrain::Forest:
    case Terrain::Hills:
    case Terrain::Mountains:
        if (n < 0.14) return r.dark;
        if (n > 0.9) return r.light;
        if (m < 0.2) return r.accent;
        return r.mid;
    default: return r.mid;
    }
}

// Everything is painted into a canvas with a one-tile margin so coasts and
// blends at the view edge see their neighbours.
struct Canvas {
    const PaintWorld &w;
    int tx0, ty0, cols, rows; // tiles covered, margin included
    Pixmap pm;
    std::vector<unsigned char> landmask;
    std::vector<int> dist; // pixels to the other side of the coast

    Canvas(const PaintWorld &world, int x0, int y0, int c, int r)
        : w(world), tx0(x0 - 1), ty0(y0 - 1), cols(c + 2), rows(r + 2), pm((c + 2) * kTile, (r + 2) * kTile)
    {
    }
    int gx(int x) const { return (tx0 * kTile) + x; }
    int gy(int y) const { return (ty0 * kTile) + y; }
    const PaintTile &tile_of_px(int x, int y) const { return w.at(tx0 + x / kTile, ty0 + y / kTile); }
};

double land_value(const PaintWorld &w, int tx, int ty)
{
    return w.land(tx, ty) ? 1.0 : 0.0;
}

void build_land(Canvas &cv)
{
    const int W = cv.pm.w, H = cv.pm.h;
    cv.landmask.assign(static_cast<size_t>(W) * H, 0);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const double u = (cv.gx(x) - kTile / 2 + 0.5) / kTile;
            const double v = (cv.gy(y) - kTile / 2 + 0.5) / kTile;
            const int i = static_cast<int>(std::floor(u)), j = static_cast<int>(std::floor(v));
            const double fu = u - i, fv = v - j;
            const double a = land_value(cv.w, i, j), b = land_value(cv.w, i + 1, j);
            const double c = land_value(cv.w, i, j + 1), d = land_value(cv.w, i + 1, j + 1);
            // Smoothstep makes the bilinear field read as rounded coasts.
            const double su = fu * fu * (3 - 2 * fu), sv = fv * fv * (3 - 2 * fv);
            double val = (a + (b - a) * su) * (1 - sv) + (c + (d - c) * su) * sv;
            val += (hashf(cv.gx(x) / 2, cv.gy(y) / 2, 201) - 0.5) * 0.16;
            // The middle of a land tile is always land; of a sea tile, sea.
            const PaintTile &own = cv.tile_of_px(x, y);
            const int lx = cv.gx(x) % kTile, ly = cv.gy(y) % kTile;
            const bool core = lx >= 9 && lx <= 22 && ly >= 9 && ly <= 22;
            bool land = val > 0.5;
            if (core) {
                land = own.terrain != Terrain::Ocean;
            }
            cv.landmask[static_cast<size_t>(y) * W + x] = land ? 1 : 0;
        }
    }
    // Chebyshev distance to the opposite kind, capped at 12.
    cv.dist.assign(static_cast<size_t>(W) * H, 99);
    std::deque<int> q;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const unsigned char me = cv.landmask[static_cast<size_t>(y) * W + x];
            bool edge = false;
            for (int dy = -1; dy <= 1 && !edge; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if ((dx || dy) && nx >= 0 && ny >= 0 && nx < W && ny < H && cv.landmask[static_cast<size_t>(ny) * W + nx] != me) {
                        edge = true;
                        break;
                    }
                }
            }
            if (edge) {
                cv.dist[static_cast<size_t>(y) * W + x] = 1;
                q.push_back(y * W + x);
            }
        }
    }
    while (!q.empty()) {
        const int p = q.front();
        q.pop_front();
        const int x = p % W, y = p / W;
        const int d = cv.dist[p];
        if (d >= 12) {
            continue;
        }
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) {
                    continue;
                }
                const int np = ny * W + nx;
                if (cv.landmask[np] == cv.landmask[p] && cv.dist[np] > d + 1) {
                    cv.dist[np] = d + 1;
                    q.push_back(np);
                }
            }
        }
    }
}

// For a land pixel over a sea tile (rounded coast), borrow the terrain of
// the nearest land tile.
Terrain land_terrain_at(const Canvas &cv, int x, int y)
{
    const int tx = cv.tx0 + x / kTile, ty = cv.ty0 + y / kTile;
    const PaintTile &own = cv.w.at(tx, ty);
    const int lx = x % kTile, ly = y % kTile;
    // Blend across tile edges with a jittered, dithered border.
    const int dl = lx, dr = kTile - 1 - lx, du = ly, dd = kTile - 1 - ly;
    struct Cand {
        int d;
        int nx, ny;
    };
    Cand cands[4] = {{dl, tx - 1, ty}, {dr, tx + 1, ty}, {du, tx, ty - 1}, {dd, tx, ty + 1}};
    std::sort(std::begin(cands), std::end(cands), [](const Cand &a, const Cand &b) { return a.d < b.d; });
    if (own.terrain == Terrain::Ocean) {
        for (const Cand &c : cands) {
            if (cv.w.land(c.nx, c.ny)) {
                return cv.w.at(c.nx, c.ny).terrain;
            }
        }
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (cv.w.land(tx + dx, ty + dy)) {
                    return cv.w.at(tx + dx, ty + dy).terrain;
                }
            }
        }
        return Terrain::Grassland;
    }
    const Cand &c = cands[0];
    if (c.d < 4 && cv.w.land(c.nx, c.ny)) {
        const Terrain other = cv.w.at(c.nx, c.ny).terrain;
        if (other != own.terrain) {
            const double p = (4 - c.d) / 8.0;
            if (hashf(cv.gx(x), cv.gy(y), 301) < p) {
                return other;
            }
        }
    }
    return own.terrain;
}

void paint_ground(Canvas &cv)
{
    const int W = cv.pm.w, H = cv.pm.h;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y) * W + x;
            const int gx = cv.gx(x), gy = cv.gy(y);
            const int d = cv.dist[i];
            Argb c;
            if (cv.landmask[i]) {
                const Terrain t = land_terrain_at(cv, x, y);
                c = ground_px(t, gx, gy);
                if (d == 1) {
                    c = t == Terrain::Arctic ? rgb(0xF8FCFF) : rgb(0xC8B47C);
                } else if (d == 2 && t != Terrain::Arctic) {
                    c = hashf(gx, gy, 302) < 0.7 ? rgb(0xDCCB94) : c;
                }
            } else {
                // Water: foam, shallows, then deep sea with drifting wave marks.
                if (d == 1) {
                    c = ((gx + gy) % 3 == 0) ? rgb(0x7CB8E0) : rgb(0xD6ECF6);
                } else if (d <= 3) {
                    c = rgb(0x5A9ED2);
                } else if (d <= 6) {
                    c = bayer4(gx, gy) < (6.0 - d) / 4.0 ? rgb(0x5A9ED2) : rgb(0x3C80BC);
                } else if (d <= 10) {
                    c = bayer4(gx, gy) < (10.0 - d) / 5.0 ? rgb(0x3C80BC) : rgb(0x24569A);
                } else {
                    c = rgb(0x24569A);
                }
                // Wave dashes.
                const int wx = gx % 12, wy = gy % 9;
                const int cell = static_cast<int>(hash3(gx / 12, gy / 9, 303) % 5);
                if (d > 3 && wy == 4 && wx >= 3 + cell && wx < 7 + cell) {
                    c = d > 8 ? rgb(0x3A72B4) : rgb(0x74B2DE);
                }
            }
            cv.pm.px[i] = c;
        }
    }
}

// --- line helpers -----------------------------------------------------------

template <typename F>
void walk_line(double x0, double y0, double x1, double y1, F f)
{
    const int steps = static_cast<int>(std::ceil(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)))) * 2 + 1;
    for (int s = 0; s <= steps; ++s) {
        const double t = static_cast<double>(s) / steps;
        f(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, t);
    }
}

void disc(Pixmap &pm, double cx, double cy, double r, Argb c)
{
    for (int y = static_cast<int>(std::floor(cy - r)); y <= static_cast<int>(std::ceil(cy + r)); ++y) {
        for (int x = static_cast<int>(std::floor(cx - r)); x <= static_cast<int>(std::ceil(cx + r)); ++x) {
            const double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
            if (dx * dx + dy * dy <= r * r) {
                pm.set(x, y, c);
            }
        }
    }
}

// --- rivers -------------------------------------------------------------------

void paint_rivers(Canvas &cv)
{
    // Pass 1 banks, pass 2 water, pass 3 glints, so joins stay clean.
    for (int pass = 0; pass < 3; ++pass) {
        for (int ty = 0; ty < cv.rows; ++ty) {
            for (int tx = 0; tx < cv.cols; ++tx) {
                const int wx = cv.tx0 + tx, wy = cv.ty0 + ty;
                const PaintTile &t = cv.w.at(wx, wy);
                if (!t.river) {
                    continue;
                }
                const double cx = tx * kTile + 15.5, cy = ty * kTile + 15.5;
                const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                int links = 0;
                for (const auto &d : dirs) {
                    const PaintTile &n = cv.w.at(wx + d[0], wy + d[1]);
                    // Join river tiles; run into the sea only where the
                    // course ends (one river link), so it meets the coast.
                    bool link = n.river;
                    if (!link && n.terrain == Terrain::Ocean) {
                        int rl = 0;
                        for (const auto &e : dirs) {
                            rl += cv.w.at(wx + e[0], wy + e[1]).river ? 1 : 0;
                        }
                        link = rl <= 1;
                    }
                    if (!link) {
                        continue;
                    }
                    ++links;
                    const double ex = cx + d[0] * 16.5, ey = cy + d[1] * 16.5;
                    // A gentle meander, fixed by the shared edge so joins line up.
                    const int seed = static_cast<int>(hash3(std::min(wx, wx + d[0]) * 2 + (d[0] != 0), std::min(wy, wy + d[1]), 401) % 1000);
                    const double amp = 2.2 + (seed % 3) * 0.6;
                    walk_line(cx, cy, ex, ey, [&](double x, double y, double s) {
                        const double off = std::sin(s * 3.14159) * amp * ((seed & 1) ? 1 : -1);
                        const double px = x + (d[1] != 0 ? off : 0), py = y + (d[0] != 0 ? off : 0);
                        if (pass == 0) {
                            disc(cv.pm, px, py, 3.0, rgb(0x2C4A3A));
                        } else if (pass == 1) {
                            disc(cv.pm, px, py, 2.1, rgb(0x3A78C4));
                        } else if (hash3(static_cast<int>(px), static_cast<int>(py), 402) % 5 == 0) {
                            cv.pm.set(static_cast<int>(px), static_cast<int>(py), rgb(0x8CC4EC));
                        }
                    });
                }
                if (links == 1 && pass < 2) {
                    disc(cv.pm, cx, cy, pass == 0 ? 3.6 : 2.6, pass == 0 ? rgb(0x2C4A3A) : rgb(0x3A78C4)); // spring
                }
            }
        }
    }
}

// --- improvements ---------------------------------------------------------------

void paint_irrigation(Canvas &cv, int ox, int oy)
{
    // Tidy crop rows between two ditches.
    for (int y = 3; y < 29; ++y) {
        for (int x = 3; x < 29; ++x) {
            Argb c = (y % 4 == 0) ? rgb(0x6E7A2C) : ((y % 4 == 1) ? rgb(0xC8C860) : rgb(0x9AB040));
            if (y % 4 == 2 && (x + y) % 3 == 0) {
                c = rgb(0xE0D878);
            }
            if (x == 10 || x == 21) {
                c = rgb(0x4C8ED0);
            } else if (x == 11 || x == 22) {
                c = rgb(0x2C5C8C);
            } else if (x == 9 || x == 20) {
                c = rgb(0x8CC4EC);
            }
            if (x == 3 || y == 3 || x == 28 || y == 28) {
                c = rgb(0x5E6A28);
            }
            cv.pm.set(ox + x, oy + y, c);
        }
    }
}

void paint_roads(Canvas &cv)
{
    for (int pass = 0; pass < 3; ++pass) {
        for (int ty = 0; ty < cv.rows; ++ty) {
            for (int tx = 0; tx < cv.cols; ++tx) {
                const int wx = cv.tx0 + tx, wy = cv.ty0 + ty;
                const PaintTile &t = cv.w.at(wx, wy);
                if (!t.road) {
                    continue;
                }
                const double cx = tx * kTile + 15.5, cy = ty * kTile + 15.5;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (!(dx || dy) || !cv.w.at(wx + dx, wy + dy).road) {
                            continue;
                        }
                        if (t.rail && cv.w.at(wx + dx, wy + dy).rail) {
                            continue;   // the railroad pass draws this link
                        }
                        const double ex = cx + dx * 16.0, ey = cy + dy * 16.0;
                        walk_line(cx, cy, ex, ey, [&](double x, double y, double) {
                            if (pass == 0) {
                                disc(cv.pm, x, y, 2.6, rgb(0x4E3820));
                            } else if (pass == 1) {
                                disc(cv.pm, x, y, 1.7, rgb(0x96764A));
                            } else {
                                const int ix = static_cast<int>(x), iy = static_cast<int>(y);
                                if ((ix + iy) % 4 == 0) {
                                    cv.pm.set(ix, iy, rgb(0xC0A070));
                                }
                            }
                        });
                    }
                }
            }
        }
    }
}

// Railroads: a dark ballast bed, wooden sleepers across it and two steel
// rails along it, from the square's middle to each railroad neighbour.
void paint_rails(Canvas &cv)
{
    for (int pass = 0; pass < 3; ++pass) {
        for (int ty = 0; ty < cv.rows; ++ty) {
            for (int tx = 0; tx < cv.cols; ++tx) {
                const int wx = cv.tx0 + tx, wy = cv.ty0 + ty;
                if (!cv.w.at(wx, wy).rail) {
                    continue;
                }
                const double cx = tx * kTile + 15.5, cy = ty * kTile + 15.5;
                int links = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (!(dx || dy) || !cv.w.at(wx + dx, wy + dy).rail) {
                            continue;
                        }
                        ++links;
                        const double len = std::sqrt(static_cast<double>(dx * dx + dy * dy));
                        const double nx = -dy / len, ny = dx / len;   // across the line
                        const double ex = cx + dx * 16.0, ey = cy + dy * 16.0;
                        if (pass == 0) {
                            walk_line(cx, cy, ex, ey, [&](double x, double y, double) { disc(cv.pm, x, y, 3.0, rgb(0x3A3634)); });
                        } else if (pass == 1) {
                            const double span = 16.0 * len;
                            for (double d = 2.0; d < span; d += 4.0) {
                                const double px = cx + dx * 16.0 * d / span, py = cy + dy * 16.0 * d / span;
                                walk_line(px - nx * 3.4, py - ny * 3.4, px + nx * 3.4, py + ny * 3.4,
                                          [&](double x, double y, double) { cv.pm.set(static_cast<int>(x), static_cast<int>(y), rgb(0x6E4C2C)); });
                            }
                        } else {
                            for (int side = -1; side <= 1; side += 2) {
                                const double ox = nx * 1.6 * side, oy = ny * 1.6 * side;
                                walk_line(cx + ox, cy + oy, ex + ox, ey + oy, [&](double x, double y, double) {
                                    cv.pm.set(static_cast<int>(x), static_cast<int>(y), rgb(side < 0 ? 0xD8DCE4 : 0x9AA0AA));
                                });
                            }
                        }
                    }
                }
                if (!links && pass == 0) {
                    // A lone stretch: a short east-west siding.
                    for (int x = -7; x <= 7; ++x) {
                        for (int y = -2; y <= 2; ++y) cv.pm.set(static_cast<int>(cx) + x, static_cast<int>(cy) + y, rgb(0x3A3634));
                    }
                    for (int x = -6; x <= 6; x += 3) {
                        for (int y = -3; y <= 3; ++y) cv.pm.set(static_cast<int>(cx) + x, static_cast<int>(cy) + y, rgb(0x6E4C2C));
                    }
                    for (int x = -7; x <= 7; ++x) {
                        cv.pm.set(static_cast<int>(cx) + x, static_cast<int>(cy) - 2, rgb(0xD8DCE4));
                        cv.pm.set(static_cast<int>(cx) + x, static_cast<int>(cy) + 1, rgb(0x9AA0AA));
                    }
                }
            }
        }
    }
}

// --- terrain features -----------------------------------------------------------

void tree(Pixmap &pm, int cx, int cy, int r, bool jungle, int seed)
{
    const Argb out = rgb(0x14301A);
    const Argb dark = jungle ? rgb(0x1E6420) : rgb(0x24561E);
    const Argb mid = jungle ? rgb(0x34922C) : rgb(0x347A2A);
    const Argb light = jungle ? rgb(0x7CC444) : rgb(0x58A03C);
    // trunk
    pm.fill_rect(cx - 1, cy + r - 2, 2, 4, rgb(0x5A3A1E));
    pm.set(cx - 1, cy + r + 1, out);
    pm.set(cx, cy + r + 1, out);
    disc(pm, cx + 0.5, cy + 0.5, r + 0.9, out);
    disc(pm, cx + 0.5, cy + 0.5, r, dark);
    disc(pm, cx - 0.3, cy - 0.5, r - 1.3, mid);
    disc(pm, cx - 1.5, cy - 1.8, std::max(1.0, r - 3.6), light);
    if (jungle) {
        // Broad leaf tips poking out.
        const int k = seed % 4;
        pm.set(cx + r, cy - 1 + k % 2, light);
        pm.set(cx - r - 1, cy + 1, mid);
        pm.set(cx + 1, cy - r - 1, light);
    } else {
        pm.set(cx + 1, cy + 1, light);
    }
}

void palm(Pixmap &pm, int cx, int by)
{
    const Argb out = rgb(0x14301A);
    for (int i = 0; i < 9; ++i) {
        pm.set(cx + (i > 5 ? 1 : 0), by - i, rgb(0x7A5228));
    }
    const int tx = cx + 1, ty = by - 9;
    const int fr[][2] = {{-4, 1}, {-3, 0}, {-2, -1}, {-1, -1}, {0, -1}, {1, -1}, {2, -1}, {3, 0}, {4, 1},
                         {-3, 2}, {3, 2},  {0, -2},  {-5, 2}, {5, 2}};
    for (const auto &f : fr) {
        pm.set(tx + f[0], ty + f[1] + 1, out);
    }
    for (const auto &f : fr) {
        pm.set(tx + f[0], ty + f[1], (f[0] + f[1]) % 2 ? rgb(0x5CB838) : rgb(0x2E8A26));
    }
}

void hill(Pixmap &pm, int cx, int by, int rw, int rh, int seed)
{
    const Argb out = rgb(0x3C4620);
    const Argb shade = rgb(0x5E6E2E);
    const Argb mid = rgb(0x96A850);
    const Argb light = rgb(0xBCCC70);
    for (int y = by - rh; y <= by; ++y) {
        for (int x = cx - rw; x <= cx + rw; ++x) {
            const double nx = (x - cx) / static_cast<double>(rw), ny = (by - y) / static_cast<double>(rh);
            const double inside = nx * nx + ny * ny;
            if (inside > 1.0 || y > by) {
                continue;
            }
            Argb c = mid;
            if (inside > 0.78) {
                c = out;
            } else if (nx > 0.25 && ny < 0.75) {
                c = (bayer4(x, y) < 0.5 || nx > 0.55) ? shade : mid;
            } else if (nx < -0.1 && ny > 0.45) {
                c = (bayer4(x, y) < 0.6) ? light : mid;
            }
            pm.set(x, y, c);
        }
    }
    (void)seed;
}

void peak(Pixmap &pm, int cx, int by, int half, int h, bool snow)
{
    const Argb out = rgb(0x26221E);
    const Argb lit = rgb(0xB8B2A2);
    const Argb mid = rgb(0x8C8678);
    const Argb dark = rgb(0x5A5448);
    const Argb snow_c = rgb(0xF6F8FC), snow_s = rgb(0xB8C8DC);
    for (int y = by - h; y <= by; ++y) {
        const double t = static_cast<double>(y - (by - h)) / h; // 0 top .. 1 base
        const int span = static_cast<int>(std::round(half * t));
        for (int x = cx - span - 1; x <= cx + span + 1; ++x) {
            if (y == by - h && x != cx) {
                continue;
            }
            Argb c;
            const bool edge = x <= cx - span - 1 + 0 || x >= cx + span + 1;
            if (edge) {
                c = out;
            } else if (x < cx) {
                c = (x == cx - 1 && hash3(x, y, 501) % 3 == 0) ? mid : lit;
            } else if (x == cx) {
                c = mid;
            } else {
                c = (bayer4(x, y) < 0.25) ? mid : dark;
            }
            if (snow && !edge && t < 0.32 + 0.08 * std::sin(x * 1.7)) {
                c = x <= cx ? snow_c : snow_s;
            }
            pm.set(x, y, c);
        }
    }
    for (int x = cx - half - 1; x <= cx + half + 1; ++x) {
        pm.set(x, by + 1, out);
    }
}

void reeds(Pixmap &pm, int x, int by)
{
    for (int i = 0; i < 4; ++i) {
        pm.set(x, by - i, rgb(0x9AA24C));
        pm.set(x + 2, by - i + 1, rgb(0x7E8A3C));
    }
    pm.set(x, by - 4, rgb(0x5A3A1E));
    pm.set(x + 2, by - 3, rgb(0x5A3A1E));
}

void puddle(Pixmap &pm, int cx, int cy, int rw, int rh)
{
    for (int y = cy - rh; y <= cy + rh; ++y) {
        for (int x = cx - rw; x <= cx + rw; ++x) {
            const double nx = (x - cx) / static_cast<double>(rw), ny = (y - cy) / static_cast<double>(rh);
            const double d = nx * nx + ny * ny;
            if (d <= 1.0) {
                pm.set(x, y, d > 0.65 ? rgb(0x34483A) : (ny < -0.2 && nx < 0.2 ? rgb(0x6A9C9A) : rgb(0x3E6C6C)));
            }
        }
    }
}

struct Jit {
    int x, y;
};

void paint_features(Canvas &cv, int tx, int ty)
{
    const int wx = cv.tx0 + tx, wy = cv.ty0 + ty;
    const PaintTile &t = cv.w.at(wx, wy);
    if (cv.w.city_at(wx, wy) || t.irrigated) {
        return;
    }
    const int ox = tx * kTile, oy = ty * kTile;
    const bool busy = t.river || t.road;
    auto jit = [&](int i) { return Jit{static_cast<int>(hash3(wx, wy, 600 + i) % 3) - 1, static_cast<int>(hash3(wx, wy, 700 + i) % 3) - 1}; };
    switch (t.terrain) {
    case Terrain::Forest: {
        const int pos[][3] = {{8, 6, 5}, {21, 5, 5}, {14, 12, 6}, {26, 14, 4}, {5, 17, 5}, {19, 20, 5}, {9, 25, 5}, {25, 26, 4}};
        for (int i = 0; i < 8; ++i) {
            const Jit j = jit(i);
            const int x = pos[i][0] + j.x, y = pos[i][1] + j.y;
            if (busy && std::abs(x - 16) < 8 && std::abs(y - 16) < 8) {
                continue;
            }
            tree(cv.pm, ox + x, oy + y, pos[i][2], false, i);
        }
        break;
    }
    case Terrain::Jungle: {
        const int pos[][3] = {{6, 5, 4}, {15, 4, 4}, {25, 6, 4}, {10, 12, 5}, {21, 13, 5}, {4, 20, 4}, {15, 20, 4}, {26, 21, 4}, {9, 27, 4}, {21, 27, 4}};
        for (int i = 0; i < 10; ++i) {
            const Jit j = jit(i);
            const int x = pos[i][0] + j.x, y = pos[i][1] + j.y;
            if (busy && std::abs(x - 16) < 7 && std::abs(y - 16) < 7) {
                continue;
            }
            if (i == 3 || i == 8) {
                palm(cv.pm, ox + x, oy + y + 4);
            } else {
                tree(cv.pm, ox + x, oy + y, pos[i][2], true, i);
            }
        }
        break;
    }
    case Terrain::Hills: {
        hill(cv.pm, ox + 10, oy + 14, 9, 8, 0);
        hill(cv.pm, ox + 23, oy + 17, 8, 7, 1);
        hill(cv.pm, ox + 14, oy + 27, 10, 8, 2);
        if (t.mine) {
            cv.pm.blit(mine_sprite(), ox + 9, oy + 14);
        }
        break;
    }
    case Terrain::Mountains: {
        peak(cv.pm, ox + 10, oy + 20, 9, 16, true);
        peak(cv.pm, ox + 23, oy + 23, 8, 14, hash3(wx, wy, 801) % 2);
        peak(cv.pm, ox + 15, oy + 30, 9, 13, false);
        break;
    }
    case Terrain::Swamp: {
        puddle(cv.pm, ox + 9, oy + 9, 5, 2);
        puddle(cv.pm, ox + 22, oy + 15, 6, 3);
        puddle(cv.pm, ox + 10, oy + 23, 6, 2);
        reeds(cv.pm, ox + 15, oy + 10);
        reeds(cv.pm, ox + 4, oy + 16);
        reeds(cv.pm, ox + 25, oy + 25);
        reeds(cv.pm, ox + 18, oy + 25);
        break;
    }
    default: break;
    }
}



Pixmap render_canvas(const PaintWorld &w, int x0, int y0, int cols, int rows)
{
    Canvas cv(w, x0, y0, cols, rows);
    build_land(cv);
    paint_ground(cv);
    for (int ty = 0; ty < cv.rows; ++ty) {
        for (int tx = 0; tx < cv.cols; ++tx) {
            const PaintTile &t = w.at(cv.tx0 + tx, cv.ty0 + ty);
            if (t.irrigated && !w.city_at(cv.tx0 + tx, cv.ty0 + ty)) {
                paint_irrigation(cv, tx * kTile, ty * kTile);
            }
        }
    }
    paint_rivers(cv);
    paint_roads(cv);
    paint_rails(cv);
    for (int ty = 0; ty < cv.rows; ++ty) {
        for (int tx = 0; tx < cv.cols; ++tx) {
            paint_features(cv, tx, ty);
        }
    }
    for (int ty = 0; ty < cv.rows; ++ty) {
        for (int tx = 0; tx < cv.cols; ++tx) {
            const int wx = cv.tx0 + tx, wy = cv.ty0 + ty;
            const PaintTile &t = w.at(wx, wy);
            if (t.special != Special::None && !w.city_at(wx, wy)) {
                Pixmap s = special_sprite(t.special);
                if (t.special == Special::Shield) {
                    cv.pm.blit(s, tx * kTile + 3, ty * kTile + 22);
                } else {
                    cv.pm.blit(s, tx * kTile + 16 - s.w / 2, ty * kTile + 16 - s.h / 2);
                }
            }
        }
    }
    for (int ty = 0; ty < cv.rows; ++ty) {
        for (int tx = 0; tx < cv.cols; ++tx) {
            const PaintTile &t = w.at(cv.tx0 + tx, cv.ty0 + ty);
            if (t.city) {
                continue;
            }
            if (t.mine && t.terrain != Terrain::Hills) {
                cv.pm.blit(mine_sprite(), tx * kTile + 9, ty * kTile + 14);
            }
            if (t.hut) {
                Pixmap hs = hut_sprite();
                cv.pm.blit(hs, tx * kTile + 16 - hs.w / 2, ty * kTile + 17 - hs.h / 2);
            }
        }
    }
    return cv.pm.crop(kTile, kTile, cols * kTile, rows * kTile);
}

} // namespace

Pixmap render_terrain(const PaintWorld &w, int x0, int y0, int cols, int rows)
{
    return render_canvas(w, x0, y0, cols, rows);
}
void bracket(Pixmap &pm, int ox, int oy, Argb c, Argb shadow)
{
    const int L = 7;
    auto px2 = [&](int x, int y) {
        pm.set(ox + x + 1, oy + y + 1, shadow);
    };
    auto pxc = [&](int x, int y) { pm.set(ox + x, oy + y, c); };
    for (int pass = 0; pass < 2; ++pass) {
        auto put = [&](int x, int y) { pass == 0 ? px2(x, y) : pxc(x, y); };
        for (int i = 0; i < L; ++i) {
            for (int t = 0; t < 2; ++t) {
                put(i, t);
                put(t, i);
                put(31 - i, t);
                put(31 - t, i);
                put(i, 31 - t);
                put(t, 31 - i);
                put(31 - i, 31 - t);
                put(31 - t, 31 - i);
            }
        }
    }
}

void apply_fog(Pixmap &pm, int tx0, int ty0, const std::function<bool(int, int)> &explored)
{
    const int cols = (pm.w + kTile - 1) / kTile + 2, rows = (pm.h + kTile - 1) / kTile + 2;
    // Explored flags with a one-square margin.
    std::vector<unsigned char> ex(static_cast<size_t>(cols) * rows);
    for (int j = 0; j < rows; ++j) {
        for (int i = 0; i < cols; ++i) {
            ex[static_cast<size_t>(j) * cols + i] = explored(tx0 + i - 1, ty0 + j - 1) ? 1 : 0;
        }
    }
    auto known = [&](int i, int j) { return ex[static_cast<size_t>(j + 1) * cols + (i + 1)] != 0; };
    for (int ty = 0; ty * kTile < pm.h; ++ty) {
        for (int tx = 0; tx * kTile < pm.w; ++tx) {
            const int x0 = tx * kTile, y0 = ty * kTile;
            const int x1 = std::min(pm.w, x0 + kTile), y1 = std::min(pm.h, y0 + kTile);
            if (!known(tx, ty)) {
                for (int y = y0; y < y1; ++y) {
                    std::fill(pm.px.begin() + static_cast<long>(y) * pm.w + x0, pm.px.begin() + static_cast<long>(y) * pm.w + x1,
                              rgb(0x000000));
                }
                continue;
            }
            bool edge = false;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    edge |= !known(tx + dx, ty + dy);
                }
            }
            if (!edge) {
                continue;
            }
            // Ragged dithered edge where an explored square meets the dark.
            for (int y = y0; y < y1; ++y) {
                const int ly = y - y0;
                for (int x = x0; x < x1; ++x) {
                    const int lx = x - x0;
                    int d = 99;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (!(dx || dy) || known(tx + dx, ty + dy)) {
                                continue;
                            }
                            const int ddx = dx < 0 ? lx : (dx > 0 ? kTile - 1 - lx : 0);
                            const int ddy = dy < 0 ? ly : (dy > 0 ? kTile - 1 - ly : 0);
                            const int dd = (dx && dy) ? std::max(ddx, ddy) : (dx ? ddx : ddy);
                            d = std::min(d, dd);
                        }
                    }
                    if (d < 5) {
                        const int gx = (tx0 + tx) * kTile + lx, gy = (ty0 + ty) * kTile + ly;
                        const double jag = hashf(gx / 2, gy / 2, 901) * 1.5;
                        if (bayer4(gx, gy) * 5.0 + jag > d + 1.2) {
                            pm.px[static_cast<size_t>(y) * pm.w + x] = rgb(0x000000);
                        }
                    }
                }
            }
        }
    }
}

PaintTile &PaintWorld::at(int x, int y)
{
    x = ((x % w) + w) % w;
    y = std::clamp(y, 0, h - 1);
    return tiles[static_cast<size_t>(y) * w + x];
}

const PaintTile &PaintWorld::at(int x, int y) const
{
    x = ((x % w) + w) % w;
    y = std::clamp(y, 0, h - 1);
    return tiles[static_cast<size_t>(y) * w + x];
}

PaintTile paint_tile(const civ::Tile &t, bool city)
{
    using CT = civ::Terrain;
    PaintTile p;
    p.road = t.road || city;
    p.rail = t.railroad;
    p.irrigated = t.irrigation;
    p.mine = t.mine;
    p.hut = t.hut;
    p.city = city;
    Special sp = Special::None;
    switch (t.terrain) {
    case CT::Arctic: p.terrain = Terrain::Arctic; sp = Special::Seals; break;
    case CT::Desert: p.terrain = Terrain::Desert; sp = Special::Oasis; break;
    case CT::Forest: p.terrain = Terrain::Forest; sp = Special::Game; break;
    case CT::Grassland: p.terrain = Terrain::Grassland; sp = Special::Shield; break;
    case CT::Hills: p.terrain = Terrain::Hills; sp = Special::Coal; break;
    case CT::Jungle: p.terrain = Terrain::Jungle; sp = Special::Gems; break;
    case CT::Mountains: p.terrain = Terrain::Mountains; sp = Special::Gold; break;
    case CT::Ocean: p.terrain = Terrain::Ocean; sp = Special::Fish; break;
    case CT::Plains: p.terrain = Terrain::Plains; sp = Special::Horses; break;
    case CT::River: p.terrain = Terrain::Grassland; p.river = true; sp = Special::Shield; break;
    case CT::Swamp: p.terrain = Terrain::Swamp; sp = Special::Oil; break;
    case CT::Tundra: p.terrain = Terrain::Tundra; sp = Special::Game; break;
    default: break;
    }
    if (t.special) {
        p.special = sp;
    }
    return p;
}

PaintWorld paint_world(const civ::Map &m, const std::vector<std::pair<int, int>> &city_squares)
{
    PaintWorld w;
    for (int y = 0; y < m.h; ++y) {
        for (int x = 0; x < m.w; ++x) {
            w.at(x, y) = paint_tile(m.at(x, y), false);
        }
    }
    for (const auto &c : city_squares) {
        w.at(c.first, c.second) = paint_tile(m.at(c.first, c.second), true);
    }
    return w;
}

Argb minimap_color(civ::Terrain t)
{
    using CT = civ::Terrain;
    switch (t) {
    case CT::Ocean: return rgb(0x1E4A8C);
    case CT::Grassland: return rgb(0x4E9A34);
    case CT::Plains: return rgb(0x9AA444);
    case CT::Desert: return rgb(0xDCC07A);
    case CT::Tundra: return rgb(0x8C8C6E);
    case CT::Arctic: return rgb(0xE6EEF4);
    case CT::Forest: return rgb(0x2F6E28);
    case CT::Jungle: return rgb(0x2E8A2A);
    case CT::Swamp: return rgb(0x5E7A50);
    case CT::Hills: return rgb(0x8A9E48);
    case CT::Mountains: return rgb(0x8A8478);
    case CT::River: return rgb(0x3C86C8);
    default: return rgb(0x000000);
    }
}

Pixmap render_terrain_sample(Terrain t, bool river, bool road, bool irrigated, bool mine, Special special, int variant)
{
    PaintWorld w;
    for (PaintTile &tile : w.tiles) {
        tile.terrain = t;
    }
    const int cx = 40 + variant * 3, cy = 25;
    PaintTile &c = w.at(cx, cy);
    c.river = river;
    c.road = road;
    c.irrigated = irrigated;
    c.mine = mine;
    c.special = special;
    if (river) {
        w.at(cx, cy - 1).river = true;
        w.at(cx, cy + 1).river = true;
    }
    if (road) {
        w.at(cx - 1, cy).road = true;
        w.at(cx + 1, cy + 1).road = true;
    }
    return render_canvas(w, cx, cy, 1, 1);
}

} // namespace art
