// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Dialogs: new game, city screen, production, science, rates, revolution,
// saves, advisors, audiences with foreign leaders, the Databank, combat and
// news reports.

#include "app.hpp"

#include "databank.hpp"

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
    // Taxman: a tall brown hat and a green coat with a gold coin.
    static const std::vector<std::string> taxman = {
        ".KBBBK.", "KBBBBBK", "KsKsKsK", "KsssssK", "KsKKKsK", ".KsssK.", "KEEyEEK", "KEyYyEK", "KEEyEEK", "KKK.KKK",
    };
    // Scientist: grey hair, spectacles and a white coat with a blue flask.
    static const std::vector<std::string> scientist = {
        ".KKKKK.", "KgggggK", "KcKgKcK", "KsssssK", "KsKKKsK", ".KsssK.", "KwwuwwK", "KwWUWwK", "KwwwwwK", "KKK.KKK",
    };
    switch (kind) {
    case 0: return happy;
    case 1: return content;
    case 2: return unhappy;
    case 4: return taxman;
    case 5: return scientist;
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
    case DialogKind::WonderBuilt: draw_wonder_built(cr, d); break;
    case DialogKind::Wonders: draw_wonders(cr, d); break;
    case DialogKind::Spaceship: draw_spaceship(cr, d); break;
    case DialogKind::DiplomatMenu: draw_diplomat_menu(cr, d); break;
    case DialogKind::CaravanMenu: draw_caravan_menu(cr, d); break;
    case DialogKind::ConfirmSell: draw_confirm_sell(cr, d); break;
    case DialogKind::Talk: draw_talk(cr, d); break;
    case DialogKind::Audience: draw_audience(cr, d); break;
    case DialogKind::Databank: draw_databank(cr, d); break;
    case DialogKind::CustomWorld: draw_custom_world(cr, d); break;
    case DialogKind::Nuke: draw_nuke(cr, d); break;
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
    const double w = std::min(860, geo_.win_w - 16), h = std::min(700.0, geo_.win_h - 70.0);
    const double x = (geo_.win_w - w) / 2, y = game_ ? (geo_.win_h - h) / 2 : std::max(54.0, (geo_.win_h - h) / 2 + 20);
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, R = x + w - 24;
    const bool roomy = h >= 680;
    section(cr, "DIFFICULTY", L, cy, R);
    cy += 20;
    for (int i = 0; i < static_cast<int>(Difficulty::Count); ++i) {
        const double bw = (R - L - 4 * 8) / 5, bx = L + i * (bw + 8);
        const bool chosen = static_cast<int>(settings.difficulty) == i;
        wide_button(cr, bx, cy, bw, 32, difficulty_name(static_cast<Difficulty>(i)), "", chosen, hover(bx, cy, bw, 32));
        hot(bx, cy, bw, 32, [this, i] { settings.difficulty = static_cast<Difficulty>(i); });
    }
    cy += 38;
    const std::string level_note = "Content citizens per city: " + std::to_string(content_base(settings.difficulty)) +
                                   ". Research costs rise faster on harder levels.";
    text(cr, kTagFont, level_note, L, cy, kSlateDim);
    cy += roomy ? 26 : 20;
    section(cr, "CIVILIZATIONS IN THE WORLD", L, cy, R);
    cy += 20;
    for (int n = 2; n <= 7; ++n) {
        const double bw = 60, bx = L + (n - 2) * (bw + 8);
        wide_button(cr, bx, cy, bw, 32, std::to_string(n), "", settings.civs == n, hover(bx, cy, bw, 32));
        hot(bx, cy, bw, 32, [this, n] { settings.civs = n; });
    }
    cy += roomy ? 46 : 40;
    section(cr, "YOUR PEOPLE", L, cy, R);
    cy += 20;
    // The fourteen peoples, two to a colour, and one of your own making.
    const double tw = (R - L - 6 * 6) / 7, th = 34;
    for (int t = 0; t < kTribeCount; ++t) {
        const TribeInfo &ti = tribe_info(t);
        const double bx = L + ti.slot * (tw + 6), by = cy + (t / kTribeSlots) * (th + 6);
        const bool chosen = settings.player_tribe == t;
        const bool lit = hover(bx, by, tw, th);
        round_rect(cr, bx, by, tw, th, 5);
        set(cr, chosen ? kSlateGoldDim : (lit ? kSlateEdge : kSlatePanel));
        cairo_fill(cr);
        round_rect(cr, bx + 6, by + 9, 16, 16, 3);
        set_argb(cr, civ_colors(ti.color).main);
        cairo_fill(cr);
        text(cr, kSmallBold, ti.name, bx + 28, by + 9, chosen ? kSlateDeep : kSlateInk);
        hot(bx, by, tw, th, [this, t] {
            settings.player_tribe = t;
            if (new_game_field_ < 2) new_game_field_ = 2;
        });
    }
    cy += 2 * (th + 6);
    {
        const bool chosen = settings.player_tribe < 0;
        const double bw = 210;
        wide_button(cr, L, cy, bw, 32, "A people of your own", "", chosen, hover(L, cy, bw, 32));
        hot(L, cy, bw, 32, [this] {
            settings.player_tribe = -1;
            new_game_field_ = 0;
        });
        if (!chosen) {
            const TribeInfo &ti = tribe_info(settings.player_tribe);
            const std::string lean = std::string(ti.leader) + " of the " + ti.name + " \u00b7 " + mood_name(ti.mood) + ", " + policy_name(ti.policy) +
                                     ", " + ideology_name(ti.ideology);
            text(cr, kBoldFont, lean, L + bw + 16, cy + 6, kSlateGold);
        }
    }
    cy += 42;
    const double half = (R - L - 16) / 2;
    if (settings.player_tribe < 0) {
        text(cr, kTagFont, "NAME OF YOUR PEOPLE", L, cy, kSlateDim);
        text(cr, kTagFont, "YOUR NAME AS LEADER", L + half + 16, cy, kSlateDim);
        cy += 16;
        field(cr, L, cy, half, settings.player_name, new_game_field_ == 0, blink_);
        hot(L, cy, half, 34, [this] { new_game_field_ = 0; });
        field(cr, L + half + 16, cy, half, settings.player_leader, new_game_field_ == 1, blink_);
        hot(L + half + 16, cy, half, 34, [this] { new_game_field_ = 1; });
        cy += 42;
        for (int c = 0; c < 8; ++c) {
            const double sx = L + c * 34;
            round_rect(cr, sx, cy, 28, 28, 5);
            set_argb(cr, civ_colors(c).main);
            cairo_fill_preserve(cr);
            set(cr, settings.player_color == c ? kSlateGold : kSlateDeep);
            cairo_set_line_width(cr, settings.player_color == c ? 3 : 1);
            cairo_stroke(cr);
            hot(sx, cy, 28, 28, [this, c] { settings.player_color = c; });
        }
        text(cr, kTagFont, "COLOR: the people of that colour stay out of the game.", L + 8 * 34 + 8, cy + 8, kSlateDim);
        cy += 38;
    } else {
        // Portrait and leanings of the chosen people's leader.
        const TribeInfo &ti = tribe_info(settings.player_tribe);
        round_rect(cr, L - 3, cy - 3, 70, 70, 5);
        set(cr, kSlateGoldDim);
        cairo_fill(cr);
        paint_pixmap(cr, art::leader_portrait(settings.player_tribe, civ_colors(ti.color), 1), L, cy, 2);
        const std::string facts = std::string("Cities: ") + ti.cities[0] + ", " + ti.cities[1] + ", " + ti.cities[2] + "\u2026  \u00b7  Paired with the " +
                                  tribe_info(tribe_pair(settings.player_tribe)).name + ", who share their colour.";
        text(cr, kUiFont, facts, L + 82, cy + 6, kSlateInk, 1.0, static_cast<int>(R - L - 82));
        cy += 80;
    }
    section(cr, "WORLD", L, cy, R);
    cy += 20;
    const char *worlds[3] = {"Random", "Earth", "Customize..."};
    const double ww = (half - 16) / 3;
    for (int i = 0; i < 3; ++i) {
        const double bx = L + i * (ww + 8);
        wide_button(cr, bx, cy, ww, 34, worlds[i], "", world_choice_ == i, hover(bx, cy, ww, 34));
        hot(bx, cy, ww, 34, [this, i] {
            if (i == 2) {
                Dialog c;
                c.kind = DialogKind::CustomWorld;
                c.title = "Customize World";
                open(c);
            } else {
                world_choice_ = i;
            }
        });
    }
    text(cr, kTagFont, "WORLD SEED", L + half + 16, cy - 14, kSlateDim);
    field(cr, L + half + 16, cy, half, seed_text_, new_game_field_ == 2, blink_);
    hot(L + half + 16, cy, half, 34, [this] { new_game_field_ = 2; });
    cy += 40;
    if (world_choice_ == 2) {
        static const char *t3[] = {"Cool", "Temperate", "Warm"}, *c3[] = {"Arid", "Normal", "Wet"}, *a3[] = {"3", "4", "5"};
        const std::string land = settings.land_pct <= 24 ? "Small" : (settings.land_pct >= 40 ? "Large" : "Normal");
        text(cr, kTagFont,
             "Land " + land + " \u00b7 " + t3[std::clamp(settings.temperature, 0, 2)] + " \u00b7 " + c3[std::clamp(settings.climate, 0, 2)] +
                 " \u00b7 " + a3[std::clamp(settings.age, 0, 2)] + " billion years",
             L, cy, kSlateDim);
    } else if (world_choice_ == 1) {
        text(cr, kTagFont, "The continents of the Earth; the seed still sets the starting places.", L, cy, kSlateDim);
    }
    const double by = y + h - 54;
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
    if (!cp || (cp->owner != human() && !d.view_only)) {
        close_top();
        return;
    }
    City &c = *cp;
    const bool ro = d.view_only;   // a Diplomat's report: look, don't touch
    g().update_city(c);
    const CityStats &s = c.stats;
    const double w = std::min(1040, geo_.win_w - 20), h = std::min(700, geo_.win_h - 20);
    const double x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    const std::string title = c.name + (ro ? " (" + g().civs[c.owner].name + ")" : std::string()) + "  \u00b7  size " + std::to_string(c.size) + "  \u00b7  " +
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
                px += c.size > 12 ? 18 : 24;
            }
        };
        const int workers = c.size - c.specialists();
        const int happy = std::min(s.happy, workers), unhappy = std::min(s.unhappy, workers - happy);
        face(0, happy);
        face(1, std::max(0, workers - happy - unhappy));
        face(2, unhappy);
        px += 8;
        // Specialists: click one to make an entertainer a taxman, a taxman
        // a scientist and a scientist an entertainer again (size 5 and up).
        const int cid = c.id;
        const int kinds[3] = {3, 4, 5};
        const int counts[3] = {c.entertainers, c.taxmen, c.scientists};
        int index = 0;
        for (int k = 0; k < 3; ++k) {
            for (int i = 0; i < counts[k]; ++i, ++index) {
                const double sx = px;
                face(kinds[k], 1);
                if (!ro) {
                    const double hw = px - sx;
                    if (hover(sx - 2, py - 2, hw, 34)) {
                        round_rect(cr, sx - 2, py - 2, hw, 34, 3);
                        set(cr, kSlateGold);
                        cairo_set_line_width(cr, 2);
                        cairo_stroke(cr);
                    }
                    hot(sx - 2, py - 2, hw, 34, [this, cid, index] {
                        if (!g().cycle_specialist(cid, index))
                            message("Specialists", {"A city must have five population units to support taxmen or scientists."});
                    });
                }
            }
        }
        cy += 54;
        std::string mood = std::to_string(s.happy) + " happy \u00b7 " + std::to_string(s.content) + " content \u00b7 " +
                           std::to_string(s.unhappy) + " unhappy";
        text(cr, kUiFont, mood, L, cy, kSlateDim);
        cy += 20;
        if (c.specialists()) {
            // What the specialists make: 2 each, before any multipliers.
            std::string sp;
            auto add = [&](int n, const char *what) {
                if (n) sp += (sp.empty() ? "+" : " \u00b7 +") + std::to_string(2 * n) + " " + what;
            };
            add(c.entertainers, "lux");
            add(c.taxmen, "gold");
            add(c.scientists, "sci");
            const int n = c.specialists();
            text(cr, kUiFont, sp + " from " + std::to_string(n) + (n == 1 ? " specialist" : " specialists"), L, cy, kSlateInk);
            cy += 20;
        }
        if (s.disorder) {
            text(cr, kBoldFont, "CIVIL DISORDER: no production or taxes!", L, cy, kSlateBad);
            cy += 20;
        } else if (c.celebrating) {
            const Government gv = g().civs[c.owner].govt;
            const bool grows = gv == Government::Republic || gv == Government::Democracy;
            text(cr, kBoldFont, std::string("\u2605 We Love the ") + g().leader_title(c.owner) + " Day!", L, cy, kSlateGold);
            cy += 20;
            text(cr, kTagFont, grows ? "+1 trade on worked squares with trade; the city grows each turn it has spare food."
                                     : (gv == Government::Monarchy || gv == Government::Communism ? "+1 trade on worked squares with trade."
                                                                                                   : "No despotic -1 on rich squares."),
                 L, cy, kSlateDim, 1.0, static_cast<int>(lw));
            cy += 18;
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
        {
            // Where the corruption comes from (the DOS formula: trade x distance x 3 / government).
            const Government gv = g().civs[c.owner].govt;
            std::string why;
            if (gv == Government::Democracy) why = "Democracy: no corruption";
            else if (gv == Government::Communism) why = "Palace 10 sq (Communism)";
            else if (c.has(Building::Palace)) why = "The Palace is here: no corruption";
            else if (s.palace_distance >= kNoPalaceDistance) why = "32+ sq from the Palace";
            else why = std::to_string(s.palace_distance) + " sq from the Palace";
            if (c.has(Building::Courthouse) && gv != Government::Democracy) why += " \u00b7 Courthouse \u00f72";
            text(cr, kUiFont, why, L, cy, kSlateDim);
            cy += 20;
        }
        text(cr, kUiFont, "Upkeep: " + std::to_string(s.shield_upkeep) + " shields, " + std::to_string(s.food_upkeep) + " food",
             L, cy, kSlateDim);
        cy += 20;
        if (!c.routes.empty()) {
            std::string r = "Trade routes:";
            bool first = true;
            for (int id : c.routes) {
                const City *o = g().city(id);
                if (!o) continue;
                r += std::string(first ? " " : " \u00b7 ") + o->name + " +" + std::to_string(g().route_value(c, *o));
                first = false;
            }
            text(cr, kUiFont, r, L, cy, kSlateGold, 1.0, static_cast<int>(lw));
            cy += 20;
        }
        cy += 8;
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
            if (u.x != c.x || u.y != c.y || u.owner != c.owner) continue;
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
                if (!ro) {
                    const int cid = c.id;
                    hot(tx, ty, t, t, [this, cid, ddx, ddy] { g().toggle_tile(cid, ddx, ddy); });
                }
            }
        }
        set(cr, kSlateEdge);
        cairo_set_line_width(cr, 1);
        cairo_rectangle(cr, ox - 0.5, oy - 0.5, 5 * t + 1, 5 * t + 1);
        cairo_stroke(cr);
        text(cr, kUiFont, ro ? "Your Diplomat's report: a look inside a foreign city." : (c.manual ? "Workers placed by hand. Click the centre for automatic." : "Click a square to move a worker."),
             mx, oy + 5 * t + 8, kSlateDim, 1.0, 320);
    }

    // --- right column: production and improvements
    const double rx = mx + 320 + 24, rr = x + w - 20, rw = rr - rx;
    double ry = y + 54;
    section(cr, "PRODUCTION", rx, ry, rr);
    ry += 20;
    {
        double eh = 22;
        if (!c.build_unit)
            eh = measure(cr, kUiFont, building_info(c.build).effect,
                         static_cast<int>(rw - (is_wonder_build(c.build) || is_space_build(c.build) ? 94 : 24))).h;
        const double boxh = std::max(100.0, 34 + eh + 46);
        round_rect(cr, rx, ry, rw, boxh, 8);
        set(cr, kSlateDeep);
        cairo_fill(cr);
        const int cost = g().production_cost(c);
        std::string name;
        const bool wonder = !c.build_unit && (is_wonder_build(c.build) || is_space_build(c.build));
        if (c.build_unit) {
            paint_pixmap(cr, art::unit_card(static_cast<UnitType>(c.build), civ_colors(g().civs[c.owner].color)), rx + 8, ry + 8, 2);
            name = unit_info(c.build).name;
        } else if (wonder) {
            paint_pixmap(cr, is_space_build(c.build) ? art::space_part_icon(c.build - kSpaceBase) : art::wonder_icon(c.build - kWonderBase), rx + 8,
                         ry + 8, 2);
            name = building_info(c.build).name;
            text(cr, kUiFont, building_info(c.build).effect, rx + 82, ry + 34, kSlateDim, 1.0, static_cast<int>(rw - 94));
        } else {
            name = building_info(c.build).name;
            text(cr, kUiFont, building_info(c.build).effect, rx + 12, ry + 34, kSlateDim, 1.0, static_cast<int>(rw - 24));
        }
        const double nx = (c.build_unit || wonder) ? rx + 82 : rx + 12;
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
        wide_button(cr, rx, ry, bw, 34, "Change", "C", false, hover(rx, ry, bw, 34), !ro);
        const int cid = c.id;
        if (!ro) hot(rx, ry, bw, 34, [this, cid] {
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
        if (can_buy && !ro) hot(rx + bw + 8, ry, bw, 34, [this, cid] { g().buy(cid); });
        ry += 38;
        if (price > 0) {
            // How the price comes about (the DOS formula).
            const int r = std::clamp(cost - c.shields, 0, 999);
            std::string how = c.build_unit ? std::to_string(r) + "\u00b2/20 + 2\u00d7" + std::to_string(r)
                                           : (wonder ? "4\u00d7" : "2\u00d7") + std::to_string(r);
            how += " shields left";
            if (c.shields == 0) how += ", doubled: nothing built yet";
            text(cr, kTagFont, "Buy: " + how + (c.bought ? " \u00b7 bought this turn" : ""), rx + 2, ry, kSlateDim);
        }
        ry += 18;
    }
    section(cr, "IMPROVEMENTS", rx, ry, rr);
    ry += 20;
    {
        int n = 0;
        for (int b = 0; b < kBuildingCount; ++b) {
            if (!c.has(static_cast<Building>(b))) continue;
            if (ry > y + h - 80) break;
            text(cr, kUiFont, building_info(b).name, rx + 4, ry, kSlateInk);
            const int up = g().building_upkeep(c.owner, b);
            const bool sell = !ro && c.owner == human() && g().can_sell(c, b);
            if (up) text_right(cr, kTagFont, std::to_string(up) + "g", sell || (c.sold && b != 0) ? rr - 86 : rr, ry + 3, kSlateDim);
            if (sell) {
                const std::string lab = "Sell " + with_commas(building_info(b).cost) + "g";
                const bool lit = hover(rr - 78, ry - 2, 78, 22);
                round_rect(cr, rr - 78, ry - 2, 78, 22, 4);
                set(cr, lit ? kSlateGoldDim : kSlateDeep);
                cairo_fill(cr);
                text_center(cr, kTagFont, lab, rr - 39, ry + 2, lit ? kSlateDeep : kSlateGold);
                const int cid2 = c.id;
                hot(rr - 78, ry - 2, 78, 22, [this, cid2, b] {
                    Dialog q;
                    q.kind = DialogKind::ConfirmSell;
                    q.title = "Sell Improvement";
                    q.city_id = cid2;
                    q.other = b;
                    open(q);
                });
            }
            ry += 22;
            ++n;
        }
        for (int wv = 0; wv < kWonderCount; ++wv) {
            if (g().wonder_city[wv] != c.id) continue;
            if (ry > y + h - 92) break;
            paint_pixmap(cr, art::wonder_icon(wv), rx + 2, ry, 1);
            const bool obs = g().wonder_obsolete(static_cast<Wonder>(wv));
            text(cr, kBoldFont, wonder_info(wv).name, rx + 42, ry + 6, obs ? kSlateDim : kSlateGold);
            text_right(cr, kTagFont, obs ? "obsolete" : "wonder", rr, ry + 9, kSlateDim);
            ry += 36;
            ++n;
        }
        if (!n) text(cr, kUiFont, "None yet.", rx + 4, ry, kSlateDim);
        if (c.sold && !ro) text(cr, kTagFont, "An improvement was sold here this turn.", rx + 4, ry + 2, kSlateDim);
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
    bool wonder_header = false;
    for (int wv = 0; wv < kWonderCount; ++wv) {
        const int b = kWonderBase + wv;
        if (!g().can_build(c, false, b)) continue;
        if (!wonder_header) {
            by += 6;
            section(cr, "WONDERS OF THE WORLD", M, by, M + colw);
            by += 20;
            wonder_header = true;
        }
        if (by + 36 > y + h - 70) break;
        const bool cur = !c.build_unit && c.build == b;
        const bool hv = hover(M, by, colw, 34);
        round_rect(cr, M, by, colw, 34, 5);
        set(cr, cur ? hex(0x3E4A5E) : (hv ? hex(0x354154) : hex(0x29323F)));
        cairo_fill(cr);
        paint_pixmap(cr, art::wonder_icon(wv), M + 1, by + 1, 1);
        const BuildingInfo &bi = building_info(b);
        text(cr, kBoldFont, bi.name, M + 42, by + 7, cur ? kSlateGold : kSlateInk);
        text_right(cr, kUiFont, std::to_string(bi.cost) + " · " + eta(bi.cost), M + colw - 10, by + 8, kSlateDim);
        const int cid = c.id;
        hot(M, by, colw, 34, [this, cid, b] {
            g().set_production(cid, false, b);
            close_top();
        });
        by += 38;
    }
    bool space_header = false;
    for (int part = 0; part < kSpacePartCount; ++part) {
        const int b = kSpaceBase + part;
        if (!g().can_build(c, false, b)) continue;
        if (!space_header) {
            by += 6;
            section(cr, "SPACESHIP", M, by, M + colw);
            by += 20;
            space_header = true;
        }
        if (by + 36 > y + h - 70) break;
        const bool cur = !c.build_unit && c.build == b;
        const bool hv = hover(M, by, colw, 34);
        round_rect(cr, M, by, colw, 34, 5);
        set(cr, cur ? hex(0x3E4A5E) : (hv ? hex(0x354154) : hex(0x29323F)));
        cairo_fill(cr);
        paint_pixmap(cr, art::space_part_icon(part), M + 1, by + 1, 1);
        const BuildingInfo &bi = building_info(b);
        text(cr, kBoldFont, bi.name, M + 42, by + 7, cur ? kSlateGold : kSlateInk);
        text_right(cr, kUiFont, std::to_string(bi.cost) + " · " + eta(bi.cost), M + colw - 10, by + 8, kSlateDim);
        const int cid = c.id;
        hot(M, by, colw, 34, [this, cid, b] {
            g().set_production(cid, false, b);
            close_top();
        });
        by += 38;
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
                          " advances. The next one costs " + std::to_string(g().research_cost(me)) + " bulbs.",
         L, cy, kSlateDim);
    cy += 22;
    {
        // The DOS formula, spelled out: known x bulbs per advance, x2 in AD.
        const bool ad = year_for_turn(g().turn) > 0;
        const int known = g().techs_known(me), ratio = g().research_ratio(me);
        std::string f = "(" + std::to_string(known) + " known \u00d7 " + std::to_string(ratio) + " + 1)";
        if (ad) f += " \u00d7 2 in AD";
        f += " \u00b7 " + std::to_string(ratio) + " a step on " + difficulty_name(g().difficulty);
        if (g().tech_catchup) f += ", " + std::to_string(g().tech_catchup) + " of it catch-up";
        if ((ad ? 2 : 1) * (known * ratio + 1) < Game::kMinResearchCost)
            f += " \u00b7 at least " + std::to_string(Game::kMinResearchCost);
        text(cr, kUiFont, f, L, cy, kSlateDim);
        cy += 24;
    }
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
            if (ct.has(static_cast<Building>(b))) upkeep += g().building_upkeep(me, b);
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
    const double cols[] = {L, L + 190, L + 240, L + 300, L + 370, L + 430, L + 540};
    const char *heads[] = {"CITY", "SIZE", "FOOD", "SHIELDS", "TRADE", "CORRUPT", "PRODUCING"};
    for (int i = 0; i < 7; ++i) text(cr, kTagFont, heads[i], cols[i], cy, kSlateDim);
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
        // Corruption, with the distance it is worked out from.
        text(cr, kUiFont, std::to_string(s.corruption) + "  \u00b7 " + (c->has(Building::Palace) && g().civs[me].govt != Government::Communism
                                                                          ? std::string("Palace")
                                                                          : std::to_string(s.palace_distance) + " sq"),
             cols[5], cy + 4, s.corruption ? kSlateInk : kSlateDim);
        const std::string prod = c->build_unit ? unit_info(c->build).name : building_info(c->build).name;
        text(cr, kUiFont, prod + "  (" + std::to_string(c->shields) + "/" + std::to_string(g().production_cost(*c)) + ")", cols[6], cy + 4,
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
    int rivals = 0;
    for (const Civ &c : g().civs) rivals += !c.barbarian;
    const double w = 760, h = 160 + (rivals - 1) * 64.0, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, R = x + w - 24;
    for (int k = 0; k < static_cast<int>(g().civs.size()); ++k) {
        if (k == me || g().civs[k].barbarian) continue;   // raiders keep no embassies
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
        std::string status = !c.alive ? "Destroyed" : (!met ? "Not yet met" : treaty_name(g().treaty(me, k)));
        if (met && c.alive) status += " \u00b7 " + std::to_string(ncity) + " cities \u00b7 " + government_info(c.govt).name;
        const bool embassy = (g().civs[me].embassy >> k) & 1u;
        if (met && c.alive && embassy) {
            status += " \u00b7 " + with_commas(c.gold) + " gold \u00b7 " + std::to_string(g().techs_known(k)) + " advances";
            if (c.research != Advance::None) status += std::string(" \u00b7 studying ") + advance_info(c.research).name;
        }
        if (met && c.alive) {
            text_right(cr, kTagFont, embassy ? "EMBASSY" : "NO EMBASSY", R - 182, cy + 10, embassy ? kSlateGood : kSlateEdge);
        }
        text(cr, kUiFont, status, L + 50, cy + 30, g().at_war(me, k) ? kSlateBad : kSlateDim);
        if (met && c.alive) {
            const double bx = R - 170;
            wide_button(cr, bx, cy + 10, 160, 36, "Talk", "", false, hover(bx, cy + 10, 160, 36));
            hot(bx, cy + 10, 160, 36, [this, k] {
                Dialog a;
                a.kind = DialogKind::Audience;
                a.title = "Audience";
                a.other = k;
                a.lines = {g().leader_greeting(k) + " receives your envoy."};
                open(a);
            });
        }
        cy += 64;
    }
    if (g().senate(me))
        text(cr, kUiFont, "Your Senate accepts every offer of peace, and will not let you break a treaty.", L, y + h - 46, kSlateDim, 1.0,
             static_cast<int>(R - L - 160));
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
        if (unit_info(u).flags & kNoBuild) continue;
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
    if (won && g().space_win) {
        msg = "Your colonists have reached Alpha Centauri. The " + g().civs[human()].name + " begin a new world among the stars, and your name, " +
              g().civs[human()].leader + ", goes with them.";
    } else if (g().space_win && g().winner >= 0) {
        msg = "The " + g().civs[g().winner].name + " reached Alpha Centauri first. The future belongs to them.";
    } else if (won) {
        msg = "The " + g().civs[human()].name + " rule the whole world. Your name, " + g().civs[human()].leader +
              ", will be remembered for all time.";
    } else if (g().human_dead) {
        msg = "The " + g().civs[human()].name + " have been destroyed. History will forget them.";
    } else if (g().winner >= 0) {
        msg = "The " + g().civs[g().winner].name + " have conquered the world.";
    }
    text(cr, kUiFont, msg, x + 24, cy, kSlateInk, 1.0, static_cast<int>(w - 48));
    const double fy = cy + measure(cr, kUiFont, msg, static_cast<int>(w - 48)).h + 14;
    text(cr, kUiFont, "Final year: " + year_text(year_for_turn(g().turn)), x + 24, std::max(cy + 70, fy), kSlateDim);
    const double bx = x + w - 24 - 160, by = y + h - 54;
    wide_button(cr, bx, by, 160, 38, "New Game", "", true, hover(bx, by, 160, 38));
    hot(bx, by, 160, 38, [this] { open_new_game_dialog(); });
    wide_button(cr, bx - 170, by, 160, 38, "Look Around", "Esc", false, hover(bx - 170, by, 160, 38));
    hot(bx - 170, by, 160, 38, [this] { dialogs_.pop_back(); });
}

void App::draw_help(cairo_t *cr, Dialog &d)
{
    const double w = std::min(1140, geo_.win_w - 16), h = 600, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, mid = x + w * 0.54;
    section(cr, "KEYBOARD AND MOUSE", L, cy, mid - 24);
    section(cr, "CONTROLLER", mid, cy, x + w - 24);
    cy += 22;
    const std::pair<const char *, const char *> rows[] = {
        {"Arrows, keypad", "Move the active unit (7 9 1 3 diagonals)"},
        {"B", "Found a city (or join one)"},
        {"R  I  M  P", "Road/railroad, irrigate, mine, fortress"},
        {"F  S", "Fortify, sentry"},
        {"G", "Go to a square (then click it)"},
        {"W  Space  N", "Wait, skip this turn, next unit"},
        {"Shift+D", "Disband"},
        {"Shift+P  Shift+H  U", "Pillage, make this city home, unload a ship"},
        {"Enter", "End the turn"},
        {"C  H", "Center on the unit, on the capital"},
        {"+  -  wheel", "Zoom"},
        {"T  A  V", "Tax rates, science, cities"},
        {"O  Y  E", "Wonders, spaceship, Databank"},
        {"Ctrl+S  Ctrl+L", "Save, load"},
        {"Click", "Select a unit, open a city, move next door"},
        {"Right click", "Center the map there"},
    };
    double ly = cy;
    for (const auto &r : rows) {
        text(cr, kBoldFont, r.first, L, ly, kSlateGold);
        text(cr, kUiFont, r.second, L + 150, ly, kSlateInk, 1.0, static_cast<int>(mid - 24 - L - 150));
        ly += 28;
    }
    const std::pair<std::string, std::string> pads[] = {
        {"D-pad, left stick", "Move the cursor; in menus and dialogs, the focus"},
        {glyphs_.a, "Next door: move or attack. On the unit: its orders. A city: open it. Further: go there"},
        {glyphs_.b, "Back, close; on the map, back to the unit, then wait"},
        {glyphs_.x, "Orders menu"},
        {glyphs_.y, "Settlers found a city; troops fortify"},
        {glyphs_.lb + "  " + glyphs_.rb, "Previous, next unit"},
        {glyphs_.view, "End the turn"},
        {glyphs_.menu, "Menus (OK in dialogs)"},
        {"Triggers", "Zoom out, in"},
        {"Right stick", "Scroll the map"},
        {"Stick clicks", "Center on the unit, on the cursor"},
    };
    double py = cy;
    for (const auto &r : pads) {
        text(cr, kBoldFont, r.first, mid, py, kSlateGold);
        text(cr, kUiFont, r.second, mid + 150, py, kSlateInk, 1.0, static_cast<int>(x + w - 24 - mid - 150));
        py += std::max(28.0, measure(cr, kUiFont, r.second, static_cast<int>(x + w - 24 - mid - 150)).h + 8.0);
    }
    const double bx = x + w - 24 - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Close", "Esc", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

// A Wonder of the World completed somewhere: the plaque, large, with the news.
void App::draw_wonder_built(cairo_t *cr, Dialog &d)
{
    const int wv = std::clamp(d.other, 0, kWonderCount - 1);
    const WonderInfo &wi = wonder_info(wv);
    const double w = 640, h = 380, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, R = x + w - 24;
    // The plaque at 6x on a deep well with a gold rule.
    round_rect(cr, L, cy, 208, 208, 8);
    set(cr, kSlateDeep);
    cairo_fill(cr);
    paint_pixmap(cr, art::wonder_icon(wv), L + 8, cy + 8, 6);
    const double tx = L + 228, tw = R - tx;
    text(cr, kBoldFont, wi.name, tx, cy + 4, kSlateGold, 1.0, static_cast<int>(tw));
    double ty = cy + 4 + measure(cr, kBoldFont, wi.name, static_cast<int>(tw)).h + 10;
    const std::string news = d.lines.empty() ? std::string() : d.lines.front();
    text(cr, kUiFont, news, tx, ty, kSlateInk, 1.0, static_cast<int>(tw));
    ty += measure(cr, kUiFont, news, static_cast<int>(tw)).h + 14;
    fill(cr, tx, ty, tw, 1, kSlateEdge);
    ty += 10;
    text(cr, kTagFont, "EFFECT", tx, ty, kSlateDim);
    ty += 16;
    text(cr, kUiFont, wi.effect, tx, ty, kSlateInk, 1.0, static_cast<int>(tw));
    ty += measure(cr, kUiFont, wi.effect, static_cast<int>(tw)).h + 10;
    const std::string ends = wi.obsolete == Advance::None ? std::string("Never obsolete.")
                                                          : std::string("Obsolete once its owner learns ") + advance_info(wi.obsolete).name + ".";
    text(cr, kTagFont, ends, tx, ty, kSlateDim, 1.0, static_cast<int>(tw));
    const double bx = R - 140, by = y + h - 56;
    wide_button(cr, bx, by, 140, 38, "OK", "Enter", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

// The Wonders advisor: all 21, who holds each, what is under way.
void App::draw_wonders(cairo_t *cr, Dialog &d)
{
    const int me = human();
    const double w = std::min(1180, geo_.win_w - 16), h = std::min(784, geo_.win_h - 16);
    const double x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 16;
    const int cols = 3;
    const int rows = (kWonderCount + cols - 1) / cols;
    const double cw = (w - 32 - (cols - 1) * 8) / cols;
    const double rh = std::min(86.0, (h - 54 - 70 - (rows - 1) * 6) / rows);
    int mine = 0, built = 0;
    for (int wv = 0; wv < kWonderCount; ++wv) {
        const Wonder ww = static_cast<Wonder>(wv);
        const double cx = L + (wv % cols) * (cw + 8), ry = cy + (wv / cols) * (rh + 6);
        const WonderInfo &wi = wonder_info(wv);
        const City *home = g().wonder_home(ww);
        const bool lost = g().wonder_built(ww) && !home;
        const bool obs = g().wonder_obsolete(ww);
        built += g().wonder_built(ww);
        const bool ours = home && home->owner == me;
        mine += ours;
        round_rect(cr, cx, ry, cw, rh, 6);
        set(cr, ours ? hex(0x323C4C) : hex(0x29323F));
        cairo_fill(cr);
        const int scale = rh >= 76 ? 2 : 1;
        const double icon = 32.0 * scale;
        paint_pixmap(cr, art::wonder_icon(wv), cx + 6, ry + (rh - icon) / 2, scale);
        if (!g().wonder_built(ww) && !g().has(me, wi.tech)) {
            // Not yet within reach: dim the plaque.
            cairo_rectangle(cr, cx + 6, ry + (rh - icon) / 2, icon, icon);
            cairo_set_source_rgba(cr, 0.08, 0.1, 0.14, 0.55);
            cairo_fill(cr);
        }
        const double tx = cx + 14 + icon, tw = cw - (tx - cx) - 8;
        text(cr, kBoldFont, wi.name, tx, ry + 6, ours ? kSlateGold : kSlateInk, 1.0, static_cast<int>(tw));
        std::string status;
        Ink col = kSlateDim;
        if (lost) {
            status = "Destroyed with its city";
            col = kSlateBad;
        } else if (home) {
            const bool met = home->owner == me || ((g().civs[me].contact >> home->owner) & 1u);
            status = (met ? "The " + g().civs[home->owner].name : std::string("Unknown people")) + " \u00b7 " + (met ? home->name : "?");
            if (obs) status += " \u00b7 obsolete";
            col = ours ? kSlateGood : kSlateInk;
        } else {
            const City *ours_building = nullptr;
            for (const City &c : g().cities) {
                if (c.owner == me && !c.build_unit && c.build == kWonderBase + wv) ours_building = &c;
            }
            if (ours_building) {
                status = "Building in " + ours_building->name + " (" + std::to_string(ours_building->shields) + "/" +
                         std::to_string(g().production_cost(*ours_building)) + ")";
                col = kSlateGold;
            } else if (g().has(me, wi.tech)) {
                status = "Available \u00b7 " + std::to_string(wi.cost) + " shields";
                col = kSlateGood;
            } else {
                status = std::string("Needs ") + advance_info(wi.tech).name;
            }
        }
        text(cr, kTagFont, status, tx, ry + 28, col, 1.0, static_cast<int>(tw));
        if (rh >= 60) text(cr, kTagFont, wi.effect, tx, ry + 44, kSlateDim, 1.0, static_cast<int>(tw));
    }
    text(cr, kUiFont,
         std::to_string(built) + " of " + std::to_string(kWonderCount) + " wonders built \u00b7 " + std::to_string(mine) + (mine == 1 ? " is yours." : " are yours."),
         x + 24, y + h - 46, kSlateDim);
    const double bx = x + w - 24 - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Close", "Esc", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

// A Diplomat faces a foreign city or unit: the six actions of the DOS game.
void App::draw_diplomat_menu(cairo_t *cr, Dialog &d)
{
    const Unit *u = g().unit(d.unit_id);
    if (!u) {
        close_top();
        return;
    }
    const int me = human();
    const City *c = g().city_at(d.tx, d.ty);
    const int them = c ? c->owner : g().owner_at(d.tx, d.ty);
    const double w = 700, h = 520, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    std::string where;
    if (c) where = c->name + " \u00b7 the " + g().civs[them].name;
    else if (them >= 0) where = "a unit of the " + g().civs[them].name;
    double cy = dialog_frame(cr, x, y, w, h, "Diplomat: " + where);
    const double L = x + 24, R = x + w - 24;
    paint_pixmap(cr, art::unit_card(UnitType::Diplomat, civ_colors(g().civs[me].color)), L, cy, 2);
    const bool war = them >= 0 && g().at_war(me, them);
    text(cr, kUiFont,
         std::string(war ? "We are at war with them." : "We are at peace with them: hostile acts (marked) mean war.") +
             " The Diplomat is spent on any action. Treasury: " + with_commas(g().civs[me].gold) + " gold.",
         L + 76, cy + 4, kSlateDim, 1.0, static_cast<int>(R - L - 76));
    cy += 72;
    for (int a = 0; a < static_cast<int>(DiploAction::Count); ++a) {
        const DiploAction act = static_cast<DiploAction>(a);
        const bool ok = g().diplomat_can(d.unit_id, d.tx, d.ty, act);
        std::string note;
        switch (act) {
        case DiploAction::Embassy:
            note = c && ((g().civs[me].embassy >> them) & 1u) ? "We already have an embassy." : "See their treasury, advances and plans.";
            break;
        case DiploAction::Investigate: note = "A look inside the city."; break;
        case DiploAction::StealTech: note = c && ((c->stolen >> me) & 1u) ? "Already robbed by our agents." : "Take one advance they know and we don't."; break;
        case DiploAction::Sabotage: note = "Destroy production or a building."; break;
        case DiploAction::Incite: {
            const int cost = c ? g().incite_cost(*c, me) : -1;
            note = !c ? "Only a city can revolt." : (cost < 0 ? "A capital never revolts." : "The city joins us for " + with_commas(cost) + " gold.");
            break;
        }
        case DiploAction::Bribe: {
            const std::vector<int> here = g().units_at(d.tx, d.ty);
            if (c || here.size() != 1) note = "Only a lone unit outside a city.";
            else if (const Unit *v = g().unit(here.front())) note = std::string("The ") + unit_info(v->type).name + " joins us for " + with_commas(g().bribe_cost(*v, me)) + " gold.";
            break;
        }
        default: break;
        }
        const bool hv = ok && hover(L, cy, R - L, 48);
        round_rect(cr, L, cy, R - L, 48, 6);
        set(cr, hv ? hex(0x354154) : hex(0x29323F));
        cairo_fill(cr);
        text(cr, kSmallBold, std::to_string(a + 1), L + 12, cy + 15, ok ? kSlateGold : kSlateEdge);
        text(cr, kBoldFont, diplo_action_name(act), L + 34, cy + 5, ok ? kSlateInk : kSlateEdge);
        text(cr, kTagFont, note, L + 34, cy + 28, ok ? kSlateDim : kSlateEdge);
        if (g().diplomat_hostile(act) && !war) text_right(cr, kTagFont, "ACT OF WAR", R - 12, cy + 16, ok ? kSlateBad : kSlateEdge);
        if (ok) {
            const int uid = d.unit_id, tx = d.tx, ty = d.ty;
            hot(L, cy, R - L, 48, [this, uid, tx, ty, act] { diplomat_choose(uid, tx, ty, act); });
        }
        cy += 54;
    }
    const double bx = R - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Cancel", "Esc", false, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

void App::diplomat_choose(int unit_id, int tx, int ty, DiploAction a)
{
    if (!g().diplomat_can(unit_id, tx, ty, a)) return;
    close_top();
    const City *c = g().city_at(tx, ty);
    const int cid = c ? c->id : -1;
    g().diplomat_act(unit_id, tx, ty, a);
    refresh_terrain();
    if (a == DiploAction::Investigate && cid >= 0) {
        collect_news(false);
        dialogs_.clear();
        Dialog v;
        v.kind = DialogKind::City;
        v.city_id = cid;
        v.view_only = true;
        open(v);
    } else {
        collect_news(false);
    }
    select_next(true);
}

// A Caravan reaches a city: help with a wonder, open a trade route, or go on.
void App::draw_caravan_menu(cairo_t *cr, Dialog &d)
{
    const Unit *u = g().unit(d.unit_id);
    const City *c = g().city(d.city_id);
    if (!u || !c) {
        close_top();
        return;
    }
    const int me = human();
    const double w = 660, h = 360, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, "Caravan in " + c->name);
    const double L = x + 24, R = x + w - 24;
    paint_pixmap(cr, art::unit_card(UnitType::Caravan, civ_colors(g().civs[me].color)), L, cy, 2);
    const City *home = g().city(u->home);
    text(cr, kUiFont,
         std::string("From ") + (home ? home->name : std::string("no home city")) + ". " +
             (c->owner == me ? "One of our cities." : "A city of the " + g().civs[c->owner].name + "."),
         L + 76, cy + 4, kSlateDim, 1.0, static_cast<int>(R - L - 76));
    cy += 72;
    struct Opt {
        std::string name, note;
        bool ok;
    };
    std::vector<Opt> opts;
    const bool help = g().caravan_can_help(*u, *c);
    opts.push_back({"Help Build Wonder",
                    help ? "Adds " + std::to_string(unit_info(UnitType::Caravan).cost) + " shields to " + wonder_title(c->build - kWonderBase) + "."
                         : "The city is not building a Wonder of the World.",
                    help});
    const bool trade = g().caravan_can_trade(*u, *c);
    std::string tnote;
    if (trade) {
        tnote = "About " + std::to_string(g().trade_bonus(*u, *c)) + " gold and bulbs now, and +" + std::to_string(g().route_value(*home, *c)) +
                " trade a turn in " + home->name + ".";
    } else if (!home) {
        tnote = "The Caravan has no home city.";
    } else if (g().map.distance(home->x, home->y, c->x, c->y) < 9) {
        tnote = "Too close to " + home->name + " (9 squares or more).";
    } else {
        tnote = home->name + " has no room for another route with this city.";
    }
    opts.push_back({"Establish Trade Route", tnote, trade});
    if (c->owner == me) opts.push_back({"Keep Moving", "Stay in the city for now.", true});
    for (size_t i = 0; i < opts.size(); ++i) {
        const bool hv = opts[i].ok && hover(L, cy, R - L, 48);
        round_rect(cr, L, cy, R - L, 48, 6);
        set(cr, hv ? hex(0x354154) : hex(0x29323F));
        cairo_fill(cr);
        text(cr, kSmallBold, std::to_string(i + 1), L + 12, cy + 15, opts[i].ok ? kSlateGold : kSlateEdge);
        text(cr, kBoldFont, opts[i].name, L + 34, cy + 5, opts[i].ok ? kSlateInk : kSlateEdge);
        text(cr, kTagFont, opts[i].note, L + 34, cy + 28, opts[i].ok ? kSlateDim : kSlateEdge);
        if (opts[i].ok) {
            const int uid = d.unit_id, cid = d.city_id, k = static_cast<int>(i);
            hot(L, cy, R - L, 48, [this, uid, cid, k] { caravan_choose(uid, cid, k); });
        }
        cy += 54;
    }
    const double bx = R - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Cancel", "Esc", false, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

void App::caravan_choose(int unit_id, int city_id, int choice)
{
    const Unit *u = g().unit(unit_id);
    const City *c = g().city(city_id);
    if (!u || !c) return;
    if (choice == 0 && !g().caravan_can_help(*u, *c)) return;
    if (choice == 1 && !g().caravan_can_trade(*u, *c)) return;
    if (choice == 2 && c->owner != human()) return;
    close_top();
    if (choice == 0) g().caravan_help(unit_id, city_id);
    else if (choice == 1) g().caravan_trade(unit_id, city_id);
    else {
        if (Unit *m = g().unit(unit_id)) m->done = true;
    }
    collect_news(false);
    select_next(true);
}

// The spaceship: a blueprint of the ship as built so far, its numbers, the
// launch button, and what rivals have on the pad or in flight.
void App::draw_spaceship(cairo_t *cr, Dialog &d)
{
    const int me = human();
    const Civ &c = g().civs[me];
    const SpaceStats st = g().space_stats(me);
    const double w = std::min(1180, geo_.win_w - 16), h = std::min(560, geo_.win_h - 16);
    const double x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 20, R = x + w - 20;
    const bool flying = c.launch_turn >= 0;
    // Blueprint well.
    const int sc = 2;
    const double bw = art::spaceship_width() * sc + 24, bh = art::spaceship_height() * sc + 24;
    round_rect(cr, L, cy, bw, bh, 8);
    set(cr, hex(0x0E1424));
    cairo_fill(cr);
    cairo_save(cr);
    round_rect(cr, L, cy, bw, bh, 8);
    cairo_clip(cr);
    for (double gx = L + 12; gx < L + bw; gx += 16.0 * sc) fill(cr, gx, cy, 1, bh, hex(0x16203A));
    for (double gy = cy + 12; gy < cy + bh; gy += 16.0 * sc) fill(cr, L, gy, bw, 1, hex(0x16203A));
    for (int k = 0; k < 40; ++k) {
        const double sx = L + 6 + std::fmod(k * 197.3, bw - 12), sy = cy + 6 + std::fmod(k * 71.9, bh - 12);
        fill(cr, std::round(sx), std::round(sy), k % 5 ? 2 : 3, k % 5 ? 2 : 3, k % 3 ? hex(0x8A9CC8) : hex(0xE8ECF8));
    }
    cairo_restore(cr);
    paint_pixmap(cr, art::spaceship(c.structurals, c.components, c.modules, flying), L + 12, cy + 12, sc);
    text(cr, kTagFont, flying ? "IN FLIGHT" : (c.structurals + c.components + c.modules ? "ON THE LAUNCH PAD" : "BLUEPRINT"), L + 14, cy + 8,
         flying ? kSlateGold : kSlateDim);

    // Numbers.
    const double sx = L + bw + 20, sw = R - sx;
    double sy = cy;
    auto row = [&](const std::string &label, const std::string &value, Ink ink) {
        text(cr, kTagFont, label, sx, sy + 3, kSlateDim);
        text_right(cr, kBoldFont, value, R, sy, ink);
        sy += 24;
    };
    section(cr, "PARTS", sx, sy, R);
    sy += 20;
    row("STRUCTURALS", std::to_string(c.structurals) + " / " + std::to_string(kMaxStructurals), kSlateInk);
    row("COMPONENTS", std::to_string(c.components) + " / " + std::to_string(kMaxComponents), kSlateInk);
    text(cr, kUiFont, std::to_string(st.fuel) + " fuel \u00b7 " + std::to_string(st.propulsion) + " propulsion", sx + 12, sy - 4, kSlateDim);
    sy += 18;
    row("MODULES", std::to_string(c.modules) + " / " + std::to_string(kMaxModules), kSlateInk);
    text(cr, kUiFont, std::to_string(st.habitation) + " habitation \u00b7 " + std::to_string(st.life_support) + " life \u00b7 " + std::to_string(st.solar) + " solar",
         sx + 12, sy - 4, kSlateDim);
    sy += 24;
    section(cr, "FLIGHT", sx, sy, R);
    sy += 20;
    row("COLONISTS", with_commas(st.population), kSlateInk);
    row("LIFE SUPPORT", std::to_string(st.life_pct) + "%", st.life_pct >= 100 ? kSlateGood : kSlateBad);
    row("ENERGY", std::to_string(st.energy_pct) + "%", st.energy_pct >= 100 ? kSlateGood : kSlateBad);
    row("MASS", with_commas(st.mass) + " tons", kSlateInk);
    char yrs[32];
    std::snprintf(yrs, sizeof yrs, "%.1f years", st.years);
    row("FLIGHT TIME", st.pairs ? yrs : "\u2014", kSlateInk);
    row("SUCCESS", std::to_string(st.success_pct) + "%", st.success_pct >= 75 ? kSlateGood : (st.success_pct > 0 ? kSlateGold : kSlateBad));
    sy += 4;
    std::string status;
    if (flying) {
        status = "Arrives at Alpha Centauri in " + year_text(g().arrival_year(me)) + ". Lose the capital and the ship is lost.";
    } else if (!g().space_open()) {
        status = "No spaceship can be built until the Apollo Program stands somewhere in the world.";
    } else if (st.unsupported > 0) {
        status = "Needs " + std::to_string(st.unsupported) + " more structural" + (st.unsupported == 1 ? "" : "s") + " to hold every part.";
    } else if (!st.viable) {
        status = "Needs at least one fuel and propulsion pair and one of each module.";
    } else {
        status = "Ready to launch from the capital.";
    }
    text(cr, kUiFont, status, sx, sy, flying || st.viable ? kSlateGold : kSlateDim, 1.0, static_cast<int>(sw));

    // Rivals.
    double ry = cy + bh + 14;
    section(cr, "OTHER SPACESHIPS", L, ry, L + bw);
    ry += 20;
    bool any = false;
    for (int k = 0; k < static_cast<int>(g().civs.size()); ++k) {
        const Civ &o = g().civs[k];
        if (k == me || o.barbarian || !o.alive || !(o.structurals + o.components + o.modules)) continue;
        any = true;
        const std::string what = o.launch_turn >= 0 ? "in flight, arriving " + year_text(g().arrival_year(k))
                                                    : std::to_string(o.structurals + o.components + o.modules) + " parts on the pad";
        round_rect(cr, L, ry + 3, 12, 12, 3);
        set_argb(cr, civ_colors(o.color).main);
        cairo_fill(cr);
        text(cr, kBoldFont, o.name, L + 20, ry, kSlateInk);
        text(cr, kUiFont, what, L + 220, ry + 1, o.launch_turn >= 0 ? kSlateBad : kSlateDim);
        ry += 22;
    }
    if (!any) text(cr, kUiFont, "No rival has begun a spaceship.", L, ry, kSlateDim);

    const double by = y + h - 54;
    const bool can = g().can_launch(me);
    double bx = R - 140;
    wide_button(cr, bx, by, 140, 38, "Close", "Esc", false, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
    if (!flying) {
        bx -= 170;
        wide_button(cr, bx, by, 160, 38, "Launch", "L", true, can && hover(bx, by, 160, 38), can);
        if (can) hot(bx, by, 160, 38, [this] { launch_spaceship(); });
    }
}

void App::launch_spaceship()
{
    const int me = human();
    if (!g().launch(me)) return;
    collect_news(false);
}


// ---------------------------------------------------------------- M4 dialogs

void App::draw_confirm_sell(cairo_t *cr, Dialog &d)
{
    const City *c = g().city(d.city_id);
    if (!c || d.other < 0) {
        close_top();
        return;
    }
    const double w = 560, h = 220, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const BuildingInfo &b = building_info(d.other);
    text(cr, kUiFont,
         std::string("Sell the ") + b.name + " in " + c->name + " for " + with_commas(b.cost) + " gold? A city may sell one improvement a turn.",
         x + 24, cy, kSlateInk, 1.0, static_cast<int>(w - 48));
    const double bx = x + w - 24 - 160, by = y + h - 54;
    wide_button(cr, bx, by, 160, 38, "Sell", "Enter", true, hover(bx, by, 160, 38));
    hot(bx, by, 160, 38, [this] {
        KeyEvent k;
        k.key = Key::Enter;
        key(k);
    });
    wide_button(cr, bx - 170, by, 160, 38, "Keep It", "Esc", false, hover(bx - 170, by, 160, 38));
    hot(bx - 170, by, 160, 38, [this] { close_top(); });
}

// The leader's portrait in a gilt frame.
static void portrait(cairo_t *cr, const Game &g, int civ, int mood, double x, double y, int scale)
{
    const Civ &c = g.civs[civ];
    round_rect(cr, x - 6, y - 6, 32 * scale + 12, 32 * scale + 12, 6);
    set(cr, kSlateGoldDim);
    cairo_fill(cr);
    paint_pixmap(cr, art::leader_portrait(c.tribe, civ_colors(c.color), mood), x, y, scale);
}

void App::draw_talk(cairo_t *cr, Dialog &d)
{
    if (g().talks.empty()) {
        dialogs_.pop_back();
        return;
    }
    const Talk &t = g().talks.front();
    const bool choice = talk_has_choice(t);
    const bool angry = t.kind == TalkKind::Tribute || t.kind == TalkKind::TechDemand || t.kind == TalkKind::WarDeclared ||
                       t.kind == TalkKind::TreatyCancelled || t.kind == TalkKind::CeasefireOver;
    const double w = std::min(860, geo_.win_w - 20), h = 420, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, "Audience with the " + g().civs[t.from].name);
    const int scale = 6;
    portrait(cr, g(), t.from, angry ? -1 : (t.kind == TalkKind::Greeting || t.kind == TalkKind::Embassy ? 1 : 0), x + 30, cy + 6, scale);
    const double tx = x + 30 + 32 * scale + 30, tw = x + w - 24 - tx;
    text(cr, kBoldFont, g().leader_greeting(t.from), tx, cy + 4, kSlateGold);
    const Civ &them = g().civs[t.from];
    std::string sub = treaty_name(g().treaty(human(), t.from));
    if (them.tribe >= 0) {
        const TribeInfo &ti = tribe_info(them.tribe);
        sub += std::string(" \u00b7 ") + mood_name(ti.mood) + ", " + policy_name(ti.policy) + ", " + ideology_name(ti.ideology);
    }
    text(cr, kTagFont, sub, tx, cy + 28, kSlateDim);
    text(cr, kUiFont, "\u201c" + t.text + "\u201d", tx, cy + 56, kSlateInk, 1.0, static_cast<int>(tw));
    if (t.forced) text(cr, kBoldFont, "The Senate has spoken.", tx, cy + 210, kSlateGoldDim);
    int left = 0;
    for (const Dialog &o : dialogs_) left += o.kind == DialogKind::Talk;
    if (left > 1) text(cr, kTagFont, std::to_string(left - 1) + " more waiting", x + 24, y + h - 46, kSlateDim);
    const double bw = 220, by = y + h - 56, bx = x + w - 24 - bw;
    if (choice) {
        std::string yes = "Accept", no = "Refuse";
        switch (t.kind) {
        case TalkKind::Tribute: yes = "Pay " + with_commas(t.gold) + " gold"; break;
        case TalkKind::TechDemand: yes = std::string("Give ") + advance_info(t.tech).name; break;
        case TalkKind::Ceasefire: yes = "Agree to cease-fire"; no = "Fight on"; break;
        case TalkKind::Peace: yes = "Sign the treaty"; no = "Decline"; break;
        case TalkKind::TechForPeace: yes = "Make peace"; no = "Decline"; break;
        case TalkKind::Embassy: yes = "Exchange embassies"; no = "Decline"; break;
        default: break;
        }
        wide_button(cr, bx, by, bw, 38, yes, "Y", true, hover(bx, by, bw, 38));
        hot(bx, by, bw, 38, [this] { answer_talk(true); });
        wide_button(cr, bx - bw - 10, by, bw, 38, no, "N", false, hover(bx - bw - 10, by, bw, 38));
        hot(bx - bw - 10, by, bw, 38, [this] { answer_talk(false); });
    } else {
        wide_button(cr, bx, by, bw, 38, "OK", "Enter", true, hover(bx, by, bw, 38));
        hot(bx, by, bw, 38, [this] { answer_talk(true); });
    }
    (void)d;
}

void App::draw_audience(cairo_t *cr, Dialog &d)
{
    const int k = d.other;
    if (k < 0 || k >= static_cast<int>(g().civs.size())) {
        close_top();
        return;
    }
    const int me = human();
    const Treaty tr = g().treaty(me, k);
    const double w = std::min(860, geo_.win_w - 20), h = 440, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, "Audience with the " + g().civs[k].name);
    const int scale = 6;
    const Attitude at = g().attitude(k, me);
    portrait(cr, g(), k, at.hostile ? -1 : (d.view_only ? 1 : 0), x + 30, cy + 6, scale);
    const double tx = x + 30 + 32 * scale + 30, tw = x + w - 24 - tx;
    text(cr, kBoldFont, g().leader_greeting(k), tx, cy + 4, kSlateGold);
    std::string sub = std::string("Relations: ") + treaty_name(tr);
    if (tr == Treaty::Ceasefire) sub += " (ends " + year_text(year_for_turn(g().civs[me].ceasefire_end[k])) + ")";
    if ((g().civs[k].vendetta >> me) & 1u) sub += " \u00b7 they bear a grudge";
    text(cr, kTagFont, sub, tx, cy + 28, kSlateDim);
    double ly = cy + 56;
    for (const std::string &l : d.lines) {
        text(cr, kUiFont, l, tx, ly, kSlateInk, 1.0, static_cast<int>(tw));
        ly += measure(cr, kUiFont, l, static_cast<int>(tw)).h + 8;
    }
    struct Opt {
        Proposal p;
        std::string label;
        bool ok;
    };
    const bool embassy = ((g().civs[me].embassy >> k) & 1u) && ((g().civs[k].embassy >> me) & 1u);
    const Opt opts[4] = {
        {Proposal::Ceasefire, "Propose cease-fire", tr == Treaty::War},
        {Proposal::Peace, "Propose peace", tr == Treaty::Ceasefire || tr == Treaty::None},
        {Proposal::Embassy, "Exchange embassies", !embassy},
        {Proposal::CancelTreaty, g().senate(me) ? "Cancel treaty (Senate)" : "Cancel treaty", tr == Treaty::Ceasefire || tr == Treaty::Peace},
    };
    double oy = cy + 216;
    const double ow = (tw - 10) / 2;
    for (int i = 0; i < 4; ++i) {
        const double ox = tx + (i % 2) * (ow + 10), yy = oy + (i / 2) * 46;
        wide_button(cr, ox, yy, ow, 38, opts[i].label, std::to_string(i + 1), false, hover(ox, yy, ow, 38), opts[i].ok);
        const Proposal p = opts[i].p;
        if (opts[i].ok) hot(ox, yy, ow, 38, [this, k, p] { audience(k, p); });
    }
    const double bx = x + w - 24 - 160, by = y + h - 54;
    wide_button(cr, bx, by, 160, 38, "Farewell", "Esc", true, hover(bx, by, 160, 38));
    hot(bx, by, 160, 38, [this] {
        close_top();
        collect_news(false);
    });
}

void App::draw_databank(cairo_t *cr, Dialog &d)
{
    const DatabankTopic topic = static_cast<DatabankTopic>(d.other);
    const std::vector<DatabankEntry> entries = databank(topic);
    if (entries.empty()) {
        close_top();
        return;
    }
    d.scroll = std::clamp(d.scroll, 0, static_cast<int>(entries.size()) - 1);
    const double w = std::min(1040, geo_.win_w - 16), h = std::min(720, geo_.win_h - 16);
    const double x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, std::string("Databank: ") + databank_topic_name(topic));
    // Topic tabs.
    const int nt = static_cast<int>(DatabankTopic::Count);
    const double tabw = (w - 48 - (nt - 1) * 6) / nt;
    for (int t = 0; t < nt; ++t) {
        const double bx = x + 24 + t * (tabw + 6);
        wide_button(cr, bx, cy, tabw, 32, databank_topic_name(static_cast<DatabankTopic>(t)), "", t == d.other, hover(bx, cy, tabw, 32));
        hot(bx, cy, tabw, 32, [this, t] {
            Dialog &top = dialogs_.back();
            top.other = t;
            top.scroll = 0;
            top.dx = 0;
        });
    }
    cy += 46;
    // The list.
    const double lw = 250, lx = x + 24, row = 26, bottom = y + h - 70;
    const int fit = std::max(1, static_cast<int>((bottom - cy) / row));
    if (d.scroll < d.dx) d.dx = d.scroll;
    if (d.scroll >= d.dx + fit) d.dx = d.scroll - fit + 1;
    d.dx = std::clamp(d.dx, 0, std::max(0, static_cast<int>(entries.size()) - fit));
    round_rect(cr, lx, cy, lw, fit * row + 8, 6);
    set(cr, kSlateDeep);
    cairo_fill(cr);
    for (int i = d.dx; i < static_cast<int>(entries.size()) && i < d.dx + fit; ++i) {
        const double ry = cy + 4 + (i - d.dx) * row;
        const bool sel = i == d.scroll;
        if (sel) {
            round_rect(cr, lx + 4, ry, lw - 8, row - 2, 4);
            set(cr, kSlateGoldDim);
            cairo_fill(cr);
        } else if (hover(lx, ry, lw, row)) {
            fill(cr, lx + 4, ry, lw - 8, row - 2, kSlatePanel);
        }
        text(cr, kUiFont, entries[static_cast<size_t>(i)].name, lx + 12, ry + 3, sel ? kSlateDeep : kSlateInk);
        hot(lx, ry, lw, row, [this, i] { dialogs_.back().scroll = i; });
    }
    if (static_cast<int>(entries.size()) > fit)
        text(cr, kTagFont, std::to_string(d.dx + 1) + "-" + std::to_string(std::min(static_cast<int>(entries.size()), d.dx + fit)) + " of " +
                               std::to_string(entries.size()) + " \u00b7 Up/Down",
             lx, cy + fit * row + 14, kSlateDim);
    // The facts.
    const DatabankEntry &e = entries[static_cast<size_t>(d.scroll)];
    const double fx = lx + lw + 24, fw = x + w - 24 - fx;
    double fy = cy;
    const int color = game_ ? g().civs[human()].color : settings.player_color;
    double head = 0;
    if (topic == DatabankTopic::Units) {
        for (int u = 0; u < kUnitTypeCount; ++u) {
            if (e.name == unit_info(u).name) paint_pixmap(cr, art::unit_card(static_cast<UnitType>(u), civ_colors(color)), fx, fy, 2);
        }
        head = 72;
    } else if (topic == DatabankTopic::Wonders) {
        for (int wv = 0; wv < kWonderCount; ++wv) {
            if (e.name == wonder_info(wv).name) paint_pixmap(cr, art::wonder_icon(wv), fx, fy, 2);
        }
        head = 72;
    }
    text(cr, kTitleFont, e.name, fx + (head ? 80 : 0), fy + (head ? 14 : 0), kSlateGold);
    fy += head ? head : 44;
    section(cr, "FACTS", fx, fy, fx + fw);
    fy += 22;
    for (const std::string &f : e.facts) {
        if (fy > bottom) break;
        fill(cr, fx + 2, fy + 8, 5, 5, kSlateGoldDim);
        text(cr, kUiFont, f, fx + 16, fy, kSlateInk, 1.0, static_cast<int>(fw - 16));
        fy += measure(cr, kUiFont, f, static_cast<int>(fw - 16)).h + 6;
    }
    text(cr, kTagFont, "Left/Right: topic \u00b7 Up/Down: entry", x + 24, y + h - 44, kSlateDim);
    const double bx = x + w - 24 - 140, by = y + h - 54;
    wide_button(cr, bx, by, 140, 38, "Close", "Esc", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

void App::draw_custom_world(cairo_t *cr, Dialog &d)
{
    const double w = 640, h = 430, x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    const double L = x + 24, R = x + w - 24;
    struct Row {
        const char *title;
        const char *names[3];
        int *value;
        int values[3];
    };
    Row rows[4] = {
        {"LAND MASS", {"Small", "Normal", "Large"}, &settings.land_pct, {24, 32, 40}},
        {"TEMPERATURE", {"Cool", "Temperate", "Warm"}, &settings.temperature, {0, 1, 2}},
        {"CLIMATE", {"Arid", "Normal", "Wet"}, &settings.climate, {0, 1, 2}},
        {"AGE", {"3 billion years", "4 billion years", "5 billion years"}, &settings.age, {0, 1, 2}},
    };
    const double bw = (R - L - 16) / 3;
    for (Row &r : rows) {
        section(cr, r.title, L, cy, R);
        cy += 20;
        for (int i = 0; i < 3; ++i) {
            const double bx = L + i * (bw + 8);
            const bool chosen = *r.value == r.values[i];
            wide_button(cr, bx, cy, bw, 34, r.names[i], "", chosen, hover(bx, cy, bw, 34));
            int *v = r.value;
            const int val = r.values[i];
            hot(bx, cy, bw, 34, [v, val] { *v = val; });
        }
        cy += 48;
    }
    text(cr, kUiFont, "A young world is rugged with hills and mountains; an old one is worn smooth.", L, cy - 6, kSlateDim, 1.0,
         static_cast<int>(R - L));
    const double by = y + h - 56;
    wide_button(cr, R - 180, by, 180, 38, "Done", "Enter", true, hover(R - 180, by, 180, 38));
    hot(R - 180, by, 180, 38, [this] {
        world_choice_ = 2;
        close_top();
    });
}

void App::draw_nuke(cairo_t *cr, Dialog &d)
{
    const double w = 700;
    double body = 0;
    for (const std::string &l : d.lines) body += measure(cr, kUiFont, l, static_cast<int>(w - 260)).h + 8;
    const double h = std::max(330.0, 54 + body + 90), x = (geo_.win_w - w) / 2, y = (geo_.win_h - h) / 2;
    double cy = dialog_frame(cr, x, y, w, h, d.title);
    // A mushroom cloud, square by square.
    static const std::vector<std::string> cloud = {
        "......oooooo......", "....ooyyyyyyoo....", "...oyywwwwwyyyo...", "..oyywwwwwwwwyyo..", "..oyyyywwwwyyyyo..",
        "...ooyyyyyyyyoo...", ".....oooyyooo.....", ".......oyyo.......", ".......oyyo.......", "......ooyyoo......",
        ".......oyyo.......", "......oyyyyo......", "....ooyyyyyyoo....", "..ooyyyywwyyyyoo..", ".RRRRRRRRRRRRRRRR.",
        "RRrrrrrrrrrrrrrrRR",
    };
    paint_pixmap(cr, art::sprite(cloud), x + 30, cy + 10, 11);
    double ty = cy;
    for (const std::string &l : d.lines) {
        text(cr, kUiFont, l, x + 240, ty, kSlateInk, 1.0, static_cast<int>(w - 264));
        ty += measure(cr, kUiFont, l, static_cast<int>(w - 264)).h + 8;
    }
    const double bx = x + w - 24 - 140, by = y + h - 56;
    wide_button(cr, bx, by, 140, 38, "OK", "Enter", true, hover(bx, by, 140, 38));
    hot(bx, by, 140, 38, [this] { close_top(); });
}

} // namespace ui
