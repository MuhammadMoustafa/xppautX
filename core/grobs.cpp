/* Text labels, arrows, pointers and markers drawn on the plot windows, and
   the Text,etc and Makewindow commands. Moved out of many_pops.c (X11);
   the window handling itself stays in the front end. What draw_label draws
   is also reported as data (marks_data.h). */
#include "grobs.h"
#include "xpp_globals.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "menus.h"
#include "graphics.h"
#include "browse.h"
#include "graf_par.h"
#include "marks_data.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

/* the prompts and titles of the C dialogs, which take char * and do not
   write to them */
char *str(const char *s) { return const_cast<char *>(s); }

} // namespace

LABEL lb[MAXLAB];
GROB grob[MAXGROB];

int add_label(char *s, int x, int y, int size, int font)
{
    float xp, yp;
    scale_to_real(x, y, &xp, &yp);
    for (int i = 0; i < MAXLAB; i++) {
        if (lb[i].use == 0) {
            lb[i].use = 1;
            lb[i].x = xp;
            lb[i].y = yp;
            lb[i].w = draw_win;
            lb[i].font = font;
            lb[i].size = size;
            std::snprintf(lb[i].s, sizeof lb[i].s, "%s", s);
            return i;
        }
    }
    return -1;
}

void draw_marker(double xd, double yd, double size, int type)
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
    const float dx = static_cast<float>((MyGraph->xhi - MyGraph->xlo) * WDMARK * size);
    const float dy = static_cast<float>((MyGraph->yhi - MyGraph->ylo) * HTMARK * size);
    for (int ind = 0;; ind++) {
        const int offset = 48 * type + 3 * ind;
        const int pen = sym_dir[offset];
        if (pen == 3) break;
        x2 = dx * sym_dir[offset + 1] + x1;
        y2 = dy * sym_dir[offset + 2] + y1;
        if (pen == 1) line_abs(x1, y1, x2, y2);
        x1 = x2;
        y1 = y2;
    }
}

void draw_grob(int i)
{
    const float xs = grob[i].xs, ys = grob[i].ys, xe = grob[i].xe, ye = grob[i].ye;
    set_linestyle(grob[i].color);
    if (grob[i].type == POINTER) line_abs(xs, ys, xe, ye);
    if (grob[i].type == ARROW || grob[i].type == POINTER) arrow_head(xs, ys, xe, ye, grob[i].size);
    if (grob[i].type >= MARKER) draw_marker(xs, ys, grob[i].size, grob[i].type - 2);
}

void arrow_head(double xsd, double ysd, double xed, double yed, double size)
{
    /* in single precision, as it always was */
    const float xs = static_cast<float>(xsd), ys = static_cast<float>(ysd);
    const float xe = static_cast<float>(xed), ye = static_cast<float>(yed);
    const float l = xe - xs, h = ye - ys;
    const float ar = static_cast<float>((MyGraph->xhi - MyGraph->xlo) / (MyGraph->yhi - MyGraph->ylo));
    const float x0 = static_cast<float>(xs + size * l), y0 = static_cast<float>(ys + size * h);
    const float xp = static_cast<float>(x0 + .5 * size * h * ar), yp = static_cast<float>(y0 - .5 * size * l / ar);
    const float xm = static_cast<float>(x0 - .5 * size * h * ar), ym = static_cast<float>(y0 + .5 * size * l / ar);
    line_abs(xs, ys, xp, yp);
    line_abs(xs, ys, xm, ym);
}

void destroy_grob(XppWinId w)
{
    for (int i = 0; i < MAXGROB; i++) {
        if (grob[i].use == 1 && grob[i].w == w) {
            grob[i].use = 0;
            grob[i].w = 0;
        }
    }
}

void destroy_label(XppWinId w)
{
    for (int i = 0; i < MAXLAB; i++) {
        if (lb[i].use == 1 && lb[i].w == w) {
            lb[i].use = 0;
            lb[i].w = 0;
        }
    }
}

void draw_label(XppWinId w)
{
    char text[256];
    GrCol();
    for (int i = 0; i < MAXLAB; i++) {
        if (lb[i].use == 1 && lb[i].w == w) {
            /* \{expr} filled in once: an expression may set a parameter.
               The filled text has none left, so fancy_text_abs leaves it. */
            fillintext(lb[i].s, text);
            marks_data_label(w, i, text);
            fancy_text_abs(lb[i].x, lb[i].y, text, lb[i].size, lb[i].font);
        }
    }
    for (int i = 0; i < MAXGROB; i++) {
        if (grob[i].use == 1 && grob[i].w == w) {
            marks_data_grob(w, i);
            draw_grob(i);
        }
    }
    BaseCol();
}

void add_grob(double xs, double ys, double xe, double ye, double size, int type, int color)
{
    for (int i = 0; i < MAXGROB; i++) {
        if (grob[i].use == 0) {
            grob[i].use = 1;
            grob[i].xs = static_cast<float>(xs);
            grob[i].xe = static_cast<float>(xe);
            grob[i].ys = static_cast<float>(ys);
            grob[i].ye = static_cast<float>(ye);
            grob[i].w = draw_win;
            grob[i].size = size;
            grob[i].color = color;
            grob[i].type = type;
            return;
        }
    }
}

int select_marker_type(int *type)
{
    int ival = *type - MARKER;
    static char box[] = "Box", diamond[] = "Diamond", triangle[] = "Triangle", plus[] = "Plus", x[] = "X",
                circle[] = "Circle";
    static char *list[] = {box, diamond, triangle, plus, x, circle};
    static char key[] = "bdtpxc";
    static char title[] = "Markers";
    XppMenu m = {"markers", title, 6, list, key, no_hint, -1, 9, 4};
    const char ch = static_cast<char>(menu_choose(&m, ival));
    if (ch == 27) return 0;
    for (int i = 0; i < 6; i++) {
        if (ch == key[i]) ival = i;
    }
    if (ival < 6) *type = MARKER + ival;
    return 1;
}

int man_xy(float *xe, float *ye)
{
    double x = 0, y = 0;
    if (new_float(str("x: "), &x)) return 0;
    if (new_float(str("y: "), &y)) return 0;
    *xe = static_cast<float>(x);
    *ye = static_cast<float>(y);
    return 1;
}

int get_marker_info(void)
{
    static char *n[] = {str("*5Type"), str("*4Color"), str("Size")};
    char values[3][MAX_LEN_SBOX];
    std::snprintf(values[0], sizeof values[0], "%d", markinfo.type);
    std::snprintf(values[1], sizeof values[1], "%d", markinfo.color);
    std::snprintf(values[2], sizeof values[2], "%g", markinfo.size);
    const int status = do_string_box(3, 3, 1, str("Add Marker"), n, values, 25);
    if (status != 0) {
        markinfo.type = std::atoi(values[0]);
        markinfo.size = std::atof(values[2]);
        markinfo.color = std::atoi(values[1]);
        return 1;
    }
    return 0;
}

int get_markers_info(void)
{
    static char *n[] = {str("*5Type"), str("*4Color"), str("Size"), str("Number"), str("Row1"), str("Skip")};
    char values[6][MAX_LEN_SBOX];
    std::snprintf(values[0], sizeof values[0], "%d", markinfo.type);
    std::snprintf(values[1], sizeof values[1], "%d", markinfo.color);
    std::snprintf(values[2], sizeof values[2], "%g", markinfo.size);
    std::snprintf(values[3], sizeof values[3], "%d", markinfo.number);
    std::snprintf(values[4], sizeof values[4], "%d", markinfo.start);
    std::snprintf(values[5], sizeof values[5], "%d", markinfo.skip);
    const int status = do_string_box(6, 6, 1, str("Add Markers"), n, values, 25);
    if (status != 0) {
        markinfo.type = std::atoi(values[0]);
        markinfo.size = std::atof(values[2]);
        markinfo.color = std::atoi(values[1]);
        markinfo.number = std::atoi(values[3]);
        markinfo.start = std::atoi(values[4]);
        markinfo.skip = std::atoi(values[5]);
        return 1;
    }
    return 0;
}

void add_marker(void)
{
    int i1, j1;
    float xs, ys;
    if (get_marker_info() == 0) return;
    MessageBox(str("Position"));
    const int flag = GetMouseXY(&i1, &j1);
    KillMessageBox();
    FlushDisplay();
    if (flag == 0) return;
    scale_to_real(i1, j1, &xs, &ys);
    add_grob(xs, ys, 0.0f, 0.0f, markinfo.size, markinfo.type, markinfo.color);
    redraw_all();
}

void add_marker_old(void)
{
    double size = 1;
    int i1, j1, color = 0;
    float xs, ys;
    int type = MARKER;
    if (select_marker_type(&type) == 0) return;
    if (new_float(str("Size: "), &size)) return;
    if (new_int(str("Color: "), &color)) return;
    MessageBox(str("Position"));
    const int flag = GetMouseXY(&i1, &j1);
    KillMessageBox();
    FlushDisplay();
    if (flag == 0) return;
    if (flag == -3) {
        if (man_xy(&xs, &ys)) add_grob(xs, ys, 0.0f, 0.0f, size, type, color);
        redraw_all();
        return;
    }
    scale_to_real(i1, j1, &xs, &ys);
    add_grob(xs, ys, 0.0f, 0.0f, size, type, color);
    redraw_all();
}

/* markers at every skip-th row of the window's first curve */
static void add_markers_at(int number, int start, int skip, double size, int type, int color)
{
    float xs, ys, x, y, z;
    for (int i = 0; i < number; i++) {
        get_data_xyz(&x, &y, &z, MyGraph->xv[0], MyGraph->yv[0], MyGraph->zv[0], start + i * skip);
        if (MyGraph->ThreeDFlag == 0) {
            xs = x;
            ys = y;
        } else {
            threed_proj(x, y, z, &xs, &ys);
        }
        add_grob(xs, ys, 0.0f, 0.0f, size, type, color);
    }
    redraw_all();
}

void add_markers(void)
{
    if (get_markers_info() == 0) return;
    add_markers_at(markinfo.number, markinfo.start, markinfo.skip, markinfo.size, markinfo.type, markinfo.color);
}

void add_markers_old(void)
{
    double size = 1;
    int color = 0;
    int nm = 1, nskip = 1, nstart = 0;
    int type = MARKER;
    if (select_marker_type(&type) == 0) return;
    if (new_float(str("Size: "), &size)) return;
    if (new_int(str("Color: "), &color)) return;
    if (new_int(str("Number of markers: "), &nm)) return;
    if (new_int(str("Starting at: "), &nstart)) return;
    if (new_int(str("Skip between: "), &nskip)) return;
    add_markers_at(nm, nstart, nskip, size, type, color);
}

void add_pntarr(int type)
{
    double size = .1;
    int i1, j1, i2, j2, color = 0;
    float xe, ye, xs, ys;
    if (new_float(str("Size: "), &size)) return;
    if (new_int(str("Color: "), &color)) return;
    MessageBox(str("Choose start/end"));
    const int flag = rubber_band(&i1, &j1, &i2, &j2, 1);
    KillMessageBox();
    FlushDisplay();
    if (flag) {
        scale_to_real(i1, j1, &xs, &ys);
        scale_to_real(i2, j2, &xe, &ye);
        if (i1 == i2 && j1 == j2) return;
        add_grob(xs, ys, xe, ye, size, type, color);
        redraw_all();
    }
}

/* Text,etc/Edit: move (0), change (1) or delete (2) the label or graphic
   object nearest to a click */
void edit_object_com(int com)
{
    char ans, s[80];
    int i, j, ilab = -1, flag, type;
    float x, y;
    float dist = 1e20f, dd;

    MessageBox(str("Choose Object"));
    flag = GetMouseXY(&i, &j);
    KillMessageBox();
    FlushDisplay();
    if (!flag) return;
    scale_to_real(i, j, &x, &y);
    /* now search all labels to find the best */
    type = 0; /* label =  0, arrows, etc =1 */
    for (i = 0; i < MAXLAB; i++) {
        if (lb[i].use == 1 && lb[i].w == draw_win) {
            dd = (x - lb[i].x) * (x - lb[i].x) + (y - lb[i].y) * (y - lb[i].y);
            if (dd < dist) {
                ilab = i;
                dist = dd;
            }
        }
    }
    for (i = 0; i < MAXGROB; i++) {
        if (grob[i].use == 1 && grob[i].w == draw_win) {
            dd = (x - grob[i].xs) * (x - grob[i].xs) + (y - grob[i].ys) * (y - grob[i].ys);
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
            std::snprintf(s, sizeof s, "Move %s ?", lb[ilab].s);
            ans = static_cast<char>(TwoChoice(str("Yes"), str("No"), s, str("yn")));
            if (ans == 'y') {
                MessageBox(str("Click on new position"));
                flag = GetMouseXY(&i, &j);
                KillMessageBox();
                FlushDisplay();
                if (flag) {
                    scale_to_real(i, j, &x, &y);
                    lb[ilab].x = x;
                    lb[ilab].y = y;
                    clr_scrn();
                    redraw_all();
                }
            }
            break;
        case 1:
            std::snprintf(s, sizeof s, "Change %s ?", lb[ilab].s);
            ans = static_cast<char>(TwoChoice(str("Yes"), str("No"), s, str("yn")));
            if (ans == 'y') {
                new_string(str("Text: "), lb[ilab].s);
                new_int(str("Size 0-4 :"), &lb[ilab].size);
                if (lb[ilab].size > 4) lb[ilab].size = 4;
                if (lb[ilab].size < 0) lb[ilab].size = 0;
                clr_scrn();
                redraw_all();
            }
            break;
        case 2:
            std::snprintf(s, sizeof s, "Delete %s ?", lb[ilab].s);
            ans = static_cast<char>(TwoChoice(str("Yes"), str("No"), s, str("yn")));
            if (ans == 'y') {
                lb[ilab].w = 0;
                lb[ilab].use = 0;
                clr_scrn();
                redraw_all();
            }
            break;
        }
    }
    if (ilab >= 0 && type == 1) {
        switch (com) {
        case 0:
            std::snprintf(s, sizeof s, "Move graphic at (%f,%f)", grob[ilab].xs, grob[ilab].ys);
            ans = static_cast<char>(TwoChoice(str("Yes"), str("No"), s, str("yn")));
            if (ans == 'y') {
                MessageBox(str("Reposition"));
                flag = GetMouseXY(&i, &j);
                KillMessageBox();
                FlushDisplay();
                if (flag) {
                    scale_to_real(i, j, &x, &y);
                    grob[ilab].xe = grob[ilab].xe - grob[ilab].xs + x;
                    grob[ilab].ye = grob[ilab].ye - grob[ilab].ys + y;
                    grob[ilab].xs = x;
                    grob[ilab].ys = y;
                    clr_scrn();
                    redraw_all();
                }
            }
            break;
        case 1:
            std::snprintf(s, sizeof s, "Change graphic at (%f,%f)", grob[ilab].xs, grob[ilab].ys);
            ans = static_cast<char>(TwoChoice(str("Yes"), str("No"), s, str("yn")));
            if (ans == 'y') {
                if (grob[ilab].type >= MARKER) select_marker_type(&grob[ilab].type);
                new_float(str("Size "), &grob[ilab].size);
                new_int(str("Color :"), &grob[ilab].color);
                clr_scrn();
                redraw_all();
            }
            break;
        case 2:
            std::snprintf(s, sizeof s, "Delete graphic at (%f,%f)", grob[ilab].xs, grob[ilab].ys);
            ans = static_cast<char>(TwoChoice(str("Yes"), str("No"), s, str("yn")));
            if (ans == 'y') {
                grob[ilab].w = 0;
                grob[ilab].use = 0;
                clr_scrn();
                redraw_all();
            }
            break;
        }
    }
}

void do_gr_objs_com(int com)
{
    switch (com) {
    case 0:
        cput_text();
        break;
    case 1:
        add_pntarr(ARROW);
        break;
    case 2:
        add_pntarr(POINTER);
        break;
    case 3:
        add_marker();
        break;
    case 6:
        add_markers();
        break;
    case 5:
        destroy_label(draw_win);
        destroy_grob(draw_win);
        clr_scrn();
        redraw_all();
        break;
    }
}

void do_windows_com(int c)
{
    switch (c) {
    case 0:
        create_a_pop();
        break;
    case 1:
        if (yes_no_box()) kill_all_pops();
        break;
    case 3:
        xpp_ui.lower_plot_window();
        break;
    case 2:
        destroy_a_pop();
        break;
    case 5:
        set_restore(0);
        break;
    case 4:
        set_restore(1);
        break;
    case 6:
        SimulPlotFlag = 1 - SimulPlotFlag;
        break;
    }
    set_active_windows();
}

void set_restore(int flag)
{
    for (int i = 0; i < MAXPOP; i++) {
        if (graph[i].w == draw_win) {
            graph[i].Restore = flag;
            graph[i].Nullrestore = flag;
            return;
        }
    }
}

int is_col_plotted(int nc)
{
    for (int i = 0; i < MAXPOP; i++) {
        if (graph[i].Use == 1) {
            const int nv = graph[i].nvars;
            for (int j = 0; j < nv; j++) {
                if (graph[i].xv[j] == nc || graph[i].yv[j] == nc || graph[i].zv[j] == nc) return 1;
            }
        }
    }
    return 0;
}

void change_plot_vars(int k)
{
    for (int i = 0; i < MAXPOP; i++) {
        if (graph[i].Use) {
            const int np = graph[i].nvars;
            for (int ip = 0; ip < np; ip++) {
                if (graph[i].xv[ip] > k) graph[i].xv[ip] = graph[i].xv[ip] - 1;
                if (graph[i].yv[ip] > k) graph[i].yv[ip] = graph[i].yv[ip] - 1;
                if (graph[i].zv[ip] > k) graph[i].zv[ip] = graph[i].zv[ip] - 1;
            }
        }
    }
}

int check_active_plot(int k)
{
    for (int i = 0; i < MAXPOP; i++) {
        if (graph[i].Use) {
            const int np = graph[i].nvars;
            for (int ip = 0; ip < np; ip++) {
                if (graph[i].xv[ip] == k || graph[i].yv[ip] == k || graph[i].zv[ip] == k) return 1;
            }
        }
    }
    return 0;
}

int graph_used(int i) { return graph[i].Use; }
