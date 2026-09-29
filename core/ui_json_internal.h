/* What the files of the JSON protocol front end share (C++ only; the C API
   is ui_json.h). The front end is split by responsibility:

   ui_json.cpp       the XppUi table, the command dispatch (handle_line),
                     the input classifier, script replay, install and hello
   json_io.cpp       the output lines, the input lines, the JSON reader
   json_prompts.cpp  asks (prompts, dialogs, menus, mouse), long loops
   json_state.cpp    the state event, the data browser, parameter and IC
                     edits, the data subscription, equations, source
   json_windows.cpp  plot windows, pixels, the kinescope, the array plot
   json_auto.cpp     the AUTO window, its diagram data and settings
   json_ani.cpp      the animation window

   docs/protocol.md is the contract. */
#ifndef XPP_UI_JSON_INTERNAL_H
#define XPP_UI_JSON_INTERNAL_H

#ifndef __cplusplus
#error "ui_json_internal.h is C++ only"
#endif

#include "xpp_ui.h"
#include "xpp_io.h"
#include "xpp_mem.h"
#include "display_state.h"
#include <stddef.h>
#include <atomic>
#include <deque>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

/* window ids the client draws into; plot windows are graph index + 1 */
#define WIN_AUTO 101
#define WIN_ANI 104
#define WIN_APLOT 105

/* control_line()'s answer for the animation's Pause */
#define ANI_PAUSE (-1)

/* a string literal into a Buf */
#define BUF_LIT(b, lit) xpp::json::buf_add(b, lit, sizeof(lit) - 1)

namespace xpp {
struct ModelRequest; /* model_switch.h */
}

namespace xpp::json {

/* ---- ui_json.cpp ---- */

/* the session's mutable state that more than one file needs */
/* a command line kept for after the running command (defer_line) */
struct DeferredLine {
    std::string line;
    unsigned long seq;
    bool refused; /* classify() refused it during a computation */
};

struct ProtocolSession {
    /* --script FILE (docs/protocol.md "Scripts"): script_mode is set by
       json_ui_set_script(), script_error by an error message, which makes
       the process exit 1 at the end of the file */
    int script_mode, script_error;
    /* the main-window menu shown (MAIN_MENU, FILE_MENU, NUM_MENU), for the
       reader thread's classify(): which menu a key is an item of */
    std::atomic<int> menu{0};
    /* the lines a command's prompt or computation took that were meant for
       after it, in arrival order; the command loop runs them first */
    std::deque<DeferredLine> deferred;
};
extern ProtocolSession session;

[[noreturn]] void quit_session(void); /* exit 1 after a script's error, else 0 */
int handle_async(const char *line);   /* commands that make sense at any moment */
int control_line(const char *line);   /* a control line taken by a checkpoint */
int during_run(const char *line);     /* what a running computation takes (ui_json.cpp) */
/* keep the line read last (read_line) for after the running command;
   refused: during_run() refused it (the command loop answers it so) */
void defer_line(const char *line, bool refused);
char line_kind(const char *line); /* a command's kind, menus.h XPP_KIND_* (ui_json.cpp) */
/* a script line that does not fit the dialogue: stop at once */
[[noreturn]] void script_fail(const char *what, const char *line, const char *ask);
void script_next(void); /* the script's next line, and an interruption after it */

/* ---- json_silent.cpp ---- */

/* -silent's built-in script: its command lines, each made when its turn
   comes (xpp_inbox_start_generated) */
std::function<std::optional<std::string>()> silent_script(void);

/* ---- json_io.cpp: output ---- */

/* an event line being built */
struct Buf {
    std::string s;
};

/* a failed allocation ends the program (xpp_out_of_memory, xpp_mem.h):
   no exception leaves the front end's functions, which the core calls as C */

void buf_add(Buf *b, const char *s, size_t n);
/* std::format into a Buf (type-checked at compile time; the formatting
   itself is xpp::vformat_append's, compiled once in xpp_io.cpp) */
template <class... Args>
void buf_format(Buf *b, std::format_string<Args...> fmt, Args &&...args) noexcept
{
    xpp::vformat_append(b->s, fmt.get(), std::make_format_args(args...));
}
void buf_str(Buf *b, const char *s); /* a JSON string ("" for NULL) */
void buf_str(Buf *b, std::string_view s);
void buf_str_array(Buf *b, const char *const *v, int n);
void buf_str_array(Buf *b, std::span<const std::string> v);
/* a JSON number, "null" when v is not finite (json_number.h): sig
   significant digits, e.g. 16 for a full double, 7 for AUTO's diagram */
void buf_num(Buf *b, double v, int sig);

void open_protocol_stdout(void); /* the protocol on the stdout of now */
void out_line(const char *s, size_t n);
void out_flush(void);
void flush_pending(void);
void send_buf(Buf *b);
void send_simple(const char *ev, const char *key, const char *text);
void json_flush(void);
void data_emit(const char *line, size_t n);

/* ---- json_io.cpp: input and the JSON reader ---- */

char *read_line(int which, int wait_ms);
unsigned long read_line_seq(void); /* the sequence number of read_line()'s line */
bool read_line_refused(void);      /* whether classify() refused read_line()'s line */

const char *skip_ws(const char *p);
const char *skip_value(const char *p);
const char *js_find(const char *obj, const char *key);
/* the JSON string at v into out, cut to max - 1 bytes (a short keyword's
   bound; a name or a value is read whole); false (out empty) when v is
   not a string */
bool js_string(const char *v, std::string &out, size_t max = std::string::npos);
double js_num(const char *v, double def);
int js_number(const char *v, double *out);
const char *js_elem(const char *arr, int i);
bool get_string(const char *obj, const char *key, std::string &out, size_t max = std::string::npos);
double get_num(const char *obj, const char *key, double def);
int get_int(const char *obj, const char *key, double def); /* get_num cut to an int */
int is_cmd(const char *line, const char *name);
/* the zoom range `key` of a command: an array [lo, hi], or null for the
   window's own. 0 when the key is absent; 1 read (r set or unset); -1 not a
   range (r untouched) */
int get_range(const char *obj, const char *key, xpp::AxisRange &r);
int key_code(const char *k);

/* ---- json_prompts.cpp ---- */

int ask_begin(Buf *b, const char *kind);
int ask_wait(Buf *b, int id);
const char *ask_answer(void); /* the answer ask_wait() took */
void answer_point(unsigned long win, int k, int *x, int *y);
/* the answer's points into v, x and y by turns (v.size() / 2 of them) */
int mouse_ask(unsigned long win, const char *kind, int flag, std::span<int> v);
int ask_drag(unsigned long win, int *x, int *y);

void j_err_msg(const char *msg);
void j_ping(void);
void j_bottom_msg(int line, const char *msg);
void j_message_box(const char *msg);
void j_kill_message_box(void);
void j_title_text(const char *s);
int j_dialog(const char *title, const char *name, std::string &value, const char *ok, const char *cancel, int kind);
int j_new_string(const char *name, std::string &value, int kind);
int j_yes_no_box(void);
int j_two_choice(const char *c1, const char *c2, const char *q, const char *key, const char *title);
void j_respond_box(const char *button, const char *message);
int j_checklist(const char *title, const char *const *names, int *flags, int n);
int j_string_box(int row, int col, const char *title, const char *const *names, std::span<std::string> values,
                 const int *kinds);
int j_file_selector(const char *title, std::string &file, const char *wild);
int j_get_mouse_xy(int *x, int *y);
int j_rubber_band(int *i1, int *j1, int *i2, int *j2, int flag);
int j_menu_choose(const struct XppMenu *m, int def);
void j_show_menu(int which);
void j_open_help(const char *chapter, const char *anchor);
void j_copy_text(const char *what, const char *text);
int j_check_abort(void);
int j_progress_begin(void);
void j_progress(int nit, int icount, int cwidth);
void j_q_calc(void);

/* ---- json_state.cpp ---- */

void send_state(void);
void send_state_if_dirty(void);
void j_state_dirty(void);
void j_state_dirty_i(int i);
void j_state_dirty_is(int i, const char *s);
void browser_rows(const char *line);
void browser_command(const char *line);
void browser_key(int ch, const char *line);
void browser_update(void); /* at a command's end */
void j_browser_changed(int i);
void j_rows_stored(int nrows);
void plotvars_command(const char *line);
void data_command(const char *line);
void send_equations(void);
void j_show_eq_box(int cp, int cm, int rp, int rm, int im, double *y, double *ev, int n);
void equilibrium_key(int ch);
void j_make_txtview(void);
void action_command(const char *line);
void apply_set(const char *line);
void default_command(const char *line);
void slide_command(const char *line);
void values_command(const char *line);
void state_forget(void); /* what the model before showed (its last equilibrium) goes */

/* ---- json_windows.cpp ---- */

void windows_init(void);
void send_main_window(const char *title);
void send_window(const char *what, unsigned long id, int w, int h, const char *title);
void select_graph(int i);
void click_command(const char *line);
/* {"cmd":"display","win":N,"x":[lo,hi]|null,"y":...,"runs":bool}: the zoom shown in plot window N and whether its earlier runs are drawn (display_state.h) */
void display_command(const char *line);
void j_get_draw_size(unsigned int *w, unsigned int *h);
void j_blank_draw_window(void);
void j_redraw_all(void);
void j_redraw_graph(void);
void j_redraw_screens(void);
void j_clear_screens(void);
void j_reset_graphics(void);
void j_activate_graph(int i, int flag);
void j_create_plot_window(void);
void j_destroy_plot_window(void);
void j_kill_plot_windows(void);
void j_cput_text(void);
void j_draw_freeze(void);
void j_scroll_window(void);
void j_new_colormap(int type);

std::vector<unsigned char> ask_pixels(int win, int film, int *w, int *h); /* empty: cancelled */
int write_ppm(const char *file, std::span<const unsigned char> rgb, int w, int h);
void web_safe_colors(std::span<unsigned char> rgb); /* at most 256 colours, for the GIF writer */

int j_film_clip(void);
void j_reset_film(void);
void j_movie_play_back(void);
void j_movie_auto_play(void);
void j_movie_save(const char *basename, int fmat);
void j_movie_make_anigif(void);

void aplot_changed(void); /* the data behind an array plot changed */
void aplot_update(void);  /* at a command's end */
void aplot_command(const char *line);
void aplot_key(int ch);
void j_aplot_make(const char *name);
void j_aplot_redraw(void);
void j_aplot_draw_one(const char *tag);

/* ---- json_auto.cpp ---- */

void diag_flush(int final);
void diag_forget(void); /* the client has no diagram */
int diag_point_of_node(int node);
void auto_command(const char *line);
/* the `autoview` event (hidden branches and zoom), for a client that asked for autoinfo */
void auto_view_subscribe(int on);
void auto_view_update(void);
void auto_key(int ch);
void auto_redraw_for_client(void);
int is_auto_set(const char *line);
void defer_auto_set(const char *line);
void apply_deferred_sets(void);
void j_auto_make_window(const char *wname, const char *iname);
int j_auto_check_abort(int *iflag);
int j_auto_rubber(int *i1, int *j1, int *i2, int *j2, int flag);
int j_auto_grab_event(int *x, int *y);
void j_auto_show_hint(void);
void j_auto_scroll_window(void);
void j_auto_diagram(const XppDiagPoint *p);
void j_auto_refresh(void);

/* ---- json_model.cpp ---- */

/* loads req's model (File > Open model, Reload) in place of this one, and
   serves it; when it cannot be loaded, the model before stays */
void switch_model(const xpp::ModelRequest &req);

/* ---- json_ani.cpp ---- */

int ani_speed_op(const char *o, const char *line);
void ani_command(const char *line);
void ani_key(int ch);
void j_ani_slider(void);
void j_new_vcr(void);
void j_ani_show(void);

} // namespace xpp::json

#endif
