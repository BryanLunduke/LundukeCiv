// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Dialogs: new game, city screen, production, science, rates, revolution,
// saves, advisors, the unit Civilopedia, combat and news reports.

#include "app.hpp"

#include <algorithm>
#include <cmath>

namespace ui {

using namespace civ;

namespace {

const std::vector<std::string> &citizen_rows(int kind)
{
    static const std::vector<std::string> happy = {
        ".KKKKK.", "KhhhhhK", "KsKsKsK", "KsssssK", "KsKKKsK", ".KsssK.", "KyyyyyK", "KyYyYyK", "KyyyyyK", "KKK.KKK",
    };
    static const std::vector<std::string> content = {
        ".KKKKK.", "KhhhhhK", "KsKsKsK", "KsssssK", "KsssssK", ".KKKKK.", "KgggggK", "KgGgGgK", "KgggggK", "KKK.KKK",
    };
    static const std::vector<std::string> unhappy = {
        ".KKKKK.", "KhhhhhK", "KsKsKsK", "KsssssK", "KsKKKsK", ".KsssK.", "KrrrrrK", "KrRrRrK", "KrrrrrK", "KKK.KKK",
    };
    static const std::vector<std::string> entertainer = {
        "K.KKK.K", "KpKKKpK", "KsKsKsK", "KsssssK", "KsKKKsK", ".KsssK.", "KpppppK", "KpPpPpK", "KpppppK", "KKK.KKK",
    };
    switch (kind) {
    case 0: return happy;
    case 1: return content;
    case 2: return unhappy;
    default: return entertainer;
    }
}

std::string turns_text(int n)
{
    return n < 0 ? "never" : (std::to_string(n) + (n == 1 ? " turn" : " turns"));
}

std::string enables_text(Advance a)
{
    std::vector<std::string> out;
    for (int u = 0; u < kUnitTypeCount; ++u) {
        if (unit_info(u).tech == a) out.push_back(unit_info(u).name);
    }
    for (int b = 0; b < kBuildingCount; ++b) {
        if (building_info(b).tech == a) out.push_back(building_info(b).name);
    }
    for (int gv = 0; gv < static_cast<int>(Government::Count); ++gv) {
        if (government_info(static_cast<Government>(gv)).tech == a) out.push_back(government_info(static_cast<Government>(gv)).name);
    }
    std::string s;
    for (size_t i = 0; i < out.size(); ++i) s += (i ? ", " : "") + out[i];
    return s;
}

} // namespace

void App::draw_dialog(cairo_t *cr, Dialog &d)
{
    switch (d.kind) {
    case DialogKind::NewGame: draw_new_game(cr, d); break;
    case DialogKind::Message:
    case DialogKind::Report: draw_message(cr, d); break;
    case DialogKind::Combat: draw_combat(cr, d); break;
    case DialogKind::City: draw_city(cr, d); break;
    case DialogKind::Production: draw_production(cr, d); break;
    case DialogKind::Science: draw_science(cr, d); break;
    case DialogKind::Rates: draw_rates(cr, d); break;
    case DialogKind::Revolution: draw_revolution(cr, d); break;
    case DialogKind::SaveLoad: draw_saveload(cr, d); break;
    case DialogKind::CityName: draw_city_name(cr, d); break;
    case DialogKind::Cities: draw_cities(cr, d); break;
    case DialogKind::Foreign: draw_foreign(cr, d); break;
    case DialogKind::UnitSheet: draw_unit_sheet(cr, d); break;
    case DialogKind::ConfirmWar: draw_confirm_war(cr, d); break;
    case DialogKind::GameOver: draw_game_over(cr, d); break;
    case DialogKind::Help: draw_help(cr, d); break;
    default: break;
    }
}

// A text field: deep well, gold ring when focused, a caret.
static void field(cairo_t *cr, double x, double y, double w, const std::string &value, bool focus, bool blink)
{
    const double h = 34;
    round_rect(cr, x + 0.5, y + 0.5, w - 1, h - 1, 5);
    set(cr, kSlateDeep);
    cairo_fill_preserve(cr);
    set(cr, focus ? kSlateGold : kSlateEdge);
    cairo_set_line_width(cr, focus ? 2 : 1);
    cairo_stroke(cr);
    text(cr, kUiFont, value, x + 10, y + 7, kSlateInk);
    if (focus && blink) {
        const double cx = x + 11 + measure(cr, kUiFont, value).w;
        fill(cr, cx, y + 8, 2, h - 16, kSlateGold);
    }
}

void App::draw_new_game(cairo_t *cr, Dialog &d)
{
    const double w = 640, h = 516, x = (geo_.win_w - w) / 2, y = game_ ? (geo_.win_h - h) / 2 : std::max(90.0, (geo_.win_h - 152 - h) / 2 + 40);
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, R = x + w - 24;
    section(cr, "DIFFICULTY", L, cy, R);
    cy += 20;
    for (int i = 0; i < static_cast<int>(Difficulty::Count); ++i) {
        const double bw = (R - L - 4 * 8) / 5, bx = L + i * (bw + 8);
        const bool chosen = static_cast<int>(settings.difficulty) == i;
        wide_button(cr, bx, cy, bw, 34, difficulty_name(static_cast<Difficulty>(i)), "", chosen, hover(bx, cy, bw, 34));
        hot(bx, cy, bw, 34, [this, i] { settings.difficulty = static_cast<Difficulty>(i); });
    }
    cy += 44;
    const std::string level_note = "Content citizens per city: " + std::to_string(content_base(settings.difficulty)) +
                                   ". Research costs rise faster on harder levels.";
    text(cr, kUiFont, level_note, L, cy, kSlateDim, 1.0, static_cast<int>(R - L));
    cy += std::max(34.0, static_cast<double>(measure(cr, kUiFont, level_note, static_cast<int>(R - L)).h) + 12);
    section(cr, "CIVILIZATIONS IN THE WORLD", L, cy, R);
    cy += 20;
    for (int n = 2; n <= 7; ++n) {
        const double bw = 64, bx = L + (n - 2) * (bw + 8);
        wide_button(cr, bx, cy, bw, 34, std::to_string(n), "", settings.civs == n, hover(bx, cy, bw, 34));
        hot(bx, cy, bw, 34, [this, n] { settings.civs = n; });
    }
    cy += 50;
    section(cr, "YOUR PEOPLE", L, cy, R);
    cy += 20;
    const double half = (R - L - 16) / 2;
    text(cr, kTagFont, "NAME OF YOUR PEOPLE", L, cy, kSlateDim);
    text(cr, kTagFont, "YOUR NAME AS LEADER", L + half + 16, cy, kSlateDim);
    cy += 16;
    field(cr, L, cy, half, settings.player_name, new_game_field_ == 0, blink_);
    hot(L, cy, half, 34, [this] { new_game_field_ = 0; });
    field(cr, L + half + 16, cy, half, settings.player_leader, new_game_field_ == 1, blink_);
    hot(L + half + 16, cy, half, 34, [this] { new_game_field_ = 1; });
    cy += 48;
    text(cr, kTagFont, "COLOR", L, cy, kSlateDim);
    text(cr, kTagFont, "WORLD SEED", L + half + 16, cy, kSlateDim);
    cy += 16;
    for (int c = 0; c < 8; ++c) {
        const double sx = L + c * 34;
        round_rect(cr, sx, cy + 3, 28, 28, 5);
        set_argb(cr, civ_colors(c).main);
        cairo_fill_preserve(cr);
        set(cr, settings.player_color == c ? kSlateGold : kSlateDeep);
        cairo_set_line_width(cr, settings.player_color == c ? 3 : 1);
        cairo_stroke(cr);
        hot(sx, cy + 3, 28, 28, [this, c] { settings.player_color = c; });
    }
    field(cr, L + half + 16, cy, half, seed_text_, new_game_field_ == 2, blink_);
    hot(L + half + 16, cy, half, 34, [this] { new_game_field_ = 2; });
    cy += 52;
    text(cr, kUiFont, "Tab moves between fields. The other peoples are drawn from history.", L, cy, kSlateDim, 1.0,
         static_cast<int>(R - L));
    const double by = y + h - 56;
    const bool can_cancel = game_ != nullptr;
    wide_button(cr, R - 180, by, 180, 38, "Start", "Enter", true, hover(R - 180, by, 180, 38));
    hot(R - 180, by, 180, 38, [this] {
        KeyEvent k;
        k.key = Key::Enter;
        key(k);
    });
    wide_button(cr, R - 372, by, 180, 38, can_cancel ? "Cancel" : "Quit", "Esc", false, hover(R - 372, by, 180, 38));
    hot(R - 372, by, 180, 38, [this] { close_top(); });
    wide_button(cr, L, by, 196, 38, "Load Game", "Ctrl+L", false, hover(L, by, 196, 38));
    hot(L, by, 196, 38, [this] {
        Dialog l;
        l.kind = DialogKind::SaveLoad;
        l.title = "Load Game";
        open(l);
    });
}

void App::draw_message(cairo_t *cr, Dialog &d)
{
    const double w = 620;
    double body = 0;
    for (const std::string &l : d.lines) body += measure(cr, kUiFont, l, static_cast<int>(w - 48)).h + 8;
    const double h = std::min<double>(geo_.win_h - 60, 54 + body + 80);
    const double x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    for (const std::string &l : d.lines) {
        if (cy > y + h - 80) break;
        fill(cr, x + 24, cy + 8, 5, 5, kSlateGoldDim);
        text(cr, kUiFont, l, x + 38, cy, kSlateInk, 1.0, static_cast<int>(w - 62));
        cy += measure(cr, kUiFont, l, static_cast<int>(w - 62)).h + 8;
    }
    const double bx = x + w - 24 - 140, by = y + h - 56;
    wide_button(cr, bx, by, 140, 38, "OK", "Enter", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

void App::draw_combat(cairo_t *cr, Dialog &d)
{
    const CombatResult &r = g().last_combat;
    const double w = 660, h = 420, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double colw = (w - 48) / 2;
    for (int side = 0; side < 2; ++side) {
        const double cx = x + 24 + side * colw;
        const int owner = side == 0 ? r.att_owner : r.def_owner;
        const UnitType t = side == 0 ? r.att_type : r.def_type;
        const bool lost = side == 0 ? !r.attacker_won : r.attacker_won;
        text(cr, kTagFont, side == 0 ? "ATTACKER" : "DEFENDER", cx, cy, kSlateDim);
        round_rect(cr, cx, cy + 18, 112, 112, 8);
        set(cr, kSlateDeep);
        cairo_fill(cr);
        paint_pixmap(cr, art::unit_card(t, civ_colors(g().civs[owner].color)), cx + 8, cy + 26, 3);
        if (lost) {
            cairo_set_line_width(cr, 6);
            set(cr, kSlateBad, 0.85);
            cairo_move_to(cr, cx + 14, cy + 32);
            cairo_line_to(cr, cx + 98, cy + 116);
            cairo_move_to(cr, cx + 98, cy + 32);
            cairo_line_to(cr, cx + 14, cy + 116);
            cairo_stroke(cr);
        }
        text(cr, kTagFont, g().civs[owner].name, cx + 124, cy + 20, kSlateDim);
        text(cr, kBoldFont, unit_info(t).name, cx + 124, cy + 34, kSlateInk);
        char buf[64];
        std::snprintf(buf, sizeof buf, side == 0 ? "Attack %.1f" : "Defense %.1f", side == 0 ? r.att : r.def);
        text(cr, kUiFont, buf, cx + 124, cy + 58, kSlateDim);
        text(cr, kBoldFont, lost ? "Destroyed" : "Victorious", cx + 124, cy + 82, lost ? kSlateBad : kSlateGood);
        // Hit points: before (dim) and after (bright).
        const int hp0 = side == 0 ? r.att_hp0 : r.def_hp0, hp1 = side == 0 ? r.att_hp : r.def_hp;
        const double bx = cx + 124, by = cy + 108, bw = colw - 140;
        fill(cr, bx, by, bw, 8, kSlateDeep);
        fill(cr, bx, by, bw * hp0 / kUnitHp, 8, kSlateEdge);
        fill(cr, bx, by, bw * hp1 / kUnitHp, 8, lost ? kSlateBad : kSlateGood);
        text(cr, kTagFont, "HP " + std::to_string(hp1) + " / " + std::to_string(kUnitHp) + "  (was " + std::to_string(hp0) + ")",
             bx, by + 12, kSlateDim);
    }
    cy += 150;
    // The fight, round by round: one pip per round, gold where the attacker
    // scored, red where the defender did.
    section(cr, "ROUNDS (" + std::to_string(r.rounds.size()) + ")", x + 24, cy, x + w - 24);
    cy += 20;
    {
        const double pw = 12, gap = 3, avail = w - 48;
        const int per_row = static_cast<int>((avail + gap) / (pw + gap));
        for (size_t i = 0; i < r.rounds.size(); ++i) {
            const int row = static_cast<int>(i) / per_row, col = static_cast<int>(i) % per_row;
            const double px = x + 24 + col * (pw + gap), py = cy + row * (pw + gap);
            round_rect(cr, px, py, pw, pw, 3);
            set(cr, r.rounds[i] ? kSlateGold : kSlateBad);
            cairo_fill(cr);
        }
        const int rows = r.rounds.empty() ? 1 : (static_cast<int>(r.rounds.size()) + per_row - 1) / per_row;
        cy += rows * (pw + gap) + 10;
    }
    char pct[16];
    if (r.odds > 0.995 && r.odds < 1.0) {
        std::snprintf(pct, sizeof pct, ">99%%");
    } else if (r.odds < 0.005 && r.odds > 0.0) {
        std::snprintf(pct, sizeof pct, "<1%%");
    } else {
        std::snprintf(pct, sizeof pct, "%.0f%%", r.odds * 100);
    }
    char odds[160];
    std::snprintf(odds, sizeof odds, "Attacker's chance: %s to win  \u00b7  %.0f%% each round", pct, r.round_odds * 100);
    text(cr, kBoldFont, odds, x + 24, cy, kSlateGold);
    cy += 26;
    std::string extra;
    if (r.attacker_won && r.killed > 1) extra = "The whole stack of " + std::to_string(r.killed) + " units was lost. ";
    if (r.city_shrank) extra += "The city shrinks. ";
    extra += "Each round the loser loses one hit point; strength counts terrain, fortification, walls and veterans.";
    text(cr, kUiFont, extra, x + 24, cy, kSlateDim, 1.0, static_cast<int>(w - 48));
    const double bx = x + w - 24 - 140, by = y + h - 56;
    wide_button(cr, bx, by, 140, 38, "OK", "Enter", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

void App::draw_city(cairo_t *cr, Dialog &d)
{
    City *cp = g().city(d.city_id);
    if (!cp || cp->owner != human()) {
        close_top();
        return;
    }
    City &c = *cp;
    g().update_city(c);
    const CityStats &s = c.stats;
    const double w = std::min(1040, geo_.win_w - 20), h = std::min(700, geo_.win_h - 20);
    const double x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    const std::string title = c.name + "  \u00b7  size " + std::to_string(c.size) + "  \u00b7  " +
                              with_commas(static_cast<long long>(c.size) * (c.size + 1) / 2 * 10000) + " people";
    double cy = dialog_frame(cr, x, y, w, h, title);
    text_right(cr, kUiFont, year_text(year_for_turn(g().turn)) + "  \u00b7  " + government_info(g().civs[c.owner].govt).name,
               x + w - 20, y + 12, kSlateDim);
    const double L = x + 20;

    // --- left column: citizens, resources, happiness
    const double lw = 330;
    section(cr, "CITIZENS", L, cy, L + lw);
    {
        double px = L;
        const double py = cy + 18;
        auto face = [&](int kind, int n) {
            for (int i = 0; i < n; ++i) {
                paint_pixmap(cr, art::sprite(citizen_rows(kind)), px, py, 3);
                px += n > 12 ? 18 : 24;
            }
        };
        const int workers = c.size - c.entertainers;
        const int happy = std::min(s.happy, workers), unhappy = std::min(s.unhappy, workers - happy);
        face(0, happy);
        face(1, std::max(0, workers - happy - unhappy));
        face(2, unhappy);
        px += 8;
        face(3, c.entertainers);
        cy += 54;
        std::string mood = std::to_string(s.happy) + " happy \u00b7 " + std::to_string(s.content) + " content \u00b7 " +
                           std::to_string(s.unhappy) + " unhappy";
        if (c.entertainers) mood += " \u00b7 " + std::to_string(c.entertainers) + " entertain";
        text(cr, kUiFont, mood, L, cy, kSlateDim);
        cy += 20;
        if (s.disorder) {
            text(cr, kBoldFont, "CIVIL DISORDER: no production or taxes!", L, cy, kSlateBad);
            cy += 20;
        }
    }
    cy += 8;
    section(cr, "RESOURCES", L, cy, L + lw);
    cy += 20;
    {
        const int vals[3] = {s.raw.food, s.raw.shields, s.raw.trade};
        const char *names[3] = {"FOOD", "SHIELDS", "TRADE"};
        for (int k = 0; k < 3; ++k) {
            text(cr, kTagFont, names[k], L, cy + 3, kSlateDim);
            const int n = vals[k];
            const double step = n > 14 ? (lw - 120) / n : 15;
            for (int i = 0; i < n; ++i) paint_pixmap(cr, art::yield_icon(k), L + 64 + i * step, cy, 2);
            std::string right;
            if (k == 0) right = (s.food_surplus >= 0 ? "+" : "") + std::to_string(s.food_surplus);
            if (k == 1) right = (s.shield_surplus >= 0 ? "+" : "") + std::to_string(s.shield_surplus);
            if (k == 2) right = s.corruption ? std::to_string(s.corruption) + " corrupt" : "no corruption";
            text_right(cr, kSmallBold, right, L + lw, cy + 1,
                       (k == 0 && s.food_surplus < 0) || (k == 1 && s.shield_surplus < 0) ? kSlateBad : kSlateInk);
            cy += 24;
        }
        text(cr, kUiFont, "Tax " + std::to_string(s.tax) + "  \u00b7  Lux " + std::to_string(s.lux) + "  \u00b7  Sci " +
                              std::to_string(s.sci), L, cy, kSlateInk);
        cy += 20;
        text(cr, kUiFont, "Upkeep: " + std::to_string(s.shield_upkeep) + " shields, " + std::to_string(s.food_upkeep) + " food",
             L, cy, kSlateDim);
        cy += 28;
    }
    section(cr, "FOOD STORAGE", L, cy, L + lw);
    cy += 20;
    {
        const int box = g().food_box(c);
        progress(cr, L, cy, lw, 10, static_cast<double>(c.food) / box, kSlateGood);
        cy += 16;
        std::string grow;
        if (s.food_surplus > 0) {
            const int t = (box - c.food + s.food_surplus - 1) / s.food_surplus;
            grow = c.size >= g().size_cap(c) ? "Needs an Aqueduct to grow" : "Grows in " + turns_text(t);
        } else if (s.food_surplus < 0) {
            grow = "Famine in " + turns_text(c.food / -s.food_surplus + 1);
        } else {
            grow = "Not growing";
        }
        text(cr, kUiFont, std::to_string(c.food) + " / " + std::to_string(box) + "  \u00b7  " + grow, L, cy,
             s.food_surplus < 0 ? kSlateBad : kSlateDim);
        cy += 30;
    }
    section(cr, "UNITS IN THE CITY", L, cy, L + lw);
    cy += 20;
    {
        double ux = L;
        int shown = 0;
        for (const Unit &u : g().units) {
            if (u.x != c.x || u.y != c.y || u.owner != human()) continue;
            if (shown++ >= 9) break;
            paint_pixmap(cr, art::unit_card(u.type, civ_colors(g().civs[u.owner].color), 0, u.veteran), ux, cy, 1);
            ux += 36;
        }
        if (!shown) text(cr, kUiFont, "None \u2014 the city is undefended.", L, cy + 6, kSlateBad);
        cy += 40;
        int supported = 0;
        for (const Unit &u : g().units) supported += u.home == c.id;
        text(cr, kUiFont, std::to_string(supported) + " unit" + (supported == 1 ? "" : "s") + " supported by this city", L, cy, kSlateDim);
    }

    // --- middle: the city's land
    const double mx = L + lw + 24, my = y + 54;
    section(cr, "CITY RADIUS", mx, my, mx + 320);
    {
        const int T = art::kTile;
        Pixmap pm(5 * T, 5 * T, rgb(0x000000));
        const Map &m = g().map;
        for (int j = 0; j < 5; ++j) {
            const int wy = c.y - 2 + j;
            if (!m.valid_y(wy)) continue;
            for (int i = 0; i < 5; ++i) {
                const int wx = m.wrap_x(c.x - 2 + i);
                for (int yy = 0; yy < T; ++yy) {
                    const Argb *src = &terrain_.px[static_cast<size_t>(wy * T + yy) * terrain_.w + wx * T];
                    std::copy(src, src + T, &pm.px[static_cast<size_t>(j * T + yy) * pm.w + i * T]);
                }
            }
        }
        for (const City &o : g().cities) {
            const int i = m.dx(c.x, o.x) + 2, j = o.y - c.y + 2;
            if (i >= 0 && i < 5 && j >= 0 && j < 5) {
                pm.blit(art::city_tile(o.size, o.has(Building::CityWalls), o.has(Building::Palace), civ_colors(g().civs[o.owner].color),
                                       o.x * 7 + o.y), i * T, j * T);
            }
        }
        for (const Unit &u : g().units) {
            const int i = m.dx(c.x, u.x) + 2, j = u.y - c.y + 2;
            if (u.owner != human() && i >= 0 && i < 5 && j >= 0 && j < 5 && !(i == 2 && j == 2)) {
                pm.blit(art::unit_card(u.type, civ_colors(g().civs[u.owner].color)), i * T, j * T);
            }
        }
        const int me = human();
        art::apply_fog(pm, 0, 0, [&](int i, int j) {
            const int wy = c.y - 2 + j;
            return i >= 0 && i < 5 && j >= 0 && j < 5 && m.valid_y(wy) && ((m.at(c.x - 2 + i, wy).explored >> me) & 1u);
        });
        const double ox = mx, oy = my + 20;
        const int S = 2, t = T * S;
        paint_pixmap(cr, pm, ox, oy, S);
        // Corners outside the fat cross are dimmed; worked squares show their yield.
        for (int j = 0; j < 5; ++j) {
            for (int i = 0; i < 5; ++i) {
                const int ddx = i - 2, ddy = j - 2;
                const bool corner = std::abs(ddx) == 2 && std::abs(ddy) == 2;
                const double tx = ox + i * t, ty = oy + j * t;
                if (corner) {
                    fill(cr, tx, ty, t, t, kSlateDeep, 0.8);
                    continue;
                }
                bool worked = ddx == 0 && ddy == 0;
                for (const auto &wk : c.worked) worked |= wk.first == ddx && wk.second == ddy;
                if (!worked) {
                    const bool free = g().tile_free_for(c, c.x + ddx, c.y + ddy);
                    fill(cr, tx, ty, t, t, free ? kSlateDeep : kSlateBad, free ? 0.35 : 0.25);
                } else {
                    const Yield yv = g().tile_yield(c.x + ddx, c.y + ddy, c.owner, ddx == 0 && ddy == 0);
                    const int counts[3] = {yv.food, yv.shields, yv.trade};
                    double ix = tx + 3;
                    const int total = yv.food + yv.shields + yv.trade;
                    const double step = total > 4 ? (t - 18.0) / (total - 1) : 15;
                    fill(cr, tx + 1, ty + t - 18, t - 2, 17, kSlateDeep, 0.7);
                    for (int k = 0; k < 3; ++k) {
                        for (int q = 0; q < counts[k]; ++q) {
                            paint_pixmap(cr, art::yield_icon(k), ix, ty + t - 16, 2);
                            ix += step;
                        }
                    }
                }
                if (!(ddx == 0 && ddy == 0) || true) {
                    const int cid = c.id;
                    hot(tx, ty, t, t, [this, cid, ddx, ddy] { g().toggle_tile(cid, ddx, ddy); });
                }
            }
        }
        set(cr, kSlateEdge);
        cairo_set_line_width(cr, 1);
        cairo_rectangle(cr, ox - 0.5, oy - 0.5, 5 * t + 1, 5 * t + 1);
        cairo_stroke(cr);
        text(cr, kUiFont, c.manual ? "Workers placed by hand. Click the centre for automatic." : "Click a square to move a worker.",
             mx, oy + 5 * t + 8, kSlateDim, 1.0, 320);
    }

    // --- right column: production and improvements
    const double rx = mx + 320 + 24, rr = x + w - 20, rw = rr - rx;
    double ry = y + 54;
    section(cr, "PRODUCTION", rx, ry, rr);
    ry += 20;
    {
        double eh = 22;
        if (!c.build_unit) eh = measure(cr, kUiFont, building_info(c.build).effect, static_cast<int>(rw - 24)).h;
        const double boxh = std::max(100.0, 34 + eh + 46);
        round_rect(cr, rx, ry, rw, boxh, 8);
        set(cr, kSlateDeep);
        cairo_fill(cr);
        const int cost = g().production_cost(c);
        std::string name;
        if (c.build_unit) {
            paint_pixmap(cr, art::unit_card(static_cast<UnitType>(c.build), civ_colors(g().civs[c.owner].color)), rx + 8, ry + 8, 2);
            name = unit_info(c.build).name;
        } else {
            name = building_info(c.build).name;
            text(cr, kUiFont, building_info(c.build).effect, rx + 12, ry + 34, kSlateDim, 1.0, static_cast<int>(rw - 24));
        }
        const double nx = c.build_unit ? rx + 82 : rx + 12;
        text(cr, kBoldFont, name, nx, ry + 10, kSlateGold);
        if (c.build_unit) {
            const UnitInfo &ui = unit_info(c.build);
            text(cr, kUiFont, std::to_string(ui.attack) + "-" + std::to_string(ui.defense) + "-" + std::to_string(ui.moves) + "  att-def-mov",
                 nx, ry + 32, kSlateDim);
        }
        const int per = std::max(0, s.disorder ? 0 : s.shield_surplus);
        const int left = std::max(0, cost - c.shields);
        const std::string eta = left == 0 ? "done next turn" : (per > 0 ? turns_text((left + per - 1) / per) : "stalled");
        progress(cr, rx + 12, ry + boxh - 30, rw - 24, 8, static_cast<double>(c.shields) / cost, kSlateGold);
        text(cr, kTagFont, std::to_string(c.shields) + " / " + std::to_string(cost) + " shields \u00b7 " + eta, rx + 12, ry + boxh - 18, kSlateDim);
        ry += boxh + 10;
        const double bw = (rw - 8) / 2;
        wide_button(cr, rx, ry, bw, 34, "Change", "C", false, hover(rx, ry, bw, 34));
        const int cid = c.id;
        hot(rx, ry, bw, 34, [this, cid] {
            Dialog p;
            p.kind = DialogKind::Production;
            p.title = "Change Production";
            p.city_id = cid;
            open(p);
        });
        const int price = g().buy_cost(c);
        const bool can_buy = price > 0 && !c.bought && g().civs[c.owner].gold >= price;
        wide_button(cr, rx + bw + 8, ry, bw, 34, price > 0 ? "Buy " + with_commas(price) + "g" : "Buy", "B", false,
                    hover(rx + bw + 8, ry, bw, 34), can_buy);
        if (can_buy) hot(rx + bw + 8, ry, bw, 34, [this, cid] { g().buy(cid); });
        ry += 48;
    }
    section(cr, "IMPROVEMENTS", rx, ry, rr);
    ry += 20;
    {
        int n = 0;
        for (int b = 0; b < kBuildingCount; ++b) {
            if (!c.has(static_cast<Building>(b))) continue;
            if (ry > y + h - 80) break;
            text(cr, kUiFont, building_info(b).name, rx + 4, ry, kSlateInk);
            if (building_info(b).upkeep) text_right(cr, kTagFont, std::to_string(building_info(b).upkeep) + "g", rr, ry + 3, kSlateDim);
            ry += 20;
            ++n;
        }
        if (!n) text(cr, kUiFont, "None yet.", rx + 4, ry, kSlateDim);
    }
    const double bx = x + w - 20 - 160, by = y + h - 54;
    wide_button(cr, bx, by, 160, 38, "Close", "Esc", true, hover(bx, by, 160, 38));
    hot(bx, by, 160, 38, [this] { close_top(); });
}

void App::draw_production(cairo_t *cr, Dialog &d)
{
    City *cp = g().city(d.city_id);
    if (!cp) {
        close_top();
        return;
    }
    City &c = *cp;
    const double w = std::min(1000, geo_.win_w - 20), h = std::min(700, geo_.win_h - 20);
    const double x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title + ": " + c.name);
    const double colw = (w - 60) / 2;
    const double L = x + 20, M = L + colw + 20;
    const int per = std::max(1, c.stats.shield_surplus);
    auto eta = [&](int cost) {
        return turns_text(std::max(1, (std::max(0, cost - (c.build_unit ? c.shields : c.shields / 2)) + per - 1) / per));
    };
    section(cr, "UNITS", L, cy, L + colw);
    section(cr, "IMPROVEMENTS", M, cy, M + colw);
    double uy = cy + 20;
    const double row_h = h > 650 ? 38 : 34;
    for (int u = 0; u < kUnitTypeCount; ++u) {
        if (!g().can_build(c, true, u)) continue;
        if (uy + row_h > y + h - 70) break;
        const bool cur = c.build_unit && c.build == u;
        const bool hv = hover(L, uy, colw, row_h - 2);
        round_rect(cr, L, uy, colw, row_h - 2, 5);
        set(cr, cur ? hex(0x3E4A5E) : (hv ? hex(0x354154) : hex(0x29323F)));
        cairo_fill(cr);
        paint_pixmap(cr, art::unit_card(static_cast<UnitType>(u), civ_colors(g().civs[c.owner].color)), L + 2, uy + (row_h - 34) / 2, 1);
        const UnitInfo &ui = unit_info(u);
        text(cr, kBoldFont, ui.name, L + 42, uy + (row_h - 20) / 2 - 1, cur ? kSlateGold : kSlateInk);
        text_right(cr, kUiFont,
                   std::to_string(ui.attack) + "-" + std::to_string(ui.defense) + "-" + std::to_string(ui.moves) + "   " +
                       std::to_string(ui.cost) + " \u00b7 " + eta(ui.cost),
                   L + colw - 10, uy + (row_h - 20) / 2, kSlateDim);
        const int cid = c.id;
        hot(L, uy, colw, row_h - 2, [this, cid, u] {
            g().set_production(cid, true, u);
            close_top();
        });
        uy += row_h;
    }
    double by = cy + 20;
    const double brow = 30;
    for (int b = 0; b < kBuildingCount; ++b) {
        if (!g().can_build(c, false, b)) continue;
        if (by + brow > y + h - 70) break;
        const bool cur = !c.build_unit && c.build == b;
        const bool hv = hover(M, by, colw, brow - 2);
        round_rect(cr, M, by, colw, brow - 2, 5);
        set(cr, cur ? hex(0x3E4A5E) : (hv ? hex(0x354154) : hex(0x29323F)));
        cairo_fill(cr);
        const BuildingInfo &bi = building_info(b);
        text(cr, kBoldFont, bi.name, M + 10, by + 4, cur ? kSlateGold : kSlateInk);
        text_right(cr, kUiFont, std::to_string(bi.cost) + " (" + std::to_string(bi.upkeep) + "g) \u00b7 " + eta(bi.cost), M + colw - 10, by + 5,
                   kSlateDim);
        const int cid = c.id;
        hot(M, by, colw, brow - 2, [this, cid, b] {
            g().set_production(cid, false, b);
            close_top();
        });
        by += brow;
    }
    text(cr, kUiFont, "Switching between a unit and an improvement loses half the shields.", L, y + h - 46, kSlateDim);
    const double bx = x + w - 20 - 140, byy = y + h - 54;
    wide_button(cr, bx, byy, 140, 38, "Cancel", "Esc", false, hover(bx, byy, 140, 38));
    hot(bx, byy, 140, 38, [this] { close_top(); });
}

void App::draw_science(cairo_t *cr, Dialog &d)
{
    const int me = human();
    const Civ &c = g().civs[me];
    const std::vector<Advance> opts = g().researchable(me);
    const size_t rows = opts.size() > 7 ? (opts.size() + 1) / 2 : opts.size();
    const double w = opts.size() > 7 ? 900 : 760, h = std::min<double>(geo_.win_h - 20, 230 + std::max<size_t>(rows, 3) * 50.0);
    const double x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, R = x + w - 24;
    text(cr, kUiFont, "The " + c.name + " know " + std::to_string(g().techs_known(me)) + " of " + std::to_string(kAdvanceCount) +
                          " advances. Each new advance costs " + std::to_string(g().research_cost(me)) + " bulbs.",
         L, cy, kSlateDim);
    cy += 24;
    if (c.research != Advance::None) {
        text(cr, kBoldFont, std::string("Researching ") + advance_info(c.research).name, L, cy, kSlateGold);
        const int t = research_turns(me);
        text_right(cr, kUiFont, std::to_string(c.bulbs) + " / " + std::to_string(g().research_cost(me)) + " \u00b7 " + turns_text(t), R, cy,
                   kSlateDim);
        cy += 24;
        progress(cr, L, cy, R - L, 8, static_cast<double>(c.bulbs) / g().research_cost(me), kSlateGold);
        cy += 20;
    } else {
        text(cr, kBoldFont, "Your wise men ask: what shall we study next?", L, cy, kSlateGold);
        cy += 30;
    }
    section(cr, "CHOOSE AN ADVANCE", L, cy, R);
    cy += 20;
    const bool two = opts.size() > 7;
    const double colw = two ? (R - L - 12) / 2 : R - L;
    for (size_t i = 0; i < opts.size(); ++i) {
        const Advance a = opts[i];
        const double ax = two && i % 2 ? L + colw + 12 : L;
        const double ay = cy + (two ? static_cast<double>(i / 2) : static_cast<double>(i)) * 50;
        if (ay + 46 > y + h - 70) break;
        const bool cur = c.research == a;
        const bool hv = hover(ax, ay, colw, 44);
        round_rect(cr, ax, ay, colw, 44, 6);
        set(cr, cur ? hex(0x3E4A5E) : (hv ? hex(0x354154) : hex(0x29323F)));
        cairo_fill_preserve(cr);
        set(cr, cur ? kSlateGold : hex(0x3C4859));
        cairo_set_line_width(cr, cur ? 2 : 1);
        cairo_stroke(cr);
        text(cr, kBoldFont, advance_info(a).name, ax + 12, ay + 4, cur ? kSlateGold : kSlateInk);
        const std::string en = enables_text(a);
        cairo_save(cr);
        cairo_rectangle(cr, ax, ay, colw - 6, 44);
        cairo_clip(cr);
        text(cr, kTagFont, en.empty() ? "Leads to further advances" : "Allows: " + en, ax + 12, ay + 25, kSlateDim);
        cairo_restore(cr);
        hot(ax, ay, colw, 44, [this, me, a] {
            g().civs[me].research = a;
            close_top();
        });
    }
    const double bx = R - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Close", "Esc", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

void App::draw_rates(cairo_t *cr, Dialog &d)
{
    const int me = human();
    Civ &c = g().civs[me];
    const double w = 520, h = 380, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, R = x + w - 24;
    const int mx = government_info(c.govt).max_rate;
    text(cr, kUiFont, std::string(government_info(c.govt).name) + ": no rate may be above " + std::to_string(mx * 10) + "%.", L, cy, kSlateDim);
    cy += 30;
    const char *names[3] = {"TAXES", "LUXURIES", "SCIENCE"};
    int *vals[3] = {&c.tax, &c.lux, &c.sci};
    for (int k = 0; k < 3; ++k) {
        text(cr, kBoldFont, names[k], L, cy + 8, kSlateInk);
        progress(cr, L + 120, cy + 13, 180, 8, *vals[k] / 10.0, k == 0 ? kSlateGold : (k == 1 ? hex(0xE070B0) : hex(0x60C8D8)));
        text(cr, kBoldFont, std::to_string(*vals[k] * 10) + "%", L + 312, cy + 8, kSlateInk);
        auto adjust = [this, me, k](int delta) {
            Civ &cv = g().civs[me];
            int v[3] = {cv.tax, cv.lux, cv.sci};
            const int mxr = government_info(cv.govt).max_rate;
            v[k] += delta;
            if (v[k] < 0 || v[k] > mxr) return;
            // Take from (or give to) the others, science first then taxes.
            const int order[3][2] = {{2, 1}, {2, 0}, {0, 1}};
            for (int o : order[k]) {
                const int nv = v[o] - delta;
                if (nv >= 0 && nv <= mxr) {
                    v[o] = nv;
                    g().set_rates(me, v[0], v[1], v[2]);
                    return;
                }
            }
        };
        wide_button(cr, R - 80, cy, 36, 34, "\u2212", "", false, hover(R - 80, cy, 36, 34));
        hot(R - 80, cy, 36, 34, [adjust] { adjust(-1); });
        wide_button(cr, R - 36, cy, 36, 34, "+", "", false, hover(R - 36, cy, 36, 34));
        hot(R - 36, cy, 36, 34, [adjust] { adjust(1); });
        cy += 46;
    }
    int tax = 0, sci = 0, upkeep = 0;
    for (const City &ct : g().cities) {
        if (ct.owner != me) continue;
        if (!ct.stats.disorder) {
            tax += ct.stats.tax;
            sci += ct.stats.sci;
        }
        for (int b = 0; b < kBuildingCount; ++b) {
            if (ct.has(static_cast<Building>(b))) upkeep += building_info(b).upkeep;
        }
    }
    cy += 6;
    text(cr, kUiFont, "Income " + std::to_string(tax) + " gold, upkeep " + std::to_string(upkeep) + " gold, science " + std::to_string(sci) +
                          " bulbs a turn.", L, cy, kSlateInk);
    const double bx = R - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Done", "Esc", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

void App::draw_revolution(cairo_t *cr, Dialog &d)
{
    const int me = human();
    const Civ &c = g().civs[me];
    const double w = 560, h = 470, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, R = x + w - 24;
    if (c.govt == Government::Anarchy) {
        text(cr, kUiFont, "The people are in Anarchy. A new government takes over in " + turns_text(c.anarchy) + ": " +
                              government_info(c.target).name + ".", L, cy, kSlateInk, 1.0, static_cast<int>(R - L));
    } else {
        text(cr, kUiFont, std::string("You rule by ") + government_info(c.govt).name +
                              ". A revolution brings two turns of Anarchy before the new government.",
             L, cy, kSlateDim, 1.0, static_cast<int>(R - L));
        cy += 50;
        for (int gv = static_cast<int>(Government::Despotism); gv < static_cast<int>(Government::Count); ++gv) {
            const Government gg = static_cast<Government>(gv);
            const GovernmentInfo &gi = government_info(gg);
            const bool ok = g().has(me, gi.tech) && gg != c.govt;
            const bool hv = ok && hover(L, cy, R - L, 50);
            round_rect(cr, L, cy, R - L, 50, 6);
            set(cr, hv ? hex(0x354154) : hex(0x29323F));
            cairo_fill(cr);
            text(cr, kBoldFont, gi.name, L + 12, cy + 6, ok ? kSlateInk : kSlateEdge);
            std::string note = "Max rate " + std::to_string(gi.max_rate * 10) + "%";
            if (gg == Government::Despotism) note += " \u00b7 -1 on big yields";
            if (gg == Government::Republic || gg == Government::Democracy) note += " \u00b7 +1 trade";
            if (gg == c.govt) note += " \u00b7 current";
            else if (!g().has(me, gi.tech)) note += std::string(" \u00b7 needs ") + advance_info(gi.tech).name;
            text(cr, kTagFont, note, L + 12, cy + 28, ok ? kSlateDim : kSlateEdge);
            if (ok) {
                hot(L, cy, R - L, 50, [this, me, gg] {
                    g().revolution(me, gg);
                    collect_news(false);
                    close_top();
                });
            }
            cy += 56;
        }
    }
    const double bx = R - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Close", "Esc", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

void App::draw_saveload(cairo_t *cr, Dialog &d)
{
    const double w = 620, h = 480, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, R = x + w - 24;
    text(cr, kUiFont, d.saving ? "Choose a slot to save into." : "Choose a saved game to load.", L, cy, kSlateDim);
    cy += 28;
    for (int slot = 1; slot <= 6; ++slot) {
        const std::string sum = slot_summary(slot);
        const bool empty = sum == "Empty";
        const bool ok = d.saving || !empty;
        const bool hv = ok && hover(L, cy, R - L, 46);
        round_rect(cr, L, cy, R - L, 46, 6);
        set(cr, hv ? hex(0x354154) : hex(0x29323F));
        cairo_fill(cr);
        text(cr, kBoldFont, "Slot " + std::to_string(slot), L + 12, cy + 12, ok ? kSlateGold : kSlateEdge);
        text(cr, kUiFont, sum, L + 90, cy + 12, empty ? kSlateDim : kSlateInk);
        if (ok) {
            const bool saving = d.saving;
            hot(L, cy, R - L, 46, [this, slot, saving] {
                close_top();
                if (saving) {
                    save_slot(slot);
                } else {
                    load_slot(slot);
                }
            });
        }
        cy += 52;
    }
    std::string shown = save_dir_;
    if (shown.size() > 52) {
        size_t start = shown.size() - 50;
        while (start < shown.size() && (static_cast<unsigned char>(shown[start]) & 0xC0) == 0x80) ++start;
        shown = "\u2026" + shown.substr(start);
    }
    text(cr, kTagFont, shown, L, y + h - 44, kSlateDim);
    const double bx = R - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Cancel", "Esc", false, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

void App::draw_city_name(cairo_t *cr, Dialog &d)
{
    const double w = 480, h = 220, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, R = x + w - 24;
    text(cr, kTagFont, "NAME OF THE NEW CITY", L, cy, kSlateDim);
    field(cr, L, cy + 16, R - L, d.field, true, blink_);
    const double bx = R - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Found", "Enter", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] {
        KeyEvent k;
        k.key = Key::Enter;
        key(k);
    });
    wide_button(cr, bx - 150, by, 140, 38, "Cancel", "Esc", false, hover(bx - 150, by, 140, 38));
    hot(bx - 150, by, 140, 38, [this] { close_top(); });
}

void App::draw_cities(cairo_t *cr, Dialog &d)
{
    const int me = human();
    std::vector<const City *> list;
    for (const City &c : g().cities) {
        if (c.owner == me) list.push_back(&c);
    }
    const double w = 860, h = std::min<double>(geo_.win_h - 20, 170 + std::max<size_t>(list.size(), 2) * 30.0);
    const double x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, R = x + w - 24;
    const double cols[] = {L, L + 200, L + 260, L + 330, L + 400, L + 470};
    const char *heads[] = {"CITY", "SIZE", "FOOD", "SHIELDS", "TRADE", "PRODUCING"};
    for (int i = 0; i < 6; ++i) text(cr, kTagFont, heads[i], cols[i], cy, kSlateDim);
    cy += 18;
    for (const City *c : list) {
        if (cy + 30 > y + h - 64) break;
        const bool hv = hover(L - 6, cy, R - L + 12, 28);
        if (hv) {
            round_rect(cr, L - 6, cy, R - L + 12, 28, 5);
            set(cr, hex(0x354154));
            cairo_fill(cr);
        }
        const CityStats &s = c->stats;
        text(cr, kBoldFont, c->name, cols[0], cy + 4, s.disorder ? kSlateBad : kSlateInk);
        text(cr, kUiFont, std::to_string(c->size), cols[1], cy + 4, kSlateInk);
        text(cr, kUiFont, (s.food_surplus >= 0 ? "+" : "") + std::to_string(s.food_surplus), cols[2], cy + 4,
             s.food_surplus < 0 ? kSlateBad : kSlateInk);
        text(cr, kUiFont, std::to_string(s.shield_surplus), cols[3], cy + 4, kSlateInk);
        text(cr, kUiFont, std::to_string(s.raw.trade), cols[4], cy + 4, kSlateInk);
        const std::string prod = c->build_unit ? unit_info(c->build).name : building_info(c->build).name;
        text(cr, kUiFont, prod + "  (" + std::to_string(c->shields) + "/" + std::to_string(g().production_cost(*c)) + ")", cols[5], cy + 4,
             kSlateDim);
        const int id = c->id;
        hot(L - 6, cy, R - L + 12, 28, [this, id] { open_city(id); });
        cy += 30;
    }
    if (list.empty()) text(cr, kUiFont, "You have no cities yet. Settlers found them (B).", L, cy, kSlateDim);
    const double bx = R - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Close", "Esc", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

void App::draw_foreign(cairo_t *cr, Dialog &d)
{
    const int me = human();
    const double w = 760, h = 160 + (g().civs.size() - 1) * 64.0, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, R = x + w - 24;
    for (int k = 0; k < static_cast<int>(g().civs.size()); ++k) {
        if (k == me) continue;
        const Civ &c = g().civs[k];
        const bool met = (g().civs[me].contact >> k) & 1u;
        round_rect(cr, L, cy, R - L, 56, 6);
        set(cr, hex(0x29323F));
        cairo_fill(cr);
        round_rect(cr, L + 10, cy + 14, 28, 28, 5);
        set_argb(cr, civ_colors(c.color).main);
        cairo_fill(cr);
        int ncity = 0;
        for (const City &ct : g().cities) ncity += ct.owner == k;
        text(cr, kBoldFont, met ? "The " + c.name + " \u00b7 " + c.leader : "Unknown people", L + 50, cy + 8, kSlateInk);
        std::string status = !c.alive ? "Destroyed" : (!met ? "Not yet met" : (g().at_war(me, k) ? "At war" : "At peace"));
        if (met && c.alive) status += " \u00b7 " + std::to_string(ncity) + " cities \u00b7 " + government_info(c.govt).name;
        text(cr, kUiFont, status, L + 50, cy + 30, g().at_war(me, k) ? kSlateBad : kSlateDim);
        if (met && c.alive) {
            const bool war = g().at_war(me, k);
            const double bx = R - 170;
            wide_button(cr, bx, cy + 10, 160, 36, war ? "Propose Peace" : "Declare War", "", false, hover(bx, cy + 10, 160, 36));
            hot(bx, cy + 10, 160, 36, [this, me, k, war] {
                if (war) {
                    // They accept peace when the war goes badly for them, or now and then.
                    int mine = 0, theirs = 0;
                    for (const Unit &u : g().units) {
                        if (u.owner == me) mine += unit_info(u.type).attack;
                        if (u.owner == k) theirs += unit_info(u.type).attack;
                    }
                    if (mine > theirs || g().rng.chance(0.3)) {
                        g().make_peace(me, k);
                        message("Peace", {"The " + g().civs[k].name + " accept peace."});
                    } else {
                        message("Refused", {"The " + g().civs[k].name + " laugh at your offer of peace."});
                    }
                } else {
                    g().declare_war(me, k);
                    collect_news(false);
                }
            });
        }
        cy += 64;
    }
    const double bx = R - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Close", "Esc", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

void App::draw_unit_sheet(cairo_t *cr, Dialog &d)
{
    const double w = std::min(1040, geo_.win_w - 16), h = std::min(760, geo_.win_h - 16);
    const double x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const int cols = 7;
    const double cw = (w - 32) / cols, ch = (h - 54 - 70) / 4;
    const int color = game_ ? g().civs[human()].color : settings.player_color;
    for (int u = 0; u < kUnitTypeCount; ++u) {
        const double cx = x + 16 + (u % cols) * cw, ccy = cy + (u / cols) * ch;
        const UnitInfo &ui = unit_info(u);
        paint_pixmap(cr, art::unit_card(static_cast<UnitType>(u), civ_colors(color)), cx + (cw - 64) / 2, ccy, 2);
        text_center(cr, kSmallBold, ui.name, cx + cw / 2, ccy + 66, kSlateInk);
        const std::string stats = std::to_string(ui.attack) + "-" + std::to_string(ui.defense) + "-" + std::to_string(ui.moves) + " \u00b7 " +
                                  std::to_string(ui.cost) + " shields";
        text_center(cr, kTagFont, stats, cx + cw / 2, ccy + 84, kSlateDim);
        const std::string tech = ui.tech == Advance::None ? "Available at start" : advance_info(ui.tech).name;
        text_center(cr, kTagFont, tech, cx + cw / 2, ccy + 98, ui.tech == Advance::None ? kSlateGood : kSlateGoldDim);
        if (ui.obsolete != Advance::None) {
            std::string obs = std::string("Obsolete: ") + advance_info(ui.obsolete).name;
            if (measure(cr, kTagFont, obs).w > cw - 6) obs = std::string("Obs. ") + advance_info(ui.obsolete).name;
            text_center(cr, kTagFont, obs, cx + cw / 2, ccy + 111, kSlateEdge);
        }
    }
    text(cr, kUiFont, "Attack-defense-moves on every counter.", x + 24, y + h - 46, kSlateDim);
    const double bx = x + w - 24 - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Close", "Esc", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

void App::draw_confirm_war(cairo_t *cr, Dialog &d)
{
    const double w = 520, h = 220, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const std::string who = d.other >= 0 ? g().civs[d.other].name : "them";
    text(cr, kUiFont, "We are at peace with the " + who + ". Attacking them means war. Declare war?", x + 24, cy, kSlateInk, 1.0,
         static_cast<int>(w - 48));
    const double bx = x + w - 24 - 160, by = y + h - 54;
    wide_button(cr, bx, by, 160, 38, "Declare War", "Enter", true, hover(bx, by, 160, 38));
    hot(bx, by, 160, 38, [this] {
        KeyEvent k;
        k.key = Key::Enter;
        key(k);
    });
    wide_button(cr, bx - 170, by, 160, 38, "Keep Peace", "Esc", false, hover(bx - 170, by, 160, 38));
    hot(bx - 170, by, 160, 38, [this] { close_top(); });
}

void App::draw_game_over(cairo_t *cr, Dialog &d)
{
    const double w = 560, h = 300, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const bool won = g().winner == human();
    std::string msg;
    if (won) {
        msg = "The " + g().civs[human()].name + " rule the whole world. Your name, " + g().civs[human()].leader +
              ", will be remembered for all time.";
    } else if (g().human_dead) {
        msg = "The " + g().civs[human()].name + " have been destroyed. History will forget them.";
    } else if (g().winner >= 0) {
        msg = "The " + g().civs[g().winner].name + " have conquered the world.";
    }
    text(cr, kUiFont, msg, x + 24, cy, kSlateInk, 1.0, static_cast<int>(w - 48));
    text(cr, kUiFont, "Final year: " + year_text(year_for_turn(g().turn)), x + 24, cy + 70, kSlateDim);
    const double bx = x + w - 24 - 160, by = y + h - 54;
    wide_button(cr, bx, by, 160, 38, "New Game", "", true, hover(bx, by, 160, 38));
    hot(bx, by, 160, 38, [this] { open_new_game_dialog(); });
    wide_button(cr, bx - 170, by, 160, 38, "Look Around", "Esc", false, hover(bx - 170, by, 160, 38));
    hot(bx - 170, by, 160, 38, [this] { dialogs_.pop_back(); });
}

void App::draw_help(cairo_t *cr, Dialog &d)
{
    const double w = 640, h = 560, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const std::pair<const char *, const char *> rows[] = {
        {"Arrows, keypad", "Move the active unit (keypad 7 9 1 3 for diagonals)"},
        {"B", "Found a city (or join one)"},
        {"R  I  M  P", "Settlers: road, irrigate, mine, fortress"},
        {"F  S", "Fortify, sentry"},
        {"G", "Go to a square (then click it)"},
        {"W  Space  N", "Wait, skip this turn, next unit"},
        {"Shift+D", "Disband"},
        {"Enter", "End the turn"},
        {"C  H", "Center on the unit, on the capital"},
        {"+  -  wheel", "Zoom"},
        {"T  A  V", "Tax rates, science, cities"},
        {"Ctrl+S  Ctrl+L", "Save, load"},
        {"Click", "Select a unit, open a city, move next door"},
        {"Right click", "Center the map there"},
    };
    for (const auto &r : rows) {
        text(cr, kBoldFont, r.first, x + 24, cy, kSlateGold);
        text(cr, kUiFont, r.second, x + 200, cy, kSlateInk);
        cy += 28;
    }
    const double bx = x + w - 24 - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Close", "Esc", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

} // namespace ui
