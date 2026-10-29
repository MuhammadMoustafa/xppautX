#pragma once

#include <stdio.h>
#include <span>
#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include "xpp_error.h"
#include "auto_codes.h"
#include "colormap.h"

namespace xpp {
struct Session; /* session.h */
struct AniExport; /* aniparse.h */
namespace files { struct WriteFolders; }
namespace command { struct Command; class Arguments; } /* command_def.h */

/*
 * The seam between the numerics and whatever front end is driving them.
 *
 * Core code keeps calling the historical names (err_msg, new_int, ping,
 * refresh_browser, TwoChoice, ...). Those are thin
 * dispatchers in xpp_ui.cpp that go through the XppUi table below. The
 * default table is headless: messages go to the log, the question policy
 * supplies terminal answers, and redraws do nothing. The browser front end installs its own
 * table (ui_json.cpp) before it serves a session.
 *
 * There are no pixel primitives: since protocol 2 (docs/ui-v2.md T18) the page
 * draws from data, which the code hands to the data modules (plot_data,
 * phase_data, marks_data, auto_data through auto_diagram, ani_data), and a
 * PDF or SVG picture is recorded by picture.h (W492, #556 removed the 31
 * drawing callbacks that no front end implemented).
 *
 * Conventions kept from the original code:
 *   new_string / file_selector / string_box return 0 when the user cancels.
 *   two_choice and choose return the chosen key character, 0 if none.
 *   menu_choose returns the chosen key character, 0 or 27 if none.
 *   check_abort returns a key code, 27 for escape, 64 when nothing happened.
 */

struct XppMenu; /* menus.h */

/* Save permissions on a command: a stable answer, independent of disk state. */
enum { SAVE_ASK = 0, SAVE_REPLACE = 1, SAVE_DECLINE = -1 };

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
    XPP_FIELD_BOOLEAN,    /* `true` or `false` (FormField) */
    XPP_FIELD_CHOICE,     /* one of a FormField's named choices */
    XPP_FIELD_REAL,       /* a plain real number, within a range where given (FormField) */
    XPP_FIELD_COLOR,      /* a colour: `#rrggbb`, `#rrggbbaa`, a CSS name or `foreground` (FormField) */
    XPP_FIELD_NAME = 16   /* + n: a name from the hello event's lists[n] */
};

/* One field of a typed form (ask_form): what it takes (kind: BOOLEAN,
   CHOICE, INTEGER, REAL, COLOR or TEXT, by aggregate initialisation where no factory is), its value as text (BOOLEAN `true` or
   `false`, CHOICE one of `choices` by name, a number as its digits, a COLOR as colormap.h's print_color writes it), and
   the range an INTEGER or REAL keeps to. A client shows a choice's name
   with its underscores as spaces and a capital first letter: the label is
   never written apart. ask_form checks every answer by its kind and, once
   all are good, fills flag, whole (an INTEGER's value, a CHOICE's index in
   `choices`) and real. */
struct FormField {
    std::string name;
    int kind = XPP_FIELD_TEXT;
    std::string value;
    std::vector<std::string> choices;
    std::optional<double> min, max;
    bool flag = false;
    int whole = 0;
    double real = 0;
    Color color;

    static FormField color_field(std::string name, const Color &value);
    static FormField boolean(std::string name, bool value);
    static FormField choice(std::string name, std::vector<std::string> choices, std::size_t selected);
    static FormField real_number(std::string name, double value, std::optional<double> min = std::nullopt,
                                 std::optional<double> max = std::nullopt);
};
/* a name from list n: 0 T and the variables, 1 the ODE variables,
   2 the parameters, 3 both (ui_json.cpp hello) */
#define XPP_FIELD_NAME_IN(n) (xpp::XPP_FIELD_NAME + (n))

#define XPP_AUTO_CLICK 1000
/* auto_grab_event: move the cursor to AUTO's diagram entry x (DIAGRAM.index) */
#define XPP_AUTO_NODE 1001

/* One point of the AUTO diagram as add_point() (auto_nox.cpp) plots it in
   one of its views, in that view's axis quantities (auto_xy_plot), for a
   front end that draws the diagram from data. A point the view does not
   plot has draw 0 and x, y1 and y2 NaN. */
typedef struct XppDiagPoint {
    int ibr, pt;   /* AUTO's branch and point number, signed as AUTO has them */
    AutoPointCode itp; /* AUTO's point type (auto_point_kind_name) */
    int lab;       /* label, 0 when none or when the label mark is not drawn */
    DiagramPointType type; /* stable eq, unstable eq, stable periodic, unstable periodic */
    AutoCalculation flag2;     /* two-parameter curve kind, no_calculation for one parameter */
    int draw;      /* 0 not drawn (only the start of the next line), 1 a line
                      back to the previous point, 2 filled circles at y1 and
                      y2, 3 open circles */
    int newseg;    /* the previous point is this one: no line back */
    Color color;   /* the colour its line or marker is drawn in */
    int lw;        /* its line's width in pixels */
    int node;      /* its entry in AUTO's diagram list (DIAGRAM.index) */
    int from;      /* the label the run that computed it started from, on
                      the run's first point only (0 otherwise, or unknown) */
    int jacobian;  /* the Jacobian of the run that computed it (DIAGRAM.jacobian) */
    double x, y1, y2;
} XppDiagPoint;

/* Historical names, now dispatchers. Declared here so every core file sees
   one consistent prototype. */
/* An error is command_error, err_reading or show_error with its place.
   Results use bottom_msg, the existing information route. */
/* an error about `path`, a file the command could not read (line 0: the
   page offers to add it to the model's folder, docs/protocol.md
   "Errors"), or at its line */
void err_reading(std::string_view path, std::string_view msg, int line = 0);
void ping(void);
void bottom_msg(int line, std::string_view msg, std::string_view chapter = {}, std::string_view anchor = {});
void MessageBox(std::string_view m);
void KillMessageBox(void);
void title_text(std::string_view s);
int new_int(std::string_view name, int *value);
int TwoChoice(std::string_view c1, std::string_view c2, std::string_view q, std::string_view key);
/* A question with several answers, one key each (keys[i] answers
   choices[i]); the chosen key, 0 when none. The last choice is what no
   answer means (Cancel, Refuse). yes_key is the suggested answer. */
int Choose(std::string_view title, std::string_view question, std::span<const std::string> choices,
           std::string_view keys, char yes_key);
/* A yes/no question; independent guards never take a recorded answer. */
bool confirm(std::string_view question, bool independent = false);
/* Every answer taken without asking (-y / --yes, a recording's answer in
   --silent) is printed through this: the question and the answer taken. */
void report_automatic(std::string_view question, std::string_view answer);
/* Who supplies a question's answer; kept in the active front end's table. */
enum class QuestionPolicy { User, Terminal, Suggested, None };
/* Front ends supply their context; this owner applies the answering policy.
   Scripted answers belong to the command; independent output guards never use them. */
QuestionPolicy answer_policy(bool terminal, bool scripted);
void flash(int num);
int menu_choose(const struct XppMenu *m, int def);
/* A choice among names (named sets, tables, frozen curves): the
   first PICK_LETTERS lettered a to z ("a: name"), the rest picked from the
   list by their index; the chosen name's index, -1 for none */
int pick_from_list(std::string_view id, std::string_view title, std::span<const std::string> names);
int my_abort(void);
int get_command_width(void);
void plot_command(int nit, int icount, int cwidth);
void FlushDisplay(void);
void redraw_params(void);
void redraw_ics(void);
void refreshdisplay(Session &s);
int byeauto_(int *iflag);
void init_txtview(void);
void bye_bye(void);
void draw_help(void);

/* The front end's character cell in pixels, for laying out the AUTO and
   array plot windows and text in plots: a big and a small monospace font.
   0 when headless; xppautX sets them before it serves a session. */
typedef struct {
    int big_width, big_height;
    int small_width, small_height;
} XppTextMetrics;
extern XppTextMetrics text_metrics;

/* ---- the table and the dialogs that edit text ---------------------------
   The text a user types comes back in a std::string, as long as the
   front end sends it (no dialog cuts it, W76), instead of in a
   caller's fixed char array (W32c); the text a dialog only shows is a
   std::string_view (W109e). */

typedef struct XppUi {
    /* messages */
    /* an error, shown with where it is (xpp_error.h; the headless
       default logs its text()) */
    void (*err_msg)(const Error &e);
    /* where the command that runs came from (command_place) */
    Place (*command_place)(void);
    /* A command's explicit replace decision: 0 asks, 1 permits, -1 declines. */
    int (*save_replace)(bool independent);
    /* A core decision without a dialog/command answer, for recording. */
    void (*save_decision)(int decision, bool asked);
    void (*save_result)(std::string_view file, bool saved);
    files::WriteFolders *(*write_folders)(void); /* the active front end supplies its session owner */
    void (*ping)(void);
    void (*bottom_msg)(int line, std::string_view msg, std::string_view chapter, std::string_view anchor);
    void (*message_box)(std::string_view msg);
    void (*kill_message_box)(void);
    void (*title_text)(std::string_view s);

    /* prompts */
    QuestionPolicy (*question_policy)(bool independent);
    int (*new_string)(std::string_view name, std::string &value, int kind);
    int (*choose)(std::string_view title, std::string_view question, std::span<const std::string> choices,
                  std::string_view keys, char yes_key);
    /* toggle a set of flags (1/0) by name; flags are edited in place and
       restored on cancel. Returns 1 for done, 0 for cancel. */
    int (*checklist)(std::string_view title, const char *const *names, int *flags, int n);
    /* kinds: one XPP_FIELD_* per field, or NULL (all text) */
    int (*string_box)(int row, int col, std::string_view title, const char *const *names,
                      std::span<std::string> values, const int *kinds);
    /* a typed form (ask_form): every field by its kind, choices and range;
       the answers go back into fields' value; 0 on cancel */
    int (*typed_form)(std::string_view title, std::span<FormField> fields);
    /* string_box laid out as a grid: the first columns.size() * rows.size()
       fields are its cells, row by row; the rest are plain fields below it */
    int (*grid_box)(std::string_view title, std::span<const char *const> columns, std::span<const char *const> rows,
                    const char *const *names, std::span<std::string> values, const int *kinds);
    int (*file_selector)(std::string_view title, std::string &file, std::string_view wild);
    /* one-line text entry with named buttons; returns 0 on cancel */
    int (*dialog)(std::string_view title, std::string_view name, std::string &value, std::string_view ok,
                  std::string_view cancel, int kind);
    int (*get_mouse_xy)(Session &s, int *x, int *y);

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
    void (*redraw_all)(Session &s);
    void (*redraw_bcs)(void);
    void (*redraw_delays)(void);
    void (*redraw_graph)(Session &s);
    void (*redraw_screens)(Session &s);   /* every plot window */
    void (*clear_screens)(Session &s);
    void (*clear_draw_window)(Session &s);
    void (*reset_graphics)(Session &s);
    void (*data_changed)(int length); /* browser storage grew/shrank */
    /* an integration stored row nrows-1: storage[.][0..nrows) is the run so
       far, before data_changed at its end. Called for every row: keep it
       cheap (a front end that shows the run as it grows rate-limits itself) */
    void (*rows_stored)(Session &s, int nrows);
    /* Every accepted output step, before Poincare/transient filtering. */
    void (*live_state)(Session &s, const double *values, double time);
    void (*browser_redraw)(int full); /* my_browser: 1 columns too, 0 data */

    /* plot windows */
    void (*create_plot_window)(Session &s); /* present the already-created active graph */
    void (*destroy_plot_window)(Session &s); /* the active one; not the main window */
    void (*window_renamed)(Session &s, std::string_view old_name, std::string_view new_name);
    void (*auto_closed)(Session &s, std::string_view name);
    void (*tool_window_closed)(Session &s, std::string_view name, std::string_view kind); /* the animation or array plot window destroyed */
    void (*redraw_diagram_for_client)(Session &s); /* the AUTO diagram sent to the client again, from its first point */
    void (*lower_plot_window)(void);   /* put the active one at the bottom */
    /* Text,etc (T)ext: ask for a label, place it with the mouse, draw it
       and add_label() it */
    void (*cput_text)(Session &s);
    void (*get_draw_size)(Session &s, unsigned int *w, unsigned int *h);
    void (*blank_draw_window)(Session &s);
    Result<> (*film_clip)(Session &s); /* a frame, or why it was not captured */
    void (*reset_film)(Session &s);
    /* kinescope: the captured frames live in the front end */
    void (*movie_play_back)(Session &s);  /* step through frames with keys/mouse */
    void (*movie_auto_play)(Session &s);  /* the kinescope's cycles, frame_ms apart */
    /* the frames as files `basename` names (image_format.h registry index): how many were saved, or why not */
    Result<int> (*movie_save)(Session &s, std::string_view basename, int fmat);
    Result<> (*movie_make_anigif)(Session &s, std::string_view file); /* the frames as one animated GIF */

    /* mouse interaction in the plot window. rubber_band returns 1 and the
       corners in pixels when the user drew a box (flag RUBBOX) or line
       (RUBLINE), 0 when cancelled. scroll_window lets the user drag the
       view until a key is pressed (calling update_view). */
    int (*rubber_band)(Session &s, int *i1, int *j1, int *i2, int *j2, int flag);
    void (*scroll_window)(Session &s);

    /* raw drawing primitives used by graphics.c when the plot format is
       the screen (PS and SVG are handled in graphics.c itself); no front
       end draws them (see the top of this file) */

    /* array plot window (arrayplot.h: aplot) */
    void (*aplot_make)(Session &s, std::string_view name); /* open the array plot window */
    void (*aplot_redraw)(Session &s);
    void (*aplot_reset_axes)(Session &s);  /* its title and z range labels */
    void (*aplot_draw_one)(Session &s, std::string_view tag); /* redraw, tag and save a range frame */
    Result<bool> (*aplot_gif)(Session &s, std::string_view file); /* the picture as a GIF file; false: replacing the file was declined */

    /* AUTO bifurcation window */
    void (*auto_make_window)(Session &s, std::string_view wname, std::string_view iname);
    void (*auto_refresh)(Session &s);
    int (*auto_check_abort)(int *iflag);
    int (*auto_rubber)(Session &s, int *i1, int *j1, int *i2, int *j2, int flag);
    void (*auto_scroll_window)(Session &s);
    /* Grab: wait for a key (returns its code, mykeydef.h), a click on
       the diagram (returns XPP_AUTO_CLICK with the pixel in x,y) or a
       point of the diagram by its entry (XPP_AUTO_NODE, DIAGRAM.index in x) */
    int (*auto_grab_event)(Session &s, int *x, int *y);
    void (*auto_show_hint)(Session &s); /* Auto.hinttxt changed */
    /* the grab is over: done=1 a point was taken (Enter), -1 cancelled (Esc) */
    void (*auto_grab_end)(int done);
    /* The diagram as data, beside the drawing, in view `view` (W50,
       AutoState::views): p is a point add_point() just plotted there, NULL
       that the view was cleared and its axes drawn again. */
    void (*auto_diagram)(Session &s, int view, const XppDiagPoint *p);
    /* how many points of the diagram the client holds (Clear hides the branches up to them) */
    int (*auto_points_held)(Session &s);

    /* animation (toon) window. Frames are drawn off screen, vcr.wid by
       vcr.hgt pixels (aniparse.h), then ani_show puts one on screen. */
    void (*new_vcr)(Session &s);
    void (*ani_show)(void);
    void (*ani_slider)(Session &s);    /* vcr.pos changed */
    Result<int> (*ani_go)(Session &s, const AniExport &out); /* play the frames, writing them when asked: how many were shown */

    /* misc front-end hooks called while loading an ODE file */
    void (*init_txtview)(void);

    /* equilibrium eigenvalue summary window; ev: the n eigenvalues as
       (re, im) pairs, or NULL when there is no list (a delay equation) */
    void (*show_eq_box)(Session &s, int cp, int cm, int rp, int rm, int im, double *y,
                        double *ev, int n);

    /* Whole dialogs a front end provides; the core has no logic in them
       beyond what they call back (do_calc, the ODE source in Model).
       Headless: they do nothing. */
    void (*make_txtview)(Session &s); /* File/Prt src: source and active comments */
    void (*q_calc)(Session &s);       /* File/Calculator: evaluate formulas */
    /* File/recorD: start recording the session's steps, or stop and save
       the recording (W59a, json_record.cpp) */
    void (*record_toggle)(Session &s);
    /* File/plaY recording, or Open model of a .recx: the recording's model
       loaded from it, ready to play its steps (an empty path: ask for the file;
       W59b, json_player.cpp) */
    void (*play_recording)(Session &s, std::string_view path);
    /* a recording is in progress: Quit offers to save it with the
       session (W59d) */
    bool (*recording)(void);
    /* the recording in progress saved (its name asked, as its stop asks)
       and ended: false on a cancel or a failure (W59d) */
    bool (*save_recording)(Session &s);

    /* A registry command the front end answers itself, because the state it works on is the front end's:
       the recording and the player, the user's keymap, the event subscriptions, the state and the
       source (W528). The row's arguments are validated; the result is the command's result object. */
    Result<std::string> (*front_end_command)(Session &s, const command::Command &command, const command::Arguments &args,
                                             const Place &place);

    /* program is quitting */
    void (*exit_program)(void);
} XppUi;

/* The active table. Never NULL; defaults to the headless implementation. */
extern XppUi ui;

void set_ui(const XppUi *ui); /* copies; missing entries keep defaults */

/* the historical names of the calls that act on a Session's windows or
   data: the Session is the caller's, passed to the front end (W47d6) */
int GetMouseXY(Session &s, int *x, int *y);
void rows_stored(Session &s, int nrows);
void redraw_all(Session &s);
void drw_all_scrns(Session &s);
void clr_all_scrns(Session &s);
void clear_draw_window(Session &s);
void reset_graphics(Session &s);
void destroy_a_pop(Session &s);
void kill_all_pops(Session &s);
void cput_text(Session &s);
void reset_film(Session &s);
void draw_one_array_plot(Session &s, std::string_view bob);
void make_auto(Session &s, std::string_view wname, std::string_view iname);
int auto_rubber(Session &s, int *i1, int *j1, int *i2, int *j2, int flag);
void auto_scroll_window(Session &s);
void auto_diagram(Session &s, int view, const XppDiagPoint *p);
int auto_points_held(Session &s);
void create_eq_box(Session &s, int cp, int cm, int rp, int rm, int im, double *y, double *ev, int n);
int rubber_band(Session &s, int *i1, int *j1, int *i2, int *j2, int flag);
void scroll_window(Session &s);
void make_my_aplot(Session &s, std::string_view name);
void new_vcr(Session &s);
void redraw_the_graph(Session &s);
void make_txtview(Session &s);
void q_calc(Session &s);
void record_toggle(Session &s);
void play_recording(Session &s, std::string_view path);
bool recording_in_progress(void);
bool save_recording(Session &s);

/* new_float: a number, or %formula worked out in s (0 on OK, -1 on
   cancel or an empty answer) */
int new_float(Session &s, std::string_view name, double *value);

/* new_string / new_string_of: one line of text (kind: XPP_FIELD_*), value
   the default shown and, on OK, what was typed. 0 on cancel. */
int new_string(std::string_view name, std::string &value);
int new_string_of(std::string_view name, std::string &value, int kind);
/* a form of values.size() fields named names[i], each kinds[i]
   (XPP_FIELD_*; NULL all text), every value of any length; 0 on cancel */
int do_string_box(int row, int col, std::string_view title, const char *const *names,
                  std::span<std::string> values);
int do_string_box_of(int row, int col, std::string_view title, const char *const *names,
                     std::span<std::string> values, const int *kinds);
/* a form of typed fields (FormField): every answer is checked by its kind,
   whole, before any is used. 0 on cancel or when an answer is bad (the error,
   naming the field, is shown with `title` as the command) */
int ask_form(Session &s, std::string_view title, std::span<FormField> fields);
/* do_string_box_of laid out as a grid (XppUi::grid_box) */
int do_grid_box_of(std::string_view title, std::span<const char *const> columns, std::span<const char *const> rows,
                   const char *const *names, std::span<std::string> values, const int *kinds);
/* a file name (base name or path, what the user picked) matching wild;
   0 on cancel or an empty name */
int file_selector(std::string_view title, std::string &file, std::string_view wild);
/* one-line entry of any length with named buttons; 0 on cancel */
int get_dialog(std::string_view wname, std::string_view name, std::string &value, std::string_view ok,
               std::string_view cancel);
int get_dialog_of(std::string_view wname, std::string_view name, std::string &value, std::string_view ok,
                  std::string_view cancel, int kind);

/* the one way an error is shown (xpp_error.h): what a computation the
   command ran returned, or what the command found wrong itself, with its
   place (logging where at DEBUG); an empty `what` shows nothing (the
   computation said it already) */
void show_error(const Error &e);
/* e with the line of its file as written in its place, when it names one and the file reads (what show_error adds
   to an error, and a registry command's failure gets the same) */
Error with_source_line(const Error &e);
/* Where the command that runs came from (W140): in a recording's Play, the .recx and
   the step's line; an empty Place for a command the user gave. The place
   of an error a command finds in what it was given, or that a
   computation it ran meets with no file or model line to name. */
Place command_place();
/* the error what of the command `command` (its where), at
   command_place() */
void command_error(std::string_view command, std::string_view what);
/* whether r succeeded, its error shown when it did not */
template <class T> bool ok_or_show(const Result<T> &r)
{
  if (!r) show_error(r.error());
  return r.has_value();
}

} // namespace xpp
