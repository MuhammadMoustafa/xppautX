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

std::string label_style_problem(int style)
{
    return style < 0 || style >= LABEL_STYLE_COUNT ? xpp::format("{} is not a text style (0 to {})", style, LABEL_STYLE_COUNT - 1)
                                                    : std::string();
}

std::string label_problem(const LabelLook &look)
{
    for (std::string p : {label_text_problem(look.text), label_size_problem(look.size), label_style_problem(look.style)})
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

int select_marker_type(int *type)
{
    int ival = *type - MARKER;
    static const char *const list[] = {"Box", "Diamond", "Triangle", "Plus", "X", "Circle"};
    static constexpr std::string_view key = "bdtpxc";
    static_assert(std::size(list) == MARKER_SHAPE_COUNT && key.size() == MARKER_SHAPE_COUNT);
    XppMenu m = {"markers", "Markers", MARKER_SHAPE_COUNT, list, key.data(), no_hint, -1};
    const char ch = static_cast<char>(menu_choose(&m, ival));
    if (ch == ESC) return 0;
    for (int i = 0; i < MARKER_SHAPE_COUNT; i++) {
        if (ch == key[static_cast<std::size_t>(i)]) ival = i;
    }
    if (ival < MARKER_SHAPE_COUNT) *type = MARKER + ival;
    return 1;
}

namespace {

/* the colour the user types at the prompt (offered c), false when none was given or it is not one: the error is shown */
bool ask_color(std::string_view prompt, xpp::Color &c)
{
    std::string text = xpp::print_color(c);
    if (!new_string_of(prompt, text, xpp::XPP_FIELD_COLOR)) return false;
    const std::optional<xpp::Color> parsed = xpp::parse_color(text);
    if (!parsed) {
        command_error(prompt, xpp::format("\"{}\" is not a colour: write #rrggbb, #rrggbbaa, a CSS colour name or foreground", text));
        return false;
    }
    c = *parsed;
    return true;
}

void too_many_objects()
{
    command_error("Text,etc", xpp::format("a session has at most {} arrows, pointers and markers", ANNOTATION_LIST_MAX));
}

/* the fields of the marker dialogs: type, colour and size */
std::vector<xpp::FormField> marker_fields(const MarkInfo &m)
{
    std::vector<std::string> shapes;
    for (const auto name : marker_shape_names) shapes.emplace_back(name);
    return {xpp::FormField::choice("Type", shapes, static_cast<std::size_t>(m.type - MARKER)),
            xpp::FormField::color_field("Colour", m.color), xpp::FormField::real_number("Size", m.size)};
}

/* ask_form has checked every field before any marker setting is applied. */
void take_marker_fields(MarkInfo &m, std::span<const xpp::FormField> f)
{
    m.type = MARKER + f[0].whole;
    m.color = f[1].color;
    m.size = f[2].real;
}

} // namespace

int get_marker_info(xpp::Session &s)
{
    std::vector<xpp::FormField> fields = marker_fields(s.marker);
    if (!xpp::ask_form(s, "Add Marker", fields)) return 0;
    take_marker_fields(s.marker, fields);
    return 1;
}

int get_markers_info(xpp::Session &s)
{
    std::vector<xpp::FormField> fields = marker_fields(s.marker);
    fields.push_back(xpp::FormField{.name = "Number", .kind = xpp::XPP_FIELD_INTEGER, .value = xpp::format("{:d}", s.marker.number)});
    fields.push_back(xpp::FormField{.name = "Row1", .kind = xpp::XPP_FIELD_INTEGER, .value = xpp::format("{:d}", s.marker.start)});
    fields.push_back(xpp::FormField{.name = "Skip", .kind = xpp::XPP_FIELD_INTEGER, .value = xpp::format("{:d}", s.marker.skip)});
    if (!xpp::ask_form(s, "Add Markers", fields)) return 0;
    take_marker_fields(s.marker, fields);
    s.marker.number = fields[3].whole;
    s.marker.start = fields[4].whole;
    s.marker.skip = fields[5].whole;
    return 1;
}

void add_marker(xpp::Session &s)
{
    int i1, j1;
    float xs, ys;
    if (get_marker_info(s) == 0) return;
    MessageBox("Position");
    const int flag = GetMouseXY(s,&i1, &j1);
    KillMessageBox();
    FlushDisplay();
    if (flag == 0) return;
    scale_to_real(s,i1, j1, &xs, &ys);
    if (add_grob(s, s.plot_windows.draw_win, xs, ys, 0, 0, s.marker.size, s.marker.type, s.marker.color) < 0) too_many_objects();
    redraw_all(s);
}

/* markers at every skip-th row of the window's first curve */
static void add_markers_at(xpp::Session &s, int number, int start, int skip, double size, int type, const Color &color)
{
    float xs, ys, x, y, z;
    for (int i = 0; i < number; i++) {
        get_data_xyz(s,&x, &y, &z, s.plot_windows.current->xv[0], s.plot_windows.current->yv[0], s.plot_windows.current->zv[0], start + i * skip);
        if (s.plot_windows.current->ThreeDFlag == 0) {
            xs = x;
            ys = y;
        } else {
            threed_proj(s,x, y, z, &xs, &ys);
        }
        if (add_grob(s, s.plot_windows.draw_win, xs, ys, 0, 0, size, type, color) < 0) {
            too_many_objects();
            break;
        }
    }
    redraw_all(s);
}

void add_markers(xpp::Session &s)
{
    if (get_markers_info(s) == 0) return;
    add_markers_at(s,s.marker.number, s.marker.start, s.marker.skip, s.marker.size, s.marker.type, s.marker.color);
}

void add_pntarr(xpp::Session &s, int type)
{
    double size = ARROW_SIZE_DEFAULT;
    int i1, j1, i2, j2;
    xpp::Color color;
    float xe, ye, xs, ys;
    if (new_float(s,"Size: ", &size)) return;
    if (!ask_color("Colour: ", color)) return;
    MessageBox("Choose start/end");
    const int flag = rubber_band(s,&i1, &j1, &i2, &j2, 1);
    KillMessageBox();
    FlushDisplay();
    if (flag) {
        scale_to_real(s,i1, j1, &xs, &ys);
        scale_to_real(s,i2, j2, &xe, &ye);
        if (i1 == i2 && j1 == j2) return;
        if (add_grob(s, s.plot_windows.draw_win, xs, ys, xe, ye, size, type, color) < 0) too_many_objects();
        redraw_all(s);
    }
}

/* Text,etc/Edit: move (0), change (1) or delete (2) the label or graphic
   object nearest to a click; the same core functions the protocol's
   annotation commands call, by the annotation's id */
void edit_object_com(xpp::Session &s, int com)
{
    MessageBox("Choose Object");
    int i, j;
    const bool clicked = GetMouseXY(s, &i, &j);
    KillMessageBox();
    FlushDisplay();
    if (!clicked) return;
    float x, y;
    scale_to_real(s, i, j, &x, &y);
    /* the nearest of this window's labels and objects (an object by its start) */
    const LABEL *near_label = nullptr;
    const GROB *near_grob = nullptr;
    float dist = 1e20f;
    for (const LABEL &l : s.labels) {
        const float dd = (x - l.x) * (x - l.x) + (y - l.y) * (y - l.y);
        if (l.w == s.plot_windows.draw_win && dd < dist) {
            near_label = &l;
            dist = dd;
        }
    }
    for (const GROB &g : s.grobs) {
        const float dd = (x - g.xs) * (x - g.xs) + (y - g.ys) * (y - g.ys);
        if (g.w == s.plot_windows.draw_win && dd < dist) {
            near_label = nullptr;
            near_grob = &g;
            dist = dd;
        }
    }
    if (!near_label && !near_grob) return;
    const int id = near_label ? near_label->id : near_grob->id;
    static constexpr std::array<std::string_view, 3> verbs{"Move", "Change", "Delete"};
    const std::string what = near_label ? near_label->s : xpp::format("graphic at ({:f},{:f})", static_cast<double>(near_grob->xs), static_cast<double>(near_grob->ys));
    if (TwoChoice("Yes", "No", xpp::format("{} {} ?", verbs[static_cast<std::size_t>(com)], what), "yn") != 'y') return;
    switch (com) {
    case 0:
        MessageBox(near_label ? "Click on new position" : "Reposition");
        if (GetMouseXY(s, &i, &j)) {
            scale_to_real(s, i, j, &x, &y);
            move_annotation(s, id, x, y);
        }
        KillMessageBox();
        FlushDisplay();
        break;
    case 1:
        if (near_label) {
            LabelLook look{near_label->s, near_label->size, near_label->style, near_label->color};
            new_string("Text: ", look.text);
            new_int("Size 0-4 :", &look.size);
            look.size = std::clamp(look.size, 0, LABEL_SIZE_MAX);
            if (label_problem(look).empty()) edit_label(s, id, look);
        } else {
            GrobLook look{near_grob->type, near_grob->size, near_grob->color};
            if (look.type >= MARKER) select_marker_type(&look.type);
            new_float(s, "Size ", &look.size);
            ask_color("Colour: ", look.color);
            edit_grob(s, id, look);
        }
        break;
    case 2:
        delete_annotation(s, id);
        break;
    }
    xpp::clr_scrn(s);
    redraw_all(s);
}

void do_gr_objs_com(xpp::Session &s, int com)
{
    switch (com) {
    case 0:
        cput_text(s);
        break;
    case 1:
        add_pntarr(s,ARROW);
        break;
    case 2:
        add_pntarr(s,POINTER);
        break;
    case 3:
        add_marker(s);
        break;
    case 6:
        add_markers(s);
        break;
    case 5:
        destroy_labels_and_grobs(s,s.plot_windows.draw_win);
        xpp::clr_scrn(s);
        redraw_all(s);
        break;
    }
}

} // namespace xpp
