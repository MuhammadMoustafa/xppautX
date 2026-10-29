/* What the files of the JSON protocol front end share (C++ only, namespace
   xpp::json; ui_json.h is the public face). The front end is split by
   responsibility:

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
#pragma once

#ifndef __cplusplus
#error "ui_json_internal.h is C++ only"
#endif

#include "json_reader.h"
#include "xpp_ui.h"
#include "xpp_io.h"
#include "xpp_mem.h"
#include "xpp_inbox.h"
#include "display_state.h"
#include "grobs.h"
#include "diagram.h"
#include "data_event.h"
#include "event_streams.h"
#include "auto_data.h"
#include "auto_settings.h"
#include <stddef.h>
#include <array>
#include <atomic>
#include <climits>
#include <deque>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/* window ids the client draws into; plot windows are graph index + 1 */

/* control_line()'s answer for the animation's Pause */
#define ANI_PAUSE (-1)

/* a string literal onto an event line */
#define BUF_LIT(b, lit) xpp::json::buf_add(b, lit, sizeof(lit) - 1)

namespace xpp {
struct ModelRequest; /* model_switch.h */
namespace command { struct Command; class Arguments; } /* command_def.h */
struct Session;      /* session.h */
}

namespace xpp::json {


/* A short, bounded recent-motion diagnostic, the page reads it as hello.state_inspection.tail_intervals. */
constexpr int STATE_TAIL_INTERVALS = 10;

/* ---- ui_json.cpp ---- */

/* the session's mutable state that more than one file needs */
/* a command line kept for after the running command (defer_line) */
struct DeferredLine {
    std::string line;
    unsigned long seq;
    bool refused; /* classify() refused it during a computation */
    bool applied; /* a set applied when it was taken: the command only ends */
    /* a registry command applied when it was taken (value.set_parameters): the result its done reports */
    std::optional<xpp::Result<std::string>> taken;
};

std::string auto_view_event_line(const xpp::Session &s);
/* the `windows` event: every open window with its kind and whether it is shown (window.list's
   result), sent again when it changed; for a client that asked for it with {"cmd":"data"} */
std::string windows_event_line(const xpp::Session &s);

/* what the client holds of one view of AUTO's diagram, and how far it got
   (json_auto.cpp's comment): the list of points it was sent, its axes and
   the replay in progress. The client's, so it is positional like the
   client's own lists, not the view's: a closed view leaves the lists after
   it to be sent again. */
struct ClientDiagramView {
    std::vector<XppDiagPoint> dg; /* n of them in use */
    std::vector<std::shared_ptr<const xpp::DiagramBranch>> branches; /* immutable records behind those points */
    std::map<std::shared_ptr<const xpp::DiagramBranch>,std::map<std::shared_ptr<const xpp::DiagramBranch>,bool>> branch_matches;
    int n = 0, client = 0, dirty = 0;
    int match = 0;
    bool replay = false, axes = false;
    struct {
        double xmin, xmax, ymin, ymax;
        int x0, y0, wid, hgt;
        xpp::AutoProjection plot;
        std::string xlabel, ylabel;
    } ax{};
};

struct ProtocolSession {
    int save_replace = SAVE_ASK; /* the running command's decision, never the disk's */
    bool generated = false; /* --silent model's internal commands */
    bool silent = false; /* no interface; errors determine the process result */
    std::string output_folder; /* silent recording's launch folder, never its input folder */
    xpp::Place reading; /* the current protocol command or recording snapshot source */
    /* the main-window menu shown (MAIN_MENU, FILE_MENU, NUM_MENU), for the
       reader thread's classify(): which menu a key is an item of */
    std::atomic<int> menu{0};
    /* the lines a command's prompt or computation took that were meant for
       after it, in arrival order; the command loop runs them first */
    std::deque<DeferredLine> deferred;
    /* json_state.cpp's: the state and browser events are due, and the rows a
       run had stored when it reported last (a run that starts again reports fewer) */
    int state_dirty = 0, browser_dirty = 0;
    int rows_seen = INT_MAX;
    double live_state_last = 0; /* Display pacing only; solver steps are unchanged. */
    /* AUTO subscriptions, pacing and client view tracking */
    xpp::ChangedEvent<xpp::Session, int> autoinfo{xpp::auto_data_event_line, "sending AUTO's info strip"};
    double autoinfo_last = 0;
    xpp::ChangedEvent<xpp::Session> autosettings{xpp::auto_settings_event_line, "sending AUTO's settings"};
    xpp::ChangedEvent<xpp::Session> autoview{auto_view_event_line, "sending AUTO's view"};
    xpp::ChangedEvent<xpp::Session> windows{windows_event_line, "sending the windows"};
    std::vector<ClientDiagramView> client_diagram{1}; /* the lists the client holds, one per view */
    bool diagram_subscribed = true; /* diagrams are sent until a client explicitly opts out */
    /* the heavy data events the client subscribed to (event_streams.h's names, in its order) and whether their arrays go as float32 */
    std::array<bool, xpp::event_streams::NAMES.size()> subscribed{};
    bool subscribed_f32 = false;
    /* the line json_ui_push_open pushed, until handle_line takes it: its path came from the operating system's own dialog */
    std::mutex own_open_lock;
    std::string own_open_line;
    std::vector<std::string> client_view_names;
    int client_views = 1; /* how many of them */
    int grab_key_after = 0;
    double auto_refresh_last = 0;
};
extern ProtocolSession session;

/* The Session of the client this front end serves (the session list's,
   session.h client_session). A command's functions and the XppUi
   callbacks take their Session from their caller (handle_line chooses it
   once per command); this is for what core code reaches through the seam
   with no Session at all, the front end's own: an ask (ask_wait), a
   checkpoint (j_check_abort), the pending AUTO points before an event
   (flush_pending), and the state when it is dirty (send_state_if_dirty). */
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
void defer_line(const char *line, bool refused, bool applied = false, std::optional<xpp::Result<std::string>> taken = std::nullopt);
/* a registry command a running job takes at its next check: the animation's Pause, set_speed, faster and slower */
bool is_steering(const char *line);
char line_kind(const char *line); /* a command's kind, menus.h XPP_KIND_* (ui_json.cpp) */
bool line_is_step(const char *line); /* a step of the user's (ui_json.cpp's command table) */
/* An internal silent command that does not fit the dialogue: stop at once. */
[[noreturn]] void silent_fail(const char *what, const char *line, const char *ask);
/* A recorded interruption's `at` (the stopped event's) armed for the
   running job, or the next one to begin: it
   cancels itself there, or, with a key code, is handed that key (a key
   the job read itself, xpp_job.h). False for an `at` that cannot be
   placed (what "other"): nothing is armed. The recording's replayed
   steps (json_player.cpp) arm through this. */
bool arm_recorded_stop(const char *at, int key);
/* where the running job was when it was cancelled, the stopped event's
   `at` object (docs/protocol.md "stopped"), into b */
void buf_stopped_at(std::string &b);
/* hello, the main window and state: the first events of the model of s */
void send_hello(xpp::Session &s);

/* ---- json_record.cpp: File/recorD, a recording's steps (W59a) ---- */

/* the recording's commands (json_registry.cpp calls them) */
xpp::Result<std::string> recording_start(xpp::Session &s);
xpp::Result<std::string> recording_stop(const xpp::Session &s, const std::optional<std::string> &file, bool confirm);
xpp::Result<std::string> recording_add_note(const std::string &note);
bool j_recording(void);                /* a recording is in progress */
bool j_save_recording(xpp::Session &s); /* Quit's Save: the recording saved (its name asked) and ended */
/* a command handle_line is about to run: a step begins, when recording
   and the command is one (not a request of the client's own) */
void record_begin(const char *line);
void record_save_permission();
void record_save_decision(int decision, bool asked);
int player_save_replace(int decision);
/* Optional command/file-answer decision, validated once by the protocol. */
xpp::Result<> read_save_replace(const char *line, int &decision);
/* the running command's own decision again, forgetting a file dialog's: a
   file derived from the one asked for (a later frame) was seen by no dialog,
   so the core asks about it if it exists */
xpp::Result<> reset_save_decision();
/* an ask of kind (ask_begin's) answered with the line answer; ok: not cancelled */
void record_answer(const char *kind, const char *answer, bool ok);
/* a key the running job read itself (control_line: / ending a range,
   Escape stopping the animation's Go), with where the job was */
/* the menu m's item of key ch picked (j_menu_choose): the step's label */
/* a setting a running command took and applied at once (take_setting): a
   step of its own, before that command's */
void record_control(const char *line);
void record_setting(const char *line);
/* the command ends (handle_line, before its state): its step, with where
   it stopped when cancelled */
void record_end(xpp::Session &s, bool cancelled);
/* state's "recording" member while recording */
void buf_recording(std::string &b);

/* ---- json_player.cpp: playing a recording (W59b) ---- */

/* the player's commands (json_registry.cpp calls them); recording_control's op is play, pause, step or speed, with the speed for the last */
xpp::Result<std::string> recording_open(xpp::Session &s, const std::string &file, bool play);
xpp::Result<std::string> recording_control(const std::string &op, std::optional<double> speed);
xpp::Result<std::string> recording_seek(xpp::Session &s, long long step, std::optional<bool> play);
xpp::Result<std::string> recording_set_step_note(long long step, const std::string &note, bool confirm);
xpp::Result<std::string> recording_close();
void j_play_recording(xpp::Session &s, std::string_view path); /* File/plaY recording, Open model of a .recx */
/* a recording playing pauses (a quit's question is the user's to answer) */
void player_hold(void);
/* the ms until the player acts (the command loop's and a question's wait
   for input), -1 when it waits for nothing; player_fire acts when its
   time has come (after such a wait ran out) */
int player_wait_ms(void);
void player_fire(void);
/* handle_line is about to run line: the step the player pushed begins. True while a recording's step runs (its lines are not the client's own). */
bool player_begin(const char *line);
/* an ask of kind (ask_begin's) was sent, the user's: the player's step
   answers it with the recording's next key or answer */
void player_asked(const char *kind);
/* the step's job ends with its recorded interruption still armed */
int player_controls(xpp::Session &s);
void player_stop_missed(void);
/* the command ends (handle_line, before its state): the player's step ends */
void player_step_end(void);
/* a command's model request was carried out: loaded, or it failed */
void player_model_switched(bool loaded);
/* the .recx, the line and the text of the step that plays; an empty
   Place when none does (j_command_place) */
xpp::Place player_place(void);
bool player_finished(void); /* a silent quit must not skip any recorded input */
/* state's "player" member while a recording is open in the player */
void buf_player(std::string &b);
/* Resend the step list when a page subscribes after launch or reconnects. */
void player_data(void);

/* ---- json_silent.cpp ---- */

/* --silent's built-in script for the session s: its command lines, each
   made when its turn comes (xpp::inbox::start_generated) */
std::function<std::optional<std::string>()> silent_script(xpp::Session &s);

/* ---- json_io.cpp: output ---- */

/* a failed allocation ends the program (xpp::out_of_memory, xpp_mem.h) */

void buf_add(std::string &b, const char *s, size_t n);
/* std::format onto an event line (type-checked at compile time; the formatting
   itself is xpp::vformat_append's, compiled once in xpp_io.cpp) */
template <class... Args>
void buf_format(std::string &b, std::format_string<Args...> fmt, Args &&...args) noexcept
{
    xpp::format_append(b, fmt, std::forward<Args>(args)...);
}
void buf_str(std::string &b, const char *s); /* a JSON string ("" for NULL) */
void buf_str(std::string &b, std::string_view s);
void buf_str_array(std::string &b, const char *const *v, int n);
void buf_str_array(std::string &b, std::span<const std::string> v);
/* a JSON number, "null" when v is not finite (json_number.h): sig
   significant digits, e.g. 16 for a full double, 7 for AUTO's diagram */
void buf_num(std::string &b, double v, int sig);

void open_protocol_stdout(void); /* the protocol on the stdout of now */
void out_line(const char *s, size_t n);
/* whether an event line goes anywhere (not --silent): a state event nobody reads is not built */
bool out_wanted();
void out_flush(void);
void flush_pending(void);
void send_buf(std::string &b);
void send_simple(const char *ev, const char *key = nullptr, std::string_view text = {});
void json_flush(void);
/* a data module's event line (plot_data, phase_data, marks_data, ani_data);
   the (line, n) form is the emit of auto_data, auto_settings and
   numerics_settings, whose callbacks still take a pointer and a length */
void data_emit(std::string_view line);
void data_emit(const char *line, size_t n);

/* ---- json_io.cpp: input; the JSON reader is json_reader.h ---- */

char *read_line(xpp::inbox::From which, int wait_ms);
unsigned long read_line_seq(void); /* the sequence number of read_line()'s line */
bool read_line_refused(void);      /* whether classify() refused read_line()'s line */

/* An argument of a command that is not the kind its reader asks for: a
   number or a whole number where there is a string, an object or a
   fraction. get_num and get_int throw it inside a command (an
   ArgumentCheck is live); handle_line catches it and shows the command
   error naming the command and the argument. */
struct BadArgument {
    std::string key;      /* the argument's name ("" for an array's element) */
    std::string given;    /* its JSON text */
    std::string expected; /* "a number", "a whole number" */
};
/* the number at v (absent: def; true, false, null: 1 and 0); where a command
   array's element is read, get_num otherwise */
double js_num(const char *v, double def);
/* while one lives, the readers below refuse a wrong kind (BadArgument) on this thread */
class ArgumentCheck {
 public:
    ArgumentCheck();
    ~ArgumentCheck();
    ArgumentCheck(const ArgumentCheck &) = delete;
    ArgumentCheck &operator=(const ArgumentCheck &) = delete;

 private:
    bool before_;
};
int js_number(const char *v, double *out);
/* a JSON number at v that is a whole number an int holds, into *out; 0 for anything else */
int js_whole(const char *v, int *out);
/* key of obj, when present, as js_whole: true when absent (out untouched) or read, false when it is not a whole number */
bool get_whole(const char *obj, const char *key, int &out);
bool get_string(const char *obj, const char *key, std::string &out);
double get_num(const char *obj, const char *key, double def);
int get_int(const char *obj, const char *key, double def); /* get_num cut to an int */
int is_cmd(const char *line, const char *name);
int key_code(const char *k);

/* ---- json_prompts.cpp ---- */

int ask_begin(std::string &b, const char *kind);
int ask_wait(std::string &b, int id);
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
/* the XppUi command_place: the recording's step that plays */
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
/* the text dialog (ask kind `text`): look is the start and the answer; 0 when cancelled */
int j_text_dialog(xpp::LabelLook &look);
int j_choose(std::string_view title, std::string_view q, std::span<const std::string> choices, std::string_view keys,
             char yes_key);

int j_checklist(std::string_view title, const char *const *names, int *flags, int n);
int j_string_box(int row, int col, std::string_view title, const char *const *names, std::span<std::string> values,
                 const int *kinds);
int j_typed_form(std::string_view title, std::span<FormField> fields);
int j_grid_box(std::string_view title, std::span<const char *const> columns, std::span<const char *const> rows,
               const char *const *names, std::span<std::string> values, const int *kinds);
int j_file_selector(std::string_view title, std::string &file, std::string_view wild);
int j_get_mouse_xy(xpp::Session &s, int *x, int *y);
int j_rubber_band(xpp::Session &s, int *i1, int *j1, int *i2, int *j2, int flag);
int j_check_abort(void);
int j_progress_begin(void);
void j_progress(int nit, int icount, int cwidth);
void j_q_calc(xpp::Session &s);

/* XppUi.front_end_command (json_registry.cpp): the registry commands this front end answers itself */
xpp::Result<std::string> front_end_command(xpp::Session &s, const xpp::command::Command &command, const xpp::command::Arguments &args,
                                           const xpp::Place &place);

/* ---- json_state.cpp ---- */

void send_state(xpp::Session &s);
/* the commands the front end answers itself (json_registry.cpp calls them): the state, the model source, the event subscriptions */
xpp::Result<std::string> session_get_state(xpp::Session &s);
xpp::Result<std::string> session_get_source(xpp::Session &s);
xpp::Result<std::string> events_subscribe(xpp::Session &s, const xpp::command::Arguments &args);
xpp::Result<std::string> events_unsubscribe(xpp::Session &s, const xpp::command::Arguments &args);
void send_state_if_dirty(void);
void j_state_dirty(void);
void j_state_dirty_i(int i);
void j_state_dirty_is(int i, const char *s);
void browser_update(xpp::Session &s); /* at a command's end */
void j_browser_changed(int i);
void j_rows_stored(xpp::Session &s, int nrows);
void j_live_state(xpp::Session &s, const double *values, double time);
void j_show_eq_box(xpp::Session &s, int cp, int cm, int rp, int rm, int im, double *y, double *ev, int n);
void j_make_txtview(xpp::Session &s);
void apply_set(xpp::Session &s, const char *line);
void state_forget(void); /* what the model before showed (its last equilibrium) goes */

/* ---- json_windows.cpp ---- */

void send_main_window(const xpp::Session &s,const char *title);
void send_window(const char *what, std::string_view name, int w, int h, std::string_view title = {}, std::string_view kind = "graph");
void j_window_selected(xpp::Session &s, std::string_view name, std::string_view kind);
void j_window_renamed(xpp::Session &s, std::string_view old_name, std::string_view new_name);
void j_auto_closed(xpp::Session &s, std::string_view name);
void j_tool_window_closed(xpp::Session &s, std::string_view name, std::string_view kind);
/* {"cmd":"display","win":N,"x":[lo,hi]|null,"y":...,"runs":bool}: the zoom shown in plot window N and whether its earlier runs are drawn (display_state.h) */
void j_get_draw_size(xpp::Session &s, unsigned int *w, unsigned int *h);
void j_blank_draw_window(xpp::Session &s);
void j_redraw_all(xpp::Session &s);
void redraw_graph(xpp::Session &s);
void j_redraw_graph(xpp::Session &s);
void j_redraw_screens(xpp::Session &s);
void j_clear_screens(xpp::Session &s);
void j_reset_graphics(xpp::Session &s);
void j_create_plot_window(xpp::Session &s);
void j_destroy_plot_window(xpp::Session &s);
void j_cput_text(xpp::Session &s);
void j_scroll_window(xpp::Session &s);

/* the frames of the kinescope (film >= 0), or the animation's or array plot's window */
std::vector<unsigned char> ask_pixels(int win, int film, int *w, int *h); /* empty: cancelled */

xpp::Result<> j_film_clip(xpp::Session &s);
void j_reset_film(xpp::Session &s);
void j_movie_play_back(xpp::Session &s);
void j_movie_auto_play(xpp::Session &s);
xpp::Result<int> j_movie_save(xpp::Session &s, std::string_view basename, int fmat);
xpp::Result<> j_movie_make_anigif(xpp::Session &s, std::string_view file);

void aplot_changed(void); /* the data behind an array plot changed */
void aplot_update(xpp::Session &s); /* at a command's end */
void j_aplot_make(xpp::Session &s, std::string_view name);
void j_aplot_redraw(xpp::Session &s);
void j_aplot_draw_one(xpp::Session &s, std::string_view tag);
xpp::Result<bool> j_aplot_gif(xpp::Session &s, std::string_view file);

/* ---- json_auto.cpp ---- */

void diag_flush(const xpp::Session &s, int final);
void diag_forget(void); /* the client has no diagram */
int diag_point_of_node(const xpp::Session &s, int node); /* the index in the diagram data the client holds of AUTO's entry `node`, or -1 */
/* the `autoview` event (hidden branches and zoom), for a client that asked for autoinfo */
void auto_view_subscribe(int on);
void auto_view_update(xpp::Session &s);
void auto_redraw_for_client(xpp::Session &s);
void j_auto_make_window(xpp::Session &s, std::string_view wname, std::string_view iname);
int j_auto_check_abort(int *iflag);
int j_auto_grab_event(xpp::Session &s, int *x, int *y);
void j_auto_show_hint(xpp::Session &s);
void j_auto_diagram(xpp::Session &s, int view, const XppDiagPoint *p);
int j_auto_points_held(xpp::Session &s); /* XppUi.auto_points_held: the points the active view holds */
void j_auto_refresh(xpp::Session &s);

/* ---- json_keymap.cpp ---- */

/* the user's keymap as the page keeps it: hello's `keymap` object (xpp_keymap.h) */
std::string hello_keymap();
/* the keymap commands: the user's keymap, replaced in the parts given, or reset */
xpp::Result<std::string> keymap_get();
xpp::Result<std::string> keymap_set(const xpp::command::Arguments &args);
xpp::Result<std::string> keymap_reset();

/* ---- json_model.cpp ---- */

/* loads req's model (File > Open model, Reload) in place of the one of
   before, and serves it: its Session, current from now on (before is
   gone); when it cannot be loaded, before, which stays */
xpp::Result<xpp::Session *> switch_model(xpp::Session &before, const xpp::ModelRequest &req);

/* ---- json_ani.cpp ---- */

xpp::Result<int> j_ani_go(xpp::Session &s, const xpp::AniExport &out);
void j_ani_slider(xpp::Session &s);
void j_new_vcr(xpp::Session &s);
void j_ani_show(void);

} // namespace xpp::json

