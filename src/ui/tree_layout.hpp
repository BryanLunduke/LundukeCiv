// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

// A reusable, data-driven dependency-tree layout: longest-path layering
// left to right, barycenter crossing reduction, and era bands taken from
// depth. There is no game type here, so a later Culture or Religion tree
// can feed the same code. Cycle edges in the prerequisites are ignored
// so a bad table cannot hang the layout.

#include <string>
#include <unordered_map>
#include <vector>

namespace ui {

struct TreeNode {
    int id = -1;
    std::string name;
    std::vector<int> prereqs;   // ids; missing ids are ignored
    std::string category;       // optional; used only as a stable initial order
};

enum class TreeNodeState { Known, Researching, Available, Locked };

struct TreeNodeBox {
    int id = -1;
    int depth = 0;
    double x = 0, y = 0, w = 0, h = 0;
};

struct TreeEdge {
    int from = -1, to = -1;   // prerequisite -> node
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

struct TreeEraBand {
    std::string name;
    int depth0 = 0, depth1 = 0;
    double x = 0, w = 0;
};

struct TreeLayout {
    std::vector<TreeNodeBox> boxes;
    std::vector<TreeEdge> edges;
    std::vector<TreeEraBand> eras;
    int max_depth = 0;
    double content_w = 0, content_h = 0;
    std::unordered_map<int, int> index;   // id -> boxes[]
};

struct TreeLayoutParams {
    double col_w = 118;
    double row_h = 38;
    double canvas_h = 400;
    double gap = 8;
};

// Depth buckets used for era bands. A shallower tree simply leaves later
// bands empty. These names are a presentation policy, not a data table.
const char *tree_era_name(int depth);

inline TreeNodeState classify_node(bool known, bool researching, bool prereqs_met)
{
    if (researching) return TreeNodeState::Researching;
    if (known) return TreeNodeState::Known;
    if (prereqs_met) return TreeNodeState::Available;
    return TreeNodeState::Locked;
}

// Longest-path depth from the roots. Back-edges from a cycle are skipped.
// Unknown prerequisite ids are skipped. An empty input yields an empty map.
std::unordered_map<int, int> tree_depths(const std::vector<TreeNode> &nodes);

// Every ancestor of `goal` (including itself). Safe if the graph has a cycle
// or `goal` is not in the list (then the result is empty).
std::vector<int> tree_ancestor_path(const std::vector<TreeNode> &nodes, int goal);

// True when every listed prerequisite is in `known` (or the node has none).
bool tree_prereqs_met(const TreeNode &n, const std::vector<int> &known_ids);

TreeLayout layout_dependency_tree(const std::vector<TreeNode> &nodes, const TreeLayoutParams &p);

} // namespace ui
