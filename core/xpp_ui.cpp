/* Headless defaults for the UI seam, plus the dispatchers that keep the
   historical function names working. See xpp_ui.h. */
#include "xpp_ui.h"
#include "session.h"
#include "xpp_job.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xpp_util.h"

namespace xpp {


/* ---- headless defaults ------------------------------------------------ */

static void hl_err_msg(const Error &e) { log(XPP_LOG_ERROR, "{}\n", e.text()); }
static void hl_void(void) {}
static void hl_str(std::string_view) {}
static void hl_int(int) {}
static void hl_bottom_msg(int, std::string_view) {}
static void hl_s(Session &) {}
static void hl_s_int(Session &, int) {}
static void hl_s_str(Session &, std::string_view) {}
static int hl_new_string(std::string_view, std::string &, int) { return 0; }
static int hl_no(void) { return 0; }
static int hl_two_choice(std::string_view, std::string_view, std::string_view, std::string_view, std::string_view)
{
    return 0;
}
static int hl_string_box(int, int, std::string_view, const char *const *, std::span<std::string>, const int *)
{
    return 0;
}
static int hl_file_selector(std::string_view, std::string &, std::string_view)
{
    return 0;
}
static int hl_menu_choose(const struct XppMenu *, int)
{
    return 0;
}
static int hl_get_mouse_xy(Session &, int *, int *) { return 0; }
static int hl_check_abort(void) { return 64; }
static void hl_progress(int, int, int) {}
static void hl_activate_graph(Session &, int, int) {}
static void hl_get_draw_size(Session &s, unsigned int *w, unsigned int *h)
{
    /* whatever the graph last had, else a sensible canvas */
    const GRAPH *g = s.plot_windows.current;
    *w = g && g->x11Wid > 0 ? static_cast<unsigned int>(g->x11Wid) : 640;
    *h = g && g->x11Hgt > 0 ? static_cast<unsigned int>(g->x11Hgt) : 480;
}
static void hl_put_text(int, int, const char *) {}
static int hl_film_clip(Session &) { return 1; }
static void hl_draw_point(int, int) {}
static void hl_draw_line(int, int, int, int) {}
static void hl_draw_frect(int, int, int, int) {}
static void hl_draw_special_text(int, int, const char *, int) {}
static void hl_auto_make_window(Session &, std::string_view, std::string_view) {}
static void hl_auto_circle(int, int, int) {}
static void hl_auto_diagram(Session &, int, const XppDiagPoint *) {}
static void hl_auto_draw_info(std::string_view, int, int) {}
static int hl_auto_grab_event(Session &, int *, int *) { return 27; }
static int hl_auto_check_abort(int *iflag) { *iflag = 0; return 0; }
static int hl_auto_rubber(Session &, int *, int *, int *, int *, int)
{
    return 0;
}
static void hl_show_eq_box(Session &, int cp, int cm, int rp, int rm, int im, double *y,
                           double *ev, int n)
{
    int i;
    xpp::log(XPP_LOG_DEBUG, "Equilibrium: c+={} c-={} r+={} r-={} im={}\n", cp, cm, rp, rm, im);
    for (i = 0; i < n; i++) {
        if (ev) xpp::log(XPP_LOG_DEBUG, "  y[{}]={:.8g}  eig={:.8g}{:+.8g}i\n", i, y[i], ev[2 * i], ev[2 * i + 1]);
        else xpp::log(XPP_LOG_DEBUG, "  y[{}]={:.8g}\n", i, y[i]);
    }
}
static int hl_dialog(std::string_view, std::string_view, std::string &, std::string_view, std::string_view, int)
{
    return 0;
}
static void hl_ani_font(int, int, int) {}
static void hl_ani_box(int, int, int, int, int) {}
static void hl_param_box_set(int, const char *) {}
static void hl_respond_box(std::string_view, std::string_view message) { xpp::log(XPP_LOG_WARN, "{}\n", message); }
static int hl_checklist(std::string_view, const char *const *, int *, int)
{
    return 0;
}
static void hl_movie_save(Session &, std::string_view, int) {}
static void hl_open_help(std::string_view, std::string_view) {}
static void hl_copy_text(std::string_view, std::string_view text) { xpp::log(XPP_LOG_INFO, "{}\n", text); }
static void hl_record_toggle(Session &) { xpp::log_printf(XPP_LOG_WARN, "Recording needs the page or --server\n"); }
static void hl_play_recording(Session &, std::string_view) { xpp::log_printf(XPP_LOG_WARN, "Playing a recording needs the page or --server\n"); }
static bool hl_recording(void) { return false; }
static bool hl_save_recording(Session &) { return true; }
static void hl_exit_program(void) { exit(1); }

XppTextMetrics text_metrics;

XppUi ui = {
    .err_msg = hl_err_msg,
    .ping = hl_void,
    .bottom_msg = hl_bottom_msg,
    .message_box = hl_str,
    .kill_message_box = hl_void,
    .title_text = hl_str,
    .new_string = hl_new_string,
    .yes_no_box = hl_no,
    .two_choice = hl_two_choice,
    .respond_box = hl_respond_box,
    .checklist = hl_checklist,
    .string_box = hl_string_box,
    .file_selector = hl_file_selector,
    .dialog = hl_dialog,
    .get_mouse_xy = hl_get_mouse_xy,
    .menu_flash = hl_int,
    .show_menu = hl_int,
    .redraw_menu = hl_void,
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
    .redraw_all = hl_s,
    .redraw_bcs = hl_void,
    .redraw_delays = hl_void,
    .redraw_graph = hl_s,
    .redraw_screens = hl_s,
    .clear_screens = hl_s,
    .clear_draw_window = hl_s,
    .reset_graphics = hl_s,
    .data_changed = hl_int,
    .rows_stored = hl_s_int,
    .browser_redraw = hl_int,
    .activate_graph = hl_activate_graph,
    .create_plot_window = hl_s,
    .destroy_plot_window = hl_s,
    .kill_plot_windows = hl_s,
    .lower_plot_window = hl_void,
    .gr_col = hl_void,
    .base_col = hl_void,
    .cput_text = hl_s,
    .get_draw_size = hl_get_draw_size,
    .draw_freeze = hl_s,
    .blank_draw_window = hl_s,
    .small_base = hl_void,
    .small_gr = hl_void,
    .film_clip = hl_film_clip,
    .reset_film = hl_s,
    .movie_play_back = hl_s,
    .movie_auto_play = hl_s,
    .movie_save = hl_movie_save,
    .movie_make_anigif = hl_s,
    .rubber_band = hl_auto_rubber,
    .scroll_window = hl_s,
    .new_colormap = hl_int,
    .draw_point = hl_draw_point,
    .draw_line = hl_draw_line,
    .draw_bead = hl_draw_point,
    .draw_frect = hl_draw_frect,
    .draw_text = hl_put_text,
    .draw_special_text = hl_draw_special_text,
    .draw_linestyle = hl_int,
    .set_color = hl_int,
    .aplot_make = hl_s_str,
    .aplot_redraw = hl_s,
    .aplot_reset_axes = hl_s,
    .aplot_draw_one = hl_s_str,
    .auto_make_window = hl_auto_make_window,
    .auto_line = hl_draw_line,
    .auto_text = hl_put_text,
    .auto_circle = hl_auto_circle,
    .auto_fill_circle = hl_auto_circle,
    .auto_xor_cross = hl_draw_point,
    .auto_line_width = hl_int,
    .auto_col = hl_int,
    .auto_bw = hl_void,
    .auto_clear_plot = hl_void,
    .auto_redraw_menus = hl_void,
    .auto_clear_info = hl_void,
    .auto_draw_info = hl_auto_draw_info,
    .auto_refresh = hl_s,
    .auto_check_abort = hl_auto_check_abort,
    .auto_rubber = hl_auto_rubber,
    .auto_scroll_window = hl_s,
    .auto_grab_event = hl_auto_grab_event,
    .auto_show_hint = hl_s,
    .auto_grab_end = hl_int,
    .auto_diagram = hl_auto_diagram,
    .new_vcr = hl_s,
    .ani_clear = hl_void,
    .ani_show = hl_void,
    .ani_color = hl_int,
    .ani_thick = hl_int,
    .ani_font = hl_ani_font,
    .ani_line = hl_draw_line,
    .ani_rect = hl_ani_box,
    .ani_arc = hl_ani_box,
    .ani_text = hl_put_text,
    .ani_slider = hl_s,
    .init_txtview = hl_void,
    .show_eq_box = hl_show_eq_box,
    .make_txtview = hl_s,
    .q_calc = hl_s,
    .open_help = hl_open_help,
    .copy_text = hl_copy_text,
    .record_toggle = hl_record_toggle,
    .play_recording = hl_play_recording,
    .recording = hl_recording,
    .save_recording = hl_save_recording,
    .exit_program = hl_exit_program,
};

void set_ui(const XppUi *table)
{
    /* every non-NULL entry of *table replaces the current one; NULL keeps the
       default. Done field by field via the pointer table trick below. */
    XppUi d = ui;
    const void *const *src = reinterpret_cast<const void *const *>(table);
    const void **dst = reinterpret_cast<const void **>(&d);
    size_t n = sizeof(XppUi) / sizeof(void *);
    size_t i;
    for (i = 0; i < n; i++)
        if (src[i]) dst[i] = src[i];
    ui = d;
}

/* ---- dispatchers with the historical names ---------------------------- */

void err_msg(std::string_view msg) { show_error(Error{{}, std::string(msg)}); }
void err_reading(std::string_view path, std::string_view msg, int line)
{
    show_error(Error{"reading", std::string(msg), Place{std::string(path), line}});
}

void show_error(const Error &e)
{
    if (e.place.line > 0 && e.place.source.empty() && !e.place.file.empty()) {
        /* the line as written, when the file reads */
        Error with_source = e;
        with_source.place.source = LineReader(e.place.file).line(e.place.line);
        if (!with_source.place.source.empty()) return show_error(with_source);
    }
    if (!e.where.empty()) xpp::log(XPP_LOG_DEBUG, "{} failed: {}\n", e.where, e.what);
    if (!e.what.empty()) ui.err_msg(e);
}
void ping(void) { ui.ping(); }
void bottom_msg(int line, std::string_view msg) { ui.bottom_msg(line, msg); }
void MessageBox(std::string_view m) { ui.message_box(m); }
void KillMessageBox(void) { ui.kill_message_box(); }
void title_text(std::string_view s) { ui.title_text(s); }
int new_string(std::string_view name, std::string &value) { return ui.new_string(name, value, XPP_FIELD_TEXT); }
int new_string_of(std::string_view name, std::string &value, int kind) { return ui.new_string(name, value, kind); }
int yes_no_box(void) { return ui.yes_no_box(); }
int TwoChoice(std::string_view c1, std::string_view c2, std::string_view q, std::string_view key)
{
    return ui.two_choice(c1, c2, q, key, "");
}
void respond_box(std::string_view button, std::string_view message) { ui.respond_box(button, message); }
int do_string_box(int row, int col, std::string_view title, const char *const *names,
                  std::span<std::string> values)
{
    return ui.string_box(row, col, title, names, values, NULL);
}
int do_string_box_of(int row, int col, std::string_view title, const char *const *names,
                     std::span<std::string> values, const int *kinds)
{
    return ui.string_box(row, col, title, names, values, kinds);
}
int file_selector(std::string_view title, std::string &file, std::string_view wild)
{
    return ui.file_selector(title, file, wild);
}
int get_dialog(std::string_view wname, std::string_view name, std::string &value, std::string_view ok,
               std::string_view cancel)
{
    return ui.dialog(wname, name, value, ok, cancel, XPP_FIELD_TEXT);
}
int get_dialog_of(std::string_view wname, std::string_view name, std::string &value, std::string_view ok,
                  std::string_view cancel, int kind)
{
    return ui.dialog(wname, name, value, ok, cancel, kind);
}
int GetMouseXY(Session &s, int *x, int *y) { return ui.get_mouse_xy(s, x, y); }
void flash(int num) { ui.menu_flash(num); }
int menu_choose(const struct XppMenu *m, int def) { return ui.menu_choose(m, def); }
/* the running job's checkpoint (xpp_job.h): Escape as soon as the job is
   cancelled, else the front end's own poll at most every 50 ms; its Escape
   (or Abort button) cancels the job, so later checks need no poll */
int my_abort(void)
{
    int ch;
    if (xpp::job::cancelled()) return 27;
    if (int key = xpp::job::take_key()) return key; /* a replayed / (xpp_job.h) */
    if (!xpp::job::poll_due()) return 64;
    ch = ui.check_abort();
    if (ch == 27) xpp::job::cancel_current();
    return ch;
}
int get_command_width(void) { return ui.progress_begin(); }
void plot_command(int nit, int icount, int cwidth) { ui.progress(nit, icount, cwidth); }
void rows_stored(Session &s, int nrows) { ui.rows_stored(s, nrows); }
void FlushDisplay(void) { ui.flush(); }
void redraw_params(void) { ui.redraw_params(); }
void redraw_ics(void) { ui.redraw_ics(); }
void redraw_all(Session &s) { ui.redraw_all(s); }
void drw_all_scrns(Session &s) { ui.redraw_screens(s); }
void clr_all_scrns(Session &s) { ui.clear_screens(s); }
void clear_draw_window(Session &s) { ui.clear_draw_window(s); }
void reset_graphics(Session &s) { ui.reset_graphics(s); }
void create_a_pop(Session &s) { ui.create_plot_window(s); }
void destroy_a_pop(Session &s) { ui.destroy_plot_window(s); }
void kill_all_pops(Session &s) { ui.kill_plot_windows(s); }
void GrCol(void) { ui.gr_col(); }
void BaseCol(void) { ui.base_col(); }
void cput_text(Session &s) { ui.cput_text(s); }
void SmallBase(void) { ui.small_base(); }
void SmallGr(void) { ui.small_gr(); }
void reset_film(Session &s) { ui.reset_film(s); }
void set_color(int col) { ui.set_color(col); }
void draw_one_array_plot(Session &s, std::string_view bob) { ui.aplot_draw_one(s, bob); }
void make_auto(Session &s, std::string_view wname, std::string_view iname) { ui.auto_make_window(s, wname, iname); }
void ALINE(int a, int b, int c, int d) { ui.auto_line(a, b, c, d); }
void ATEXT(int a, int b, const char *c) { ui.auto_text(a, b, c); }
void Circle(int x, int y, int r) { ui.auto_circle(x, y, r); }
void FillCircle(int x, int y, int r) { ui.auto_fill_circle(x, y, r); }
void XORCross(int x, int y) { ui.auto_xor_cross(x, y); }
void LineWidth(int wid) { ui.auto_line_width(wid); }
void autocol(int col) { ui.auto_col(col); }
void autobw(void) { ui.auto_bw(); }
void clear_auto_plot(void) { ui.auto_clear_plot(); }
void redraw_auto_menus(void) { ui.auto_redraw_menus(); }
void clear_auto_info(void) { ui.auto_clear_info(); }
void draw_auto_info(std::string_view bob, int x, int y) { ui.auto_draw_info(bob, x, y); }
void refreshdisplay(Session &s) { ui.auto_refresh(s); }
int byeauto_(int *iflag) /* AUTO's checkpoint, as my_abort() */
{
    int r;
    *iflag = 0;
    if (xpp::job::cancelled()) {
        *iflag = 1;
        return 0;
    }
    if (!xpp::job::poll_due()) return 0;
    r = ui.auto_check_abort(iflag);
    if (*iflag == 1) xpp::job::cancel_current();
    return r;
}
int auto_rubber(Session &s, int *i1, int *j1, int *i2, int *j2, int flag)
{
    return ui.auto_rubber(s, i1, j1, i2, j2, flag);
}
void auto_scroll_window(Session &s) { ui.auto_scroll_window(s); }
void auto_diagram(Session &s, int view, const XppDiagPoint *p) { ui.auto_diagram(s, view, p); }
void init_txtview(void) { ui.init_txtview(); }
void create_eq_box(Session &s, int cp, int cm, int rp, int rm, int im, double *y,
                   double *ev, int n)
{
    ui.show_eq_box(s, cp, cm, rp, rm, im, y, ev, n);
}
void bye_bye(void) { ui.exit_program(); }
void draw_help(void) { ui.redraw_menu(); }
int rubber_band(Session &s, int *i1, int *j1, int *i2, int *j2, int flag)
{
    return ui.rubber_band(s, i1, j1, i2, j2, flag);
}
void scroll_window(Session &s) { ui.scroll_window(s); }
void NewColormap(int type) { ui.new_colormap(type); }
void make_my_aplot(Session &s, std::string_view name) { ui.aplot_make(s, name); }
void new_vcr(Session &s) { ui.new_vcr(s); }
void redraw_the_graph(Session &s) { ui.redraw_graph(s); }
void make_txtview(Session &s) { ui.make_txtview(s); }
void q_calc(Session &s) { ui.q_calc(s); }
void open_help(std::string_view chapter, std::string_view anchor) { ui.open_help(chapter, anchor); }
void copy_text(std::string_view what, std::string_view text) { ui.copy_text(what, text); }
void record_toggle(Session &s) { ui.record_toggle(s); }
void play_recording(Session &s, std::string_view path) { ui.play_recording(s, path); }
bool recording_in_progress(void) { return ui.recording(); }
bool save_recording(Session &s) { return ui.save_recording(s); }

/* new_int and new_float were in ggets.c; they never touched X. plintf()
   was a thin wrapper around xpp::log_printf() at INFO (the banner, "All formulas
   are valid!!", parser statistics, duplicate-name notes) and was retired
   at W25: call xpp::log_printf(XPP_LOG_INFO, ...) / xpp::log(XPP_LOG_INFO, ...)
   directly -- xpp::log_vprintf() itself now honours xpp::log_settings.verbose (the
   ODE file's own QUIET option, load_eqn.c) for INFO-level messages, the
   same gating plintf() used to do itself (see xpp_log.c/xpp_log.h). A
   real error uses err_msg()/xpp::log_printf(..., XPP_LOG_ERROR/WARN) instead. */

int new_int(std::string_view name, int *value)
{
    std::string svalue = xpp::format("{}", *value);
    if (new_string_of(name, svalue, XPP_FIELD_INTEGER) == 0 || svalue.empty()) return -1;
    *value = atoi(svalue.c_str());
    return 0;
}

int new_float(Session &s, std::string_view name, double *value)
{
    std::string tvalue = xpp::format("{:.16g}", *value);
    if (new_string_of(name, tvalue, XPP_FIELD_FORMULA) == 0 || tvalue.empty()) return -1;

    if (tvalue[0] == '%') {
        double newz;
        if (xpp::do_calc(s, tvalue.c_str() + 1, &newz) != -1) *value = newz;
        return 0;
    }
    *value = atof(tvalue.c_str());

    return 0;
}

} // namespace xpp
