// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

// Thin adapter: the game's advances become a TreeViewModel. Layout and
// drawing stay generic so a later Culture or Religion tree can reuse them.

#include "tree_view.hpp"

#include "game.hpp"

namespace ui {

const char *advance_category(civ::Advance a);
TreeViewModel make_advance_tree_model(const civ::Game &g, int civ, int goal_id);

} // namespace ui
