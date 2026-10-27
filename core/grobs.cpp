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
    for (int i = 0; i < MAXLAB; i++) {
        LABEL &l = s.labels[i];
        if (l.use == 0) {
            l = LABEL{w, x, y, stored_text(s, look.text), 1, 0, look.size, look.style, look.color};
            marks_data_label(s, l.w, i, shown_label_text(s, i));
            return i;
        }
    }
    return -1;
}

void change_label(xpp::Session &s, int id, LabelLook look, bool move, float x, float y)
{
    LABEL &l = s.labels[id];
    l.s = stored_text(s, look.text);
    l.size = look.size;
    l.style = look.style;
    l.color = look.color;
    if (move) {
        l.x = x;
        l.y = y;
    }
    marks_data_label(s, l.w, id, shown_label_text(s, id));
}

void delete_label(xpp::Session &s, int id)
{
    s.labels[id].w = 0;
    s.labels[id].use = 0;
}

std::string shown_label_text(xpp::Session &s, int id)
{
    return fill_in_text(s, s.labels[id].s);
}

void draw_marker(xpp::Session &s, double xd, double yd, double size, int type)
{
    static const int sym_dir[] = {
        /*          box              */
        0, -6, -6, 1, 12, 0, 1, 0, 12, 1, -12, 0,
        1, 0, -12, 3, 0, 0, 3, 0, 0, 3, 0, 0,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,

        /*          diamond             */
        0, 8, 0, 1, -8, -8, 1, 8, -8, 1, 8, 8,
        1, -8, 8, 3, 0, 0, 3, 0, 0, 3, 0, 0,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,
        /*          triangle         */
        0, -6, -6, 1, 12, 0, 1, -6, 12, 1, -6, -12,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,

        /*          plus            */
        0, -6, 0, 1, 12, 0, 0, -6, -6, 1, 0, 12,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,

        /*          cross            */
        0, -6, 6, 1, 12, -12, 0, -12, 0, 1, 12, 12,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,
        3, 0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0,

        /*          circle           */
        0, 6, 0, 1, -1, 3, 1, -2, 2, 1, -3, 1,
        1, -3, -1, 1, -2, -2, 1, -1, -3, 1, 1, -3,
        1, 2, -2, 1, 3, -1, 1, 3, 1, 1, 2, 2,
        1, 1, 3, 3, 0, 0, 3, 0, 0, 3, 0, 0,
    };
    float x1 = static_cast<float>(xd), y1 = static_cast<float>(yd), x2, y2;
    const float dx = static_cast<float>((s.plot_windows.current->xhi - s.plot_windows.current->xlo) * WDMARK * size);
    const float dy = static_cast<float>((s.plot_windows.current->yhi - s.plot_windows.current->ylo) * HTMARK * size);
    for (int ind = 0;; ind++) {
        const int offset = 48 * type + 3 * ind;
        const int pen = sym_dir[offset];
        if (pen == 3) break;
        x2 = dx * sym_dir[offset + 1] + x1;
        y2 = dy * sym_dir[offset + 2] + y1;
        if (pen == 1) line_abs(s,x1, y1, x2, y2);
        x1 = x2;
        y1 = y2;
    }
}

void draw_grob(xpp::Session &s, int i)
{
    const float xs = s.grobs[i].xs, ys = s.grobs[i].ys, xe = s.grobs[i].xe, ye = s.grobs[i].ye;
    set_pen(s,s.grobs[i].color,LineStyle{});
    if (s.grobs[i].type == POINTER) line_abs(s,xs, ys, xe, ye);
    if (s.grobs[i].type == ARROW || s.grobs[i].type == POINTER) arrow_head(s,xs, ys, xe, ye, s.grobs[i].size);
    if (s.grobs[i].type >= MARKER) draw_marker(s,xs, ys, s.grobs[i].size, s.grobs[i].type - 2);
}

void arrow_head(xpp::Session &s, double xsd, double ysd, double xed, double yed, double size)
{
    /* in single precision, as it always was */
    const float xs = static_cast<float>(xsd), ys = static_cast<float>(ysd);
    const float xe = static_cast<float>(xed), ye = static_cast<float>(yed);
    const float l = xe - xs, h = ye - ys;
    const float ar = static_cast<float>((s.plot_windows.current->xhi - s.plot_windows.current->xlo) / (s.plot_windows.current->yhi - s.plot_windows.current->ylo));
    const float x0 = static_cast<float>(xs + size * l), y0 = static_cast<float>(ys + size * h);
    const float xp = static_cast<float>(x0 + .5 * size * h * ar), yp = static_cast<float>(y0 - .5 * size * l / ar);
    const float xm = static_cast<float>(x0 - .5 * size * h * ar), ym = static_cast<float>(y0 + .5 * size * l / ar);
    line_abs(s,xs, ys, xp, yp);
    line_abs(s,xs, ys, xm, ym);
}

namespace {
/* frees the slots of `slots` (labels or graphic objects) that window w
   holds */
template <typename Slot, std::size_t N>
void release_slots_of(std::array<Slot, N> &slots, XppWinId w)
{
    for (Slot &x : slots) {
        if (x.use == 1 && x.w == w) {
            x.use = 0;
            x.w = 0;
        }
    }
}
} // namespace

void destroy_labels_and_grobs(xpp::Session &s, XppWinId w)
{
    release_slots_of(s.labels, w);
    release_slots_of(s.grobs, w);
}

void draw_label(xpp::Session &s, XppWinId w)
{
    for (int i = 0; i < MAXLAB; i++) {
        if (s.labels[i].use == 1 && s.labels[i].w == w) {
            /* \{expr} filled in once: an expression may set a parameter.
               The filled text has none left, so fancy_text_abs leaves it. */
            const std::string text = shown_label_text(s, i);
            marks_data_label(s,w, i, text);
            set_text_color(s,s.labels[i].color);
            fancy_text_abs(s,s.labels[i].x, s.labels[i].y, text.c_str(), s.labels[i].size, s.labels[i].font);
        }
    }
    for (int i = 0; i < MAXGROB; i++) {
        if (s.grobs[i].use == 1 && s.grobs[i].w == w) {
            marks_data_grob(s,w, i);
            draw_grob(s,i);
        }
    }
    set_text_color(s,foreground_color);
}

void add_grob(xpp::Session &s, double xs, double ys, double xe, double ye, double size, int type, const Color &color)
{
    for (int i = 0; i < MAXGROB; i++) {
        if (s.grobs[i].use == 0) {
            s.grobs[i].use = 1;
            s.grobs[i].xs = static_cast<float>(xs);
            s.grobs[i].xe = static_cast<float>(xe);
            s.grobs[i].ys = static_cast<float>(ys);
            s.grobs[i].ye = static_cast<float>(ye);
            s.grobs[i].w = s.plot_windows.draw_win;
            s.grobs[i].size = size;
            s.grobs[i].color = color;
            s.grobs[i].type = type;
            return;
        }
    }
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
    add_grob(s,xs, ys, 0.0f, 0.0f, s.marker.size, s.marker.type, s.marker.color);
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
        add_grob(s,xs, ys, 0.0f, 0.0f, size, type, color);
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
    double size = .1;
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
        add_grob(s,xs, ys, xe, ye, size, type, color);
        redraw_all(s);
    }
}

/* Text,etc/Edit: move (0), change (1) or delete (2) the label or graphic
   object nearest to a click */
void edit_object_com(xpp::Session &s, int com)
{
    char ans;
    int i, j, ilab = -1, flag, type;
    float x, y;
    float dist = 1e20f, dd;

    MessageBox("Choose Object");
    flag = GetMouseXY(s,&i, &j);
    KillMessageBox();
    FlushDisplay();
    if (!flag) return;
    scale_to_real(s,i, j, &x, &y);
    /* now search all labels to find the best */
    type = 0; /* label =  0, arrows, etc =1 */
    for (i = 0; i < MAXLAB; i++) {
        if (s.labels[i].use == 1 && s.labels[i].w == s.plot_windows.draw_win) {
            dd = (x - s.labels[i].x) * (x - s.labels[i].x) + (y - s.labels[i].y) * (y - s.labels[i].y);
            if (dd < dist) {
                ilab = i;
                dist = dd;
            }
        }
    }
    for (i = 0; i < MAXGROB; i++) {
        if (s.grobs[i].use == 1 && s.grobs[i].w == s.plot_windows.draw_win) {
            dd = (x - s.grobs[i].xs) * (x - s.grobs[i].xs) + (y - s.grobs[i].ys) * (y - s.grobs[i].ys);
            if (dd < dist) {
                ilab = i;
                dist = dd;
                type = 1;
            }
        }
    }
    if (ilab >= 0 && type == 0) {
        switch (com) {
        case 0:
            ans = static_cast<char>(TwoChoice("Yes", "No", xpp::format("Move {} ?", s.labels[ilab].s), "yn"));
            if (ans == 'y') {
                MessageBox("Click on new position");
                flag = GetMouseXY(s,&i, &j);
                KillMessageBox();
                FlushDisplay();
                if (flag) {
                    scale_to_real(s,i, j, &x, &y);
                    s.labels[ilab].x = x;
                    s.labels[ilab].y = y;
                    xpp::clr_scrn(s);
                    redraw_all(s);
                }
            }
            break;
        case 1:
            ans = static_cast<char>(TwoChoice("Yes", "No", xpp::format("Change {} ?", s.labels[ilab].s), "yn"));
            if (ans == 'y') {
                LabelLook look{s.labels[ilab].s, s.labels[ilab].size, s.labels[ilab].style, s.labels[ilab].color};
                new_string("Text: ", look.text);
                new_int("Size 0-4 :", &look.size);
                look.size = std::clamp(look.size, 0, LABEL_SIZE_MAX);
                if (label_problem(look).empty()) change_label(s, ilab, look, false, 0, 0);
                xpp::clr_scrn(s);
                redraw_all(s);
            }
            break;
        case 2:
            ans = static_cast<char>(TwoChoice("Yes", "No", xpp::format("Delete {} ?", s.labels[ilab].s), "yn"));
            if (ans == 'y') {
                delete_label(s, ilab);
                xpp::clr_scrn(s);
                redraw_all(s);
            }
            break;
        }
    }
    if (ilab >= 0 && type == 1) {
        switch (com) {
        case 0:
            ans = static_cast<char>(TwoChoice("Yes", "No", xpp::format("Move graphic at ({:f},{:f})", static_cast<double>(s.grobs[ilab].xs), static_cast<double>(s.grobs[ilab].ys)), "yn"));
            if (ans == 'y') {
                MessageBox("Reposition");
                flag = GetMouseXY(s,&i, &j);
                KillMessageBox();
                FlushDisplay();
                if (flag) {
                    scale_to_real(s,i, j, &x, &y);
                    s.grobs[ilab].xe = s.grobs[ilab].xe - s.grobs[ilab].xs + x;
                    s.grobs[ilab].ye = s.grobs[ilab].ye - s.grobs[ilab].ys + y;
                    s.grobs[ilab].xs = x;
                    s.grobs[ilab].ys = y;
                    xpp::clr_scrn(s);
                    redraw_all(s);
                }
            }
            break;
        case 1:
            ans = static_cast<char>(TwoChoice("Yes", "No", xpp::format("Change graphic at ({:f},{:f})", static_cast<double>(s.grobs[ilab].xs), static_cast<double>(s.grobs[ilab].ys)), "yn"));
            if (ans == 'y') {
                if (s.grobs[ilab].type >= MARKER) select_marker_type(&s.grobs[ilab].type);
                new_float(s,"Size ", &s.grobs[ilab].size);
                ask_color("Colour: ", s.grobs[ilab].color);
                xpp::clr_scrn(s);
                redraw_all(s);
            }
            break;
        case 2:
            ans = static_cast<char>(TwoChoice("Yes", "No", xpp::format("Delete graphic at ({:f},{:f})", static_cast<double>(s.grobs[ilab].xs), static_cast<double>(s.grobs[ilab].ys)), "yn"));
            if (ans == 'y') {
                s.grobs[ilab].w = 0;
                s.grobs[ilab].use = 0;
                xpp::clr_scrn(s);
                redraw_all(s);
            }
            break;
        }
    }
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

void do_windows_com(xpp::Session &s, int c)
{
    switch (c) {
    case 0:
        create_a_pop(s);
        break;
    case 1:
        if (yes_no_box()) kill_all_pops(s);
        break;
    case 3:
        ui.lower_plot_window();
        break;
    case 2:
        destroy_a_pop(s);
        break;
    case 5:
        set_restore(s,0);
        break;
    case 4:
        set_restore(s,1);
        break;
    case 6:
        s.plot_windows.simul = 1 - s.plot_windows.simul;
        break;
    }
    xpp::set_active_windows(s);
}

void set_restore(xpp::Session &s, int flag)
{
    s.plot_windows.current->Nullrestore=flag;
}

} // namespace xpp
