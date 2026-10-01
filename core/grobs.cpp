/* Text labels, arrows, pointers and markers drawn on the plot windows, and
   the Text,etc and Makewindow commands. Moved out of many_pops.c (X11);
   the window handling itself stays in the front end. What draw_label draws
   is also reported as data (marks_data.h). */
#include "xpp_ui.h"
#include "session.h"
#include "xpp_util.h"
#include "menus.h"
#include "graphics.h"
#include "browse.h"
#include "marks_data.h"
#include <string>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace xpp {

namespace {

const int POINTER = 0;
const int ARROW = 1;
const int MARKER = 2; /* markers start at 2; there are several of them */

const double WDMARK = .001;
const double HTMARK = .0016;

struct MarkInfo {
    int type, color;
    int number, start, skip;
    double size;
};

MarkInfo markinfo = {2, 0, 1, 0, 1, 1.0};

} // namespace


int add_label(xpp::Session &s, std::string_view text, int x, int y, int size, int font)
{
    float xp, yp;
    scale_to_real(s,x, y, &xp, &yp);
    for (int i = 0; i < MAXLAB; i++) {
        if (s.labels[i].use == 0) {
            s.labels[i].use = 1;
            s.labels[i].x = xp;
            s.labels[i].y = yp;
            s.labels[i].w = s.plot_windows.draw_win;
            s.labels[i].font = font;
            s.labels[i].size = size;
            s.labels[i].s = text;
            return i;
        }
    }
    return -1;
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
    set_linestyle(s,s.grobs[i].color);
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
    GrCol();
    for (int i = 0; i < MAXLAB; i++) {
        if (s.labels[i].use == 1 && s.labels[i].w == w) {
            /* \{expr} filled in once: an expression may set a parameter.
               The filled text has none left, so fancy_text_abs leaves it. */
            const std::string text = fill_in_text(s, s.labels[i].s);
            marks_data_label(s.plot_windows,w, i, text);
            fancy_text_abs(s,s.labels[i].x, s.labels[i].y, text.c_str(), s.labels[i].size, s.labels[i].font);
        }
    }
    for (int i = 0; i < MAXGROB; i++) {
        if (s.grobs[i].use == 1 && s.grobs[i].w == w) {
            marks_data_grob(s.plot_windows,w, i);
            draw_grob(s,i);
        }
    }
    BaseCol();
}

void add_grob(xpp::Session &s, double xs, double ys, double xe, double ye, double size, int type, int color)
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
    XppMenu m = {"markers", "Markers", 6, list, key.data(), no_hint, -1};
    const char ch = static_cast<char>(menu_choose(&m, ival));
    if (ch == 27) return 0;
    for (int i = 0; i < 6; i++) {
        if (ch == key[i]) ival = i;
    }
    if (ival < 6) *type = MARKER + ival;
    return 1;
}

int get_marker_info(void)
{
    static const char *n[] = {"*5Type", "*4Color", "Size"};
    std::array<std::string, 3> values;
    values[0] = xpp::format("{:d}", markinfo.type);
    values[1] = xpp::format("{:d}", markinfo.color);
    values[2] = xpp::format("{:g}", markinfo.size);
    static const int kinds[] = {XPP_FIELD_NAME_IN(5), XPP_FIELD_NAME_IN(4), XPP_FIELD_NUMBER};
    const int status = do_string_box_of(3, 1, "Add Marker", n, values, kinds);
    if (status != 0) {
        markinfo.type = std::atoi(values[0].c_str());
        markinfo.size = std::atof(values[2].c_str());
        markinfo.color = std::atoi(values[1].c_str());
        return 1;
    }
    return 0;
}

int get_markers_info(void)
{
    static const char *n[] = {"*5Type", "*4Color", "Size", "Number", "Row1", "Skip"};
    std::array<std::string, 6> values;
    values[0] = xpp::format("{:d}", markinfo.type);
    values[1] = xpp::format("{:d}", markinfo.color);
    values[2] = xpp::format("{:g}", markinfo.size);
    values[3] = xpp::format("{:d}", markinfo.number);
    values[4] = xpp::format("{:d}", markinfo.start);
    values[5] = xpp::format("{:d}", markinfo.skip);
    static const int kinds[] = {XPP_FIELD_NAME_IN(5), XPP_FIELD_NAME_IN(4), XPP_FIELD_NUMBER,
                                XPP_FIELD_INTEGER, XPP_FIELD_INTEGER, XPP_FIELD_INTEGER};
    const int status = do_string_box_of(6, 1, "Add Markers", n, values, kinds);
    if (status != 0) {
        markinfo.type = std::atoi(values[0].c_str());
        markinfo.size = std::atof(values[2].c_str());
        markinfo.color = std::atoi(values[1].c_str());
        markinfo.number = std::atoi(values[3].c_str());
        markinfo.start = std::atoi(values[4].c_str());
        markinfo.skip = std::atoi(values[5].c_str());
        return 1;
    }
    return 0;
}

void add_marker(xpp::Session &s)
{
    int i1, j1;
    float xs, ys;
    if (get_marker_info() == 0) return;
    MessageBox("Position");
    const int flag = GetMouseXY(s,&i1, &j1);
    KillMessageBox();
    FlushDisplay();
    if (flag == 0) return;
    scale_to_real(s,i1, j1, &xs, &ys);
    add_grob(s,xs, ys, 0.0f, 0.0f, markinfo.size, markinfo.type, markinfo.color);
    redraw_all(s);
}

/* markers at every skip-th row of the window's first curve */
static void add_markers_at(xpp::Session &s, int number, int start, int skip, double size, int type, int color)
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
    if (get_markers_info() == 0) return;
    add_markers_at(s,markinfo.number, markinfo.start, markinfo.skip, markinfo.size, markinfo.type, markinfo.color);
}

void add_pntarr(xpp::Session &s, int type)
{
    double size = .1;
    int i1, j1, i2, j2, color = 0;
    float xe, ye, xs, ys;
    if (new_float(s,"Size: ", &size)) return;
    if (new_int("Color: ", &color)) return;
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
                std::string text = s.labels[ilab].s;
                new_string("Text: ", text);
                s.labels[ilab].s = text;
                new_int("Size 0-4 :", &s.labels[ilab].size);
                if (s.labels[ilab].size > 4) s.labels[ilab].size = 4;
                if (s.labels[ilab].size < 0) s.labels[ilab].size = 0;
                xpp::clr_scrn(s);
                redraw_all(s);
            }
            break;
        case 2:
            ans = static_cast<char>(TwoChoice("Yes", "No", xpp::format("Delete {} ?", s.labels[ilab].s), "yn"));
            if (ans == 'y') {
                s.labels[ilab].w = 0;
                s.labels[ilab].use = 0;
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
                new_int("Color :", &s.grobs[ilab].color);
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
    for (int i = 0; i < MAXPOP; i++) {
        if (s.plot_windows.graph[i].w == s.plot_windows.draw_win) {
            s.plot_windows.graph[i].Nullrestore = flag;
            return;
        }
    }
}

} // namespace xpp
