// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#include "draw.hpp"

#include <pango/pangocairo.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace ui {

const char *kUiFont = "Sans 13";
const char *kBoldFont = "Sans Bold 13";
const char *kMenuFont = "Sans 13";
const char *kTagFont = "Sans Bold 9";
const char *kSmallBold = "Sans Bold 10";
const char *kTitleFont = "Sans Bold 18";

void set(cairo_t *cr, const Ink &k, double a) { cairo_set_source_rgba(cr, k.r, k.g, k.b, a); }

void set_argb(cairo_t *cr, Argb c, double a)
{
    cairo_set_source_rgba(cr, ((c >> 16) & 255) / 255.0, ((c >> 8) & 255) / 255.0, (c & 255) / 255.0, a);
}

void fill(cairo_t *cr, double x, double y, double w, double h, const Ink &k, double a)
{
    set(cr, k, a);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
}

void round_rect(cairo_t *cr, double x, double y, double w, double h, double r)
{
    r = std::min(r, std::min(w, h) / 2.0);
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
}

namespace {

// Pango layouts are cached by font, text and wrap width: the screen redraws
// the same strings every frame.
struct LayoutCache {
    PangoContext *ctx = nullptr;
    std::map<std::tuple<const char *, std::string, int>, PangoLayout *> map;
};

LayoutCache &cache()
{
    static LayoutCache c;
    return c;
}

PangoLayout *layout_for(cairo_t *cr, const char *font, const std::string &text, int wrap)
{
    LayoutCache &c = cache();
    if (!c.ctx) {
        PangoFontMap *fm = pango_cairo_font_map_get_default();
        c.ctx = pango_font_map_create_context(fm);
        cairo_font_options_t *fo = cairo_font_options_create();
        cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_GRAY);
        cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_SLIGHT);
        pango_cairo_context_set_font_options(c.ctx, fo);
        cairo_font_options_destroy(fo);
    }
    (void)cr;
    auto key = std::make_tuple(font, text, wrap);
    auto it = c.map.find(key);
    if (it != c.map.end()) {
        return it->second;
    }
    if (c.map.size() > 4000) {
        for (auto &kv : c.map) {
            g_object_unref(kv.second);
        }
        c.map.clear();
    }
    PangoLayout *l = pango_layout_new(c.ctx);
    PangoFontDescription *d = pango_font_description_from_string(font);
    pango_layout_set_font_description(l, d);
    pango_font_description_free(d);
    if (wrap > 0) {
        pango_layout_set_width(l, wrap * PANGO_SCALE);
        pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR);
    }
    pango_layout_set_text(l, text.c_str(), -1);
    c.map[key] = l;
    return l;
}

} // namespace

Metrics measure(cairo_t *cr, const char *font, const std::string &text, int wrap)
{
    PangoLayout *l = layout_for(cr, font, text, wrap);
    int w = 0, h = 0;
    pango_layout_get_pixel_size(l, &w, &h);
    return {w, h};
}

void text(cairo_t *cr, const char *font, const std::string &s, double x, double y, const Ink &k, double a, int wrap)
{
    PangoLayout *l = layout_for(cr, font, s, wrap);
    set(cr, k, a);
    cairo_move_to(cr, std::round(x), std::round(y));
    pango_cairo_show_layout(cr, l);
}

void text_right(cairo_t *cr, const char *font, const std::string &s, double right, double y, const Ink &k)
{
    text(cr, font, s, right - measure(cr, font, s).w, y, k);
}

void text_center(cairo_t *cr, const char *font, const std::string &s, double cx, double y, const Ink &k)
{
    text(cr, font, s, cx - measure(cr, font, s).w / 2.0, y, k);
}

void section(cairo_t *cr, const std::string &title, double x, double y, double right)
{
    const Metrics m = measure(cr, kTagFont, title);
    text(cr, kTagFont, title, x, y, kSlateGoldDim);
    fill(cr, std::round(x + m.w + 6), std::round(y + m.h / 2.0), right - (x + m.w + 6), 1, kSlateEdge);
}

void label_value(cairo_t *cr, const std::string &label, const std::string &value, double x, double y, const Ink &value_ink)
{
    text(cr, kTagFont, label, x, y, kSlateDim);
    text(cr, kBoldFont, value, x, y + 13, value_ink);
}

void slate_button(cairo_t *cr, double x, double y, double s, bool chosen, bool hover)
{
    const double r = std::max(3.0, s * 0.14);
    if (chosen) {
        for (int ring = 3; ring >= 1; --ring) {
            round_rect(cr, x - ring * 1.5, y - ring * 1.5, s + ring * 3.0, s + ring * 3.0, r + ring * 1.5);
            set(cr, kSlateGold, 0.10 + 0.08 * (3 - ring));
            cairo_fill(cr);
        }
    }
    round_rect(cr, x + 0.5, y + 0.5, s - 1, s - 1, r);
    set(cr, chosen ? hex(0x3E4A5E) : (hover ? hex(0x354154) : hex(0x29323F)));
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, chosen ? 2.0 : 1.0);
    set(cr, chosen ? kSlateGold : hex(0x3C4859));
    cairo_stroke(cr);
}

void wide_button(cairo_t *cr, double x, double y, double w, double h, const std::string &label, const std::string &key,
                 bool gold, bool hover, bool enabled)
{
    round_rect(cr, x + 0.5, y + 0.5, w - 1, h - 1, 6);
    set(cr, gold ? hex(0x3E4A5E) : (hover && enabled ? hex(0x354154) : hex(0x29323F)));
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, gold ? 2.0 : 1.0);
    set(cr, gold ? kSlateGold : hex(0x3C4859));
    cairo_stroke(cr);
    const Metrics m = measure(cr, kBoldFont, label);
    const Ink ink = !enabled ? kSlateEdge : (gold ? kSlateGold : kSlateInk);
    if (key.empty()) {
        text(cr, kBoldFont, label, x + (w - m.w) / 2.0, y + (h - m.h) / 2.0, ink);
    } else {
        text(cr, kBoldFont, label, x + 12, y + (h - m.h) / 2.0, ink);
        const Metrics k = measure(cr, kTagFont, key);
        text(cr, kTagFont, key, x + w - k.w - 12, y + (h - k.h) / 2.0, enabled ? kSlateDim : kSlateEdge);
    }
}

double dialog_frame(cairo_t *cr, double x, double y, double w, double h, const std::string &title)
{
    cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
    round_rect(cr, x + 4, y + 6, w, h, 10);
    cairo_fill(cr);
    round_rect(cr, x + 0.5, y + 0.5, w - 1, h - 1, 10);
    set(cr, kSlateBand);
    cairo_fill_preserve(cr);
    set(cr, kSlateEdge);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    // Title strip: deep slate with the gold underline of a lit menu title.
    cairo_save(cr);
    round_rect(cr, x + 1, y + 1, w - 2, h - 2, 9);
    cairo_clip(cr);
    fill(cr, x, y, w, 40, hex(0x303D50));
    fill(cr, x, y + 40, w, 2, kSlateGold);
    cairo_restore(cr);
    text(cr, kTitleFont, title, x + 16, y + 8, kSlateInk);
    return y + 54;
}

void progress(cairo_t *cr, double x, double y, double w, double h, double frac, const Ink &k)
{
    frac = std::clamp(frac, 0.0, 1.0);
    round_rect(cr, x, y, w, h, h / 2);
    set(cr, kSlateDeep);
    cairo_fill(cr);
    if (frac > 0) {
        round_rect(cr, x, y, std::max(h, w * frac), h, h / 2);
        set(cr, k);
        cairo_fill(cr);
    }
}

const art::CivColors &civ_colors(int index)
{
    static const art::CivColors colors[] = {
        {rgb(0x3A6CCF), rgb(0x8CB0F0), rgb(0x1E3C80)}, // blue
        {rgb(0xC83A2E), rgb(0xF08070), rgb(0x7E2018)}, // red
        {rgb(0xD8A818), rgb(0xF8E080), rgb(0x806010)}, // yellow
        {rgb(0x3E9A3A), rgb(0x90D880), rgb(0x1E5A1C)}, // green
        {rgb(0x2AA0A8), rgb(0x80E0E8), rgb(0x145A60)}, // teal
        {rgb(0x8A4AC0), rgb(0xC8A0F0), rgb(0x4A2270)}, // purple
        {rgb(0xE07A2A), rgb(0xF8B880), rgb(0x8A4210)}, // orange
        {rgb(0xCFCAC0), rgb(0xFFFFFF), rgb(0x76706A)}, // white
    };
    return colors[((index % 8) + 8) % 8];
}

std::string with_commas(long long n)
{
    std::string s = std::to_string(n < 0 ? -n : n);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) {
        s.insert(static_cast<size_t>(i), ",");
    }
    return n < 0 ? "-" + s : s;
}

} // namespace ui
