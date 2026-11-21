// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// The main screen: map, menu bar, empire band, sidebar and news line, plus
// the unit orders and turn flow. Dialogs live in dialogs.cpp.

#include "app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>

namespace ui {

using namespace civ;

namespace {

const char *kMenuTitles[] = {"Game", "Orders", "Advisors", "World", "Civilopedia"};
constexpr int kMenuCount = 5;

char status_letter(const Unit &u)
{
    switch (u.order) {
    case Order::Fortifying:
    case Order::Fortified: return 'F';
    case Order::Sentry: return 'S';
    case Order::Goto: return 'G';
    case Order::Road: return 'R';
    case Order::Irrigate: return 'I';
    case Order::Mine: return 'M';
    case Order::Fortress: return 'P';
    default: return 0;
    }
}

std::string moves_text(int thirds)
{
    const int whole = thirds / 3, part = thirds % 3;
    std::string s = whole > 0 || part == 0 ? std::to_string(whole) : "";
    if (part) {
        s += (whole > 0 ? " " : "") + std::to_string(part) + "/3";
    }
    return s;
}

const std::vector<std::string> &hourglass_rows()
{
    static const std::vector<std::string> r = {
        "KKKKKKKKKK", ".KwwwwwwK.", ".KyyyyyyK.", "..KyyyyK..", "...KyyK...",
        "....KK....", "...KwwK...", "..KwyywK..", ".KwyyyywK.", "KKKKKKKKKK",
    };
    return r;
}

const std::vector<std::string> &done_rows()
{
    static const std::vector<std::string> r = {
        "........KK", ".......KeK", "......KeK.", "KK...KeK..", "KeK.KeK...", ".KeKeK....", "..KeK.....", "...K......",
    };
    return r;
}

} // namespace

App::App()
{
    settings.civs = 5;
    settings.difficulty = Difficulty::Prince;
}

App::~App() = default;

// ---------------------------------------------------------------- setup

void App::open_new_game_dialog()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch().count();
    seed_text_ = std::to_string(static_cast<unsigned long long>(now) % 1000000ull);
    new_game_field_ = 0;
    Dialog d;
    d.kind = DialogKind::NewGame;
    d.title = "New Game";
    dialogs_.clear();
    open(d);
}

void App::start_game(const GameSettings &s)
{
    game_ = std::make_unique<Game>(Game::create(s));
    after_load();
    seen_messages_ = 0;
    collect_news(false);
    begin_turn();
}

void App::after_load()
{
    dialogs_.clear();
    menu_open_ = -1;
    goto_mode_ = false;
    active_ = -1;
    seen_messages_ = g().messages.size();
    rebuild_terrain();
    ensure_active();
    if (const Unit *u = g().unit(active_)) {
        center_on(u->x, u->y);
    } else if (City *c = g().capital(human())) {
        center_on(c->x, c->y);
    }
}

bool App::load_file(const std::string &path, std::string *error)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "Can't open " + path;
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    auto ng = std::make_unique<Game>();
    if (!load_game(ss.str(), *ng, error)) {
        return false;
    }
    game_ = std::move(ng);
    after_load();
    news_ = "Game loaded: " + year_text(year_for_turn(g().turn)) + ".";
    return true;
}

void App::rebuild_terrain()
{
    std::vector<std::pair<int, int>> cs;
    for (const City &c : g().cities) cs.push_back({c.x, c.y});
    painted_ = art::paint_world(g().map, cs);
    terrain_ = art::render_terrain(painted_, 0, 0, g().map.w, g().map.h);
}

// Repaint the squares whose look changed (roads, farms, mines, cities...).
void App::refresh_terrain()
{
    if (!game_) return;
    std::vector<std::pair<int, int>> cs;
    for (const City &c : g().cities) cs.push_back({c.x, c.y});
    art::PaintWorld now = art::paint_world(g().map, cs);
    std::vector<std::pair<int, int>> changed;
    for (int y = 0; y < now.h; ++y) {
        for (int x = 0; x < now.w; ++x) {
            const art::PaintTile &a = now.at(x, y), &b = painted_.at(x, y);
            if (a.terrain != b.terrain || a.special != b.special || a.river != b.river || a.road != b.road ||
                a.irrigated != b.irrigated || a.mine != b.mine || a.hut != b.hut || a.city != b.city) {
                changed.push_back({x, y});
            }
        }
    }
    painted_ = std::move(now);
    for (const auto &c : changed) {
        const Pixmap patch = art::render_terrain(painted_, c.first - 1, c.second - 1, 3, 3);
        for (int ty = 0; ty < 3; ++ty) {
            const int wy = c.second - 1 + ty;
            if (wy < 0 || wy >= g().map.h) continue;
            for (int tx = 0; tx < 3; ++tx) {
                const int wx = g().map.wrap_x(c.first - 1 + tx);
                for (int y = 0; y < art::kTile; ++y) {
                    for (int x = 0; x < art::kTile; ++x) {
                        terrain_.px[static_cast<size_t>(wy * art::kTile + y) * terrain_.w + wx * art::kTile + x] =
                            patch.px[static_cast<size_t>(ty * art::kTile + y) * patch.w + tx * art::kTile + x];
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------- view

void App::center_on(int x, int y)
{
    if (!game_) return;
    view_x_ = g().map.wrap_x(x - view_cols() / 2);
    const int rows = geo_.map_h / tile_px();
    view_y_ = std::clamp(y - rows / 2, 0, std::max(0, g().map.h - rows));
}

void App::keep_in_view(int x, int y)
{
    const int cols = geo_.map_w / tile_px(), rows = geo_.map_h / tile_px();
    const int dx = g().map.wrap_x(x - view_x_);
    const int dy = y - view_y_;
    if (dx < 1 || dx >= cols - 1 || dy < 1 || dy >= rows - 1) center_on(x, y);
}

bool App::map_tile_at(double px, double py, int &tx, int &ty) const
{
    if (px < geo_.map_x || py < geo_.map_y || px >= geo_.map_x + geo_.map_w || py >= geo_.map_y + geo_.map_h) return false;
    const int t = art::kTile * zoom_;
    tx = game_->map.wrap_x(view_x_ + static_cast<int>(px - geo_.map_x) / t);
    ty = view_y_ + static_cast<int>(py - geo_.map_y) / t;
    return game_->map.valid_y(ty);
}

// ---------------------------------------------------------------- turn flow

void App::ensure_active()
{
    if (!game_) return;
    const Unit *u = g().unit(active_);
    if (!u || u->owner != human() || u->moves <= 0 || u->order != Order::None || u->done) select_next(false);
}

void App::select_next(bool from_current)
{
    const int after = from_current ? active_ : -1;
    active_ = g().next_unit_for(human(), after);
    end_of_turn_prompt_ = active_ < 0;
    goto_mode_ = false;
    if (const Unit *u = g().unit(active_)) keep_in_view(u->x, u->y);
}

void App::begin_turn()
{
    if (!game_) return;
    if (g().winner >= 0 || g().human_dead) {
        Dialog d;
        d.kind = DialogKind::GameOver;
        d.title = g().winner == human() ? "Victory!" : "Game Over";
        open(d);
        return;
    }
    active_ = -1;
    ensure_active();
    if (const Unit *u = g().unit(active_)) center_on(u->x, u->y);
    const Civ &c = g().civs[human()];
    if (c.research == Advance::None && !g().researchable(human()).empty() && !c.ai) {
        Dialog d;
        d.kind = DialogKind::Science;
        d.title = "Science Advisor";
        open(d);
    }
}

void App::end_turn()
{
    if (!game_ || g().winner >= 0 || g().human_dead) return;
    goto_mode_ = false;
    g().end_turn();
    refresh_terrain();
    collect_news(true);
    begin_turn();
}

void App::collect_news(bool report)
{
    std::vector<std::string> lines, popup;
    std::vector<std::pair<int, std::string>> wonders;
    std::string popup_title;
    for (size_t i = seen_messages_; i < g().messages.size(); ++i) {
        const Message &m = g().messages[i];
        if (m.civ != human() && m.civ != -1) continue;
        news_ = m.text;
        lines.push_back(m.text);
        if (m.kind == MsgKind::Wonder && m.wonder >= 0) wonders.push_back({m.wonder, m.text});
        if (m.kind == MsgKind::Tech || m.kind == MsgKind::Good || m.kind == MsgKind::Bad) {
            popup.push_back(m.text);
            if (popup_title.empty()) popup_title = m.kind == MsgKind::Tech ? "Science" : (m.kind == MsgKind::Bad ? "Bad News" : "Good News");
        }
    }
    seen_messages_ = g().messages.size();
    if (report && !lines.empty()) {
        Dialog d;
        d.kind = DialogKind::Report;
        d.title = "News of " + year_text(year_for_turn(g().turn));
        if (lines.size() > 14) lines.erase(lines.begin(), lines.end() - 14);
        d.lines = lines;
        open(d);
    } else if (!report && !popup.empty()) {
        message(popup_title, popup);
    }
    for (const auto &w : wonders) {
        Dialog d;
        d.kind = DialogKind::WonderBuilt;
        d.title = "Wonder of the World";
        d.other = w.first;
        d.lines = {w.second};
        open(d);
    }
}

void App::message(const std::string &title, const std::vector<std::string> &lines)
{
    Dialog d;
    d.kind = DialogKind::Message;
    d.title = title;
    d.lines = lines;
    open(d);
}

void App::close_top()
{
    if (!dialogs_.empty()) {
        if (dialogs_.back().kind == DialogKind::NewGame && !game_) {
            quit_ = true;
            return;
        }
        dialogs_.pop_back();
    }
    if (game_) ensure_active();
}

void App::unit_moved()
{
    refresh_terrain();
    collect_news(false);
    if (g().winner >= 0 || g().human_dead) {
        begin_turn();
        return;
    }
    const Unit *u = g().unit(active_);
    if (u) keep_in_view(u->x, u->y);
    if (!u || u->moves <= 0 || u->order != Order::None) select_next(true);
}

void App::try_move(int dx, int dy)
{
    Unit *u = g().unit(active_);
    if (!u) {
        // No unit: the arrows scroll the map.
        view_x_ = g().map.wrap_x(view_x_ + dx * 2);
        const int rows = geo_.map_h / tile_px();
        view_y_ = std::clamp(view_y_ + dy * 2, 0, std::max(0, g().map.h - rows));
        return;
    }
    const int tx = g().map.wrap_x(u->x + dx), ty = u->y + dy;
    u->order = Order::None;
    const MoveResult r = g().move(active_, dx, dy);
    switch (r) {
    case MoveResult::NeedWar: {
        Dialog d;
        d.kind = DialogKind::ConfirmWar;
        d.title = "Declare War?";
        d.unit_id = active_;
        d.other = g().owner_at(tx, ty);
        d.dx = dx;
        d.dy = dy;
        open(d);
        return;
    }
    case MoveResult::Attacked: {
        const size_t before = dialogs_.size();
        unit_moved();
        Dialog d;
        d.kind = DialogKind::Combat;
        d.title = g().last_combat.attacker_won ? "Victory" : "Defeat";
        dialogs_.insert(dialogs_.begin() + static_cast<long>(before), d);
        return;
    }
    case MoveResult::Zoc: news_ = "Can't move there: an enemy unit's zone of control."; return;
    case MoveResult::Blocked:
        if (ty >= 0 && ty < g().map.h && g().map.ocean(tx, ty)) news_ = "Your " + std::string(unit_info(u->type).name) + " can't go to sea.";
        return;
    case MoveResult::NoMoves: select_next(true); return;
    default: break;
    }
    unit_moved();
}

void App::do_order(char order)
{
    if (!game_) return;
    Unit *u = g().unit(active_);
    if (!u) {
        return;
    }
    const UnitInfo &info = unit_info(u->type);
    const bool settlers = info.flags & kSettlers;
    switch (order) {
    case 'b':
        if (!settlers) return;
        if (const City *c = g().city_at(u->x, u->y)) {
            if (c->owner == human() && g().join_city(active_)) {
                news_ = "The Settlers join " + c->name + ".";
                unit_moved();
            }
            return;
        }
        {
            bool ok = g().map.land(u->x, u->y);
            for (const City &c : g().cities) ok = ok && g().map.distance(c.x, c.y, u->x, u->y) > 1;
            if (!ok) {
                news_ = "Too close to another city to build here.";
                return;
            }
            Dialog d;
            d.kind = DialogKind::CityName;
            d.title = "Found a City";
            d.unit_id = active_;
            d.field = g().suggest_city_name(human());
            open(d);
        }
        return;
    case 'r':
    case 'i':
    case 'm': {
        const Order job = order == 'r' ? Order::Road : (order == 'i' ? Order::Irrigate : Order::Mine);
        if (!settlers) return;
        if (!g().start_work(active_, job)) {
            news_ = order == 'r' ? "No road can be built here." : (order == 'i' ? "No water nearby to irrigate from." : "Nothing to mine here.");
            return;
        }
        unit_moved();
        return;
    }
    case 'p':
        if (settlers && g().start_work(active_, Order::Fortress)) unit_moved();
        return;
    case 'f':
        if (settlers) {
            if (g().can_work(*u, Order::Fortress)) {
                g().start_work(active_, Order::Fortress);
                unit_moved();
            }
            return;
        }
        g().fortify(active_);
        unit_moved();
        return;
    case 's':
        g().sentry(active_);
        unit_moved();
        return;
    case 'w': select_next(true); return;
    case ' ':
        u->done = true;
        select_next(true);
        return;
    case 'd': {
        const std::string n = info.name;
        g().disband(active_);
        news_ = "Your " + n + " is disbanded.";
        active_ = -1;
        select_next(false);
        return;
    }
    case 'g':
        goto_mode_ = true;
        news_ = "Goto: click a destination on the map (Esc cancels).";
        return;
    default: return;
    }
}

void App::click_map(int tx, int ty, int button)
{
    if (button == 3) {
        center_on(tx, ty);
        return;
    }
    if (goto_mode_) {
        goto_mode_ = false;
        if (g().unit(active_)) {
            g().set_goto(active_, tx, ty);
            g().step_goto(active_);
            for (int i = 0; i < 20; ++i) {
                Unit *u = g().unit(active_);
                if (!u || u->moves <= 0 || u->order != Order::Goto) break;
                if (!g().step_goto(active_)) break;
            }
            unit_moved();
        }
        return;
    }
    if (const City *c = g().city_at(tx, ty)) {
        if (c->owner == human()) {
            const Unit *u = g().unit(active_);
            if (!u || g().map.distance(u->x, u->y, tx, ty) != 1) {
                open_city(c->id);
                return;
            }
        }
    }
    // A unit of ours there: select it (clicking again cycles the stack).
    std::vector<int> mine;
    for (int id : g().units_at(tx, ty)) {
        if (g().unit(id)->owner == human()) mine.push_back(id);
    }
    if (!mine.empty() && !(g().unit(active_) && g().unit(active_)->x == tx && g().unit(active_)->y == ty && mine.size() == 1)) {
        auto it = std::find(mine.begin(), mine.end(), active_);
        int pick = it == mine.end() || it + 1 == mine.end() ? mine.front() : *(it + 1);
        Unit *u = g().unit(pick);
        if (u->order != Order::None && u->order != Order::Goto) {
            if (u->order == Order::Sentry || u->order == Order::Fortified || u->order == Order::Fortifying) u->order = Order::None;
        }
        if (u->order == Order::Goto) u->order = Order::None;
        u->done = false;
        active_ = pick;
        end_of_turn_prompt_ = false;
        return;
    }
    if (const Unit *u = g().unit(active_)) {
        const int ddx = g().map.dx(u->x, tx), ddy = ty - u->y;
        if (std::abs(ddx) <= 1 && std::abs(ddy) <= 1 && (ddx || ddy)) {
            try_move(ddx, ddy);
            return;
        }
    }
    center_on(tx, ty);
}

void App::open_city(int id)
{
    Dialog d;
    d.kind = DialogKind::City;
    d.city_id = id;
    if (City *c = g().city(id)) {
        g().update_city(*c);
        d.title = c->name;
    }
    open(d);
}

int App::research_turns(int ci) const
{
    int sci = 0;
    for (const City &c : game_->cities) {
        if (c.owner == ci && !c.stats.disorder) sci += c.stats.sci;
    }
    const int need = game_->research_cost(ci) - game_->civs[ci].bulbs;
    if (need <= 0) return 1;
    if (sci <= 0) return -1;
    return (need + sci - 1) / sci;
}

long long App::population(int ci) const
{
    long long p = 0;
    for (const City &c : game_->cities) {
        if (c.owner == ci) p += static_cast<long long>(c.size) * (c.size + 1) / 2 * 10000;
    }
    return p;
}

// ---------------------------------------------------------------- saves

std::string App::save_path(int slot) const
{
    return save_dir_ + "slot" + std::to_string(slot) + ".lciv";
}

void App::save_slot(int slot)
{
    std::ofstream out(save_path(slot), std::ios::binary | std::ios::trunc);
    if (!out) {
        message("Save Failed", {"Can't write " + save_path(slot) + "."});
        return;
    }
    out << save_game(g());
    news_ = "Game saved to slot " + std::to_string(slot) + " (" + year_text(year_for_turn(g().turn)) + ").";
}

void App::load_slot(int slot)
{
    std::string err;
    if (!load_file(save_path(slot), &err)) {
        message("Load Failed", {err.empty() ? "That slot is empty." : err});
    }
}

std::string App::slot_summary(int slot) const
{
    std::ifstream in(save_path(slot));
    if (!in) return "Empty";
    std::string line;
    int turn = 0;
    std::string civ_name, leader;
    while (std::getline(in, line)) {
        if (line.rfind("game|", 0) == 0) turn = std::atoi(line.c_str() + 5);
        if (line.rfind("civ|", 0) == 0) {
            const size_t a = 4, b = line.find('|', a), c = line.find('|', b + 1);
            civ_name = line.substr(a, b - a);
            leader = line.substr(b + 1, c - b - 1);
            break;
        }
    }
    if (turn <= 0) return "Unreadable";
    return civ_name + " (" + leader + ") \u00b7 " + year_text(year_for_turn(turn)) + " \u00b7 turn " + std::to_string(turn);
}

// ---------------------------------------------------------------- input

bool App::wants_text() const
{
    return !dialogs_.empty() && (dialogs_.back().kind == DialogKind::NewGame || dialogs_.back().kind == DialogKind::CityName);
}

bool App::text_input(const std::string &utf8)
{
    if (dialogs_.empty()) return false;
    Dialog &d = dialogs_.back();
    if (d.kind == DialogKind::CityName) {
        if (d.field.size() < 24) d.field += utf8;
        return true;
    }
    if (d.kind == DialogKind::NewGame) {
        std::string &f = new_game_field_ == 0 ? settings.player_name : (new_game_field_ == 1 ? settings.player_leader : seed_text_);
        if (new_game_field_ == 2) {
            for (char ch : utf8) {
                if (ch >= '0' && ch <= '9' && f.size() < 12) f += ch;
            }
        } else if (f.size() < 22) {
            for (char ch : utf8) {
                if (ch != '|') f += ch;
            }
        }
        return true;
    }
    return false;
}

static void pop_utf8(std::string &s)
{
    while (!s.empty()) {
        const unsigned char c = static_cast<unsigned char>(s.back());
        s.pop_back();
        if ((c & 0xC0) != 0x80) break;
    }
}

bool App::key(const KeyEvent &k)
{
    // Dialogs first.
    if (!dialogs_.empty()) {
        Dialog &d = dialogs_.back();
        if (d.kind == DialogKind::NewGame) {
            if (k.ctrl && (k.ch == 'l' || k.ch == 'o')) {
                Dialog l;
                l.kind = DialogKind::SaveLoad;
                l.title = "Load Game";
                open(l);
            } else if (k.ctrl && k.ch == 'q') {
                quit_ = true;
            } else if (k.key == Key::Backspace) {
                pop_utf8(new_game_field_ == 0 ? settings.player_name : (new_game_field_ == 1 ? settings.player_leader : seed_text_));
            } else if (k.key == Key::Tab) {
                new_game_field_ = (new_game_field_ + 1) % 3;
            } else if (k.key == Key::Enter) {
                GameSettings s = settings;
                s.seed = seed_text_.empty() ? 1 : std::strtoull(seed_text_.c_str(), nullptr, 10);
                if (s.player_name.empty()) s.player_name = "Lundukians";
                if (s.player_leader.empty()) s.player_leader = "Bryan";
                start_game(s);
            } else if (k.key == Key::Escape) {
                close_top();
            }
            return true;
        }
        if (d.kind == DialogKind::CityName) {
            if (k.key == Key::Backspace) {
                pop_utf8(d.field);
            } else if (k.key == Key::Enter) {
                const int uid = d.unit_id;
                const std::string name = d.field;
                dialogs_.pop_back();
                if (g().found_city(uid, name)) {
                    refresh_terrain();
                    collect_news(false);
                    const City &c = g().cities.back();
                    select_next(false);
                    open_city(c.id);
                }
            } else if (k.key == Key::Escape) {
                close_top();
            }
            return true;
        }
        if (k.key == Key::Escape || k.key == Key::Enter || (k.key == Key::Space && d.kind != DialogKind::City)) {
            if (d.kind == DialogKind::GameOver && k.key != Key::Escape) {
                dialogs_.pop_back();
                return true;
            }
            if (d.kind == DialogKind::ConfirmWar && k.key == Key::Enter) {
                const Dialog copy = d;
                dialogs_.pop_back();
                g().declare_war(human(), copy.other);
                active_ = copy.unit_id;
                try_move(copy.dx, copy.dy);
                return true;
            }
            close_top();
            return true;
        }
        if (d.kind == DialogKind::City && k.ch == 'c') {
            Dialog p;
            p.kind = DialogKind::Production;
            p.title = "Change Production";
            p.city_id = d.city_id;
            open(p);
            return true;
        }
        if (d.kind == DialogKind::City && k.ch == 'b') {
            g().buy(d.city_id);
            return true;
        }
        return true;
    }
    if (menu_open_ >= 0) {
        if (k.key == Key::Escape) menu_open_ = -1;
        return true;
    }
    if (!game_) return false;
    if (k.ctrl) {
        if (k.ch == 's') {
            Dialog d;
            d.kind = DialogKind::SaveLoad;
            d.title = "Save Game";
            d.saving = true;
            open(d);
        } else if (k.ch == 'l' || k.ch == 'o') {
            Dialog d;
            d.kind = DialogKind::SaveLoad;
            d.title = "Load Game";
            open(d);
        } else if (k.ch == 'n') {
            open_new_game_dialog();
        } else if (k.ch == 'q') {
            quit_ = true;
        }
        return true;
    }
    switch (k.key) {
    case Key::Up: try_move(0, -1); return true;
    case Key::Down: try_move(0, 1); return true;
    case Key::Left: try_move(-1, 0); return true;
    case Key::Right: try_move(1, 0); return true;
    case Key::UpLeft: try_move(-1, -1); return true;
    case Key::UpRight: try_move(1, -1); return true;
    case Key::DownLeft: try_move(-1, 1); return true;
    case Key::DownRight: try_move(1, 1); return true;
    case Key::Enter:
        end_turn();
        return true;
    case Key::Space: do_order(' '); return true;
    case Key::Escape:
        if (goto_mode_) {
            goto_mode_ = false;
            news_.clear();
        }
        return true;
    default: break;
    }
    if (k.key != Key::Char) return false;
    if (k.ch == '?') {
        Dialog d;
        d.kind = DialogKind::Help;
        d.title = "Keys";
        open(d);
        return true;
    }
    if (k.shift && k.ch == 'd') {
        do_order('d');
        return true;
    }
    switch (k.ch) {
    case 'b': case 'r': case 'i': case 'm': case 'f': case 's': case 'w': case 'g': case 'p': do_order(k.ch); return true;
    case 'd': do_order('d'); return true;
    case 'n': select_next(true); return true;
    case 'c':
        if (const Unit *u = g().unit(active_)) center_on(u->x, u->y);
        return true;
    case 'h':
        if (City *c = g().capital(human())) center_on(c->x, c->y);
        return true;
    case '+':
        if (zoom_ < 3) zoom_++;
        if (const Unit *u = g().unit(active_)) center_on(u->x, u->y);
        return true;
    case '-':
        if (zoom_ > 1) zoom_--;
        if (const Unit *u = g().unit(active_)) center_on(u->x, u->y);
        return true;
    case 't': {
        Dialog d;
        d.kind = DialogKind::Rates;
        d.title = "Tax Rates";
        open(d);
        return true;
    }
    case 'a': {
        Dialog d;
        d.kind = DialogKind::Science;
        d.title = "Science Advisor";
        open(d);
        return true;
    }
    case 'o': {
        Dialog d;
        d.kind = DialogKind::Wonders;
        d.title = "Wonders of the World";
        open(d);
        return true;
    }
    case 'v': {
        Dialog d;
        d.kind = DialogKind::Cities;
        d.title = "Cities";
        open(d);
        return true;
    }
    case '1': case '2': case '3': case '4': case '6': case '7': case '8': case '9': {
        static const int ddx[] = {0, -1, 0, 1, -1, 0, 1, -1, 0, 1};
        static const int ddy[] = {0, 1, 1, 1, 0, 0, 0, -1, -1, -1};
        const int n = k.ch - '0';
        try_move(ddx[n], ddy[n]);
        return true;
    }
    default: return false;
    }
}

void App::hot(double x, double y, double w, double h, std::function<void()> fn)
{
    hots_.push_back({x, y, w, h, std::move(fn)});
}

bool App::hover(double x, double y, double w, double h) const
{
    return mouse_x_ >= x && mouse_y_ >= y && mouse_x_ < x + w && mouse_y_ < y + h;
}

bool App::mouse_down(double x, double y, int button)
{
    mouse_x_ = x;
    mouse_y_ = y;
    // Topmost first.
    for (auto it = hots_.rbegin(); it != hots_.rend(); ++it) {
        if (it->contains(x, y)) {
            auto fn = it->fn; // the list is rebuilt on the next draw
            if (button == 1 || dialogs_.empty()) {
                fn();
            }
            return true;
        }
    }
    if (menu_open_ >= 0) {
        menu_open_ = -1;
        return true;
    }
    if (!dialogs_.empty() || !game_) return false;
    int tx, ty;
    if (map_tile_at(x, y, tx, ty)) {
        click_map(tx, ty, button);
        return true;
    }
    return false;
}

bool App::mouse_move(double x, double y)
{
    mouse_x_ = x;
    mouse_y_ = y;
    return true;
}

bool App::wheel(double x, double y, double dy)
{
    if (!game_ || !dialogs_.empty()) return false;
    int tx, ty;
    if (!map_tile_at(x, y, tx, ty)) return false;
    const int old = zoom_;
    zoom_ = std::clamp(zoom_ + (dy > 0 ? 1 : -1), 1, 3);
    if (zoom_ != old) center_on(tx, ty);
    return zoom_ != old;
}

bool App::tick(unsigned long long ms)
{
    if (ms - last_blink_ >= 450) {
        last_blink_ = ms;
        blink_ = !blink_;
        return game_ && dialogs_.empty() && active_ >= 0;
    }
    return false;
}

// ---------------------------------------------------------------- menus

std::vector<App::MenuItem> App::menu_items(int menu)
{
    auto dialog = [this](DialogKind k, const std::string &title, bool saving = false) {
        return [this, k, title, saving]() {
            Dialog d;
            d.kind = k;
            d.title = title;
            d.saving = saving;
            open(d);
        };
    };
    std::vector<MenuItem> items;
    const bool have = game_ != nullptr;
    const Unit *u = have ? g().unit(active_) : nullptr;
    const bool settlers = u && (unit_info(u->type).flags & kSettlers);
    switch (menu) {
    case 0:
        items.push_back({"New Game", "Ctrl+N", [this] { open_new_game_dialog(); }});
        items.push_back({"Load Game...", "Ctrl+L", dialog(DialogKind::SaveLoad, "Load Game")});
        items.push_back({"Save Game...", "Ctrl+S", dialog(DialogKind::SaveLoad, "Save Game", true), have});
        items.push_back({"Tax Rates...", "T", dialog(DialogKind::Rates, "Tax Rates"), have});
        items.push_back({"Revolution...", "", dialog(DialogKind::Revolution, "Revolution"), have});
        items.push_back({"Keys...", "F1", dialog(DialogKind::Help, "Keys")});
        items.push_back({"Quit", "Ctrl+Q", [this] { quit_ = true; }});
        break;
    case 1:
        items.push_back({"Found City / Join City", "B", [this] { do_order('b'); }, settlers});
        items.push_back({"Build Road", "R", [this] { do_order('r'); }, settlers});
        items.push_back({"Irrigate", "I", [this] { do_order('i'); }, settlers});
        items.push_back({"Build Mine", "M", [this] { do_order('m'); }, settlers});
        items.push_back({"Build Fortress", "P", [this] { do_order('p'); }, settlers && g().can_work(*u, Order::Fortress)});
        items.push_back({"Fortify", "F", [this] { do_order('f'); }, u && !settlers});
        items.push_back({"Sentry", "S", [this] { do_order('s'); }, u != nullptr});
        items.push_back({"Go To...", "G", [this] { do_order('g'); }, u != nullptr});
        items.push_back({"Wait", "W", [this] { do_order('w'); }, u != nullptr});
        items.push_back({"Skip Turn", "Space", [this] { do_order(' '); }, u != nullptr});
        items.push_back({"Disband", "Shift+D", [this] { do_order('d'); }, u != nullptr});
        items.push_back({"End Turn", "Enter", [this] { end_turn(); }, have});
        break;
    case 2:
        items.push_back({"Cities", "V", dialog(DialogKind::Cities, "Cities"), have});
        items.push_back({"Science", "A", dialog(DialogKind::Science, "Science Advisor"), have});
        items.push_back({"Foreign Affairs", "", dialog(DialogKind::Foreign, "Foreign Affairs"), have});
        items.push_back({"Wonders of the World", "O", dialog(DialogKind::Wonders, "Wonders of the World"), have});
        break;
    case 3:
        items.push_back({"Zoom In", "+", [this] { zoom_ = std::min(3, zoom_ + 1); if (const Unit *a = g().unit(active_)) center_on(a->x, a->y); }, have});
        items.push_back({"Zoom Out", "-", [this] { zoom_ = std::max(1, zoom_ - 1); if (const Unit *a = g().unit(active_)) center_on(a->x, a->y); }, have});
        items.push_back({"Center on Unit", "C", [this] { if (const Unit *a = g().unit(active_)) center_on(a->x, a->y); }, u != nullptr});
        items.push_back({"Center on Capital", "H", [this] { if (City *c = g().capital(human())) center_on(c->x, c->y); }, have});
        break;
    default:
        items.push_back({"Units", "", dialog(DialogKind::UnitSheet, "Civilopedia: Units")});
        break;
    }
    return items;
}

// ---------------------------------------------------------------- drawing

void App::render(cairo_t *cr, int w, int h)
{
    geo_.win_w = w;
    geo_.win_h = h;
    geo_.map_x = geo_.side_w;
    geo_.map_y = geo_.menu_h + geo_.band_h;
    geo_.map_w = w - geo_.side_w;
    geo_.map_h = h - geo_.map_y - geo_.status_h;
    hots_.clear();
    fill(cr, 0, 0, w, h, kSlateDeep);
    if (game_) {
        draw_map(cr);
        draw_sidebar(cr);
        draw_band(cr);
        draw_status(cr);
    } else {
        draw_title(cr);
    }
    draw_menubar(cr);
    if (!dialogs_.empty()) {
        hots_.clear();
        cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
        cairo_rectangle(cr, 0, 0, w, h);
        cairo_fill(cr);
        draw_dialog(cr, dialogs_.back());
    } else if (menu_open_ >= 0) {
        draw_menu_dropdown(cr);
    }
}

void App::draw_title(cairo_t *cr)
{
    // A quiet backdrop for the New Game dialog: the title and two rows of
    // unit counters along the foot of the screen.
    text_center(cr, "Sans Bold 30", "LUNDUKE CIV", geo_.win_w / 2.0, 44, kSlateGold);
    const int n = kUnitTypeCount, per_row = 14;
    const double gap = (geo_.win_w - 40.0) / per_row;
    for (int i = 0; i < n; ++i) {
        const Pixmap card = art::unit_card(static_cast<UnitType>(i), civ_colors(i % 8));
        const double x = 20 + (i % per_row) * gap + (gap - 64) / 2, y = geo_.win_h - 152 + (i / per_row) * 72;
        paint_pixmap(cr, card, x, y, 2);
    }
}

void App::draw_menubar(cairo_t *cr)
{
    const Geometry &g0 = geo_;
    cairo_pattern_t *bar = cairo_pattern_create_linear(0, 0, 0, g0.menu_h);
    cairo_pattern_add_color_stop_rgb(bar, 0, 0x1D / 255.0, 0x25 / 255.0, 0x30 / 255.0);
    cairo_pattern_add_color_stop_rgb(bar, 1, 0x15 / 255.0, 0x1B / 255.0, 0x23 / 255.0);
    cairo_set_source(cr, bar);
    cairo_rectangle(cr, 0, 0, g0.win_w, g0.menu_h);
    cairo_fill(cr);
    cairo_pattern_destroy(bar);
    fill(cr, 0, g0.menu_h - 1, g0.win_w, 1, hex(0x2E3846));
    double x = 4;
    for (int i = 0; i < kMenuCount; ++i) {
        const Metrics m = measure(cr, kMenuFont, kMenuTitles[i]);
        const double tw = m.w + 20;
        const bool lit = menu_open_ == i || (dialogs_.empty() && hover(x, 0, tw, g0.menu_h));
        if (lit) {
            fill(cr, x, 3, tw, g0.menu_h - 6, hex(0x303D50));
            fill(cr, x + 6, g0.menu_h - 5, tw - 12, 2, kSlateGold);
        }
        text(cr, kMenuFont, kMenuTitles[i], x + 10, 9, kSlateInk);
        if (dialogs_.empty()) {
            const int idx = i;
            hot(x, 0, tw, g0.menu_h, [this, idx] { menu_open_ = menu_open_ == idx ? -1 : idx; });
        }
        x += tw + 2;
    }
    if (game_) {
        const std::string right = "Turn " + std::to_string(g().turn) + "  \u00b7  Zoom " + std::to_string(zoom_ * 100) + "%";
        text_right(cr, kMenuFont, right, g0.win_w - 12, 9, kSlateDim);
    }
}

void App::draw_menu_dropdown(cairo_t *cr)
{
    double x = 4;
    for (int i = 0; i < menu_open_; ++i) x += measure(cr, kMenuFont, kMenuTitles[i]).w + 22;
    std::vector<MenuItem> items = menu_items(menu_open_);
    double w = 220;
    for (const MenuItem &it : items) {
        w = std::max(w, measure(cr, kMenuFont, it.label).w + measure(cr, kTagFont, it.key).w + 60.0);
    }
    const double ih = 28, h = items.size() * ih + 8, y = geo_.menu_h;
    // Swallow clicks on the panel's padding.
    hot(x, y, w, h, [] {});
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_rectangle(cr, x + 3, y + 4, w, h);
    cairo_fill(cr);
    fill(cr, x, y, w, h, hex(0x232C38));
    set(cr, hex(0x4A566A));
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, x + 0.5, y + 0.5, w - 1, h - 1);
    cairo_stroke(cr);
    double iy = y + 4;
    for (const MenuItem &it : items) {
        const bool hv = it.enabled && hover(x, iy, w, ih);
        if (hv) fill(cr, x + 2, iy, w - 4, ih, hex(0x3A4B63));
        text(cr, kMenuFont, it.label, x + 14, iy + 5, it.enabled ? kSlateInk : kSlateEdge);
        if (!it.key.empty()) text_right(cr, kTagFont, it.key, x + w - 12, iy + 9, it.enabled ? kSlateDim : kSlateEdge);
        auto fn = it.fn;
        const bool en = it.enabled;
        hot(x, iy, w, ih, [this, fn, en] {
            menu_open_ = -1;
            if (en) fn();
        });
        iy += ih;
    }
}

void App::draw_band(cairo_t *cr)
{
    const double y = geo_.menu_h, h = geo_.band_h;
    cairo_pattern_t *grad = cairo_pattern_create_linear(0, y, 0, y + h);
    cairo_pattern_add_color_stop_rgb(grad, 0, kSlateBand.r, kSlateBand.g, kSlateBand.b);
    cairo_pattern_add_color_stop_rgb(grad, 1, 0x19 / 255.0, 0x20 / 255.0, 0x2A / 255.0);
    cairo_set_source(cr, grad);
    cairo_rectangle(cr, 0, y, geo_.win_w, h);
    cairo_fill(cr);
    cairo_pattern_destroy(grad);
    fill(cr, 0, y + h - 1, geo_.win_w, 1, kSlateGoldDim, 0.55);
    const Civ &c = g().civs[human()];
    std::string research = "Choose an advance";
    if (c.research != Advance::None) {
        const int t = research_turns(human());
        research = std::string(advance_info(c.research).name) + " \u00b7 " + (t < 0 ? "stalled" : std::to_string(t) + (t == 1 ? " turn" : " turns"));
    }
    struct Item {
        const char *label;
        std::string value;
        Ink ink;
        std::function<void()> fn;
    };
    auto dlg = [this](DialogKind k, const char *t) {
        return [this, k, t] {
            Dialog d;
            d.kind = k;
            d.title = t;
            open(d);
        };
    };
    const std::vector<Item> items = {
        {"CIVILIZATION", c.name, kSlateInk, dlg(DialogKind::Foreign, "Foreign Affairs")},
        {"YEAR", year_text(year_for_turn(g().turn)), kSlateInk, nullptr},
        {"TREASURY", with_commas(c.gold) + " gold", kSlateGold, dlg(DialogKind::Rates, "Tax Rates")},
        {"TAX \u00b7 LUX \u00b7 SCI", std::to_string(c.tax * 10) + " \u00b7 " + std::to_string(c.lux * 10) + " \u00b7 " + std::to_string(c.sci * 10) + "%",
         kSlateInk, dlg(DialogKind::Rates, "Tax Rates")},
        {"POPULATION", with_commas(population(human())), kSlateInk, dlg(DialogKind::Cities, "Cities")},
        {"GOVERNMENT", government_info(c.govt).name, c.govt == Government::Anarchy ? kSlateBad : kSlateInk,
         dlg(DialogKind::Revolution, "Revolution")},
        {"RESEARCH", research, c.research == Advance::None ? kSlateGold : kSlateInk, dlg(DialogKind::Science, "Science Advisor")},
    };
    double x = 12;
    for (const Item &it : items) {
        const double lw = measure(cr, kTagFont, it.label).w;
        const double vw = measure(cr, kBoldFont, it.value).w;
        const double w = std::max(lw, vw);
        if (it.fn && dialogs_.empty() && hover(x - 6, y, w + 12, h)) fill(cr, x - 6, y + 3, w + 12, h - 7, hex(0x29323F));
        text(cr, kTagFont, it.label, x, y + 6, kSlateDim);
        text(cr, kBoldFont, it.value, x, y + 20, it.ink);
        if (it.fn) hot(x - 6, y, w + 12, h, it.fn);
        x += w + 16;
        fill(cr, std::round(x), y + 8, 1, h - 16, kSlateEdge);
        x += 16;
    }
}

void App::draw_map(cairo_t *cr)
{
    const int T = art::kTile;
    const int cols = geo_.map_w / (T * zoom_) + 2, rows = geo_.map_h / (T * zoom_) + 2;
    const Map &m = g().map;
    Pixmap pm(cols * T, rows * T, rgb(0x000000));
    for (int ty = 0; ty < rows; ++ty) {
        const int wy = view_y_ + ty;
        if (wy < 0 || wy >= m.h) continue;
        for (int tx = 0; tx < cols; ++tx) {
            const int wx = m.wrap_x(view_x_ + tx);
            for (int y = 0; y < T; ++y) {
                const Argb *src = &terrain_.px[static_cast<size_t>(wy * T + y) * terrain_.w + wx * T];
                std::copy(src, src + T, &pm.px[static_cast<size_t>(ty * T + y) * pm.w + tx * T]);
            }
        }
    }
    const int me = human();
    auto in_view = [&](int x, int y, int &tx, int &ty) {
        tx = m.wrap_x(x - view_x_);
        ty = y - view_y_;
        return tx < cols && ty >= 0 && ty < rows;
    };
    // Fortresses.
    static const Pixmap fort = art::fortress_sprite();
    for (int ty = 0; ty < rows; ++ty) {
        for (int tx = 0; tx < cols; ++tx) {
            const int wy = view_y_ + ty;
            if (m.valid_y(wy) && m.at(view_x_ + tx, wy).fortress) pm.blit(fort, tx * T, ty * T);
        }
    }
    // Cities.
    for (const City &c : g().cities) {
        int tx, ty;
        if (!in_view(c.x, c.y, tx, ty) || !((m.at(c.x, c.y).explored >> me) & 1u)) continue;
        pm.blit(art::city_tile(c.size, c.has(Building::CityWalls), c.has(Building::Palace), civ_colors(g().civs[c.owner].color),
                               c.x * 7 + c.y),
                tx * T, ty * T);
    }
    // Units: one counter per square, the active unit on top of its stack.
    std::map<int, std::vector<const Unit *>> stacks;
    for (const Unit &u : g().units) {
        int tx, ty;
        if (!in_view(u.x, u.y, tx, ty)) continue;
        if (u.owner != me && !((m.at(u.x, u.y).visible >> me) & 1u)) continue;
        stacks[ty * cols + tx].push_back(&u);
    }
    for (auto &kv : stacks) {
        const int tx = kv.first % cols, ty = kv.first / cols;
        std::vector<const Unit *> &st = kv.second;
        const Unit *top = nullptr;
        for (const Unit *u : st) {
            if (u->id == active_) top = u;
        }
        const bool city_here = g().city_at(st.front()->x, st.front()->y) != nullptr;
        if (!top) {
            if (city_here) continue; // the city shows; its garrison is inside
            double best = -1;
            for (const Unit *u : st) {
                const UnitInfo &ui = unit_info(u->type);
                double s = ui.defense + (ui.domain == Domain::Sea ? 100 : 0);
                if (s > best) {
                    best = s;
                    top = u;
                }
            }
        } else if (!blink_ && city_here) {
            continue;
        }
        const art::CivColors &cc = civ_colors(g().civs[top->owner].color);
        if (st.size() > 1) {
            // A second counter peeks out from under the first.
            const Unit *under = st.front() == top ? st[1] : st.front();
            pm.blit(art::unit_card(under->type, civ_colors(g().civs[under->owner].color)), tx * T + 2, ty * T - 2);
        }
        pm.blit(art::unit_card(top->type, cc, status_letter(*top), top->veteran), tx * T, ty * T);
        if (top->hp < kUnitHp) {
            // Wounded: a hit-point bar down the left edge of the counter.
            const int bh = 24, filled = std::max(1, bh * top->hp / kUnitHp);
            const Argb col = top->hp * 3 > kUnitHp * 2 ? rgb(0x6CCB7A) : (top->hp * 3 > kUnitHp ? rgb(0xF2C14E) : rgb(0xE5675A));
            pm.fill_rect(tx * T, ty * T + 4, 4, bh + 2, rgb(0x151B23));
            pm.fill_rect(tx * T + 1, ty * T + 5 + (bh - filled), 2, filled, col);
        }
        if (top->id == active_ && blink_ && dialogs_.empty()) art::bracket(pm, tx * T, ty * T, rgb(0xF2C14E), 0xA0000000);
    }
    art::apply_fog(pm, view_x_, view_y_, [&](int x, int y) { return m.valid_y(y) && ((m.at(x, y).explored >> me) & 1u); });
    cairo_save(cr);
    cairo_rectangle(cr, geo_.map_x, geo_.map_y, geo_.map_w, geo_.map_h);
    cairo_clip(cr);
    paint_pixmap(cr, pm, geo_.map_x, geo_.map_y, zoom_);
    // Goto: highlight the square under the mouse.
    int htx, hty;
    if (goto_mode_ && dialogs_.empty() && map_tile_at(mouse_x_, mouse_y_, htx, hty)) {
        const int t = T * zoom_;
        const double hx = geo_.map_x + m.wrap_x(htx - view_x_) * t, hy = geo_.map_y + (hty - view_y_) * t;
        round_rect(cr, hx + 2, hy + 2, t - 4, t - 4, 6);
        set(cr, kSlateGold);
        cairo_set_line_width(cr, 3);
        cairo_stroke(cr);
    }
    // City name pills.
    const int t = T * zoom_;
    for (const City &c : g().cities) {
        int tx, ty;
        if (!in_view(c.x, c.y, tx, ty) || !((m.at(c.x, c.y).explored >> me) & 1u)) continue;
        const double cx = geo_.map_x + tx * t + t / 2.0, by = geo_.map_y + ty * t + t - 2;
        const std::string label = c.owner == me ? c.name + "  " + std::to_string(c.size) : c.name;
        const Metrics mm = measure(cr, kSmallBold, label);
        const double pw = mm.w + 22, ph = mm.h + 4;
        const double px = std::round(cx - pw / 2), py = std::round(by);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
        round_rect(cr, px + 1, py + 2, pw, ph, 5);
        cairo_fill(cr);
        round_rect(cr, px, py, pw, ph, 5);
        set(cr, kSlateDeep, 0.9);
        cairo_fill_preserve(cr);
        cairo_set_line_width(cr, 1);
        set(cr, c.has(Building::Palace) ? kSlateGoldDim : kSlateEdge);
        cairo_stroke(cr);
        set_argb(cr, civ_colors(g().civs[c.owner].color).main);
        cairo_rectangle(cr, px + 7, py + ph / 2 - 3.5, 7, 7);
        cairo_fill(cr);
        text(cr, kSmallBold, label, px + 17, py + 2, c.stats.disorder && c.owner == me ? kSlateBad : kSlateInk);
    }
    if (end_of_turn_prompt_ && dialogs_.empty()) {
        const std::string msg = "No units waiting \u2014 press Enter to end the turn";
        const Metrics mm = measure(cr, kBoldFont, msg);
        const double w = mm.w + 28, h = mm.h + 16;
        const double x = geo_.map_x + (geo_.map_w - w) / 2, y = geo_.map_y + geo_.map_h - h - 16;
        cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
        round_rect(cr, x + 2, y + 3, w, h, 8);
        cairo_fill(cr);
        round_rect(cr, x, y, w, h, 8);
        set(cr, kSlateBand, blink_ ? 0.96 : 0.9);
        cairo_fill_preserve(cr);
        set(cr, kSlateGoldDim);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
        text(cr, kBoldFont, msg, x + 14, y + 8, kSlateGold);
    }
    cairo_restore(cr);
}

void App::draw_sidebar(cairo_t *cr)
{
    const double top = geo_.menu_h + geo_.band_h;
    const double bottom = geo_.win_h - geo_.status_h;
    fill(cr, 0, top, geo_.side_w, bottom - top, kSidebar);
    fill(cr, geo_.side_w - 1, top, 1, bottom - top, kSlateEdge);
    const double L = 8, R = geo_.side_w - 9;
    double y = top + 8;
    const int me = human();
    const Map &m = g().map;

    section(cr, "WORLD", L, y, R);
    y += 18;
    {
        const int s = 3;
        Pixmap mm(m.w * s, m.h * s, rgb(0x000000));
        for (int ty = 0; ty < m.h; ++ty) {
            for (int tx = 0; tx < m.w; ++tx) {
                const Tile &t = m.at(tx, ty);
                if ((t.explored >> me) & 1u) mm.fill_rect(tx * s, ty * s, s, s, art::minimap_color(t.terrain));
            }
        }
        for (const City &c : g().cities) {
            if (!((m.at(c.x, c.y).explored >> me) & 1u)) continue;
            mm.fill_rect(c.x * s - 1, c.y * s - 1, s + 2, s + 2, rgb(0x000000));
            mm.fill_rect(c.x * s, c.y * s, s, s, c.owner == me ? rgb(0xFFFFFF) : civ_colors(g().civs[c.owner].color).main);
        }
        for (const Unit &u : g().units) {
            if (u.owner != me && !((m.at(u.x, u.y).visible >> me) & 1u)) continue;
            mm.fill_rect(u.x * s + 1, u.y * s + 1, 1, 1, civ_colors(g().civs[u.owner].color).light);
        }
        const double mx = L, my = y + 1;
        fill(cr, mx - 2, my - 2, mm.w + 4, mm.h + 4, kSlateDeep);
        paint_pixmap(cr, mm, mx, my, 1);
        cairo_save(cr);
        cairo_rectangle(cr, mx, my, mm.w, mm.h);
        cairo_clip(cr);
        const int vc = geo_.map_w / tile_px(), vr = geo_.map_h / tile_px();
        for (int k = -1; k <= 0; ++k) {
            cairo_rectangle(cr, mx + (view_x_ + k * m.w) * s - 0.5, my + view_y_ * s - 0.5, vc * s + 1, std::min(vr, m.h) * s + 1);
        }
        set(cr, kSlateGold);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
        cairo_restore(cr);
        hot(mx, my, mm.w, mm.h, [this, mx, my, s] { center_on(static_cast<int>((mouse_x_ - mx) / s), static_cast<int>((mouse_y_ - my) / s)); });
        y += mm.h + 14;
    }

    section(cr, "ACTIVE UNIT", L, y, R);
    y += 20;
    const Unit *u = g().unit(active_);
    if (u) {
        round_rect(cr, L, y, 72, 72, 6);
        set(cr, kSlateDeep);
        cairo_fill(cr);
        paint_pixmap(cr, art::unit_card(u->type, civ_colors(g().civs[u->owner].color), status_letter(*u), u->veteran), L + 4, y + 4, 2);
        const double tx = L + 84;
        text(cr, kBoldFont, std::string(u->veteran ? "Vet. " : "") + unit_info(u->type).name, tx, y - 1, kSlateInk);
        const City *home = g().city(u->home);
        text(cr, kUiFont, home ? "Home: " + home->name : "No home city", tx, y + 18, kSlateDim);
        text(cr, kUiFont, "Moves " + moves_text(u->moves) + " of " + std::to_string(unit_info(u->type).moves),
             tx, y + 36, u->moves > 0 ? kSlateGood : kSlateDim);
        if (u->hp < kUnitHp) {
            text_right(cr, kSmallBold, "HP " + std::to_string(u->hp) + "/" + std::to_string(kUnitHp), R, y + 1,
                       u->hp * 2 > kUnitHp ? kSlateGold : kSlateBad);
        }
        {
            // Hit points under the counter.
            const double bw = 64;
            fill(cr, L + 4, y + 66, bw, 3, kSlatePanel);
            fill(cr, L + 4, y + 66, bw * u->hp / kUnitHp, 3, u->hp * 2 > kUnitHp ? kSlateGood : kSlateBad);
        }
        const Tile &t = m.at(u->x, u->y);
        std::string where = terrain_info(t.terrain).name;
        if (t.special) where = terrain_info(t.terrain).special;
        if (t.road) where += ", Road";
        if (t.irrigation) where += ", Irr.";
        if (t.mine) where += ", Mine";
        text(cr, kUiFont, where, tx, y + 54, kSlateDim);
        y += 80;
        const Yield yv = g().tile_yield(u->x, u->y, me, false);
        const int counts[3] = {yv.food, yv.shields, yv.trade};
        const char *names[3] = {"FOOD", "SHIELDS", "TRADE"};
        // Three label + icon + number groups spread across the sidebar, so
        // wider fonts (DejaVu on Windows) never run them together.
        double widths[3], total = 0;
        for (int k = 0; k < 3; ++k) {
            widths[k] = measure(cr, kTagFont, names[k]).w + 5 + 17 + measure(cr, kSmallBold, std::to_string(counts[k])).w;
            total += widths[k];
        }
        const double gap = std::max(6.0, (R - L - 4 - total) / 2);
        double ix = L + 2;
        for (int k = 0; k < 3; ++k) {
            if (k > 0) ix += widths[k - 1] + gap;
            text(cr, kTagFont, names[k], ix, y, kSlateDim);
            const double gx = ix + measure(cr, kTagFont, names[k]).w + 5;
            paint_pixmap(cr, art::yield_icon(k), gx, y - 1, 2);
            text(cr, kSmallBold, std::to_string(counts[k]), gx + 17, y - 1, counts[k] ? kSlateInk : kSlateDim);
        }
        y += 24;
    } else {
        text(cr, kUiFont, "No unit is waiting for orders.", L + 2, y, kSlateDim, 1.0, static_cast<int>(R - L));
        y += 44;
    }

    section(cr, "ORDERS", L, y, R);
    y += 20;
    static std::vector<Pixmap> icons;
    if (icons.empty()) {
        const art::CivColors &blue = civ_colors(0);
        icons.push_back(art::city_tile(1, false, false, blue, 3));
        icons.push_back(art::render_terrain_sample(art::Terrain::Plains, false, true));
        icons.push_back(art::render_terrain_sample(art::Terrain::Grassland, false, false, true));
        icons.push_back(art::render_terrain_sample(art::Terrain::Hills, false, false, false, true));
        icons.push_back(art::unit_card(UnitType::Phalanx, blue, 'F'));
        icons.push_back(art::unit_card(UnitType::Militia, blue, 'S'));
        icons.push_back(art::sprite(hourglass_rows()));
        icons.push_back(art::sprite(done_rows()));
    }
    const bool settlers = u && (unit_info(u->type).flags & kSettlers);
    const char keys[8] = {'b', 'r', 'i', 'm', 'f', 's', 'w', ' '};
    const char *labels[8] = {"B", "R", "I", "M", "F", "S", "W", "\u2423"};
    bool enabled[8];
    enabled[0] = settlers;
    enabled[1] = settlers && g().can_work(*u, Order::Road);
    enabled[2] = settlers && g().can_work(*u, Order::Irrigate);
    enabled[3] = settlers && g().can_work(*u, Order::Mine);
    enabled[4] = u && (!settlers || g().can_work(*u, Order::Fortress));
    enabled[5] = enabled[6] = enabled[7] = u != nullptr;
    const double cell = 60, bs = 54;
    for (int i = 0; i < 8; ++i) {
        const double bx = L + (i % 4) * cell + 1, by = y + (i / 4) * cell;
        const bool hv = enabled[i] && dialogs_.empty() && hover(bx, by, bs, bs);
        slate_button(cr, bx, by, bs, false, hv);
        const Pixmap &icon = icons[i];
        const int sc = icon.w <= 12 ? 2 : 1;
        const double ix = std::round(bx + (bs - icon.w * sc) / 2.0), iy = std::round(by + (bs - icon.h * sc) / 2.0) - 3;
        if (i >= 1 && i <= 3) fill(cr, ix - 1, iy - 1, icon.w + 2, icon.h + 2, hex(0x14121A));
        paint_pixmap(cr, icon, ix, iy, sc);
        if (!enabled[i]) {
            round_rect(cr, bx + 1, by + 1, bs - 2, bs - 2, 6);
            set(cr, hex(0x29323F), 0.7);
            cairo_fill(cr);
        }
        const Metrics km = measure(cr, kTagFont, labels[i]);
        text(cr, kTagFont, labels[i], bx + bs - km.w - 5, by + bs - km.h - 2, enabled[i] ? kSlateDim : kSlateEdge);
        if (enabled[i]) {
            const char k = keys[i];
            hot(bx, by, bs, bs, [this, k] { do_order(k); });
        }
    }
    y += cell * 2 + 6;

    const double bh = 34;
    const double fy = bottom - 10 - bh * 2 - 8;
    if (fy > y) {
        const bool hn = dialogs_.empty() && hover(L, fy, R - L, bh);
        wide_button(cr, L, fy, R - L, bh, "Next Unit", "N", false, hn);
        hot(L, fy, R - L, bh, [this] { select_next(true); });
        const double ey = fy + bh + 8;
        const bool he = dialogs_.empty() && hover(L, ey, R - L, bh);
        wide_button(cr, L, ey, R - L, bh, "End Turn", "Enter", true, he);
        hot(L, ey, R - L, bh, [this] { end_turn(); });
    }
}

void App::draw_status(cairo_t *cr)
{
    const double y = geo_.win_h - geo_.status_h;
    fill(cr, 0, y, geo_.win_w, geo_.status_h, kSlateDeep);
    fill(cr, 0, y, geo_.win_w, 1, kSlateEdge);
    const Metrics hm = measure(cr, kUiFont, "Ag");
    const double ty = y + (geo_.status_h - hm.h) / 2.0;
    const Metrics tag = measure(cr, kTagFont, "NEWS");
    const double tw = tag.w + 14, th = tag.h + 6, tyy = y + (geo_.status_h - th) / 2.0;
    round_rect(cr, 10, tyy, tw, th, 4);
    set(cr, kSlateGold);
    cairo_fill(cr);
    text(cr, kTagFont, "NEWS", 17, tyy + 3, kSlateDeep);
    std::string tool;
    if (goto_mode_) {
        tool = "Go To: click a square  [Esc]";
    } else if (const Unit *u = g().unit(active_)) {
        tool = std::string(unit_info(u->type).name) + ((unit_info(u->type).flags & kSettlers) ? ": B R I M" : ": F S W G");
    }
    const Metrics tm = measure(cr, kBoldFont, tool);
    double right = geo_.win_w - 10 - tm.w;
    if (!tool.empty()) text(cr, kBoldFont, tool, right, ty, kSlateGold);
    const std::string hint = "Arrows move \u00b7 Enter ends turn";
    const Metrics hm2 = measure(cr, kUiFont, hint);
    right -= 24 + hm2.w;
    text(cr, kUiFont, hint, right, ty, kSlateDim);
    // The news itself, clipped before the hints.
    cairo_save(cr);
    cairo_rectangle(cr, 10 + tw + 10, y, right - 24 - (10 + tw + 10), geo_.status_h);
    cairo_clip(cr);
    text(cr, kUiFont, news_, 10 + tw + 10, ty, kSlateInk);
    cairo_restore(cr);
}

} // namespace ui
