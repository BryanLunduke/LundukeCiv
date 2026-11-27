// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#include "map.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace civ {

int Map::distance(int x0, int y0, int x1, int y1) const
{
    return std::max(std::abs(dx(x0, x1)), std::abs(y1 - y0));
}

int Map::dos_distance(int x0, int y0, int x1, int y1) const
{
    const int ax = std::abs(dx(x0, x1)), ay = std::abs(y1 - y0);
    return std::max(ax, ay) + std::min(ax, ay) / 2;
}

bool Map::coastal(int x, int y) const
{
    if (!land(x, y)) {
        return false;
    }
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            if ((i || j) && ocean(x + i, y + j)) {
                return true;
            }
        }
    }
    return false;
}

bool Map::next_to_land(int x, int y) const
{
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            if ((i || j) && land(x + i, y + j)) {
                return true;
            }
        }
    }
    return false;
}

void Map::label_continents()
{
    for (Tile &t : tiles) {
        t.continent = 0;
    }
    int next = 1;
    std::vector<int> stack;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!land(x, y) || at(x, y).continent) {
                continue;
            }
            const int id = std::min(next++, 255);
            stack.push_back(y * w + x);
            at(x, y).continent = static_cast<std::uint8_t>(id);
            while (!stack.empty()) {
                const int p = stack.back();
                stack.pop_back();
                const int px = p % w, py = p / w;
                for (int j = -1; j <= 1; ++j) {
                    for (int i = -1; i <= 1; ++i) {
                        const int nx = wrap_x(px + i), ny = py + j;
                        if (land(nx, ny) && !at(nx, ny).continent) {
                            at(nx, ny).continent = static_cast<std::uint8_t>(id);
                            stack.push_back(ny * w + nx);
                        }
                    }
                }
            }
        }
    }
}

bool special_pattern(int x, int y)
{
    // Clusters on a skewed lattice, roughly one tile in sixteen.
    const int a = ((x * 3 + y * 7) % 16 + 16) % 16;
    const int b = ((x + y * 5) % 8 + 8) % 8;
    return a == 0 || (a == 9 && b == 3);
}

namespace {

double value_noise(int x, int y, int seed)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u +
                      static_cast<std::uint32_t>(seed) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0;
}

// Smooth noise that wraps east-west with period `period` cells.
double smooth(double x, double y, int period, int seed)
{
    const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
    const double fx = x - xi, fy = y - yi;
    const double sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    auto v = [&](int i, int j) { return value_noise(((i % period) + period) % period, j, seed); };
    const double a = v(xi, yi), b = v(xi + 1, yi), c = v(xi, yi + 1), d = v(xi + 1, yi + 1);
    return (a + (b - a) * sx) * (1 - sy) + (c + (d - c) * sx) * sy;
}

} // namespace

Map generate_map(const MapSettings &s)
{
    Map m;
    Rng rng(s.seed);
    const int seed = static_cast<int>(s.seed & 0x7FFFFFFF);
    // Elevation from three octaves of wrapped noise; the threshold is chosen
    // so the requested share of the map is land.
    std::vector<double> elev(static_cast<size_t>(m.w) * m.h);
    for (int y = 0; y < m.h; ++y) {
        for (int x = 0; x < m.w; ++x) {
            double e = smooth(x / 10.0, y / 8.0, 8, seed) * 0.55 + smooth(x / 5.0, y / 4.0, 16, seed + 1) * 0.30 +
                       smooth(x / 2.5, y / 2.0, 32, seed + 2) * 0.15;
            // Keep the poles watery except for the ice rows.
            const double lat = std::fabs((y + 0.5) / m.h - 0.5) * 2.0;
            e -= std::max(0.0, lat - 0.75) * 0.8;
            elev[static_cast<size_t>(y) * m.w + x] = e;
        }
    }
    std::vector<double> sorted = elev;
    std::sort(sorted.begin(), sorted.end());
    const double sea = sorted[static_cast<size_t>(sorted.size() * (100 - s.land_pct) / 100)];
    const double hill = sorted[static_cast<size_t>(sorted.size() * (100 - s.land_pct / 5) / 100)];
    const double peak = sorted[static_cast<size_t>(sorted.size() * (100 - s.land_pct / 12) / 100)];
    for (int y = 0; y < m.h; ++y) {
        for (int x = 0; x < m.w; ++x) {
            const double e = elev[static_cast<size_t>(y) * m.w + x];
            Tile &t = m.at(x, y);
            if (e < sea) {
                t.terrain = Terrain::Ocean;
                continue;
            }
            const double lat = std::fabs((y + 0.5) / m.h - 0.5) * 2.0; // 0 equator .. 1 pole
            const double wet = smooth(x / 6.0, y / 5.0, 13, seed + 7) + (rng.unit() - 0.5) * 0.25;
            const double temp = 1.0 - lat + (smooth(x / 7.0, y / 6.0, 11, seed + 9) - 0.5) * 0.25;
            Terrain k;
            if (e > peak) {
                k = Terrain::Mountains;
            } else if (e > hill) {
                k = Terrain::Hills;
            } else if (temp < 0.18) {
                k = Terrain::Arctic;
            } else if (temp < 0.32) {
                k = Terrain::Tundra;
            } else if (temp > 0.72 && wet > 0.62) {
                k = Terrain::Jungle;
            } else if (temp > 0.6 && wet > 0.7) {
                k = Terrain::Swamp;
            } else if (temp > 0.6 && temp < 0.85 && wet < 0.33) {
                k = Terrain::Desert;
            } else if (wet > 0.58) {
                k = Terrain::Forest;
            } else if (wet > 0.42) {
                k = Terrain::Grassland;
            } else {
                k = Terrain::Plains;
            }
            if (k == Terrain::Swamp && rng.chance(0.4)) {
                k = Terrain::Grassland;
            }
            t.terrain = k;
        }
    }
    // Polar ice caps.
    for (int x = 0; x < m.w; ++x) {
        m.at(x, 0).terrain = Terrain::Arctic;
        m.at(x, m.h - 1).terrain = Terrain::Arctic;
        if (rng.chance(0.35)) m.at(x, 1).terrain = Terrain::Arctic;
        if (rng.chance(0.35)) m.at(x, m.h - 2).terrain = Terrain::Arctic;
    }
    // Rivers: start in hills or mountains, run downhill to the sea.
    const int rivers = 18 + rng.below(8);
    for (int r = 0; r < rivers; ++r) {
        int x = 0, y = 0;
        bool found = false;
        for (int tries = 0; tries < 200 && !found; ++tries) {
            x = rng.below(m.w);
            y = 3 + rng.below(m.h - 6);
            const Terrain t = m.at(x, y).terrain;
            found = (t == Terrain::Hills || t == Terrain::Mountains);
        }
        if (!found) {
            continue;
        }
        // Step to a neighbour (4-way) each time, preferring lower ground and
        // keeping clear of other water, so each river is one thin course.
        std::vector<std::pair<int, int>> path;
        auto on_path = [&](int px, int py) {
            for (const auto &p : path) {
                if (p.first == px && p.second == py) return true;
            }
            return false;
        };
        const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        path.push_back({x, y});
        for (int step = 0; step < 30; ++step) {
            int bx = -1, by = -1;
            double best = 1e9;
            for (const auto &d : dirs) {
                const int nx = m.wrap_x(x + d[0]), ny = y + d[1];
                if (!m.valid_y(ny) || on_path(nx, ny)) continue;
                double e = elev[static_cast<size_t>(ny) * m.w + nx] + rng.unit() * 0.05;
                const bool ends = m.ocean(nx, ny) || m.at(nx, ny).terrain == Terrain::River;
                if (!ends) {
                    // Crowding: river or path squares beside the candidate.
                    for (const auto &d2 : dirs) {
                        const int qx = m.wrap_x(nx + d2[0]), qy = ny + d2[1];
                        if (!m.valid_y(qy) || (qx == x && qy == y)) continue;
                        if (m.at(qx, qy).terrain == Terrain::River || on_path(qx, qy)) e += 0.6;
                    }
                }
                if (e < best) {
                    best = e;
                    bx = nx;
                    by = ny;
                }
            }
            if (bx < 0) break;
            x = bx;
            y = by;
            if (m.ocean(x, y) || m.at(x, y).terrain == Terrain::River) {
                // Reached the sea or joined another river.
                for (size_t i = 1; i < path.size(); ++i) {
                    Tile &t = m.at(path[i].first, path[i].second);
                    if (t.terrain != Terrain::Mountains && t.terrain != Terrain::Arctic) {
                        t.terrain = Terrain::River;
                    }
                }
                break;
            }
            path.push_back({x, y});
        }
    }
    // Specials and huts.
    for (int y = 0; y < m.h; ++y) {
        for (int x = 0; x < m.w; ++x) {
            Tile &t = m.at(x, y);
            t.special = special_pattern(x, y) && !(t.terrain == Terrain::Ocean && !m.next_to_land(x, y) && rng.chance(0.5));
            if (s.huts && t.terrain != Terrain::Ocean && !t.special && y > 2 && y < m.h - 3 && rng.chance(0.022)) {
                t.hut = true;
            }
        }
    }
    m.label_continents();
    return m;
}

} // namespace civ
