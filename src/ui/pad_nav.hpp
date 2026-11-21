// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

// Gamepad helpers with no SDL or drawing in them, so the tests can use
// them: button names for the pad in hand, D-pad/stick repeat timing, and
// moving a focus between the clickable areas of a dialog, as in Lunduke
// City 2.

#include <cmath>
#include <string>
#include <vector>

namespace pad {

// What the face buttons are called on the pad in hand. A Steam Deck reads
// A/B/X/Y, L1/R1, View and the three-line Menu button; a PlayStation pad
// its symbols; anything else Xbox names.
struct Glyphs {
    std::string a = "A", b = "B", x = "X", y = "Y";
    std::string lb = "LB", rb = "RB";
    std::string view = "Back", menu = "Start";
};

inline Glyphs glyphs_for(bool playstation, bool deck, bool ps5 = false)
{
    Glyphs g;
    if (playstation) {
        g.a = "\u2715";
        g.b = "\u25CB";
        g.x = "\u25A1";
        g.y = "\u25B3";
        g.lb = "L1";
        g.rb = "R1";
        g.menu = "Options";
        g.view = ps5 ? "Create" : "Share";
    } else if (deck) {
        g.lb = "L1";
        g.rb = "R1";
        g.menu = "\u2630";
        g.view = "View";
    }
    return g;
}

// Hold timing: a fresh press takes one step at once; a held direction waits,
// then repeats on a steady beat. A long frame owes at most four steps.
constexpr int kRepeatDelayMs = 300;
constexpr int kRepeatIntervalMs = 110;
constexpr float kStickDead = 0.45f;

struct Repeat {
    int dx = 0, dy = 0;          // the held direction, 0,0 when none
    long long due_ms = 0;
};

inline void repeat_press(Repeat &r, int dx, int dy, long long now)
{
    r.dx = dx;
    r.dy = dy;
    r.due_ms = now + kRepeatDelayMs;
}

inline void repeat_release(Repeat &r) { r = Repeat(); }

inline int repeat_steps(Repeat &r, long long now)
{
    if (!r.dx && !r.dy) return 0;
    int steps = 0;
    while (now >= r.due_ms && steps < 4) {
        ++steps;
        r.due_ms += kRepeatIntervalMs;
    }
    if (now >= r.due_ms) r.due_ms = now + kRepeatIntervalMs;   // drop what a stall owed
    return steps;
}

// The stick as an eight-way direction (0,0 inside the dead zone).
inline void stick_direction(float x, float y, int &dx, int &dy)
{
    dx = dy = 0;
    if (std::sqrt(x * x + y * y) < kStickDead) return;
    const double a = std::atan2(y, x);   // y grows downward
    const int octant = static_cast<int>(std::lround(a / (3.14159265358979 / 4))) & 7;
    static const int ox[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static const int oy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    dx = ox[octant];
    dy = oy[octant];
}

struct Rect {
    double x, y, w, h;
    double cx() const { return x + w / 2; }
    double cy() const { return y + h / 2; }
};

// The next area from `from` in direction (dx, dy): the nearest one whose
// centre lies that way, preferring ones straight ahead. -1 if none.
inline int next_focus(const std::vector<Rect> &areas, int from, int dx, int dy)
{
    if (from < 0 || from >= static_cast<int>(areas.size())) return areas.empty() ? -1 : 0;
    const Rect &f = areas[from];
    int best = -1;
    double best_score = 1e18;
    for (int i = 0; i < static_cast<int>(areas.size()); ++i) {
        if (i == from) continue;
        const Rect &r = areas[i];
        const double vx = r.cx() - f.cx(), vy = r.cy() - f.cy();
        const double along = vx * dx + vy * dy;
        const double across = std::fabs(dx ? vy : vx);
        if (along <= 2.0) continue;
        // Overlapping rows/columns count as straight ahead.
        const bool lined = dx ? (r.y < f.y + f.h && f.y < r.y + r.h) : (r.x < f.x + f.w && f.x < r.x + r.w);
        const double score = along + (lined ? 0.0 : across * 3.0 + 40.0);
        if (score < best_score) {
            best_score = score;
            best = i;
        }
    }
    return best;
}

// Where focus starts in a dialog: its bottom-right button (OK, Close...).
inline int default_focus(const std::vector<Rect> &areas)
{
    int best = -1;
    for (int i = 0; i < static_cast<int>(areas.size()); ++i) {
        if (best < 0 || areas[i].y + areas[i].h > areas[best].y + areas[best].h + 1 ||
            (std::fabs(areas[i].y + areas[i].h - (areas[best].y + areas[best].h)) <= 1 && areas[i].x > areas[best].x))
            best = i;
    }
    return best;
}

} // namespace pad
