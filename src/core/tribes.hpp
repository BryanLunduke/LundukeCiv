// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.
//
// The fourteen peoples of Civilization 1. Each of the seven colours (slots)
// belongs to two of them, and only one of a pair is ever in a game; a civil
// war splits a people into its pair. Leaders and leanings are the game's
// facts; the city lists are our own picks from history.

#pragma once

namespace civ {

constexpr int kTribeCount = 14;
constexpr int kTribeSlots = 7;

struct TribeInfo {
    const char *name;      // plural: "Romans"
    const char *adjective; // "Roman"
    const char *leader;
    int slot;              // 0..6: the pair of peoples sharing a colour
    int color;             // index into the civ colours
    int mood;              // -1 friendly, 0, 1 aggressive
    int policy;            // -1 perfectionist, 0, 1 expansionist
    int ideology;          // -1 militaristic, 0, 1 civilized
    const char *cities[16];
};

const TribeInfo &tribe_info(int t);
// The other people sharing t's colour: the side a civil war breaks away to.
int tribe_pair(int t);
const char *mood_name(int m);
const char *policy_name(int p);
const char *ideology_name(int i);

} // namespace civ
