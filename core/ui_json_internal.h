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

   docs/protocol.md is the contract.

   The Session a command runs in is chosen once, by handle_line, and passed
   down: a command's function takes it (xpp::Session &s) and reaches the
   Model through s.model(). The j_ functions are the XppUi table's: the
   ones that act on a Session's windows or data take it from the core code
   that calls them; the front end's own operations that core code reaches
   with no Session (an ask, a checkpoint) take the client's (client(),
   below). */
#ifndef XPP_UI_JSON_INTERNAL_H
#define XPP_UI_JSON_INTERNAL_H

#ifndef __cplusplus
#error "ui_json_internal.h is C++ only"
#endif

#include "xpp_ui.h"
#include "xpp_io.h"
#include "xpp_mem.h"
#include "xpp_inbox.h"
#include "display_state.h"
#include <stddef.h>
#include <atomic>
#include <climits>
#include <deque>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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
struct Session;      /* session.h */
}

namespace xpp::json {

struct Buf; /* an event line being built (json_io.cpp, below) */

/* the most rows and columns one `browser` block request gets (json_state.cpp
   browser_rows): a page of the table, bounded so one event stays small;
   hello.limits tells the page */
constexpr int BROWSER_MAX_ROWS = 2000;
constexpr int BROWSER_MAX_COLS = 500;

/* ---- ui_json.cpp ---- */

/* the session's mutable state that more than one file needs */
/* a command line kept for after the running command (defer_line) */
struct DeferredLine {
    std::string line;
    unsigned long seq;
    bool refused; /* classify() refused it during a computation */
    bool applied; /* a set applied when it was taken: the command only ends */
};

struct ProtocolSession {
    /* --script FILE (docs/protocol.md "Scripts"): script_mode is set by
       json_ui_set_script(); errors kept by xpp_log make
       the process exit 1 at the end of the file */
    int script_mode;
    /* the main-window menu shown (MAIN_MENU, FILE_MENU, NUM_MENU), for the
       reader thread's classify(): which menu a key is an item of */
    std::atomic<int> menu{0};
    /* the lines a command's prompt or computation took that were meant for
       after it, in arrival order; the command loop runs them first */
    std::deque<DeferredLine> deferred;
    /* json_state.cpp's: the state and browser events are due, the block
       of the browser the client asked for (its first row, rows, first
       column and columns), and the rows a run had stored when it reported
       last (a run that starts again reports fewer) */
    int state_dirty = 0, browser_dirty = 0;
    int br_from = 0, br_count = 0, br_col = 1, br_ncol = 1;
    int rows_seen = INT_MAX;
};
extern ProtocolSession session;

/* The Session of the client this front end serves (the session list's,
   session.h client_session). A command's functions and the XppUi
   callbacks take their Session from their caller (handle_line chooses it
   once per command); this is for what core code reaches through the seam
   with no Session at all, the front end's own: an ask (ask_wait), a
   checkpoint (j_check_abort), the pending AUTO points before an event
   (flush_pending), the state when it is dirty (send_state_if_dirty), and
   auto_data.cpp's point lookup (diag_point_of_node). */
xpp::Session &client();

[[noreturn]] void quit_session(void); /* exit 1 after any error, else 0 */
[[noreturn]] void quit_command(void); /* the client's quit: bye, then quit_session */
/* {"cmd":"quit","ask":true} (W59d: File > Quit's question) or
   {"cmd":"quit","save":true} (its Save session, answered in the page,
   W110): a command of its own, in its turn; a plain quit exits at once
   (scripts, --server's clients, the page's Don't save) */
bool quit_waits(const char *line);
int handle_async(xpp::Session &s, const char *line);   /* commands that make sense at any moment */
int control_line(xpp::Session &s, const char *line);   /* a control line taken by a checkpoint */
/* A setting (W106: a command of the setting kind, not a key) taken while
   a job runs, by a checkpoint or a question's wait. Once the job has begun
   computing it is kept for after the job, as a command of its own
   (defer_line): it applies then, with its error if it is not valid, state
   and idle, before any other command; the computation runs on with the
   values it started with. A `set` taken before any computation (a
   question still open, the animation's Go) applies at once, and still ends
   with its own state and idle after the job, so a client counts one idle
   per setting whenever it sends one. */
void take_setting(xpp::Session &s, const char *line);
xpp::inbox::Verdict during_run(const char *line);     /* what a running computation takes (ui_json.cpp) */
/* keep the line read last (read_line) for after the running command;
   refused: during_run() refused it (the command loop answers it so);
   applied: a set control_line() applied already (the loop only ends it) */
void defer_line(const char *line, bool refused, bool applied = false);
char line_kind(const char *line); /* a command's kind, menus.h XPP_KIND_* (ui_json.cpp) */
bool line_is_step(const char *line); /* a step of the user's (ui_json.cpp's command table) */
/* a script line that does not fit the dialogue: stop at once */
[[noreturn]] void script_fail(const char *what, const char *line, const char *ask);
void script_next(void); /* the script's next line, and an interruption after it */
/* A recorded interruption's `at` (the stopped event's: docs/protocol.md
   "Scripts") armed for the running job, or the next one to begin: it
   cancels itself there, or, with a key code, is handed that key (a key
   the job read itself, xpp_job.h). False for an `at` that cannot be
   placed (what "other"): nothing is armed. The script's abort lines and
   a recording's replayed steps (json_player.cpp) both arm through this. */
bool arm_recorded_stop(const char *at, int key);
/* the `at` object's text, for a message saying it was never reached */
std::string recorded_at(const char *at);
/* where the running job was when it was cancelled, the stopped event's
   `at` object (docs/protocol.md "stopped"), into b */
void buf_stopped_at(Buf *b);
/* hello, the main window and state: the first events of the model of s */
void send_hello(xpp::Session &s);

/* ---- json_record.cpp: File/recorD, a recording's steps (W59a) ---- */

/* {"cmd":"record","op":"start"|"stop"|"note",...} */
void record_command(xpp::Session &s, const char *line);
void j_record_toggle(xpp::Session &s); /* File/recorD: start, or stop and save */
bool j_recording(void);                /* a recording is in progress */
bool j_save_recording(xpp::Session &s); /* Quit's Save: the recording saved (its name asked) and ended */
/* a command handle_line is about to run: a step begins, when recording
   and the command is one (not a request of the client's own) */
void record_begin(const char *line);
/* an ask of kind (ask_begin's) answered with the line answer; ok: not cancelled */
void record_answer(const char *kind, const char *answer, bool ok);
/* a key the running job read itself (control_line: / ending a range,
   Escape stopping the animation's Go), with where the job was */
void record_key_read(const std::string &key);
/* the menu m's item of key ch picked (j_menu_choose): the step's label */
void record_menu_pick(const struct XppMenu *m, int ch);
/* a setting a running command took and applied at once (take_setting): a
   step of its own, before that command's */
void record_setting(const char *line);
/* the command ends (handle_line, before its state): its step, with where
   it stopped when cancelled */
void record_end(xpp::Session &s, bool cancelled);
/* state's "recording" member while recording */
void buf_recording(Buf *b);

/* ---- json_player.cpp: playing a recording (W59b) ---- */

/* {"cmd":"play","op":"open"|"from"|"note"|"close"|...} */
void play_command(xpp::Session &s, const char *line);
/* play start, pause, step, speed: at any moment (handle_async); false
   for any other line */
bool play_async(const char *line);
void j_play_recording(xpp::Session &s, std::string_view path); /* File/plaY recording, Open model of a .recx */
/* a recording playing pauses (a quit's question is the user's to answer) */
void player_hold(void);
/* the ms until the player acts (the command loop's and a question's wait
   for input), -1 when it waits for nothing; player_fire acts when its
   time has come (after such a wait ran out) */
int player_wait_ms(void);
void player_fire(void);
/* handle_line is about to run line: the step the player pushed begins */
void player_begin(const char *line);
/* an ask of kind (ask_begin's) was sent, the user's: the player's step
   answers it with the recording's next key or answer */
void player_asked(const char *kind);
/* the step's job ends with its recorded interruption still armed */
void player_stop_missed(void);
/* the command ends (handle_line, before its state): the player's step ends */
void player_step_end(void);
/* a command's model request was carried out: loaded, or it failed */
void player_model_switched(bool loaded);
/* the .recx, the line and the text of the step that plays; an empty
   Place when none does (j_command_place) */
xpp::Place player_place(void);
/* state's "player" member while a recording is open in the player */
void buf_player(Buf *b);

/* ---- json_silent.cpp ---- */

/* -silent's built-in script for the session s: its command lines, each
   made when its turn comes (xpp::inbox::start_generated) */
std::function<std::optional<std::string>()> silent_script(const xpp::Session &s);

/* ---- json_io.cpp: output ---- */

/* an event line being built */
struct Buf {
    std::string s;
};

/* a failed allocation ends the program (xpp::out_of_memory, xpp_mem.h):
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
void send_simple(const char *ev, const char *key = nullptr, std::string_view text = {});
void json_flush(void);
/* a data module's event line (plot_data, phase_data, marks_data, ani_data);
   the (line, n) form is the emit of auto_data, auto_settings and
   numerics_settings, whose callbacks still take a pointer and a length */
void data_emit(std::string_view line);
void data_emit(const char *line, size_t n);

/* ---- json_io.cpp: input and the JSON reader ---- */

char *read_line(xpp::inbox::From which, int wait_ms);
unsigned long read_line_seq(void); /* the sequence number of read_line()'s line */
bool read_line_refused(void);      /* whether classify() refused read_line()'s line */

const char *skip_ws(const char *p);
const char *skip_value(const char *p);
const char *js_find(const char *obj, const char *key);
/* the JSON text of the value at v (its whitespace after it left out;
   empty for NULL) */
std::string_view js_raw(const char *v);
/* text is one JSON value, strictly (RFC 8259), with nothing after it but
   whitespace: what a file holds before it goes into an event */
bool js_valid(const char *text);
/* the object at obj as JSON text without its members named in drop */
std::string js_object_without(const char *obj, std::initializer_list<std::string_view> drop);
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
void answer_point(xpp::Session &s, unsigned long win, int k, int *x, int *y);
/* the answer's points into v, x and y by turns (v.size() / 2 of them) */
int mouse_ask(xpp::Session &s, unsigned long win, const char *kind, int flag, std::span<int> v);
int ask_drag(xpp::Session &s, unsigned long win, int *x, int *y);

/* an error as an event: `ev` ("message", or "error" for a model that does
   not load) with its what and its place, the fields every error event
   carries (docs/protocol.md "Errors") */
void send_error(const char *ev, const xpp::Error &e);
/* the XppUi err_msg: e as a `message` event */
void j_err_msg(const xpp::Error &e);
/* the XppUi command_place: the recording's step that plays, else
   --script's line (xpp_ui.h command_place) */
xpp::Place j_command_place(void);
/* the front end's own error about the protocol command `command` (its
   where), at j_command_place() */
void j_command_error(std::string_view command, std::string what);
void j_ping(void);
void j_bottom_msg(int line, std::string_view msg, std::string_view chapter, std::string_view anchor);
void j_message_box(std::string_view msg);
void j_kill_message_box(void);
void j_title_text(std::string_view s);
int j_dialog(std::string_view title, std::string_view name, std::string &value, std::string_view ok,
             std::string_view cancel, int kind);
int j_new_string(std::string_view name, std::string &value, int kind);
int j_yes_no_box(void);
int j_two_choice(std::string_view c1, std::string_view c2, std::string_view q, std::string_view key,
                 std::string_view title);
void j_respond_box(std::string_view button, std::string_view message);
int j_checklist(std::string_view title, const char *const *names, int *flags, int n);
int j_string_box(int row, int col, std::string_view title, const char *const *names, std::span<std::string> values,
                 const int *kinds);
int j_file_selector(std::string_view title, std::string &file, std::string_view wild);
int j_get_mouse_xy(xpp::Session &s, int *x, int *y);
int j_rubber_band(xpp::Session &s, int *i1, int *j1, int *i2, int *j2, int flag);
int j_menu_choose(const struct XppMenu *m, int def);
void j_show_menu(int which);
void j_open_help(std::string_view chapter, std::string_view anchor);
void j_copy_text(std::string_view what, std::string_view text);
int j_check_abort(void);
int j_progress_begin(void);
void j_progress(int nit, int icount, int cwidth);
void j_q_calc(xpp::Session &s);

/* ---- json_state.cpp ---- */

void send_state(xpp::Session &s);
void send_state_if_dirty(void);
void j_state_dirty(void);
void j_state_dirty_i(int i);
void j_state_dirty_is(int i, const char *s);
void browser_rows(const xpp::Session &s, const char *line);
void browser_command(xpp::Session &s, const char *line);
void browser_key(xpp::Session &s, int ch, const char *line);
void browser_update(const xpp::Session &s); /* at a command's end */
void j_browser_changed(int i);
void j_rows_stored(xpp::Session &s, int nrows);
void plotvars_command(xpp::Session &s, const char *line);
void data_command(xpp::Session &s, const char *line);
void send_equations(const xpp::Session &s);
void j_show_eq_box(xpp::Session &s, int cp, int cm, int rp, int rm, int im, double *y, double *ev, int n);
void equilibrium_key(xpp::Session &s, int ch);
void j_make_txtview(xpp::Session &s);
void action_command(xpp::Session &s, const char *line);
void apply_set(xpp::Session &s, const char *line);
void default_command(xpp::Session &s, const char *line);
void slide_command(xpp::Session &s, const char *line);
void values_command(xpp::Session &s, const char *line);
void state_forget(void); /* what the model before showed (its last equilibrium) goes */

/* ---- json_windows.cpp ---- */

void windows_init(void);
void send_main_window(const char *title);
void send_window(const char *what, unsigned long id, int w, int h, std::string_view title = {});
void select_graph(xpp::Session &s, int i);
void click_command(xpp::Session &s, const char *line);
/* {"cmd":"display","win":N,"x":[lo,hi]|null,"y":...,"runs":bool}: the zoom shown in plot window N and whether its earlier runs are drawn (display_state.h) */
void display_command(xpp::Session &s, const char *line);
void j_get_draw_size(xpp::Session &s, unsigned int *w, unsigned int *h);
void j_blank_draw_window(xpp::Session &s);
void j_redraw_all(xpp::Session &s);
void redraw_graph(xpp::Session &s);
void j_redraw_graph(xpp::Session &s);
void j_redraw_screens(xpp::Session &s);
void j_clear_screens(xpp::Session &s);
void j_reset_graphics(xpp::Session &s);
void j_activate_graph(xpp::Session &s, int i, int flag);
void j_create_plot_window(xpp::Session &s);
void j_destroy_plot_window(xpp::Session &s);
void j_kill_plot_windows(xpp::Session &s);
void j_cput_text(xpp::Session &s);
void j_draw_freeze(xpp::Session &s);
void j_scroll_window(xpp::Session &s);
void j_new_colormap(int type);

std::vector<unsigned char> ask_pixels(int win, int film, int *w, int *h); /* empty: cancelled */
int write_ppm(const char *file, std::span<const unsigned char> rgb, int w, int h);
void web_safe_colors(std::span<unsigned char> rgb); /* at most 256 colours, for the GIF writer */

xpp::Result<> j_film_clip(xpp::Session &s);
void j_reset_film(xpp::Session &s);
void j_movie_play_back(xpp::Session &s);
void j_movie_auto_play(xpp::Session &s);
void j_movie_save(xpp::Session &s, std::string_view basename, int fmat);
void j_movie_make_anigif(xpp::Session &s);

void aplot_changed(void); /* the data behind an array plot changed */
void aplot_update(xpp::Session &s); /* at a command's end */
void aplot_command(xpp::Session &s, const char *line);
void aplot_key(xpp::Session &s, int ch);
void j_aplot_make(xpp::Session &s, std::string_view name);
void j_aplot_redraw(xpp::Session &s);
void j_aplot_draw_one(xpp::Session &s, std::string_view tag);

/* ---- json_auto.cpp ---- */

void diag_flush(const xpp::Session &s, int final);
void diag_forget(void); /* the client has no diagram */
int diag_point_of_node(int node);
void auto_command(xpp::Session &s, const char *line);
/* the `autoview` event (hidden branches and zoom), for a client that asked for autoinfo */
void auto_view_subscribe(int on);
void auto_view_update(xpp::Session &s);
void auto_key(xpp::Session &s, int ch);
void auto_redraw_for_client(xpp::Session &s);
void j_auto_make_window(xpp::Session &s, std::string_view wname, std::string_view iname);
int j_auto_check_abort(int *iflag);
int j_auto_rubber(xpp::Session &s, int *i1, int *j1, int *i2, int *j2, int flag);
int j_auto_grab_event(xpp::Session &s, int *x, int *y);
void j_auto_show_hint(xpp::Session &s);
void j_auto_scroll_window(xpp::Session &s);
void j_auto_diagram(xpp::Session &s, int view, const XppDiagPoint *p);
void j_auto_refresh(xpp::Session &s);

/* ---- json_model.cpp ---- */

/* loads req's model (File > Open model, Reload) in place of the one of
   before, and serves it: its Session, current from now on (before is
   gone); when it cannot be loaded, before, which stays */
xpp::Session &switch_model(xpp::Session &before, const xpp::ModelRequest &req);

/* ---- json_ani.cpp ---- */

int ani_speed_op(xpp::Session &s, const char *o, const char *line);
void ani_command(xpp::Session &s, const char *line);
void ani_key(xpp::Session &s, int ch);
void j_ani_slider(xpp::Session &s);
void j_new_vcr(xpp::Session &s);
void j_ani_show(void);

} // namespace xpp::json

#endif
