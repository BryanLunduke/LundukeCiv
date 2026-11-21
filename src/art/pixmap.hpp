// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

// A plain 32-bit ARGB pixel buffer. All in-game art is painted pixel by
// pixel into these at native size (32x32 per tile) and only then handed to
// Cairo, scaled by a whole number with nearest-neighbour filtering.

#include <cairo.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using Argb = std::uint32_t;

constexpr Argb rgb(std::uint32_t hex) { return 0xFF000000u | (hex & 0xFFFFFFu); }
constexpr Argb kClear = 0;

struct Pixmap {
    int w = 0;
    int h = 0;
    std::vector<Argb> px;

    Pixmap() = default;
    Pixmap(int width, int height, Argb fill = kClear) : w(width), h(height), px(static_cast<size_t>(width) * height, fill) {}

    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    Argb get(int x, int y) const { return inside(x, y) ? px[static_cast<size_t>(y) * w + x] : kClear; }
    void set(int x, int y, Argb c)
    {
        if (inside(x, y)) {
            px[static_cast<size_t>(y) * w + x] = c;
        }
    }
    // Simple alpha blend, used only for shadows.
    void blend(int x, int y, Argb c)
    {
        if (!inside(x, y)) {
            return;
        }
        const unsigned a = c >> 24;
        if (a == 255) {
            set(x, y, c);
            return;
        }
        Argb &d = px[static_cast<size_t>(y) * w + x];
        auto mix = [&](int shift) {
            const unsigned s = (c >> shift) & 255;
            const unsigned t = (d >> shift) & 255;
            return ((s * a + t * (255 - a)) / 255) << shift;
        };
        const unsigned da = d >> 24;
        const unsigned oa = a + da * (255 - a) / 255;
        d = (oa << 24) | mix(16) | mix(8) | mix(0);
    }
    void fill_rect(int x, int y, int rw, int rh, Argb c)
    {
        for (int j = y; j < y + rh; ++j) {
            for (int i = x; i < x + rw; ++i) {
                set(i, j, c);
            }
        }
    }
    // Copy non-transparent pixels.
    void blit(const Pixmap &src, int dx, int dy)
    {
        for (int y = 0; y < src.h; ++y) {
            for (int x = 0; x < src.w; ++x) {
                const Argb c = src.px[static_cast<size_t>(y) * src.w + x];
                if ((c >> 24) == 255) {
                    set(dx + x, dy + y, c);
                } else if ((c >> 24) != 0) {
                    blend(dx + x, dy + y, c);
                }
            }
        }
    }
    Pixmap crop(int x0, int y0, int cw, int ch) const
    {
        Pixmap out(cw, ch);
        for (int y = 0; y < ch; ++y) {
            for (int x = 0; x < cw; ++x) {
                out.px[static_cast<size_t>(y) * cw + x] = get(x0 + x, y0 + y);
            }
        }
        return out;
    }
};

// Cairo wants premultiplied ARGB.
inline cairo_surface_t *to_surface(const Pixmap &pm)
{
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pm.w, pm.h);
    cairo_surface_flush(s);
    unsigned char *data = cairo_image_surface_get_data(s);
    const int stride = cairo_image_surface_get_stride(s);
    for (int y = 0; y < pm.h; ++y) {
        auto *row = reinterpret_cast<std::uint32_t *>(data + static_cast<size_t>(y) * stride);
        for (int x = 0; x < pm.w; ++x) {
            const Argb c = pm.px[static_cast<size_t>(y) * pm.w + x];
            const unsigned a = c >> 24;
            if (a == 255 || a == 0) {
                row[x] = a == 0 ? 0 : c;
            } else {
                const unsigned r = ((c >> 16) & 255) * a / 255;
                const unsigned g = ((c >> 8) & 255) * a / 255;
                const unsigned b = (c & 255) * a / 255;
                row[x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
    cairo_surface_mark_dirty(s);
    return s;
}

// Paint a pixmap at an integer scale with crisp pixels.
inline void paint_pixmap(cairo_t *cr, const Pixmap &pm, double x, double y, int scale)
{
    cairo_surface_t *s = to_surface(pm);
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, s, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_surface_destroy(s);
}

inline std::uint32_t hash3(int x, int y, int seed)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u +
                      static_cast<std::uint32_t>(seed) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
inline double hashf(int x, int y, int seed) { return (hash3(x, y, seed) & 0xFFFFFF) / 16777216.0; }

// 4x4 Bayer threshold in [0,1).
inline double bayer4(int x, int y)
{
    static const int m[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    return (m[y & 3][x & 3] + 0.5) / 16.0;
}
