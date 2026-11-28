// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

// Technology tree: layout from a node list (including adding and removing
// an advance), cycle safety, and known / researching / available / locked
// state. One named group per meson test.

#include "tree_layout.hpp"
#include "rules.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace ui;
using civ::Advance;
using civ::AdvanceInfo;
using civ::advance_info;
using civ::kAdvanceCount;

static int failures = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                                                 \
    do {                                                                                               \
        const long long va = (a), vb = (b);                                                            \
        if (va != vb) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s == %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb); \
            ++failures;                                                                                \
        }                                                                                              \
    } while (0)

static std::vector<TreeNode> toy_chain()
{
    // A -> B -> C, and D a root beside A, C also needs D.
    return {
        {1, "A", {}, "Science"},
        {2, "B", {1}, "Science"},
        {3, "C", {2, 4}, "Civic"},
        {4, "D", {}, "Civic"},
    };
}

static std::vector<TreeNode> from_advances()
{
    std::vector<TreeNode> nodes;
    nodes.reserve(kAdvanceCount);
    for (int i = 0; i < kAdvanceCount; ++i) {
        const Advance a = static_cast<Advance>(i);
        const AdvanceInfo &info = advance_info(a);
        TreeNode n;
        n.id = i;
        n.name = info.name;
        if (info.req1 != Advance::None) n.prereqs.push_back(static_cast<int>(info.req1));
        if (info.req2 != Advance::None) n.prereqs.push_back(static_cast<int>(info.req2));
        nodes.push_back(std::move(n));
    }
    return nodes;
}

static void test_layout_from_data()
{
    const std::vector<TreeNode> nodes = from_advances();
    CHECK_EQ(static_cast<int>(nodes.size()), kAdvanceCount);
    TreeLayoutParams p;
    p.col_w = 100;
    p.row_h = 32;
    p.canvas_h = 420;
    const TreeLayout lay = layout_dependency_tree(nodes, p);
    CHECK_EQ(static_cast<int>(lay.boxes.size()), kAdvanceCount);
    CHECK(lay.max_depth >= 8);
    CHECK(lay.content_w >= (lay.max_depth + 1) * p.col_w - 0.5);
    CHECK(!lay.eras.empty());

    // Every listed prerequisite sits in an earlier (or equal? no, earlier)
    // column than the advance it leads to.
    std::unordered_map<int, const TreeNodeBox *> box;
    for (const TreeNodeBox &b : lay.boxes) box[b.id] = &b;
    for (const TreeNode &n : nodes) {
        CHECK(box.count(n.id));
        for (int r : n.prereqs) {
            CHECK(box.count(r));
            CHECK(box[r]->depth < box[n.id]->depth || box[r]->x + box[r]->w <= box[n.id]->x + 0.5);
            CHECK(box[r]->depth <= box[n.id]->depth);
        }
    }

    // No two nodes in the same layer overlap vertically.
    std::unordered_map<int, std::vector<const TreeNodeBox *>> layers;
    for (const TreeNodeBox &b : lay.boxes) layers[b.depth].push_back(&b);
    for (auto &kv : layers) {
        auto &row = kv.second;
        std::sort(row.begin(), row.end(), [](const TreeNodeBox *a, const TreeNodeBox *b) { return a->y < b->y; });
        for (size_t i = 1; i < row.size(); ++i) {
            CHECK(row[i]->y + 0.01 >= row[i - 1]->y + row[i - 1]->h);
        }
    }

    // Alphabet is a root; Future Tech is at the deep end.
    const int alphabet = static_cast<int>(Advance::Alphabet);
    const int future = static_cast<int>(Advance::FutureTech);
    CHECK_EQ(box[alphabet]->depth, 0);
    CHECK(box[future]->depth == lay.max_depth);

    // Era bands cover 0..max_depth without gaps.
    int covered = 0;
    for (const TreeEraBand &e : lay.eras) {
        CHECK(e.depth0 == covered);
        CHECK(e.depth1 >= e.depth0);
        covered = e.depth1 + 1;
    }
    CHECK_EQ(covered, lay.max_depth + 1);
}

static void test_add_remove_advance()
{
    std::vector<TreeNode> nodes = toy_chain();
    TreeLayoutParams p;
    p.col_w = 80;
    p.row_h = 24;
    p.canvas_h = 200;
    const TreeLayout a = layout_dependency_tree(nodes, p);
    CHECK_EQ(static_cast<int>(a.boxes.size()), 4);
    const auto da = tree_depths(nodes);
    CHECK_EQ(da.at(1), 0);
    CHECK_EQ(da.at(2), 1);
    CHECK_EQ(da.at(4), 0);
    CHECK_EQ(da.at(3), 2);   // C needs B (depth 1) and D (depth 0)

    // Adding E, which needs C, lengthens the tree.
    nodes.push_back({5, "E", {3}, "Science"});
    const TreeLayout b = layout_dependency_tree(nodes, p);
    CHECK_EQ(static_cast<int>(b.boxes.size()), 5);
    CHECK(b.max_depth == a.max_depth + 1);
    CHECK_EQ(tree_depths(nodes).at(5), 3);

    // Removing B: C still has D, so C becomes depth 1; E becomes 2.
    // B is gone from the layout.
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [](const TreeNode &n) { return n.id == 2; }), nodes.end());
    // C still lists 2 as a prereq; missing ids are ignored.
    const TreeLayout c = layout_dependency_tree(nodes, p);
    CHECK_EQ(static_cast<int>(c.boxes.size()), 4);
    CHECK(!c.index.count(2));
    CHECK(c.index.count(3) && c.index.count(5));
    const auto dc = tree_depths(nodes);
    CHECK_EQ(dc.at(3), 1);
    CHECK_EQ(dc.at(5), 2);
}

static void test_cycle_safety()
{
    // A -> B -> A, plus a root C that A also lists.
    const std::vector<TreeNode> nodes = {
        {1, "A", {2, 3}, "Science"},
        {2, "B", {1}, "Science"},
        {3, "C", {}, "Science"},
    };
    const auto d = tree_depths(nodes);
    CHECK(d.count(1) && d.count(2) && d.count(3));
    CHECK_EQ(d.at(3), 0);
    // The back-edge is ignored, so depths are finite and small.
    CHECK(d.at(1) >= 0 && d.at(1) <= 2);
    CHECK(d.at(2) >= 0 && d.at(2) <= 3);

    TreeLayoutParams p;
    p.canvas_h = 160;
    const TreeLayout lay = layout_dependency_tree(nodes, p);
    CHECK_EQ(static_cast<int>(lay.boxes.size()), 3);
    CHECK(lay.max_depth <= 3);

    // Ancestor walk must also stop on a cycle.
    const std::vector<int> path = tree_ancestor_path(nodes, 1);
    CHECK_EQ(static_cast<int>(path.size()), 3);   // A, B, C each once
    std::unordered_set<int> uniq(path.begin(), path.end());
    CHECK_EQ(static_cast<int>(uniq.size()), 3);

    // A goal that is not in the list is an empty path, not a hang.
    CHECK(tree_ancestor_path(nodes, 99).empty());
}

static void test_state_computation()
{
    const std::vector<TreeNode> nodes = from_advances();
    auto node = [&](Advance a) -> const TreeNode & {
        return nodes[static_cast<int>(a)];
    };

    // Starting advances: no prereqs, not yet known.
    CHECK(tree_prereqs_met(node(Advance::Alphabet), {}));
    CHECK(tree_prereqs_met(node(Advance::Masonry), {}));
    CHECK(!tree_prereqs_met(node(Advance::Writing), {}));

    // Writing needs Alphabet.
    const std::vector<int> alphabet = {static_cast<int>(Advance::Alphabet)};
    CHECK(tree_prereqs_met(node(Advance::Writing), alphabet));
    CHECK(!tree_prereqs_met(node(Advance::Literacy), alphabet));   // also needs Code of Laws

    CHECK(classify_node(false, false, true) == TreeNodeState::Available);
    CHECK(classify_node(true, false, true) == TreeNodeState::Known);
    CHECK(classify_node(false, true, true) == TreeNodeState::Researching);
    CHECK(classify_node(false, false, false) == TreeNodeState::Locked);
    // Researching wins over known, if both were ever set.
    CHECK(classify_node(true, true, true) == TreeNodeState::Researching);

    // A mid-game set used by the screen mockup: Engineering + Literacy known
    // makes Invention available; Railroad is still locked (needs Steam Engine).
    std::vector<int> known;
    const Advance k[] = {
        Advance::Alphabet,       Advance::BronzeWorking, Advance::CeremonialBurial, Advance::HorsebackRiding,
        Advance::Masonry,        Advance::Pottery,       Advance::TheWheel,         Advance::CodeOfLaws,
        Advance::MapMaking,      Advance::Mathematics,   Advance::Writing,          Advance::Currency,
        Advance::IronWorking,    Advance::Mysticism,     Advance::Construction,     Advance::Monarchy,
        Advance::Literacy,       Advance::Trade,         Advance::Engineering,      Advance::Astronomy,
        Advance::Philosophy,     Advance::Feudalism,
    };
    for (Advance a : k) known.push_back(static_cast<int>(a));
    CHECK(tree_prereqs_met(node(Advance::Invention), known));
    CHECK(!tree_prereqs_met(node(Advance::Railroad), known));
    CHECK(tree_prereqs_met(node(Advance::BridgeBuilding), known));
    CHECK(tree_prereqs_met(node(Advance::Navigation), known));
    CHECK(!tree_prereqs_met(node(Advance::Physics), known));   // needs Navigation, which is not known yet

    const std::unordered_set<int> have(known.begin(), known.end());
    auto state_of = [&](Advance a) {
        const bool kn = have.count(static_cast<int>(a));
        const bool res = a == Advance::Invention;
        return classify_node(kn, res, tree_prereqs_met(node(a), known));
    };
    CHECK(state_of(Advance::Alphabet) == TreeNodeState::Known);
    CHECK(state_of(Advance::Invention) == TreeNodeState::Researching);
    CHECK(state_of(Advance::University) == TreeNodeState::Available);
    CHECK(state_of(Advance::Railroad) == TreeNodeState::Locked);

    const std::vector<int> path = tree_ancestor_path(nodes, static_cast<int>(Advance::Railroad));
    std::unordered_set<int> on(path.begin(), path.end());
    CHECK(on.count(static_cast<int>(Advance::Railroad)));
    CHECK(on.count(static_cast<int>(Advance::SteamEngine)));
    CHECK(on.count(static_cast<int>(Advance::BridgeBuilding)));
    CHECK(on.count(static_cast<int>(Advance::Invention)));
    CHECK(on.count(static_cast<int>(Advance::Alphabet)));
    CHECK(!on.count(static_cast<int>(Advance::Pottery)));   // not on the path
}

int main(int argc, char **argv)
{
    const std::string name = argc > 1 ? argv[1] : "";
    const struct {
        const char *n;
        void (*fn)();
    } groups[] = {
        {"layout_from_data", test_layout_from_data},
        {"add_remove_advance", test_add_remove_advance},
        {"cycle_safety", test_cycle_safety},
        {"state_computation", test_state_computation},
    };
    if (name == "--list") {
        for (const auto &g : groups) std::printf("%s\n", g.n);
        return 0;
    }
    int ran = 0;
    for (const auto &g : groups) {
        if (name.empty() || name == g.n) {
            g.fn();
            ++ran;
        }
    }
    if (!name.empty() && ran == 0) {
        std::fprintf(stderr, "unknown test group: %s\n", name.c_str());
        return 2;
    }
    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
