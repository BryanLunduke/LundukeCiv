// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#include "tree_view.hpp"

#include <algorithm>
#include <cmath>
#include <pango/pangocairo.h>

namespace ui {
namespace {

void bezier(cairo_t *cr, double x0, double y0, double x1, double y1, const Ink &ink, double width, double alpha)
{
    set(cr, ink, alpha);
    cairo_set_line_width(cr, width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, x0, y0);
    const double dx = std::max(12.0, (x1 - x0) * 0.45);
    cairo_curve_to(cr, x0 + dx, y0, x1 - dx, y1, x1, y1);
    cairo_stroke(cr);
}

void hscrollbar(cairo_t *cr, double x, double y, double w, double h, double thumb_x, double thumb_w)
{
    round_rect(cr, x, y, w, h, h / 2);
    set(cr, kSlateDeep);
    cairo_fill(cr);
    const double tw = std::max(h, thumb_w);
    const double tx = std::min(x + w - tw, std::max(x, thumb_x));
    round_rect(cr, tx, y + 1, tw, h - 2, (h - 2) / 2);
    set(cr, kSlateEdge);
    cairo_fill(cr);
}

// Wrap on spaces, or shrink a long single word, rather than hyphenating.
void node_label(cairo_t *cr, const std::string &name, double x, double y, double max_w, double max_h, const Ink &ink)
{
    // One consistent size (Sans Bold 10). Long names wrap to two lines;
    // columns are wide enough that we never shrink the type.
    const char *font = kSmallBold;
    std::string chosen = name;
    const Metrics one = measure(cr, font, name);
    if (one.w > max_w + 1 && name.find(' ') != std::string::npos) {
        double best = 1e9;
        size_t pos = 0;
        while ((pos = name.find(' ', pos)) != std::string::npos) {
            const std::string s = name.substr(0, pos) + "\n" + name.substr(pos + 1);
            const Metrics m = measure(cr, font, s);
            const double overflow = std::max(0.0, m.w - max_w) + std::max(0.0, m.h - max_h);
            const double score = overflow * 1000.0 + m.w;
            if (score < best) {
                best = score;
                chosen = s;
            }
            ++pos;
        }
    }
    const Metrics m = measure(cr, font, chosen);
    const double ty = y + std::max(0.0, (max_h - m.h) / 2.0);
    text(cr, font, chosen, x, ty, ink);
}

void checkmark(cairo_t *cr, double x, double y, double s, const Ink &ink)
{
    cairo_save(cr);
    cairo_set_line_width(cr, 1.6);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    set(cr, ink);
    cairo_move_to(cr, x + s * 0.15, y + s * 0.55);
    cairo_line_to(cr, x + s * 0.40, y + s * 0.80);
    cairo_line_to(cr, x + s * 0.85, y + s * 0.22);
    cairo_stroke(cr);
    cairo_restore(cr);
}

} // namespace

void DependencyTreeView::set_model(TreeViewModel model)
{
    model_ = std::move(model);
    refresh_path();
    if (cw_ > 0 && ch_ > 0) relayout(cw_, ch_);
}

void DependencyTreeView::refresh_path()
{
    path_.clear();
    if (model_.goal < 0) return;
    for (int id : tree_ancestor_path(model_.nodes, model_.goal)) path_.insert(id);
}

TreeLayoutParams DependencyTreeView::params_for(double canvas_w, double canvas_h) const
{
    TreeLayoutParams p;
    // Wide enough for "Industrialization" at Sans Bold 10, plus a checkmark.
    // Two-line labels are 38 px tall; use a 42 px row when the tallest
    // layer (8 advances) still fits, and tighten only on a short window.
    p.col_w = canvas_w >= 1400 ? 148 : (canvas_w >= 1000 ? 150 : 146);
    p.canvas_h = std::max(80.0, canvas_h - inner_top_ - inner_bot_);
    const double avail = p.canvas_h;
    constexpr int kTallest = 8;
    auto fits = [&](double row, double gap) {
        return kTallest * row + (kTallest - 1) * gap <= avail + 0.5;
    };
    if (fits(42, 8)) {
        p.row_h = 42;
        p.gap = 8;
    } else if (fits(40, 6)) {
        p.row_h = 40;
        p.gap = 6;
    } else if (fits(38, 4)) {
        p.row_h = 38;
        p.gap = 4;
    } else {
        p.row_h = 38;
        p.gap = std::max(2.0, std::floor((avail - kTallest * 38) / (kTallest - 1)));
    }
    return p;
}

void DependencyTreeView::relayout(double canvas_w, double canvas_h)
{
    cw_ = canvas_w;
    ch_ = canvas_h;
    layout_ = layout_dependency_tree(model_.nodes, params_for(canvas_w, canvas_h));
    clamp_pan();
    center_visible_y();
}

void DependencyTreeView::set_canvas(double x, double y, double w, double h)
{
    const bool size_changed = std::fabs(w - cw_) > 0.5 || std::fabs(h - ch_) > 0.5;
    cx_ = x;
    cy_ = y;
    if (size_changed || layout_.boxes.empty()) relayout(w, h);
    else {
        cw_ = w;
        ch_ = h;
        clamp_pan();
        center_visible_y();
    }
}

void DependencyTreeView::clamp_pan()
{
    const double view_w = std::max(0.0, cw_ - inner_x_ * 2);
    const double view_h = std::max(0.0, ch_ - inner_top_ - inner_bot_);
    const double max_x = std::max(0.0, layout_.content_w - view_w);
    const double max_y = std::max(0.0, layout_.content_h - view_h);
    pan_x_ = std::clamp(pan_x_, 0.0, max_x);
    pan_y_ = std::clamp(pan_y_, 0.0, max_y);
}

void DependencyTreeView::center_visible_y()
{
    // When a horizontal pan leaves only a short band of nodes in view
    // (the Modern tail, for example), lift that band into the middle of
    // the viewport. A column that already fills the height is left alone.
    if (layout_.boxes.empty() || ch_ <= 0) return;
    const double view_w = std::max(1.0, cw_ - inner_x_ * 2);
    const double view_h = std::max(1.0, ch_ - inner_top_ - inner_bot_);
    const double left = pan_x_, right = pan_x_ + view_w;
    double min_y = 0, max_y = 0;
    bool any = false;
    for (const TreeNodeBox &b : layout_.boxes) {
        const double vis = std::min(b.x + b.w, right) - std::max(b.x, left);
        if (vis < std::min(24.0, b.w * 0.4)) continue;
        if (!any) {
            min_y = b.y;
            max_y = b.y + b.h;
            any = true;
        } else {
            min_y = std::min(min_y, b.y);
            max_y = std::max(max_y, b.y + b.h);
        }
    }
    if (!any) return;
    const double used = max_y - min_y;
    if (used >= view_h - 8) {
        // Visible nodes already fill the viewport; keep pan_y in range.
        const double lo = 0.0;
        const double hi = std::max(0.0, layout_.content_h - view_h);
        pan_y_ = std::clamp(pan_y_, lo, hi);
        return;
    }
    pan_y_ = (min_y + max_y) / 2.0 - view_h / 2.0;
}

void DependencyTreeView::pan_by(double dx, double dy)
{
    pan_x_ += dx;
    pan_y_ += dy;
    clamp_pan();
    // Recentering is for a horizontal move that changes which columns
    // are in view. A vertical nudge from the arrows is left alone.
    if (std::fabs(dx) >= std::fabs(dy)) center_visible_y();
}

void DependencyTreeView::center_on(int id)
{
    if (!layout_.index.count(id)) return;
    const TreeNodeBox &b = layout_.boxes[layout_.index[id]];
    const double view_w = std::max(1.0, cw_ - inner_x_ * 2);
    const double view_h = std::max(1.0, ch_ - inner_top_ - inner_bot_);
    pan_x_ = b.x + b.w / 2.0 - view_w / 2.0;
    pan_y_ = b.y + b.h / 2.0 - view_h / 2.0;
    clamp_pan();
    center_visible_y();
}

void DependencyTreeView::jump_home()
{
    pan_x_ = 0;
    clamp_pan();
    center_visible_y();
}

void DependencyTreeView::jump_end()
{
    pan_x_ = 1e9;
    clamp_pan();
    center_visible_y();
}

bool DependencyTreeView::in_canvas(double x, double y) const
{
    return x >= cx_ && y >= cy_ && x < cx_ + cw_ && y < cy_ + ch_;
}

int DependencyTreeView::hit_node(double window_x, double window_y) const
{
    if (!in_canvas(window_x, window_y)) return -1;
    const double lx = window_x - cx_ - inner_x_ + pan_x_;
    const double ly = window_y - cy_ - inner_top_ + pan_y_;
    for (const TreeNodeBox &b : layout_.boxes) {
        if (lx >= b.x && ly >= b.y && lx < b.x + b.w && ly < b.y + b.h) return b.id;
    }
    return -1;
}

void DependencyTreeView::draw_node(cairo_t *cr, const TreeNodeBox &b, bool hover)
{
    TreeNodeState st = TreeNodeState::Locked;
    auto it = model_.info.find(b.id);
    if (it != model_.info.end()) st = it->second.state;
    const bool on = path_.count(b.id) != 0;

    Ink fill, stroke, ink;
    double lw = 1.0;
    if (st == TreeNodeState::Researching) {
        fill = hex(0x3E4A5E);
        stroke = kSlateGold;
        ink = kSlateGold;
        lw = 2.0;
    } else if (st == TreeNodeState::Known) {
        fill = hex(0x2F3C4C);
        stroke = on ? kSlateGoldDim : hex(0x3C4859);
        ink = kSlateInk;
        lw = on ? 1.6 : 1.0;
    } else if (st == TreeNodeState::Available) {
        fill = hover ? hex(0x354154) : hex(0x313C4C);
        stroke = on ? kSlateGold : kSlateGood;
        ink = kSlateInk;
        lw = on ? 1.8 : 1.2;
    } else {
        fill = hex(0x1A212B);
        stroke = on ? kSlateGoldDim : hex(0x2A3340);
        ink = on ? kSlateDim : kSlateEdge;
        lw = on ? 1.6 : 1.0;
    }
    if (hover) fill = hex(0x354154);

    const double x = cx_ + inner_x_ - pan_x_ + b.x;
    const double y = cy_ + inner_top_ - pan_y_ + b.y;
    round_rect(cr, x + 0.5, y + 0.5, b.w - 1, b.h - 1, 5);
    set(cr, fill);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, lw);
    set(cr, stroke);
    cairo_stroke(cr);

    const char *name = "?";
    for (const TreeNode &n : model_.nodes) {
        if (n.id == b.id) {
            name = n.name.c_str();
            break;
        }
    }
    const bool mark = st == TreeNodeState::Known || st == TreeNodeState::Researching;
    const double tx = x + 5;
    const double max_w = std::max(16.0, b.w - 10 - (mark ? 12 : 0));
    node_label(cr, name, tx, y, max_w, b.h, ink);
    if (st == TreeNodeState::Known) {
        checkmark(cr, x + b.w - 15, y + (b.h - 11) / 2, 11, kSlateGood);
    } else if (st == TreeNodeState::Researching) {
        set(cr, kSlateGold);
        cairo_arc(cr, x + b.w - 9, y + b.h / 2, 3.2, 0, 6.283185307179586);
        cairo_fill(cr);
    }
}

void DependencyTreeView::draw_canvas(cairo_t *cr, double mouse_x, double mouse_y)
{
    hover_ = hit_node(mouse_x, mouse_y);

    round_rect(cr, cx_, cy_, cw_, ch_, 8);
    set(cr, kSlateDeep);
    cairo_fill(cr);

    cairo_save(cr);
    round_rect(cr, cx_, cy_, cw_, ch_, 8);
    cairo_clip(cr);

    const double ox = cx_ + inner_x_ - pan_x_;
    const double oy = cy_ + inner_top_ - pan_y_;

    // Era tints and labels.
    const struct {
        const char *name;
        double r, g, b, a;
    } tints[] = {
        {"Ancient", 0.95, 0.82, 0.45, 0.05},
        {"Classical", 0.55, 0.45, 0.80, 0.05},
        {"Industrial", 0.70, 0.55, 0.30, 0.05},
        {"Modern", 0.35, 0.60, 0.75, 0.05},
    };
    auto tint_of = [&](const std::string &name) -> const double * {
        for (const auto &t : tints) {
            if (name == t.name) return &t.r;
        }
        return nullptr;
    };
    for (const TreeEraBand &e : layout_.eras) {
        const double x0 = ox + e.x;
        const double x1 = x0 + e.w;
        if (const double *t = tint_of(e.name)) {
            cairo_set_source_rgba(cr, t[0], t[1], t[2], t[3]);
            cairo_rectangle(cr, x0, cy_, x1 - x0, ch_);
            cairo_fill(cr);
        }
        set(cr, kSlateEdge, 0.45);
        cairo_set_line_width(cr, 1);
        cairo_move_to(cr, x1 + 0.5, cy_);
        cairo_line_to(cr, x1 + 0.5, cy_ + ch_);
        cairo_stroke(cr);
        std::string label = e.name;
        for (char &c : label) {
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        }
        const Metrics m = measure(cr, kTagFont, label);
        text(cr, kTagFont, label, (x0 + x1) / 2 - m.w / 2.0, cy_ + 6, kSlateGoldDim);
    }

    for (const TreeEdge &e : layout_.edges) {
        const bool on = path_.count(e.from) && path_.count(e.to);
        const double x0 = ox + e.x0, y0 = oy + e.y0, x1 = ox + e.x1, y1 = oy + e.y1;
        if (on) {
            bezier(cr, x0, y0, x1, y1, kSlateGold, 2.2, 0.95);
        } else {
            TreeNodeState st = TreeNodeState::Locked;
            auto it = model_.info.find(e.to);
            if (it != model_.info.end()) st = it->second.state;
            const double a = (st != TreeNodeState::Locked) ? 0.50 : 0.22;
            bezier(cr, x0, y0, x1, y1, kSlateEdge, 1.15, a);
        }
    }

    for (const TreeNodeBox &b : layout_.boxes) draw_node(cr, b, b.id == hover_);

    // Horizontal scrollbar when the tree is wider than the view.
    const double view_w = std::max(1.0, cw_ - 20);
    if (layout_.content_w > view_w + 2) {
        const double track_x = cx_ + 10, track_w = cw_ - 20;
        const double thumb_w = std::max(48.0, track_w * (view_w / layout_.content_w));
        const double max_pan = std::max(1.0, layout_.content_w - view_w);
        const double thumb_x = track_x + (track_w - thumb_w) * (pan_x_ / max_pan);
        hscrollbar(cr, track_x, cy_ + ch_ - 16, track_w, 10, thumb_x, thumb_w);
    }

    cairo_restore(cr);
}

void DependencyTreeView::draw_tooltip(cairo_t *cr, double mouse_x, double mouse_y, int forced_id)
{
    const int id = forced_id >= 0 ? forced_id : hover_;
    if (id < 0 || !layout_.index.count(id)) return;
    const TreeNodeBox &b = layout_.boxes[layout_.index[id]];
    const double nx = cx_ + inner_x_ - pan_x_ + b.x;
    const double ny = cy_ + inner_top_ - pan_y_ + b.y;

    const char *name = "?";
    for (const TreeNode &n : model_.nodes) {
        if (n.id == id) {
            name = n.name.c_str();
            break;
        }
    }
    TreeNodeInfo inf;
    auto it = model_.info.find(id);
    if (it != model_.info.end()) inf = it->second;
    const char *st_name = "Locked";
    if (inf.state == TreeNodeState::Known) st_name = "Known";
    else if (inf.state == TreeNodeState::Researching) st_name = "Researching";
    else if (inf.state == TreeNodeState::Available) st_name = "Available";

    std::string meta = inf.category;
    if (!meta.empty()) meta += "  ·  ";
    meta += tree_era_name(b.depth);
    meta += "  ·  ";
    meta += st_name;
    if (!inf.cost_text.empty()) {
        meta += "  ·  ";
        meta += inf.cost_text;
    }

    struct Line {
        const char *font;
        std::string text;
        Ink ink;
    };
    std::vector<Line> lines;
    const bool gold_title = inf.state == TreeNodeState::Researching || id == model_.goal;
    lines.push_back({kBoldFont, name, gold_title ? kSlateGold : kSlateInk});
    lines.push_back({kTagFont, meta, kSlateDim});
    if (id == model_.goal) lines.push_back({kTagFont, "CURRENT GOAL", kSlateGold});
    if (!inf.requires_text.empty()) lines.push_back({kUiFont, inf.requires_text, kSlateInk});
    if (!inf.allows_text.empty()) lines.push_back({kUiFont, inf.allows_text, kSlateDim});
    if (!inf.leads_text.empty()) lines.push_back({kUiFont, inf.leads_text, kSlateDim});

    const double max_w = cw_ >= 900 ? 420 : 340;
    const double pad = 12, gap = 4;
    const double wrap = max_w - 2 * pad;
    std::vector<Metrics> sizes;
    double tw = 0, th = pad * 2;
    for (const Line &ln : lines) {
        const Metrics m = measure(cr, ln.font, ln.text, static_cast<int>(wrap));
        sizes.push_back(m);
        tw = std::max(tw, static_cast<double>(m.w));
        th += m.h + gap;
    }
    tw = std::min(max_w, tw + 2 * pad);
    th -= gap;

    auto overlap_area = [](double ax, double ay, double aw, double ah, double bx, double by, double bw, double bh) {
        const double ix = std::max(0.0, std::min(ax + aw, bx + bw) - std::max(ax, bx));
        const double iy = std::max(0.0, std::min(ay + ah, by + bh) - std::max(ay, by));
        return ix * iy;
    };
    auto path_hit = [&](double x, double y) {
        double a = 0;
        for (const TreeNodeBox &ob : layout_.boxes) {
            if (ob.id == id || !path_.count(ob.id)) continue;
            const double ox = cx_ + inner_x_ - pan_x_ + ob.x;
            const double oy = cy_ + inner_top_ - pan_y_ + ob.y;
            a += overlap_area(x, y, tw, th, ox, oy, ob.w, ob.h);
        }
        return a;
    };
    auto hovered_hit = [&](double x, double y) {
        return overlap_area(x, y, tw, th, nx, ny, b.w, b.h);
    };
    auto in_canvas = [&](double x, double y) {
        return x >= cx_ + 6 - 0.5 && y >= cy_ + 6 - 0.5 && x + tw <= cx_ + cw_ - 6 + 0.5 &&
               y + th <= cy_ + ch_ - 6 + 0.5;
    };

    struct Cand {
        double x, y;
        int pref;   // lower is better: above, below, then sides
    };
    std::vector<Cand> cands;
    auto add = [&](double x, double y, int pref) {
        cands.push_back({x, y, pref});
        const double cx = std::clamp(x, cx_ + 6, cx_ + cw_ - 6 - tw);
        const double cy = std::clamp(y, cy_ + 6, cy_ + ch_ - 6 - th);
        if (std::fabs(cx - x) > 0.5 || std::fabs(cy - y) > 0.5) cands.push_back({cx, cy, pref + 2});
    };
    // Prefer above or below the node so left/right path neighbours stay visible.
    const double xs[] = {nx + b.w / 2.0 - tw / 2.0, nx, nx + b.w - tw, nx - 8, nx + b.w + 8 - tw};
    for (double xs_ : xs) {
        add(xs_, ny - th - 8, 0);
        add(xs_, ny + b.h + 8, 1);
    }
    add(nx + b.w + 8, ny + b.h / 2.0 - th / 2.0, 4);
    add(nx + b.w + 8, ny - 8, 4);
    add(nx - tw - 8, ny + b.h / 2.0 - th / 2.0, 5);
    add(nx - tw - 8, ny - 8, 5);

    double x = nx + b.w / 2.0 - tw / 2.0, y = ny - th - 8;
    double best = 1e99;
    for (const Cand &c : cands) {
        double score = c.pref * 50.0;
        score += path_hit(c.x, c.y) * 8.0;
        score += hovered_hit(c.x, c.y) * 20.0;
        if (!in_canvas(c.x, c.y)) score += 20000.0;
        if (score < best) {
            best = score;
            x = c.x;
            y = c.y;
        }
    }
    x = std::clamp(x, cx_ + 6, cx_ + cw_ - 6 - tw);
    y = std::clamp(y, cy_ + 6, cy_ + ch_ - 6 - th);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
    round_rect(cr, x + 3, y + 4, tw, th, 8);
    cairo_fill(cr);
    round_rect(cr, x + 0.5, y + 0.5, tw - 1, th - 1, 8);
    set(cr, hex(0x222A36));
    cairo_fill_preserve(cr);
    set(cr, path_.count(id) ? kSlateGold : kSlateEdge);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
    double cy = y + pad;
    for (size_t i = 0; i < lines.size(); ++i) {
        text(cr, lines[i].font, lines[i].text, x + pad, cy, lines[i].ink, 1.0, static_cast<int>(wrap));
        cy += sizes[i].h + gap;
    }
    (void)mouse_x;
    (void)mouse_y;
}

} // namespace ui
