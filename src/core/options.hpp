// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.
//
// Game Options (the DOS Game menu's switches, less Sound), kept as plain
// text in the player's data folder next to the saves, and two small helpers
// for the Game menu: when an autosave is due and Find City.

#pragma once

#include "game.hpp"

#include <string>
#include <vector>

namespace civ {

struct GameOptions {
    bool instant_advice = false;   // advisers speak up at once (disorder, a first city...)
    bool autosave = true;          // every kAutosaveTurns turns, to the autosave slot
    bool end_of_turn = true;       // wait for Enter at the end of a turn
    bool animations = true;        // the active unit blinks
    bool enemy_moves = true;       // show where enemy units went between turns
    bool event_screens = true;     // full screens for advances, governments, cities taken
};

constexpr int kAutosaveTurns = 50;   // as in DOS

std::string save_options(const GameOptions &o);
GameOptions load_options(const std::string &text);   // unknown or bad lines keep the defaults

// An autosave is due at the start of the human player's turn on every
// kAutosaveTurns-th turn (not on the first).
bool autosave_due(int turn);

// Cities the civ knows of (its own, and others on squares it has seen)
// whose names start with the text, any case; its own first, then by name.
std::vector<int> find_cities(const Game &g, int civ, const std::string &prefix);

} // namespace civ
