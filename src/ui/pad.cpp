// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Gamepad play, after Lunduke City 2's approach: the D-pad (or left stick)
// moves a tile cursor on the map and a focus between buttons in dialogs;
// A acts, B backs out, X opens the unit's orders, Y is the unit's own
// action, the bumpers cycle units, View ends the turn and Menu opens the
// menus. The bottom bar names the buttons of the pad in hand.

#include "app.hpp"

#include <algorithm>
#include <cmath>

using namespace civ;

namespace ui {

void App::set_pad_glyphs(const pad::Glyphs &glyphs, bool deck)
{
    glyphs_ = glyphs;
    deck_ = deck;
}

void App::pad_unplugged()
{
    pad::repeat_release(repeat_);
    stick_dx_ = stick_dy_ = 0;
    pan_x_ = pan_y_ = 0;
    pad_nav_ = false;
}

void App::pointer_used()
{
    pad_nav_ = false;
}

bool App::pad_busy() const
{
    return repeat_.dx || repeat_.dy || std::fabs(pan_x_) > 0 || std::fabs(pan_y_) > 0;
}

void App::snap_cursor()
{
    if (!game_) return;
    if (const Unit *u = g().unit(active_)) {
        cursor_x_ = u->x;
        cursor_y_ = u->y;
    } else if (cursor_x_ < 0) {
        if (const City *c = g().capital(human())) {
            cursor_x_ = c->x;
            cursor_y_ = c->y;
        } else {
            cursor_x_ = view_x_ + view_cols() / 2;
            cursor_y_ = view_y_ + view_rows() / 2;
        }
    }
    cursor_unit_ = active_;
}

void App::select_prev()
{
    if (!game_) return;
    std::vector<int> ids;
    for (int id = g().next_unit_for(human(), -1); id >= 0 && std::find(ids.begin(), ids.end(), id) == ids.end();
         id = g().next_unit_for(human(), id))
        ids.push_back(id);
    if (ids.empty()) return;
    auto it = std::find(ids.begin(), ids.end(), active_);
    active_ = it == ids.end() || it == ids.begin() ? ids.back() : *(it - 1);
    end_of_turn_prompt_ = false;
    goto_mode_ = false;
    if (const Unit *u = g().unit(active_)) keep_in_view(u->x, u->y);
}

// Clickable areas a dialog focus can rest on: not the big panels that only
// swallow clicks.
std::vector<int> App::focus_hots() const
{
    std::vector<int> out;
    const double big = 0.25 * geo_.win_w * geo_.win_h;
    for (int i = 0; i < static_cast<int>(hots_.size()); ++i) {
        const Hot &h = hots_[i];
        if (h.w < 6 || h.h < 6 || h.w * h.h > big) continue;
        out.push_back(i);
    }
    return out;
}

void App::resolve_focus()
{
    focus_hot_ = -1;
    const std::vector<int> ids = focus_hots();
    if (ids.empty()) return;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        if (hots_[*it].contains(focus_cx_, focus_cy_)) {
            focus_hot_ = *it;
            return;
        }
    }
    std::vector<pad::Rect> rects;
    for (int i : ids) rects.push_back({hots_[i].x, hots_[i].y, hots_[i].w, hots_[i].h});
    const int d = pad::default_focus(rects);
    if (d >= 0) {
        focus_hot_ = ids[d];
        focus_cx_ = rects[d].cx();
        focus_cy_ = rects[d].cy();
    }
}

void App::pad_menu_move(int dir)
{
    const std::vector<MenuItem> items = menu_items(menu_open_);
    const int n = static_cast<int>(items.size());
    if (n == 0) return;
    for (int k = 0; k < n; ++k) {
        menu_item_ = ((menu_item_ + dir) % n + n) % n;
        if (items[menu_item_].enabled) return;
    }
}

void App::pad_direction(int dx, int dy)
{
    if (menu_open_ >= 0 && dialogs_.empty()) {
        if (dy) {
            pad_menu_move(dy);
        } else if (dx) {
            menu_open_ = (menu_open_ + dx + 5) % 5;
            menu_item_ = -1;
            pad_menu_move(1);
        }
        return;
    }
    if (!dialogs_.empty()) {
        // Move the focus between the dialog's buttons and rows.
        const std::vector<int> ids = focus_hots();
        std::vector<pad::Rect> rects;
        int from = -1;
        for (int i : ids) {
            if (i == focus_hot_) from = static_cast<int>(rects.size());
            rects.push_back({hots_[i].x, hots_[i].y, hots_[i].w, hots_[i].h});
        }
        // Diagonals: try the vertical part first, then the horizontal.
        int next = dy ? pad::next_focus(rects, from, 0, dy) : -1;
        if (next < 0 && dx) next = pad::next_focus(rects, from, dx, 0);
        if (next >= 0) {
            focus_cx_ = rects[next].cx();
            focus_cy_ = rects[next].cy();
        }
        return;
    }
    if (!game_) return;
    if (cursor_x_ < 0 || cursor_unit_ != active_) snap_cursor();
    cursor_x_ = g().map.wrap_x(cursor_x_ + dx);
    cursor_y_ = std::clamp(cursor_y_ + dy, 0, g().map.h - 1);
    keep_in_view(cursor_x_, cursor_y_);
}

// A: act on whatever the cursor (or focus) is on.
void App::pad_confirm()
{
    if (menu_open_ >= 0 && dialogs_.empty()) {
        std::vector<MenuItem> items = menu_items(menu_open_);
        if (menu_item_ >= 0 && menu_item_ < static_cast<int>(items.size()) && items[menu_item_].enabled) {
            auto fn = items[menu_item_].fn;
            menu_open_ = -1;
            fn();
        }
        return;
    }
    if (!dialogs_.empty()) {
        if (focus_hot_ >= 0 && focus_hot_ < static_cast<int>(hots_.size())) {
            auto fn = hots_[focus_hot_].fn;
            fn();
        } else {
            KeyEvent k;
            k.key = Key::Enter;
            key(k);
        }
        return;
    }
    if (!game_) return;
    if (cursor_x_ < 0 || cursor_unit_ != active_) snap_cursor();
    if (goto_mode_) {
        click_map(cursor_x_, cursor_y_, 1);
        return;
    }
    const Unit *u = g().unit(active_);
    if (u) {
        const int ddx = g().map.dx(u->x, cursor_x_), ddy = cursor_y_ - u->y;
        if (!ddx && !ddy) {
            // On the active unit: its orders.
            menu_open_ = 1;
            menu_item_ = -1;
            pad_menu_move(1);
            return;
        }
        if (std::abs(ddx) <= 1 && std::abs(ddy) <= 1) {
            const int id = active_;
            try_move(ddx, ddy);
            // The cursor rides along with the unit it just moved.
            if (const Unit *v = g().unit(id); v && id == active_) {
                cursor_x_ = v->x;
                cursor_y_ = v->y;
            }
            return;
        }
    }
    if (const City *c = g().city_at(cursor_x_, cursor_y_); c && c->owner == human()) {
        open_city(c->id);
        return;
    }
    for (int id : g().units_at(cursor_x_, cursor_y_)) {
        if (g().unit(id)->owner == human()) {
            click_map(cursor_x_, cursor_y_, 1);   // select (cycles a stack)
            cursor_unit_ = active_;
            return;
        }
    }
    if (u) {
        // Further away: send the unit there.
        goto_mode_ = true;
        click_map(cursor_x_, cursor_y_, 1);
    }
}

bool App::pad_button(PadButton b, bool down, unsigned long long ms)
{
    if (!down) {
        const bool dir = b == PadButton::Up || b == PadButton::Down || b == PadButton::Left || b == PadButton::Right;
        if (dir) {
            pad::repeat_release(repeat_);
            if (stick_dx_ || stick_dy_) pad::repeat_press(repeat_, stick_dx_, stick_dy_, static_cast<long long>(ms));
        }
        return false;
    }
    pad_nav_ = true;
    int dx = 0, dy = 0;
    switch (b) {
    case PadButton::Up: dy = -1; break;
    case PadButton::Down: dy = 1; break;
    case PadButton::Left: dx = -1; break;
    case PadButton::Right: dx = 1; break;
    default: break;
    }
    if (dx || dy) {
        pad_direction(dx, dy);
        pad::repeat_press(repeat_, dx, dy, static_cast<long long>(ms));
        return true;
    }
    auto send = [this](Key key, char ch = 0) {
        KeyEvent k;
        k.key = key;
        k.ch = ch;
        this->key(k);
    };
    const bool map = game_ && dialogs_.empty() && menu_open_ < 0;
    switch (b) {
    case PadButton::A: pad_confirm(); break;
    case PadButton::B:
        if (menu_open_ >= 0 && dialogs_.empty()) {
            menu_open_ = -1;
        } else if (!dialogs_.empty()) {
            send(Key::Escape);
        } else if (goto_mode_) {
            goto_mode_ = false;
        } else if (game_) {
            // Back to the active unit; on it already, B makes it wait.
            const Unit *u = g().unit(active_);
            if (u && (cursor_x_ != u->x || cursor_y_ != u->y)) {
                snap_cursor();
                keep_in_view(cursor_x_, cursor_y_);
            } else if (u) {
                do_order('w');
                snap_cursor();
            }
        }
        break;
    case PadButton::X:
        if (map) {
            menu_open_ = 1;
            menu_item_ = -1;
            pad_menu_move(1);
        }
        break;
    case PadButton::Y:
        if (map) {
            if (const Unit *u = g().unit(active_)) {
                do_order((unit_info(u->type).flags & kSettlers) ? 'b' : 'f');
                snap_cursor();
            }
        } else if (!dialogs_.empty()) {
            send(Key::Enter);
        }
        break;
    case PadButton::LB:
    case PadButton::RB:
        if (map) {
            if (b == PadButton::RB)
                select_next(true);
            else
                select_prev();
            snap_cursor();
        }
        break;
    case PadButton::View:
        if (map) {
            end_turn();
            snap_cursor();
        }
        break;
    case PadButton::Menu:
        if (!dialogs_.empty()) {
            send(Key::Enter);
        } else if (menu_open_ >= 0) {
            menu_open_ = -1;
        } else {
            menu_open_ = 0;
            menu_item_ = -1;
            pad_menu_move(1);
        }
        break;
    case PadButton::L3:
        if (map && g().unit(active_)) {
            snap_cursor();
            center_on(cursor_x_, cursor_y_);
        }
        break;
    case PadButton::R3:
        if (map && cursor_x_ >= 0) center_on(cursor_x_, cursor_y_);
        break;
    case PadButton::LT:
    case PadButton::RT:
        if (map) {
            zoom_ = std::clamp(zoom_ + (b == PadButton::RT ? 1 : -1), 1, 3);
            if (cursor_x_ >= 0) center_on(cursor_x_, cursor_y_);
        }
        break;
    default: break;
    }
    return true;
}

bool App::pad_sticks(float lx, float ly, float rx, float ry, unsigned long long ms)
{
    int dx = 0, dy = 0;
    pad::stick_direction(lx, ly, dx, dy);
    bool changed = false;
    if (dx != stick_dx_ || dy != stick_dy_) {
        stick_dx_ = dx;
        stick_dy_ = dy;
        if (dx || dy) {
            pad_nav_ = true;
            pad_direction(dx, dy);
            pad::repeat_press(repeat_, dx, dy, static_cast<long long>(ms));
        } else {
            pad::repeat_release(repeat_);
        }
        changed = true;
    }
    const float dead = pad::kStickDead;
    pan_x_ = std::fabs(rx) > dead ? rx : 0;
    pan_y_ = std::fabs(ry) > dead ? ry : 0;
    if (pan_x_ || pan_y_) pad_nav_ = true;
    return changed;
}

std::vector<std::pair<std::string, std::string>> App::map_pad_hints()
{
    std::vector<std::pair<std::string, std::string>> h;
    const Unit *u = game_ ? g().unit(active_) : nullptr;
    if (u) {
        const bool on_unit = cursor_x_ == u->x && cursor_y_ == u->y;
        const int ddx = g().map.dx(u->x, cursor_x_), ddy = cursor_y_ - u->y;
        const bool next_to = std::abs(ddx) <= 1 && std::abs(ddy) <= 1 && !on_unit;
        h.push_back({"D-pad", "cursor"});
        h.push_back({glyphs_.a, on_unit ? "orders" : (next_to ? "move" : "go to")});
        h.push_back({glyphs_.y, (unit_info(u->type).flags & kSettlers) ? "build city" : "fortify"});
        h.push_back({glyphs_.lb + "/" + glyphs_.rb, "units"});
    } else {
        h.push_back({"D-pad", "cursor"});
        h.push_back({glyphs_.a, "open city"});
    }
    h.push_back({glyphs_.view, "end turn"});
    h.push_back({glyphs_.menu, "menu"});
    return h;
}

// Key-cap pills right to left from `right`; returns the left edge used.
double App::draw_pad_hints(cairo_t *cr, double right, double y, const std::vector<std::pair<std::string, std::string>> &hints)
{
    double x = right;
    for (auto it = hints.rbegin(); it != hints.rend(); ++it) {
        const Metrics wm = measure(cr, kUiFont, it->second);
        const Metrics gm = measure(cr, kSmallBold, it->first);
        x -= wm.w;
        text(cr, kUiFont, it->second, x, y, kSlateDim);
        const double pw = std::max(22.0, gm.w + 12.0), ph = 20;
        x -= 6 + pw;
        round_rect(cr, x, y - 1, pw, ph, pw <= 24 ? ph / 2 : 5);
        set(cr, hex(0xCFD6E0));
        cairo_fill(cr);
        text(cr, kSmallBold, it->first, x + (pw - gm.w) / 2, y - 1 + (ph - gm.h) / 2, kSlateDeep);
        x -= 16;
    }
    return x;
}

// The tile cursor, the menu highlight and the dialog focus ring.
void App::draw_pad_overlay(cairo_t *cr)
{
    // The keyboard focus belongs to one dialog; a new one starts without.
    if (key_nav_ && (dialogs_.size() != key_nav_depth_ || dialogs_.back().kind != key_nav_kind_)) key_nav_ = false;
    const bool pad = pad_hints() && game_;
    if (!pad && !(key_nav_ && !dialogs_.empty())) return;
    if (pad && dialogs_.empty() && menu_open_ < 0) {
        if (cursor_x_ < 0 || cursor_unit_ != active_) snap_cursor();
        const int t = tile_px();
        const int cols = geo_.map_w / t + 1;
        const int tx = g().map.wrap_x(cursor_x_ - view_x_), ty = cursor_y_ - view_y_;
        if (tx < cols && ty >= 0 && ty * t < geo_.map_h) {
            const double x = geo_.map_x + tx * t, y = geo_.map_y + ty * t;
            cairo_save(cr);
            cairo_rectangle(cr, geo_.map_x, geo_.map_y, geo_.map_w, geo_.map_h);
            cairo_clip(cr);
            cairo_set_line_width(cr, 3);
            set(cr, hex(0x000000), 0.55);
            cairo_rectangle(cr, x + 2.5, y + 2.5, t - 3, t - 3);
            cairo_stroke(cr);
            cairo_set_line_width(cr, 2);
            set(cr, blink_ ? hex(0xFFFFFF) : hex(0xF2C14E));
            cairo_rectangle(cr, x + 1, y + 1, t - 2, t - 2);
            cairo_stroke(cr);
            cairo_restore(cr);
        }
    }
    if (!dialogs_.empty()) {
        resolve_focus();
        if (focus_hot_ >= 0) {
            const Hot &h = hots_[focus_hot_];
            cairo_set_line_width(cr, 2.5);
            round_rect(cr, h.x - 3, h.y - 3, h.w + 6, h.h + 6, 7);
            set(cr, kSlateGold);
            cairo_stroke(cr);
        }
        if (!pad) return;
        // Dialog hints along the bottom, over the dimmed status bar.
        const double y = geo_.win_h - geo_.status_h;
        fill(cr, 0, y, geo_.win_w, geo_.status_h, kSlateDeep);
        fill(cr, 0, y, geo_.win_w, 1, kSlateEdge);
        draw_pad_hints(cr, geo_.win_w - 12, y + 8,
                       {{"D-pad", "moves"}, {glyphs_.a, "chooses"}, {glyphs_.b, "closes"}, {glyphs_.menu, "OK"}});
    }
}

} // namespace ui
