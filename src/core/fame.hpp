// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.
//
// The Hall of Fame: the six best finished games, ranked as the DOS game
// ranks them by Civilization Rating (score x (level + 1) / 50, in percent).
// Kept as plain text in the player's data folder; the window code reads and
// writes the file, this only turns entries into text and back.

#pragma once

#include "game.hpp"

#include <string>
#include <vector>

namespace civ {

constexpr int kFameSize = 6;

struct FameEntry {
    std::string leader;
    std::string people;      // "Lundukians"
    int difficulty = 0;
    int year = 0;            // the final year
    long long population = 0;
    int score = 0;
    int rating() const { return score * (difficulty + 1) / 50; }
};

// The human ruler's entry for the game as it stands.
FameEntry fame_entry(Game &g);
std::string save_fame(const std::vector<FameEntry> &list);
std::vector<FameEntry> load_fame(const std::string &text);   // bad lines are skipped
// Places the entry by rating (ahead of equal ones already there, as in
// DOS a newer entry must beat an older one to pass it); the list stays at
// kFameSize. Returns the place, 0 the best, or -1 if it did not make it.
int add_fame(std::vector<FameEntry> &list, const FameEntry &e);
// The ruler of history whose name a finished game earns: higher ratings,
// greater rulers (our own list of twenty, from a cautionary tale to a
// legend). Index 0..19.
int famous_rank(int rating);
const char *famous_ruler(int rank);

} // namespace civ
