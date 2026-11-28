// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#pragma once

// DependencyTreeView: a pannable, data-driven layered graph. It draws any
// list of TreeNode records; the Technology Tree is just one adapter.

#include "draw.hpp"
#include "tree_layout.hpp"

#include <cairo.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ui {

struct TreeNodeInfo {
    TreeNodeState state = TreeNodeState::Locked;
    std::string requires_text;
    std::string allows_text;
    std::string leads_text;
    std::string cost_text;
    std::string category;
};

struct TreeViewModel {
    std::vector<TreeNode> nodes;
    std::unordered_map<int, TreeNodeInfo> info;
    int researching = -1;
    int goal = -1;
    std::string title_line;
    std::string research_name;    // empty if none
    std::string research_right;   // "94 / 221 · 4 turns"
    double research_frac = 0;
};

class DependencyTreeView {
public:
    void set_model(TreeViewModel model);
    // Rebuild the layout for this canvas size. Keeps the current pan,
    // then clamps it.
    void relayout(double canvas_w, double canvas_h);

    void pan_by(double dx, double dy);
    void center_on(int id);
    void jump_home();
    void jump_end();

    void set_canvas(double x, double y, double w, double h);
    bool in_canvas(double x, double y) const;
    // Hit-test in window coordinates. -1 if none.
    int hit_node(double window_x, double window_y) const;

    // Draw the clipped graph (era bands, edges, nodes, scrollbar). Hover
    // is taken from the mouse position in window coordinates.
    void draw_canvas(cairo_t *cr, double mouse_x, double mouse_y);
    void draw_tooltip(cairo_t *cr, double mouse_x, double mouse_y, int forced_id = -1);

    double pan_x() const { return pan_x_; }
    double pan_y() const { return pan_y_; }
    int hovered() const { return hover_; }
    const TreeLayout &layout() const { return layout_; }
    const TreeViewModel &model() const { return model_; }
    const std::unordered_set<int> &path() const { return path_; }
    double canvas_x() const { return cx_; }
    double canvas_y() const { return cy_; }
    double canvas_w() const { return cw_; }
    double canvas_h() const { return ch_; }

private:
    void clamp_pan();
    void refresh_path();
    TreeLayoutParams params_for(double canvas_w, double canvas_h) const;
    void draw_node(cairo_t *cr, const TreeNodeBox &b, bool hover);

    TreeViewModel model_;
    TreeLayout layout_;
    std::unordered_set<int> path_;
    double pan_x_ = 0, pan_y_ = 0;
    double cx_ = 0, cy_ = 0, cw_ = 0, ch_ = 0;
    int hover_ = -1;
    double inner_top_ = 26;   // era label strip
    double inner_bot_ = 18;   // scrollbar strip
    double inner_x_ = 6;
};

} // namespace ui
