// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.
//
// The Databank: plain gameplay facts about every unit, advance, improvement,
// wonder, government and terrain, read straight from the rules tables so it
// grows with them.

#pragma once

#include <string>
#include <vector>

namespace civ {

enum class DatabankTopic { Units, Advances, Improvements, Wonders, Governments, Terrain, Count };
const char *databank_topic_name(DatabankTopic t);

struct DatabankEntry {
    std::string name;
    std::vector<std::string> facts;
};

std::vector<DatabankEntry> databank(DatabankTopic t);

} // namespace civ
