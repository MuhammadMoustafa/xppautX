#include "mykeydef.h"
/* Text labels, arrows, pointers and markers drawn on the plot windows, and
   the Text,etc and Makewindow commands. The window
   handling itself is the front end's. What draw_label draws
   is also reported as data (marks_data.h). */
#include "xpp_ui.h"
#include "session.h"
#include "xpp_util.h"
#include "many_pops.h"
#include "menus.h"
#include "graphics.h"
#include "browse.h"
#include "marks_data.h"
#include "colormap.h"
#include <algorithm>
#include <string>
#include <array>
#include <span>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace xpp {

namespace {

const double WDMARK = .001;
const double HTMARK = .0016;


} // namespace


std::string label_text_problem(std::string_view text)
{
    if (text.empty()) return "a text is not empty";
    if (text.size() > LABEL_TEXT_MAX) return xpp::format("a text is at most {} characters, not {}", LABEL_TEXT_MAX, text.size());
    return {};
}

std::string label_size_problem(int size)
{
    return size < 0 || size > LABEL_SIZE_MAX ? xpp::format("{} is not a text size (0 to {})", size, LABEL_SIZE_MAX) : std::string();
}

std::string label_problem(const LabelLook &look)
{
    for (std::string p : {label_text_problem(look.text), label_size_problem(look.size)})
        if (!p.empty()) return p;
    return {};
}

namespace {
/* the text as kept: one starting with % is its rest with the \{expr}s filled in, once */
std::string stored_text(Session &s, const std::string &text)
{
    return text.starts_with('%') ? fill_in_text(s, std::string_view(text).substr(1)) : text;
}
} // namespace

int add_label(xpp::Session &s, LabelLook look, XppWinId w, float x, float y)
{
    if (s.labels.size() >= ANNOTATION_LIST_MAX || s.next_annotation_id > ANNOTATION_ID_MAX) return -1;
    const int id = s.next_annotation_id++;
    LABEL &l = s.labels.emplace_back(LABEL{w, x, y, stored_text(s, look.text), id, 0, look.size, look.style, look.color});
    marks_data_label(s, w, id, shown_label_text(s, l));
    return id;
}

int add_grob(xpp::Session &s, XppWinId w, double xs, double ys, double xe, double ye, double size, int type, const Color &color)
{
    if (s.grobs.size() >= ANNOTATION_LIST_MAX || s.next_annotation_id > ANNOTATION_ID_MAX) return -1;
    const int id = s.next_annotation_id++;
    s.grobs.push_back(GROB{static_cast<float>(xs), static_cast<float>(ys), static_cast<float>(xe), static_cast<float>(ye), size, id, w, type, color});
    marks_data_grob(s, w, id);
    return id;
}

bool edit_label(xpp::Session &s, int id, LabelLook look)
{
    LABEL *l = find_annotation(s.labels, id);
    if (!l) return false;
    l->s = stored_text(s, look.text);
    l->size = look.size;
    l->style = look.style;
    l->color = look.color;
    marks_data_label(s, l->w, id, shown_label_text(s, *l));
    return true;
}

bool edit_grob(xpp::Session &s, int id, const GrobLook &look)
{
    GROB *g = find_annotation(s.grobs, id);
    if (!g) return false;
    g->type = look.type;
    g->size = look.size;
    g->color = look.color;
    return true;
}

bool move_annotation(xpp::Session &s, int id, double x, double y)
{
    if (LABEL *l = find_annotation(s.labels, id)) {
        l->x = static_cast<float>(x);
        l->y = static_cast<float>(y);
        return true;
    }
    GROB *g = find_annotation(s.grobs, id);
    if (!g) return false;
    g->xe = g->xe - g->xs + static_cast<float>(x);
    g->ye = g->ye - g->ys + static_cast<float>(y);
    g->xs = static_cast<float>(x);
    g->ys = static_cast<float>(y);
    return true;
}

bool delete_annotation(xpp::Session &s, int id)
{
    return std::erase_if(s.labels, [id](const LABEL &l) { return l.id == id; }) + std::erase_if(s.grobs, [id](const GROB &g) { return g.id == id; }) > 0;
}

std::string shown_label_text(xpp::Session &s, const LABEL &l)
{
    return fill_in_text(s, l.s);
}

namespace {
using Point = std::array<double, 2>;
using Polyline = std::span<const Point>;
constexpr std::array<Point, 5> box_line{{{-6, -6}, {6, -6}, {6, 6}, {-6, 6}, {-6, -6}}};
constexpr std::array<Point, 5> diamond_line{{{8, 0}, {0, -8}, {-8, 0}, {0, 8}, {8, 0}}};
constexpr std::array<Point, 4> triangle_line{{{-6, -6}, {6, -6}, {0, 6}, {-6, -6}}};
constexpr std::array<Point, 2> plus_across{{{-6, 0}, {6, 0}}}, plus_up{{{0, -6}, {0, 6}}};
constexpr std::array<Point, 2> cross_down{{{-6, 6}, {6, -6}}}, cross_up{{{-6, -6}, {6, 6}}};
constexpr std::array<Point, 13> circle_line{
    {{6, 0}, {5, 3}, {3, 5}, {0, 6}, {-3, 5}, {-5, 3}, {-6, 0}, {-5, -3}, {-3, -5}, {0, -6}, {3, -5}, {5, -3}, {6, 0}}};
constexpr std::array<Polyline, 1> box_glyph{box_line}, diamond_glyph{diamond_line}, triangle_glyph{triangle_line}, circle_glyph{circle_line};
constexpr std::array<Polyline, 2> plus_glyph{plus_across, plus_up}, cross_glyph{cross_down, cross_up};
} // namespace

std::span<const std::span<const std::array<double, 2>>> marker_strokes(int shape)
{
    switch (shape) {
    case 0: return box_glyph;
    case 1: return diamond_glyph;
    case 2: return triangle_glyph;
    case 3: return plus_glyph;
    case 4: return cross_glyph;
    default: return circle_glyph;
    }
}

std::string annotation_glyphs_json()
{
    std::string o = xpp::format("{{\"half_units\":{:g},\"arrow_head_spread\":{:g},\"markers\":{{", MARKER_HALF_UNITS, ARROW_HEAD_SPREAD);
    for (int shape = 0; shape < MARKER_SHAPE_COUNT; shape++) {
        o += xpp::format("{}\"{}\":[", shape ? "," : "", marker_shape_names[static_cast<std::size_t>(shape)]);
        const auto &strokes = marker_strokes(shape);
        for (std::size_t i = 0; i < strokes.size(); i++) {
            o += i ? ",[" : "[";
            for (std::size_t k = 0; k < strokes[i].size(); k++) o += xpp::format("{}[{:g},{:g}]", k ? "," : "", strokes[i][k][0], strokes[i][k][1]);
            o += ']';
        }
        o += ']';
    }
    return o + "}}";
}

void draw_marker(xpp::Session &s, double xd, double yd, double size, int type)
{
    const float x0 = static_cast<float>(xd), y0 = static_cast<float>(yd);
    const float dx = static_cast<float>((s.plot_windows.current->xhi - s.plot_windows.current->xlo) * WDMARK * size);
    const float dy = static_cast<float>((s.plot_windows.current->yhi - s.plot_windows.current->ylo) * HTMARK * size);
    for (const auto &stroke : marker_strokes(type)) {
        float x1 = 0, y1 = 0;
        for (std::size_t k = 0; k < stroke.size(); k++) {
            const float x2 = dx * static_cast<float>(stroke[k][0]) + x0, y2 = dy * static_cast<float>(stroke[k][1]) + y0;
            if (k) line_abs(s, x1, y1, x2, y2);
            x1 = x2;
            y1 = y2;
        }
    }
}

void draw_grob(xpp::Session &s, const GROB &g)
{
    set_pen(s, g.color, LineStyle{});
    if (g.type == POINTER) line_abs(s, g.xs, g.ys, g.xe, g.ye);
    if (g.type == ARROW || g.type == POINTER) arrow_head(s, g.xs, g.ys, g.xe, g.ye, g.size);
    if (g.type >= MARKER) draw_marker(s, g.xs, g.ys, g.size, g.type - MARKER);
}

void arrow_head(xpp::Session &s, double xsd, double ysd, double xed, double yed, double size)
{
    /* in single precision, as it always was */
    const float xs = static_cast<float>(xsd), ys = static_cast<float>(ysd);
    const float xe = static_cast<float>(xed), ye = static_cast<float>(yed);
    const float l = xe - xs, h = ye - ys;
    const float ar = static_cast<float>((s.plot_windows.current->xhi - s.plot_windows.current->xlo) / (s.plot_windows.current->yhi - s.plot_windows.current->ylo));
    const float x0 = static_cast<float>(xs + size * l), y0 = static_cast<float>(ys + size * h);
    const float xp = static_cast<float>(x0 + ARROW_HEAD_SPREAD * size * h * ar), yp = static_cast<float>(y0 - ARROW_HEAD_SPREAD * size * l / ar);
    const float xm = static_cast<float>(x0 - ARROW_HEAD_SPREAD * size * h * ar), ym = static_cast<float>(y0 + ARROW_HEAD_SPREAD * size * l / ar);
    line_abs(s,xs, ys, xp, yp);
    line_abs(s,xs, ys, xm, ym);
}

void destroy_labels_and_grobs(xpp::Session &s, XppWinId w)
{
    std::erase_if(s.labels, [w](const LABEL &l) { return l.w == w; });
    std::erase_if(s.grobs, [w](const GROB &g) { return g.w == w; });
}

void draw_label(xpp::Session &s, XppWinId w)
{
    for (const LABEL &l : s.labels) {
        if (l.w != w) continue;
        /* \{expr} filled in once: an expression may set a parameter.
           The filled text has none left, so fancy_text_abs leaves it. */
        const std::string text = shown_label_text(s, l);
        marks_data_label(s, w, l.id, text);
        set_text_color(s, l.color);
        fancy_text_abs(s, l.x, l.y, text.c_str(), l.size, l.font);
    }
    for (const GROB &g : s.grobs) {
        if (g.w != w) continue;
        marks_data_grob(s, w, g.id);
        draw_grob(s, g);
    }
    set_text_color(s, foreground_color);
}

namespace {

} // namespace

/* markers at every skip-th row of the window's first curve, from row `start`: the settings are the session's
   from now on, as the Add Markers form leaves them; all or nothing, the rows checked against the stored data */
Result<int> add_data_markers(xpp::Session &s, const MarkInfo &m)
{
    if (m.number < 1 || m.skip < 1 || m.start < 0)
        return fail("Add Markers", "Number and Skip are at least 1 and Row1 at least 0", command_place());
    const DATA_VIEW &data = s.data.view;
    const long long last_row = data.row0 + m.start + static_cast<long long>(m.number - 1) * m.skip;
    if (last_row >= data.maxrow)
        return fail("Add Markers", xpp::format("The markers reach row {}, past the data: it has {} rows", last_row - data.row0, data.maxrow - data.row0), command_place());
    if (s.grobs.size() + static_cast<std::size_t>(m.number) > ANNOTATION_LIST_MAX)
        return fail("Add Markers", xpp::format("a session has at most {} arrows, pointers and markers", ANNOTATION_LIST_MAX), command_place());
    s.marker = m;
    const GRAPH &g = *s.plot_windows.current;
    for (int i = 0; i < m.number; i++) {
        float xs, ys, x, y, z;
        get_data_xyz(s, &x, &y, &z, g.xv[0], g.yv[0], g.zv[0], m.start + i * m.skip);
        if (g.ThreeDFlag == 0) {
            xs = x;
            ys = y;
        } else {
            threed_proj(s, x, y, z, &xs, &ys);
        }
        add_grob(s, s.plot_windows.draw_win, xs, ys, 0, 0, m.size, m.type, m.color);
    }
    redraw_all(s);
    return m.number;
}

} // namespace xpp
