// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

// Drawing helpers in Lunduke City 2's SVGA ("Slate") look: its colours,
// fonts, gold section tags, rounded slate buttons and dialog frames.

#include "pixmap.hpp"
#include "sprites.hpp"

#include <cairo.h>

#include <string>

namespace ui {

struct Ink {
    double r, g, b;
};

constexpr Ink hex(unsigned c) { return {((c >> 16) & 255) / 255.0, ((c >> 8) & 255) / 255.0, (c & 255) / 255.0}; }

// City 2 Slate palette.
constexpr Ink kSlateBand = hex(0x1E2631);
constexpr Ink kSlateDeep = hex(0x151B23);
constexpr Ink kSlatePanel = hex(0x29323F);
constexpr Ink kSlateEdge = hex(0x3A4658);
constexpr Ink kSlateGold = hex(0xF2C14E);
constexpr Ink kSlateGoldDim = hex(0xC9A646);
constexpr Ink kSlateInk = hex(0xEEE8DA);
constexpr Ink kSlateDim = hex(0x9AA6B8);
constexpr Ink kSlateGood = hex(0x6CCB7A);
constexpr Ink kSlateBad = hex(0xE5675A);
constexpr Ink kSidebar = hex(0x1B222C);

// City 2 font strings at UI scale 1.
extern const char *kUiFont;
extern const char *kBoldFont;
extern const char *kMenuFont;
extern const char *kTagFont;
extern const char *kSmallBold;
extern const char *kTitleFont;

struct Metrics {
    int w, h;
};

void set(cairo_t *cr, const Ink &k, double a = 1.0);
void set_argb(cairo_t *cr, Argb c, double a = 1.0);
void fill(cairo_t *cr, double x, double y, double w, double h, const Ink &k, double a = 1.0);
void round_rect(cairo_t *cr, double x, double y, double w, double h, double r);
Metrics measure(cairo_t *cr, const char *font, const std::string &text, int wrap = -1);
void text(cairo_t *cr, const char *font, const std::string &s, double x, double y, const Ink &k, double a = 1.0, int wrap = -1);
// Right-aligned and centred text.
void text_right(cairo_t *cr, const char *font, const std::string &s, double right, double y, const Ink &k);
void text_center(cairo_t *cr, const char *font, const std::string &s, double cx, double y, const Ink &k);
// Gold section tag with a rule running to the panel edge.
void section(cairo_t *cr, const std::string &title, double x, double y, double right);
void label_value(cairo_t *cr, const std::string &label, const std::string &value, double x, double y, const Ink &value_ink);
// Square slate button (tool palette).
void slate_button(cairo_t *cr, double x, double y, double s, bool chosen, bool hover);
// Wide text button; gold = the default action.
void wide_button(cairo_t *cr, double x, double y, double w, double h, const std::string &label, const std::string &key,
                 bool gold, bool hover, bool enabled = true);
// Dialog frame with title bar; returns the content top.
double dialog_frame(cairo_t *cr, double x, double y, double w, double h, const std::string &title);
// Slim progress bar.
void progress(cairo_t *cr, double x, double y, double w, double h, double frac, const Ink &k);

// Civilization colours: blue, red, yellow, green, teal, purple, orange, white.
const art::CivColors &civ_colors(int index);

std::string with_commas(long long n);

} // namespace ui
