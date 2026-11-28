// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#include "tree_layout.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <utility>
#include <unordered_set>

namespace ui {
namespace {

enum Color { White, Grey, Black };

// A stable category order for the first layer permutation. Unknown
// categories sort after these, then by name.
int category_track(const std::string &c)
{
    static const char *k[] = {"Military", "Exploration", "Economy", "Science", "Civic"};
    for (int i = 0; i < 5; ++i) {
        if (c == k[i]) return i;
    }
    return 5;
}

} // namespace

const char *tree_era_name(int depth)
{
    if (depth <= 2) return "Ancient";
    if (depth <= 5) return "Classical";
    if (depth <= 10) return "Industrial";
    return "Modern";
}

std::unordered_map<int, int> tree_depths(const std::vector<TreeNode> &nodes)
{
    std::unordered_map<int, const TreeNode *> by_id;
    by_id.reserve(nodes.size());
    for (const TreeNode &n : nodes) by_id[n.id] = &n;

    std::unordered_map<int, int> depth;
    std::unordered_map<int, Color> color;

    std::function<int(int)> rec = [&](int id) -> int {
        auto it = depth.find(id);
        if (it != depth.end()) return it->second;
        if (color[id] == Grey) return 0;   // back-edge: ignore this prereq
        const TreeNode *n = by_id.count(id) ? by_id[id] : nullptr;
        if (!n) {
            depth[id] = 0;
            return 0;
        }
        color[id] = Grey;
        int best = -1;
        for (int p : n->prereqs) {
            if (!by_id.count(p)) continue;
            if (color[p] == Grey) continue;
            best = std::max(best, rec(p));
        }
        color[id] = Black;
        const int d = best < 0 ? 0 : best + 1;
        depth[id] = d;
        return d;
    };

    for (const TreeNode &n : nodes) rec(n.id);
    return depth;
}

std::vector<int> tree_ancestor_path(const std::vector<TreeNode> &nodes, int goal)
{
    std::unordered_map<int, const TreeNode *> by_id;
    for (const TreeNode &n : nodes) by_id[n.id] = &n;
    if (!by_id.count(goal)) return {};

    std::vector<int> out;
    std::unordered_set<int> seen;
    std::function<void(int)> walk = [&](int id) {
        if (seen.count(id) || !by_id.count(id)) return;
        seen.insert(id);
        out.push_back(id);
        for (int p : by_id[id]->prereqs) walk(p);
    };
    walk(goal);
    return out;
}

bool tree_prereqs_met(const TreeNode &n, const std::vector<int> &known_ids)
{
    std::unordered_set<int> have(known_ids.begin(), known_ids.end());
    for (int p : n.prereqs) {
        if (!have.count(p)) return false;
    }
    return true;
}

TreeLayout layout_dependency_tree(const std::vector<TreeNode> &nodes, const TreeLayoutParams &p)
{
    TreeLayout out;
    if (nodes.empty()) return out;

    const auto depth = tree_depths(nodes);
    int max_d = 0;
    for (const auto &kv : depth) max_d = std::max(max_d, kv.second);
    out.max_depth = max_d;

    std::vector<std::vector<int>> layers(max_d + 1);
    std::unordered_map<int, const TreeNode *> by_id;
    for (const TreeNode &n : nodes) {
        by_id[n.id] = &n;
        auto it = depth.find(n.id);
        const int d = it == depth.end() ? 0 : it->second;
        layers[d].push_back(n.id);
    }

    // Children lists for the backward barycenter pass.
    std::unordered_map<int, std::vector<int>> children;
    for (const TreeNode &n : nodes) {
        for (int r : n.prereqs) {
            if (by_id.count(r)) children[r].push_back(n.id);
        }
    }

    auto init_order = [&](std::vector<int> &ids) {
        std::sort(ids.begin(), ids.end(), [&](int a, int b) {
            const int ta = category_track(by_id[a]->category), tb = category_track(by_id[b]->category);
            if (ta != tb) return ta < tb;
            if (by_id[a]->name != by_id[b]->name) return by_id[a]->name < by_id[b]->name;
            return a < b;
        });
    };
    for (auto &layer : layers) init_order(layer);

    auto index_in = [](const std::vector<int> &ids, int id) {
        for (size_t i = 0; i < ids.size(); ++i) {
            if (ids[i] == id) return static_cast<double>(i);
        }
        return ids.empty() ? 0.0 : ids.size() / 2.0;
    };

    for (int pass = 0; pass < 12; ++pass) {
        for (int d = 1; d <= max_d; ++d) {
            std::sort(layers[d].begin(), layers[d].end(), [&](int a, int b) {
                auto key = [&](int id) {
                    const TreeNode *n = by_id[id];
                    double s = 0;
                    int c = 0;
                    for (int r : n->prereqs) {
                        if (!by_id.count(r)) continue;
                        const int rd = depth.at(r);
                        const double norm = layers[rd].empty() ? 1.0 : static_cast<double>(layers[rd].size());
                        s += index_in(layers[rd], r) / norm;
                        ++c;
                    }
                    return std::make_pair(c ? s / c : 0.0, n->name);
                };
                return key(a) < key(b);
            });
        }
        for (int d = max_d - 1; d >= 0; --d) {
            std::sort(layers[d].begin(), layers[d].end(), [&](int a, int b) {
                auto key = [&](int id) {
                    const auto it = children.find(id);
                    if (it == children.end() || it->second.empty()) return std::make_pair(0.0, by_id[id]->name);
                    double s = 0;
                    int c = 0;
                    for (int ch : it->second) {
                        const int cd = depth.at(ch);
                        const double norm = layers[cd].empty() ? 1.0 : static_cast<double>(layers[cd].size());
                        s += index_in(layers[cd], ch) / norm;
                        ++c;
                    }
                    return std::make_pair(c ? s / c : 0.0, by_id[id]->name);
                };
                return key(a) < key(b);
            });
        }
    }

    // Vertical packing: barycenter, then a two-pass overlap resolve.
    const double min_gap = p.gap;
    const double row_h = p.row_h;
    int tallest = 1;
    for (const auto &l : layers) tallest = std::max(tallest, static_cast<int>(l.size()));
    const double need = tallest * row_h + (tallest - 1) * min_gap;
    const double pack_h = std::min(p.canvas_h, std::max(need + 16.0, std::min(p.canvas_h * 0.72, need + 80.0)));
    const double top0 = std::max(0.0, (p.canvas_h - pack_h) / 2.0);
    const double lo = top0, hi = top0 + pack_h - row_h;

    std::unordered_map<int, double> ys;
    for (const auto &layer : layers) {
        const int n = static_cast<int>(layer.size());
        if (n == 0) continue;
        if (n == 1) {
            ys[layer[0]] = top0 + (pack_h - row_h) / 2.0;
        } else {
            const double inner = pack_h - row_h;
            for (int i = 0; i < n; ++i) ys[layer[i]] = top0 + inner * i / (n - 1);
        }
    }

    auto resolve = [&](const std::vector<int> &ids) {
        if (ids.empty()) return;
        std::vector<int> order = ids;
        std::sort(order.begin(), order.end(), [&](int a, int b) { return ys[a] < ys[b]; });
        const int n = static_cast<int>(order.size());
        ys[order[0]] = std::max(lo, ys[order[0]]);
        for (int i = 1; i < n; ++i) ys[order[i]] = std::max(ys[order[i]], ys[order[i - 1]] + row_h + min_gap);
        if (ys[order[n - 1]] > hi) {
            ys[order[n - 1]] = hi;
            for (int i = n - 2; i >= 0; --i) ys[order[i]] = std::min(ys[order[i]], ys[order[i + 1]] - row_h - min_gap);
        }
        if (ys[order[0]] < lo - 0.5 || ys[order[n - 1]] > hi + 0.5) {
            if (n == 1) {
                ys[order[0]] = lo;
            } else {
                for (int i = 0; i < n; ++i) ys[order[i]] = lo + (hi - lo) * i / (n - 1);
            }
        }
    };

    for (int pass = 0; pass < 8; ++pass) {
        for (const TreeNode &n : nodes) {
            std::vector<double> pts;
            for (int r : n.prereqs) {
                if (ys.count(r)) pts.push_back(ys[r]);
            }
            for (int ch : children[n.id]) {
                if (ys.count(ch)) pts.push_back(ys[ch]);
            }
            if (!pts.empty() && ys.count(n.id)) {
                double s = 0;
                for (double v : pts) s += v;
                ys[n.id] = 0.45 * ys[n.id] + 0.55 * (s / pts.size());
            }
        }
        for (const auto &layer : layers) resolve(layer);
    }

    const double nw = p.col_w - 8;
    out.boxes.reserve(nodes.size());
    for (int d = 0; d <= max_d; ++d) {
        const double x = d * p.col_w + (p.col_w - nw) / 2.0;
        for (int id : layers[d]) {
            TreeNodeBox b;
            b.id = id;
            b.depth = d;
            b.x = x;
            b.y = ys[id];
            b.w = nw;
            b.h = row_h;
            out.index[id] = static_cast<int>(out.boxes.size());
            out.boxes.push_back(b);
        }
    }

    for (const TreeNode &n : nodes) {
        const int ti = out.index.count(n.id) ? out.index[n.id] : -1;
        if (ti < 0) continue;
        const TreeNodeBox &to = out.boxes[ti];
        for (int r : n.prereqs) {
            if (!out.index.count(r)) continue;
            const TreeNodeBox &fr = out.boxes[out.index[r]];
            TreeEdge e;
            e.from = r;
            e.to = n.id;
            e.x0 = fr.x + fr.w;
            e.y0 = fr.y + fr.h / 2.0;
            e.x1 = to.x;
            e.y1 = to.y + to.h / 2.0;
            out.edges.push_back(e);
        }
    }

    // Era bands from contiguous depth ranges of the same name.
    int d = 0;
    while (d <= max_d) {
        const char *name = tree_era_name(d);
        int d1 = d;
        while (d1 + 1 <= max_d && std::strcmp(tree_era_name(d1 + 1), name) == 0) ++d1;
        TreeEraBand band;
        band.name = name;
        band.depth0 = d;
        band.depth1 = d1;
        band.x = d * p.col_w;
        band.w = (d1 - d + 1) * p.col_w;
        out.eras.push_back(band);
        d = d1 + 1;
    }

    out.content_w = (max_d + 1) * p.col_w;
    out.content_h = p.canvas_h;
    return out;
}

} // namespace ui
