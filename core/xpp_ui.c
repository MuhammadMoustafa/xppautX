/* Headless defaults for the UI seam, plus the dispatchers that keep the
   historical function names working. See xpp_ui.h. */
#include "xpp_ui.h"
#include "xpp_globals.h"
#include "xpp_job.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "many_pops.h"

int do_calc(char *temp, double *z); /* xpp_util.c */

/* ---- headless defaults ------------------------------------------------ */

static void hl_err_msg(char *msg) { xpp_log(XPP_LOG_ERROR, "%s\n", msg); }
static void hl_void(void) {}
static void hl_str(char *s) { (void)s; }
static void hl_int(int v) { (void)v; }
static void hl_bottom_msg(int line, char *msg) { (void)line; (void)msg; }
static int hl_new_string(char *name, char *value) { (void)name; (void)value; return 0; }
static int hl_no(void) { return 0; }
static int hl_two_choice(char *c1, char *c2, char *q, char *key, char *title)
{
    (void)c1; (void)c2; (void)q; (void)key; (void)title;
    return 0;
}
static int hl_string_box(int n, int row, int col, char *title, char **names,
                         char values[][MAX_LEN_SBOX], int maxchar)
{
    (void)n; (void)row; (void)col; (void)title; (void)names; (void)values; (void)maxchar;
    return 0;
}
static int hl_file_selector(char *title, char *file, char *wild)
{
    (void)title; (void)file; (void)wild;
    return 0;
}
static int hl_menu_choose(const struct XppMenu *m, int def)
{
    (void)m; (void)def;
    return 0;
}
static int hl_get_mouse_xy(int *x, int *y) { (void)x; (void)y; return 0; }
static int hl_check_abort(void) { return 64; }
static void hl_progress(int nit, int icount, int cwidth) { (void)nit; (void)icount; (void)cwidth; }
static void hl_activate_graph(int i, int flag) { (void)i; (void)flag; }
static void hl_get_draw_size(unsigned int *w, unsigned int *h)
{
    /* whatever the graph last had, else a sensible canvas */
    *w = plot_windows.current && plot_windows.current->x11Wid > 0 ? (unsigned int)plot_windows.current->x11Wid : 640;
    *h = plot_windows.current && plot_windows.current->x11Hgt > 0 ? (unsigned int)plot_windows.current->x11Hgt : 480;
}
static void hl_put_text(int x, int y, char *s) { (void)x; (void)y; (void)s; }
static int hl_film_clip(void) { return 1; }
static void hl_draw_point(int x, int y) { (void)x; (void)y; }
static void hl_draw_line(int x1, int y1, int x2, int y2) { (void)x1; (void)y1; (void)x2; (void)y2; }
static void hl_draw_frect(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static void hl_draw_special_text(int x, int y, char *s, int size) { (void)x; (void)y; (void)s; (void)size; }
static void hl_auto_make_window(char *w, char *i) { (void)w; (void)i; }
static void hl_auto_circle(int x, int y, int r) { (void)x; (void)y; (void)r; }
static void hl_auto_diagram(const XppDiagPoint *p) { (void)p; }
static void hl_auto_draw_info(char *s, int x, int y) { (void)s; (void)x; (void)y; }
static int hl_auto_grab_event(int *x, int *y) { (void)x; (void)y; return 27; }
static int hl_auto_check_abort(int *iflag) { *iflag = 0; return 0; }
static int hl_auto_rubber(int *i1, int *j1, int *i2, int *j2, int flag)
{
    (void)i1; (void)j1; (void)i2; (void)j2; (void)flag;
    return 0;
}
static int hl_auto_choose_key(char *title, char **list, char *key, int n, int max,
                              int def, int x, int y, char **hints, char *httxt)
{
    (void)title; (void)list; (void)max; (void)x; (void)y; (void)hints; (void)httxt;
    if (def >= 0 && def < n) return key[def];
    return 0;
}
static void hl_show_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                           double *ev, int n)
{
    int i;
    plintf("Equilibrium: c+=%d c-=%d r+=%d r-=%d im=%d\n", cp, cm, rp, rm, im);
    for (i = 0; i < n; i++) {
        if (ev) plintf("  y[%d]=%.8g  eig=%.8g%+.8gi\n", i, y[i], ev[2 * i], ev[2 * i + 1]);
        else plintf("  y[%d]=%.8g\n", i, y[i]);
    }
}
static int hl_dialog(char *title, char *name, char *value, char *ok, char *cancel, int max)
{
    (void)title; (void)name; (void)value; (void)ok; (void)cancel; (void)max;
    return 0;
}
static void hl_ani_font(int size, int font, int color) { (void)size; (void)font; (void)color; }
static void hl_ani_box(int x, int y, int w, int h, int fill) { (void)x; (void)y; (void)w; (void)h; (void)fill; }
static int hl_edit_box(int n, char *title, char **names, char **values)
{
    (void)n; (void)title; (void)names; (void)values;
    return 0;
}
static void hl_param_box_set(int i, char *s) { (void)i; (void)s; }
static void hl_respond_box(char *button, char *message) { (void)button; plintf("%s\n", message); }
static int hl_checklist(char *title, char **names, int *flags, int n)
{
    (void)title; (void)names; (void)flags; (void)n;
    return 0;
}
static void hl_movie_save(char *basename, int fmat) { (void)basename; (void)fmat; }
static void hl_exit_program(void) { exit(1); }

XppTextMetrics text_metrics;

XppUi xpp_ui = {
    .err_msg = hl_err_msg,
    .ping = hl_void,
    .bottom_msg = hl_bottom_msg,
    .message_box = hl_str,
    .kill_message_box = hl_void,
    .title_text = hl_str,
    .canvas_xy = hl_str,
    .new_string = hl_new_string,
    .yes_no_box = hl_no,
    .two_choice = hl_two_choice,
    .respond_box = hl_respond_box,
    .checklist = hl_checklist,
    .string_box = hl_string_box,
    .file_selector = hl_file_selector,
    .ani_clear = hl_void,
    .ani_show = hl_void,
    .ani_color = hl_int,
    .ani_thick = hl_int,
    .ani_font = hl_ani_font,
    .ani_line = hl_draw_line,
    .ani_rect = hl_ani_box,
    .ani_arc = hl_ani_box,
    .ani_text = hl_put_text,
    .ani_slider = hl_void,
    .dialog = hl_dialog,
    .edit_box = hl_edit_box,
    .get_mouse_xy = hl_get_mouse_xy,
    .menu_flash = hl_int,
    .show_menu = hl_int,
    .menu_choose = hl_menu_choose,
    .check_abort = hl_check_abort,
    .progress_begin = hl_no,
    .progress = hl_progress,
    .flush = hl_void,
    .redraw_params = hl_void,
    .param_box_set = hl_param_box_set,
    .param_box_redraw = hl_int,
    .ic_box_set = hl_param_box_set,
    .ic_box_redraw = hl_int,
    .redraw_ics = hl_void,
    .redraw_all = hl_void,
    .redraw_bcs = hl_void,
    .redraw_delays = hl_void,
    .redraw_graph = hl_void,
    .redraw_screens = hl_void,
    .clear_screens = hl_void,
    .clear_draw_window = hl_void,
    .reset_graphics = hl_void,
    .browser_redraw = hl_int,
    .data_changed = hl_int,
    .rows_stored = hl_int,
    .activate_graph = hl_activate_graph,
    .create_plot_window = hl_void,
    .destroy_plot_window = hl_void,
    .kill_plot_windows = hl_void,
    .lower_plot_window = hl_void,
    .gr_col = hl_void,
    .base_col = hl_void,
    .cput_text = hl_void,
    .get_draw_size = hl_get_draw_size,
    .draw_freeze = hl_void,
    .blank_draw_window = hl_void,
    .put_text = hl_put_text,
    .small_base = hl_void,
    .small_gr = hl_void,
    .film_clip = hl_film_clip,
    .reset_film = hl_void,
    .movie_play_back = hl_void,
    .movie_auto_play = hl_void,
    .movie_save = hl_movie_save,
    .movie_make_anigif = hl_void,
    .draw_point = hl_draw_point,
    .draw_line = hl_draw_line,
    .draw_bead = hl_draw_point,
    .draw_frect = hl_draw_frect,
    .draw_text = hl_put_text,
    .draw_special_text = hl_draw_special_text,
    .draw_linestyle = hl_int,
    .set_color = hl_int,
    .aplot_draw_one = hl_str,
    .auto_make_window = hl_auto_make_window,
    .auto_line = hl_draw_line,
    .auto_text = hl_put_text,
    .auto_circle = hl_auto_circle,
    .auto_fill_circle = hl_auto_circle,
    .auto_xor_cross = hl_draw_point,
    .auto_line_width = hl_int,
    .auto_col = hl_int,
    .auto_bw = hl_void,
    .auto_clr_stab = hl_void,
    .auto_stab_line = hl_draw_line,
    .auto_clear_plot = hl_void,
    .auto_redraw_menus = hl_void,
    .auto_clear_info = hl_void,
    .auto_draw_info = hl_auto_draw_info,
    .auto_refresh = hl_void,
    .auto_check_abort = hl_auto_check_abort,
    .auto_rubber = hl_auto_rubber,
    .auto_choose_key = hl_auto_choose_key,
    .auto_scroll_window = hl_void,
    .auto_grab_event = hl_auto_grab_event,
    .auto_show_hint = hl_void,
    .auto_grab_end = hl_int,
    .auto_diagram = hl_auto_diagram,
    .init_txtview = hl_void,
    .show_eq_box = hl_show_eq_box,
    .redraw_menu = hl_void,
    .rubber_band = hl_auto_rubber,
    .scroll_window = hl_void,
    .new_colormap = hl_int,
    .aplot_redraw = hl_void,
    .aplot_reset_axes = hl_void,
    .aplot_make = hl_str,
    .new_vcr = hl_void,
    .make_txtview = hl_void,
    .q_calc = hl_void,
    .exit_program = hl_exit_program,
};

void xpp_set_ui(const XppUi *ui)
{
    /* every non-NULL entry of *ui replaces the current one; NULL keeps the
       default. Done field by field via the pointer table trick below. */
    XppUi d = xpp_ui;
    const void *const *src = (const void *const *)ui;
    const void **dst = (const void **)&d;
    size_t n = sizeof(XppUi) / sizeof(void *);
    size_t i;
    for (i = 0; i < n; i++)
        if (src[i]) dst[i] = src[i];
    xpp_ui = d;
}

/* ---- dispatchers with the historical names ---------------------------- */

void err_msg(char *string) { xpp_ui.err_msg(string); }
void ping(void) { xpp_ui.ping(); }
void bottom_msg(int line, char *msg) { xpp_ui.bottom_msg(line, msg); }
void MessageBox(char *m) { xpp_ui.message_box(m); }
void KillMessageBox(void) { xpp_ui.kill_message_box(); }
void title_text(char *s) { xpp_ui.title_text(s); }
void canvas_xy(char *s) { xpp_ui.canvas_xy(s); }
int new_string(char *name, char *value) { return xpp_ui.new_string(name, value); }
int yes_no_box(void) { return xpp_ui.yes_no_box(); }
int TwoChoice(char *c1, char *c2, char *q, char *key)
{
    return xpp_ui.two_choice(c1, c2, q, key, NULL);
}
void respond_box(char *button, char *message) { xpp_ui.respond_box(button, message); }
int do_string_box(int n, int row, int col, char *title, char **names,
                  char values[][MAX_LEN_SBOX], int maxchar)
{
    return xpp_ui.string_box(n, row, col, title, names, values, maxchar);
}
int file_selector(char *title, char *file, char *wild)
{
    return xpp_ui.file_selector(title, file, wild);
}
int get_dialog(char *wname, char *name, char *value, char *ok, char *cancel, int max)
{
    return xpp_ui.dialog(wname, name, value, ok, cancel, max);
}
int do_edit_box(int n, char *title, char **names, char **values)
{
    return xpp_ui.edit_box(n, title, names, values);
}
int GetMouseXY(int *x, int *y) { return xpp_ui.get_mouse_xy(x, y); }
void flash(int num) { xpp_ui.menu_flash(num); }
int menu_choose(const struct XppMenu *m, int def) { return xpp_ui.menu_choose(m, def); }
/* the running job's checkpoint (xpp_job.h): Escape as soon as the job is
   cancelled, else the front end's own poll at most every 50 ms; its Escape
   (or Abort button) cancels the job, so later checks need no poll */
int my_abort(void)
{
    int ch;
    if (xpp_job_cancelled()) return 27;
    if (!xpp_job_poll_due()) return 64;
    ch = xpp_ui.check_abort();
    if (ch == 27) xpp_job_cancel_current();
    return ch;
}
int get_command_width(void) { return xpp_ui.progress_begin(); }
void plot_command(int nit, int icount, int cwidth) { xpp_ui.progress(nit, icount, cwidth); }
void rows_stored(int nrows) { xpp_ui.rows_stored(nrows); }
void FlushDisplay(void) { xpp_ui.flush(); }
void redraw_params(void) { xpp_ui.redraw_params(); }
void redraw_ics(void) { xpp_ui.redraw_ics(); }
void redraw_all(void) { xpp_ui.redraw_all(); }
void redraw_bcs(void) { xpp_ui.redraw_bcs(); }
void redraw_delays(void) { xpp_ui.redraw_delays(); }
void drw_all_scrns(void) { xpp_ui.redraw_screens(); }
void clr_all_scrns(void) { xpp_ui.clear_screens(); }
void clear_draw_window(void) { xpp_ui.clear_draw_window(); }
void reset_graphics(void) { xpp_ui.reset_graphics(); }
void create_a_pop(void) { xpp_ui.create_plot_window(); }
void destroy_a_pop(void) { xpp_ui.destroy_plot_window(); }
void kill_all_pops(void) { xpp_ui.kill_plot_windows(); }
void GrCol(void) { xpp_ui.gr_col(); }
void BaseCol(void) { xpp_ui.base_col(); }
void cput_text(void) { xpp_ui.cput_text(); }
void SmallBase(void) { xpp_ui.small_base(); }
void SmallGr(void) { xpp_ui.small_gr(); }
void reset_film(void) { xpp_ui.reset_film(); }
void set_color(int col) { xpp_ui.set_color(col); }
void draw_one_array_plot(char *bob) { xpp_ui.aplot_draw_one(bob); }
void make_auto(char *wname, char *iname) { xpp_ui.auto_make_window(wname, iname); }
void ALINE(int a, int b, int c, int d) { xpp_ui.auto_line(a, b, c, d); }
void ATEXT(int a, int b, char *c) { xpp_ui.auto_text(a, b, c); }
void Circle(int x, int y, int r) { xpp_ui.auto_circle(x, y, r); }
void FillCircle(int x, int y, int r) { xpp_ui.auto_fill_circle(x, y, r); }
void XORCross(int x, int y) { xpp_ui.auto_xor_cross(x, y); }
void LineWidth(int wid) { xpp_ui.auto_line_width(wid); }
void autocol(int col) { xpp_ui.auto_col(col); }
void autobw(void) { xpp_ui.auto_bw(); }
void clr_stab(void) { xpp_ui.auto_clr_stab(); }
void auto_stab_line(int x, int y, int xp, int yp) { xpp_ui.auto_stab_line(x, y, xp, yp); }
void clear_auto_plot(void) { xpp_ui.auto_clear_plot(); }
void redraw_auto_menus(void) { xpp_ui.auto_redraw_menus(); }
void clear_auto_info(void) { xpp_ui.auto_clear_info(); }
void draw_auto_info(char *bob, int x, int y) { xpp_ui.auto_draw_info(bob, x, y); }
void refreshdisplay(void) { xpp_ui.auto_refresh(); }
int byeauto_(int *iflag) /* AUTO's checkpoint, as my_abort() */
{
    int r;
    *iflag = 0;
    if (xpp_job_cancelled()) {
        *iflag = 1;
        return 0;
    }
    if (!xpp_job_poll_due()) return 0;
    r = xpp_ui.auto_check_abort(iflag);
    if (*iflag == 1) xpp_job_cancel_current();
    return r;
}
int auto_rubber(int *i1, int *j1, int *i2, int *j2, int flag)
{
    return xpp_ui.auto_rubber(i1, j1, i2, j2, flag);
}
int auto_pop_up_list(char *title, char **list, char *key, int n, int max,
                     int def, int x, int y, char **hints, char *httxt)
{
    return xpp_ui.auto_choose_key(title, list, key, n, max, def, x, y, hints, httxt);
}
void auto_scroll_window(void) { xpp_ui.auto_scroll_window(); }
void auto_diagram(const XppDiagPoint *p) { xpp_ui.auto_diagram(p); }
void init_txtview(void) { xpp_ui.init_txtview(); }
void create_eq_box(int cp, int cm, int rp, int rm, int im, double *y,
                   double *ev, int n)
{
    xpp_ui.show_eq_box(cp, cm, rp, rm, im, y, ev, n);
}
void bye_bye(void) { xpp_ui.exit_program(); }
void draw_help(void) { xpp_ui.redraw_menu(); }
int rubber_band(int *i1, int *j1, int *i2, int *j2, int flag)
{
    return xpp_ui.rubber_band(i1, j1, i2, j2, flag);
}
void scroll_window(void) { xpp_ui.scroll_window(); }
void NewColormap(int type) { xpp_ui.new_colormap(type); }
void make_my_aplot(char *name) { xpp_ui.aplot_make(name); }
void new_vcr(void) { xpp_ui.new_vcr(); }
void redraw_the_graph(void) { xpp_ui.redraw_graph(); }
void make_txtview(void) { xpp_ui.make_txtview(); }
void q_calc(void) { xpp_ui.q_calc(); }

/* plintf, new_int and new_float were in ggets.c; they never touched X.

   plintf is now a thin wrapper around xpp_log() at INFO: the banner,
   "All formulas are valid!!", parser statistics, duplicate-name notes
   and the like, quiet by default and shown with --verbose/--debug (see
   xpp_log.h). It still honours log_settings.verbose, the ODE file's own QUIET
   option (load_eqn.c) -- set it to 0 there and plintf stays fully
   silent regardless of the log threshold, same as before this module
   existed. A real error uses err_msg()/xpp_log(..., XPP_LOG_ERROR/WARN)
   instead, never plintf. */

int plintf(const char *fmt, ...)
{
    va_list arglist;

    if (!log_settings.verbose) return 0; /* Don't print at all! */

    va_start(arglist, fmt);
    xpp_log_v(XPP_LOG_INFO, fmt, arglist);
    va_end(arglist);

    return 0;
}

int new_int(char *name, int *value)
{
    char svalue[200];
    XPP_SPRINTF(svalue, "%d", *value);
    if (new_string(name, svalue) == 0 || strlen(svalue) == 0) return -1;
    *value = atoi(svalue);
    return 0;
}

int new_float(char *name, double *value)
{
    int done;
    int flag;
    double newz;
    char tvalue[200];
    XPP_SPRINTF(tvalue, "%.16g", *value);
    done = new_string(name, tvalue);
    if (done == 0 || strlen(tvalue) == 0) return -1;

    if (tvalue[0] == '%') {
        flag = do_calc(&tvalue[1], &newz);
        if (flag != -1) *value = newz;
        return 0;
    }
    *value = atof(tvalue);

    return 0;
}
