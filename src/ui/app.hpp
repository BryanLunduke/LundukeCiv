// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

// The game's screens: the map with its menu bar, empire band and sidebar,
// and the dialogs on top (new game, city, science, rates, saves...). The
// window code hands it input events and a Cairo surface to draw on.

#include "draw.hpp"
#include "game.hpp"
#include "terrain_paint.hpp"

#include <cairo.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ui {

struct Geometry {
    int win_w = 1280;
    int win_h = 800;
    int menu_h = 36;
    int band_h = 44;
    int status_h = 36;
    int side_w = 256;
    int map_x = 256;
    int map_y = 80;
    int map_w = 1024;
    int map_h = 684;
};

enum class Key {
    None, Up, Down, Left, Right, UpLeft, UpRight, DownLeft, DownRight, Enter, Escape, Space, Backspace, Tab, Home,
    End, Delete, Char,
};

struct KeyEvent {
    Key key = Key::None;
    char ch = 0;        // lower-case letter or digit for Key::Char, or '+'/'-'
    bool ctrl = false;
    bool shift = false;
};

enum class DialogKind {
    None, NewGame, Message, Combat, City, Production, Science, Rates, Revolution, SaveLoad, CityName, Cities,
    Foreign, UnitSheet, ConfirmWar, GameOver, Report, Help, WonderBuilt, Wonders,
    DiplomatMenu, CaravanMenu, Spaceship,
};

struct Dialog {
    DialogKind kind = DialogKind::None;
    std::string title;
    std::vector<std::string> lines;
    int city_id = -1;
    int unit_id = -1;
    int other = -1;       // civ for war confirmation
    int dx = 0, dy = 0;   // the move waiting on a war declaration
    bool saving = false;  // SaveLoad: save or load
    int scroll = 0;
    std::string field;    // CityName text
    bool view_only = false;   // City: a foreign city a Diplomat investigates
    int tx = 0, ty = 0;       // DiplomatMenu: the square the Diplomat faces
};

struct Hot {
    double x, y, w, h;
    std::function<void()> fn;
    bool contains(double px, double py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

class App {
public:
    App();
    ~App();

    // New game settings shown in the dialog.
    civ::GameSettings settings;

    void open_new_game_dialog();
    bool load_file(const std::string &path, std::string *error = nullptr);
    void set_save_dir(const std::string &dir) { save_dir_ = dir; }

    // Input. Return true when the screen needs a redraw.
    bool key(const KeyEvent &k);
    bool text_input(const std::string &utf8);
    bool mouse_down(double x, double y, int button);
    bool mouse_move(double x, double y);
    bool wheel(double x, double y, double dy);
    bool tick(unsigned long long ms);   // blink timer
    bool wants_text() const;
    bool quit_requested() const { return quit_; }

    void render(cairo_t *cr, int w, int h);

private:
    // --- state
    std::unique_ptr<civ::Game> game_;
    int active_ = -1;
    int zoom_ = 2;
    int view_x_ = 0;
    int view_y_ = 0;
    bool blink_ = true;
    unsigned long long last_blink_ = 0;
    bool goto_mode_ = false;
    int menu_open_ = -1;
    std::vector<Dialog> dialogs_;
    std::vector<Hot> hots_;
    double mouse_x_ = 0, mouse_y_ = 0;
    bool quit_ = false;
    size_t seen_messages_ = 0;
    std::string news_;
    int new_game_field_ = 0;  // 0 civ name, 1 leader, 2 seed
    std::string seed_text_;
    std::string save_dir_;
    Geometry geo_;
    bool end_of_turn_prompt_ = false;

    // Terrain cache: the whole world at native size.
    Pixmap terrain_;
    art::PaintWorld painted_;
    void rebuild_terrain();
    void refresh_terrain();

    // --- helpers
    civ::Game &g() { return *game_; }
    int human() const { return game_ ? game_->human : 0; }
    int tile_px() const { return art::kTile * zoom_; }
    int view_cols() const { return geo_.map_w / tile_px() + 1; }
    int view_rows() const { return geo_.map_h / tile_px() + 1; }
    void center_on(int x, int y);
    void keep_in_view(int x, int y);
    void start_game(const civ::GameSettings &s);
    void after_load();
    void select_next(bool from_current = true);
    void ensure_active();
    void unit_moved();
    void try_move(int dx, int dy);
    void do_order(char order);
    void end_turn();
    void begin_turn();
    void collect_news(bool popup);
    void open(Dialog d) { dialogs_.push_back(std::move(d)); }
    void close_top();
    void message(const std::string &title, const std::vector<std::string> &lines);
    bool map_tile_at(double px, double py, int &tx, int &ty) const;
    void click_map(int tx, int ty, int button);
    void open_city(int id);
    std::string save_path(int slot) const;
    void save_slot(int slot);
    void load_slot(int slot);
    std::string slot_summary(int slot) const;
    void hot(double x, double y, double w, double h, std::function<void()> fn);
    bool hover(double x, double y, double w, double h) const;
    int research_turns(int civ) const;
    long long population(int civ) const;

    // --- drawing
    void draw_map(cairo_t *cr);
    void draw_menubar(cairo_t *cr);
    void draw_menu_dropdown(cairo_t *cr);
    void draw_band(cairo_t *cr);
    void draw_sidebar(cairo_t *cr);
    void draw_status(cairo_t *cr);
    void draw_dialog(cairo_t *cr, Dialog &d);
    void draw_new_game(cairo_t *cr, Dialog &d);
    void draw_message(cairo_t *cr, Dialog &d);
    void draw_combat(cairo_t *cr, Dialog &d);
    void draw_city(cairo_t *cr, Dialog &d);
    void draw_production(cairo_t *cr, Dialog &d);
    void draw_science(cairo_t *cr, Dialog &d);
    void draw_rates(cairo_t *cr, Dialog &d);
    void draw_revolution(cairo_t *cr, Dialog &d);
    void draw_saveload(cairo_t *cr, Dialog &d);
    void draw_city_name(cairo_t *cr, Dialog &d);
    void draw_cities(cairo_t *cr, Dialog &d);
    void draw_foreign(cairo_t *cr, Dialog &d);
    void draw_unit_sheet(cairo_t *cr, Dialog &d);
    void draw_confirm_war(cairo_t *cr, Dialog &d);
    void draw_game_over(cairo_t *cr, Dialog &d);
    void draw_help(cairo_t *cr, Dialog &d);
    void draw_wonder_built(cairo_t *cr, Dialog &d);
    void draw_wonders(cairo_t *cr, Dialog &d);
    void draw_spaceship(cairo_t *cr, Dialog &d);
    void launch_spaceship();
    void draw_diplomat_menu(cairo_t *cr, Dialog &d);
    void draw_caravan_menu(cairo_t *cr, Dialog &d);
    void diplomat_choose(int unit_id, int tx, int ty, civ::DiploAction a);
    void caravan_choose(int unit_id, int city_id, int choice);
    void draw_title(cairo_t *cr);

    struct MenuItem {
        std::string label;
        std::string key;
        std::function<void()> fn;
        bool enabled = true;
    };
    std::vector<MenuItem> menu_items(int menu);
};

} // namespace ui
