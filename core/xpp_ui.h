#ifndef XPP_UI_H
#define XPP_UI_H

#include "xpplim.h"
/*
 * The seam between the numerics and whatever front end is driving them.
 *
 * Core code keeps calling the historical names (err_msg, new_int, ping,
 * refresh_browser, TwoChoice, set_color, ALINE, ...). Those are thin
 * dispatchers in xpp_ui.cpp that go through the XppUi table below. The
 * default table is headless: messages go to the log, prompts are declined,
 * redraws and drawing do nothing. The browser front end installs its own
 * table (ui_json.cpp) before it serves a session.
 *
 * The pixel primitives (draw_*, set_color, auto_line and the rest of the
 * AUTO window's, ani_color ... ani_text) have no front end implementation
 * since protocol 2 (docs/ui-v2.md T18): the page draws from data, which
 * the code that calls them hands to the data modules (plot_data,
 * phase_data, marks_data, auto_data through auto_diagram, ani_data). They
 * stay as the headless no-ops below.
 *
 * Conventions kept from the original code:
 *   new_string / file_selector / string_box return 0 when the user cancels.
 *   yes_no_box returns 1 for yes, 0 for no.
 *   two_choice returns the chosen key character, 0 if none.
 *   menu_choose returns the chosen key character, 0 or 27 if none.
 *   check_abort returns a key code, 27 for escape, 64 when nothing happened.
 */

#include <stdio.h>
#ifdef __cplusplus
#include <span>
#include <string>
#include "xpp_error.h"
extern "C" {
#endif

struct XppMenu; /* menus.h */

/* What a prompt's field takes (T31): new_string_of, get_dialog_of and
   do_string_box_of name it, new_int and new_float say it themselves, and
   a front end checks the text while it is typed (docs/protocol.md "Asks",
   `kinds`). Every other prompt's fields are text. */
enum {
    XPP_FIELD_TEXT = 0,
    XPP_FIELD_INTEGER,    /* a whole number (read with atoi) */
    XPP_FIELD_NUMBER,     /* a number (read with atof) */
    XPP_FIELD_FORMULA,    /* a number, or %formula (new_float) */
    XPP_FIELD_EXPRESSION, /* a formula of the model's quantities */
    XPP_FIELD_FILE,       /* a file's base name */
    XPP_FIELD_NAME = 16   /* + n: a name from the hello event's lists[n] */
};
/* a name from list n: 0 T and the variables, 1 the ODE variables,
   2 the parameters, 3 both (ui_json.cpp hello) */
#define XPP_FIELD_NAME_IN(n) (XPP_FIELD_NAME + (n))

#define XPP_AUTO_CLICK 1000
/* auto_grab_event: move the cursor to AUTO's diagram entry x (DIAGRAM.index) */
#define XPP_AUTO_NODE 1001

/* One point of the AUTO diagram as add_point() (auto_nox.c) plots it in
   one of its views, in that view's axis quantities (auto_xy_plot), for a
   front end that draws the diagram from data. A point the view does not
   plot has draw 0 and x, y1 and y2 NaN. */
typedef struct XppDiagPoint {
    int ibr, pt;   /* AUTO's branch and point number, signed as AUTO has them */
    int itp;       /* AUTO's point type (auto_bif_sym) */
    int lab;       /* label, 0 when none or when the label mark is not drawn */
    int type;      /* 1 stable eq, 2 unstable eq, 3 stable periodic, 4 unstable periodic */
    int flag2;     /* two-parameter curve kind (LPE2...), 0 for one parameter */
    int draw;      /* 0 not drawn (only the start of the next line), 1 a line
                      back to the previous point, 2 filled circles at y1 and
                      y2, 3 open circles */
    int newseg;    /* the previous point is this one: no line back */
    int color, lw; /* what autocol() and LineWidth() get for it */
    int node;      /* its entry in AUTO's diagram list (DIAGRAM.index) */
    int from;      /* the label the run that computed it started from, on
                      the run's first point only (0 otherwise, or unknown) */
    double x, y1, y2;
} XppDiagPoint;

/* Historical names, now dispatchers. Declared here so every core file sees
   one consistent prototype. */
void err_msg(const char *string);
void ping(void);
void bottom_msg(int line, const char *msg);
void MessageBox(const char *m);
void KillMessageBox(void);
void title_text(const char *s);
int new_int(const char *name, int *value);
int yes_no_box(void);
int TwoChoice(const char *c1, const char *c2, const char *q, const char *key);
void respond_box(const char *button, const char *message);
void flash(int num);
int menu_choose(const struct XppMenu *m, int def);
int my_abort(void);
int get_command_width(void);
void plot_command(int nit, int icount, int cwidth);
void FlushDisplay(void);
void redraw_params(void);
void redraw_ics(void);
void GrCol(void);
void BaseCol(void);
void SmallBase(void);
void SmallGr(void);
void set_color(int col);
void ALINE(int a, int b, int c, int d);
void ATEXT(int a, int b, const char *c);
void Circle(int x, int y, int r);
void FillCircle(int x, int y, int r);
void XORCross(int x, int y);
void LineWidth(int wid);
void autocol(int col);
void autobw(void);
void clear_auto_plot(void);
void redraw_auto_menus(void);
void clear_auto_info(void);
void draw_auto_info(const char *bob, int x, int y);
void refreshdisplay(void);
int byeauto_(int *iflag);
void init_txtview(void);
void bye_bye(void);
void draw_help(void);
void NewColormap(int type);
void open_help(const char *chapter, const char *anchor);
void copy_text(const char *what, const char *text);

/* The front end's character cell in pixels, for laying out the AUTO and
   array plot windows and text in plots: a big and a small monospace font.
   0 when headless; xppautX sets them before it serves a session. */
typedef struct {
    int big_width, big_height;
    int small_width, small_height;
} XppTextMetrics;
extern XppTextMetrics text_metrics;

#ifdef __cplusplus
}

/* ---- C++: the table and the dialogs that edit text ---------------------
   The text a user types comes back in a std::string, as long as the
   front end sends it (no dialog cuts it, W76), instead of in a
   caller's fixed char array (W32c). C++ only: std::string cannot cross
   an extern "C" boundary, and every caller is C++. */

namespace xpp {
struct Session; /* session.h */
}

typedef struct XppUi {
    /* messages */
    void (*err_msg)(const char *msg);
    void (*ping)(void);
    void (*bottom_msg)(int line, const char *msg);
    void (*message_box)(const char *msg);
    void (*kill_message_box)(void);
    void (*title_text)(const char *s);

    /* prompts */
    int (*new_string)(const char *name, std::string &value, int kind);
    int (*yes_no_box)(void);
    int (*two_choice)(const char *c1, const char *c2, const char *q, const char *key, const char *title);
    void (*respond_box)(const char *button, const char *message); /* alert with one button */
    /* toggle a set of flags (1/0) by name; flags are edited in place and
       restored on cancel. Returns 1 for done, 0 for cancel. */
    int (*checklist)(const char *title, const char *const *names, int *flags, int n);
    /* kinds: one XPP_FIELD_* per field, or NULL (all text) */
    int (*string_box)(int row, int col, const char *title, const char *const *names,
                      std::span<std::string> values, const int *kinds);
    int (*file_selector)(const char *title, std::string &file, const char *wild);
    /* one-line text entry with named buttons; returns 0 on cancel */
    int (*dialog)(const char *title, const char *name, std::string &value, const char *ok,
                  const char *cancel, int kind);
    int (*get_mouse_xy)(xpp::Session &s, int *x, int *y);

    /* menus. show_menu makes MAIN_MENU, FILE_MENU or NUM_MENU (menus.h) the
       main-window menu; core sets help_menu and dispatches keys itself
       (commander in commands.c). menu_choose pops up a menu, def is the
       highlighted item. */
    void (*menu_flash)(int num);
    void (*show_menu)(int which);
    void (*redraw_menu)(void);
    int (*menu_choose)(const struct XppMenu *m, int def);

    /* long-running loops poll these */
    int (*check_abort)(void);
    int (*progress_begin)(void);                     /* returns bar width */
    void (*progress)(int nit, int icount, int cwidth);
    void (*flush)(void);

    /* things changed, please redraw */
    void (*redraw_params)(void);
    /* parameter i now has text value s; redraw that one entry */
    void (*param_box_set)(int i, const char *s);
    void (*param_box_redraw)(int i);
    /* the same for initial condition i */
    void (*ic_box_set)(int i, const char *s);
    void (*ic_box_redraw)(int i);
    void (*redraw_ics)(void);
    void (*redraw_all)(xpp::Session &s);
    void (*redraw_bcs)(void);
    void (*redraw_delays)(void);
    void (*redraw_graph)(xpp::Session &s);
    void (*redraw_screens)(xpp::Session &s);   /* every plot window */
    void (*clear_screens)(xpp::Session &s);
    void (*clear_draw_window)(xpp::Session &s);
    void (*reset_graphics)(xpp::Session &s);
    void (*data_changed)(int length); /* browser storage grew/shrank */
    /* an integration stored row nrows-1: storage[.][0..nrows) is the run so
       far, before data_changed at its end. Called for every row: keep it
       cheap (a front end that shows the run as it grows rate-limits itself) */
    void (*rows_stored)(xpp::Session &s, int nrows);
    void (*browser_redraw)(int full); /* my_browser: 1 columns too, 0 data */

    /* plot windows */
    void (*activate_graph)(xpp::Session &s, int i, int flag); /* graph i became plot_windows.current */
    void (*create_plot_window)(xpp::Session &s);
    void (*destroy_plot_window)(xpp::Session &s); /* the active one; not the main window */
    void (*kill_plot_windows)(xpp::Session &s);   /* all but the main window */
    void (*lower_plot_window)(void);   /* put the active one at the bottom */
    void (*gr_col)(void);   /* pen: graph colours (GrCol) */
    void (*base_col)(void); /* pen: window colours (BaseCol) */
    /* Text,etc (T)ext: ask for a label, place it with the mouse, draw it
       and add_label() it */
    void (*cput_text)(xpp::Session &s);
    void (*get_draw_size)(xpp::Session &s, unsigned int *w, unsigned int *h);
    void (*draw_freeze)(xpp::Session &s); /* frozen curves */
    void (*blank_draw_window)(xpp::Session &s);
    void (*small_base)(void);  /* pen selection for small text */
    void (*small_gr)(void);
    int (*film_clip)(xpp::Session &s); /* returns 0 when the movie buffer is full */
    void (*reset_film)(xpp::Session &s);
    /* kinescope: the captured frames live in the front end */
    void (*movie_play_back)(xpp::Session &s);  /* step through frames with keys/mouse */
    void (*movie_auto_play)(xpp::Session &s);  /* the kinescope's cycles, frame_ms apart */
    void (*movie_save)(xpp::Session &s, const char *basename, int fmat); /* 1 ppm, 2 gif */
    void (*movie_make_anigif)(xpp::Session &s);

    /* mouse interaction in the plot window. rubber_band returns 1 and the
       corners in pixels when the user drew a box (flag RUBBOX) or line
       (RUBLINE), 0 when cancelled. scroll_window lets the user drag the
       view until a key is pressed (calling update_view). */
    int (*rubber_band)(xpp::Session &s, int *i1, int *j1, int *i2, int *j2, int flag);
    void (*scroll_window)(xpp::Session &s);

    /* colormap changed (custom_color) */
    void (*new_colormap)(int type);

    /* raw drawing primitives used by graphics.c when the plot format is
       the screen (PS and SVG are handled in graphics.c itself); no front
       end draws them (see the top of this file) */
    void (*draw_point)(int x, int y);
    void (*draw_line)(int x1, int y1, int x2, int y2);
    void (*draw_bead)(int x, int y);
    void (*draw_frect)(int x, int y, int w, int h);
    void (*draw_text)(int x, int y, const char *s);
    void (*draw_special_text)(int x, int y, const char *s, int size);
    void (*draw_linestyle)(int ls);
    void (*set_color)(int col);

    /* array plot window (arrayplot.h: aplot) */
    void (*aplot_make)(xpp::Session &s, const char *name); /* open the array plot window */
    void (*aplot_redraw)(xpp::Session &s);
    void (*aplot_reset_axes)(xpp::Session &s);  /* its title and z range labels */
    void (*aplot_draw_one)(xpp::Session &s, const char *tag); /* redraw, tag and save a range frame */

    /* AUTO bifurcation window */
    void (*auto_make_window)(xpp::Session &s, const char *wname, const char *iname);
    void (*auto_line)(int a, int b, int c, int d);
    void (*auto_text)(int a, int b, const char *c);
    void (*auto_circle)(int x, int y, int r);
    void (*auto_fill_circle)(int x, int y, int r);
    void (*auto_xor_cross)(int x, int y);
    void (*auto_line_width)(int wid);
    void (*auto_col)(int col);
    void (*auto_bw)(void);
    void (*auto_clear_plot)(void);
    void (*auto_redraw_menus)(void);
    void (*auto_clear_info)(void);
    void (*auto_draw_info)(const char *s, int x, int y);
    void (*auto_refresh)(void);
    int (*auto_check_abort)(int *iflag);
    int (*auto_rubber)(xpp::Session &s, int *i1, int *j1, int *i2, int *j2, int flag);
    void (*auto_scroll_window)(xpp::Session &s);
    /* Grab: wait for a key (returns its code, mykeydef.h), a click on
       the diagram (returns XPP_AUTO_CLICK with the pixel in x,y) or a
       point of the diagram by its entry (XPP_AUTO_NODE, DIAGRAM.index in x) */
    int (*auto_grab_event)(xpp::Session &s, int *x, int *y);
    void (*auto_show_hint)(xpp::Session &s); /* Auto.hinttxt changed */
    /* the grab is over: done=1 a point was taken (Enter), -1 cancelled (Esc) */
    void (*auto_grab_end)(int done);
    /* The diagram as data, beside the drawing, in view `view` (W50,
       AutoState::views): p is a point add_point() just plotted there, NULL
       that the view was cleared and its axes drawn again. */
    void (*auto_diagram)(xpp::Session &s, int view, const XppDiagPoint *p);

    /* animation (toon) window. Frames are drawn off screen, vcr.wid by
       vcr.hgt pixels (aniparse.h), then ani_show puts one on screen. */
    void (*new_vcr)(xpp::Session &s);
    void (*ani_clear)(void);    /* white frame, black pen */
    void (*ani_show)(void);
    void (*ani_color)(int icol); /* 0 black, else a colour index */
    void (*ani_thick)(int t);
    void (*ani_font)(int size, int font, int color); /* font 0 roman, 1 symbol */
    void (*ani_line)(int x1, int y1, int x2, int y2);
    void (*ani_rect)(int x, int y, int w, int h, int fill);
    void (*ani_arc)(int x, int y, int w, int h, int fill); /* ellipse in box */
    void (*ani_text)(int x, int y, const char *s);
    void (*ani_slider)(xpp::Session &s);    /* vcr.pos changed */

    /* misc front-end hooks called while loading an ODE file */
    void (*init_txtview)(void);

    /* equilibrium eigenvalue summary window; ev: the n eigenvalues as
       (re, im) pairs, or NULL when there is no list (a delay equation) */
    void (*show_eq_box)(xpp::Session &s, int cp, int cm, int rp, int rm, int im, double *y,
                        double *ev, int n);

    /* Whole dialogs a front end provides; the core has no logic in them
       beyond what they call back (do_calc, the ODE source in xpp::Model).
       Headless: they do nothing. */
    void (*make_txtview)(xpp::Session &s); /* File/Prt src: source and active comments */
    void (*q_calc)(xpp::Session &s);       /* File/Calculator: evaluate formulas */
    void (*open_help)(const char *chapter, const char *anchor); /* File/Help: open the manual there */
    /* File/cOpy set line: text for the user's clipboard (what: "set") */
    void (*copy_text)(const char *what, const char *text);
    /* File/recorD: start recording the session's steps, or stop and save
       the recording (W59a, json_record.cpp) */
    void (*record_toggle)(xpp::Session &s);
    /* File/plaY recording, or Open model of a .recx: the recording's model
       loaded from it, ready to play its steps (NULL: ask for the file;
       W59b, json_player.cpp) */
    void (*play_recording)(xpp::Session &s, const char *path);
    /* a recording is in progress: Quit offers to save it with the
       session (W59d) */
    bool (*recording)(void);
    /* the recording in progress saved (its name asked, as its stop asks)
       and ended: false on a cancel or a failure (W59d) */
    bool (*save_recording)(xpp::Session &s);

    /* program is quitting */
    void (*exit_program)(void);
} XppUi;

/* The active table. Never NULL; defaults to the headless implementation. */
extern XppUi xpp_ui;

void xpp_set_ui(const XppUi *ui); /* copies; missing entries keep defaults */

/* the historical names of the calls that act on a Session's windows or
   data: the Session is the caller's, passed to the front end (W47d6) */
int GetMouseXY(xpp::Session &s, int *x, int *y);
void rows_stored(xpp::Session &s, int nrows);
void redraw_all(xpp::Session &s);
void drw_all_scrns(xpp::Session &s);
void clr_all_scrns(xpp::Session &s);
void clear_draw_window(xpp::Session &s);
void reset_graphics(xpp::Session &s);
void create_a_pop(xpp::Session &s);
void destroy_a_pop(xpp::Session &s);
void kill_all_pops(xpp::Session &s);
void cput_text(xpp::Session &s);
void reset_film(xpp::Session &s);
void draw_one_array_plot(xpp::Session &s, const char *bob);
void make_auto(xpp::Session &s, const char *wname, const char *iname);
int auto_rubber(xpp::Session &s, int *i1, int *j1, int *i2, int *j2, int flag);
void auto_scroll_window(xpp::Session &s);
void auto_diagram(xpp::Session &s, int view, const XppDiagPoint *p);
void create_eq_box(xpp::Session &s, int cp, int cm, int rp, int rm, int im, double *y, double *ev, int n);
int rubber_band(xpp::Session &s, int *i1, int *j1, int *i2, int *j2, int flag);
void scroll_window(xpp::Session &s);
void make_my_aplot(xpp::Session &s, const char *name);
void new_vcr(xpp::Session &s);
void redraw_the_graph(xpp::Session &s);
void make_txtview(xpp::Session &s);
void q_calc(xpp::Session &s);
void record_toggle(xpp::Session &s);
void play_recording(xpp::Session &s, const char *path);
bool recording_in_progress(void);
bool save_recording(xpp::Session &s);

/* new_float: a number, or %formula worked out in s (0 on OK, -1 on
   cancel or an empty answer) */
int new_float(xpp::Session &s, const char *name, double *value);

/* new_string / new_string_of: one line of text (kind: XPP_FIELD_*), value
   the default shown and, on OK, what was typed. 0 on cancel. */
int new_string(const char *name, std::string &value);
int new_string_of(const char *name, std::string &value, int kind);
/* a form of values.size() fields named names[i], each kinds[i]
   (XPP_FIELD_*; NULL all text), every value of any length; 0 on cancel */
int do_string_box(int row, int col, const char *title, const char *const *names,
                  std::span<std::string> values);
int do_string_box_of(int row, int col, const char *title, const char *const *names,
                     std::span<std::string> values, const int *kinds);
/* a file name (base name or path, what the user picked) matching wild;
   0 on cancel or an empty name */
int file_selector(const char *title, std::string &file, const char *wild);
/* one-line entry of any length with named buttons; 0 on cancel */
int get_dialog(const char *wname, const char *name, std::string &value, const char *ok, const char *cancel);
int get_dialog_of(const char *wname, const char *name, std::string &value, const char *ok,
                  const char *cancel, int kind);

namespace xpp {
/* the command layer's side of xpp::Error (xpp_error.h): shows what a
   computation it ran returned, as err_msg does (logging where at DEBUG);
   an empty `what` shows nothing (the computation said it already) */
void show_error(const Error &e);
/* whether r succeeded, its error shown when it did not */
template <class T> bool ok_or_show(const Result<T> &r)
{
  if (!r) show_error(r.error());
  return r.has_value();
}
}

#endif
#endif
