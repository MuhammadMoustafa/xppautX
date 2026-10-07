/* The protocol front end: an XppUi table that talks line-delimited JSON.

   One JSON object per line in each direction. What the core shows goes out
   as events ("ev"), as data (series, plots, nullclines, marks, the AUTO
   diagram, animation frames: the page draws them); keys, parameter edits
   and the answers to prompts come in as commands ("cmd"). A prompt (menu,
   string box, file name, mouse pick, ...) is an "ask" event with an id; the
   core blocks until the matching {"cmd":"answer","id":N,...} arrives, the
   same way the X11 front end runs a nested event loop. See docs/protocol.md.

   This file holds the XppUi table, the command dispatch, the input
   classifier, install and hello; ui_json_internal.h names
   the files that hold the rest. */
#include "model.h"
#include <cmath>
#include "session.h"
#include "ui_json.h"
#include "json_files.h"
#include "xpp_about.h"
#include "form_ode.h"
#include "xpp_batch.h"
#include "integrate.h"
#include "nullcline.h"
#include "ui_json_internal.h"
#include "xpp_log.h"
#include "xpp_http.h"
#include "xpp_inbox.h"
#include "xpp_job.h"
#include "xpp_util.h"
#include "menus.h"
#include "mykeydef.h"
#include "userbut.h"
#include "menudrive.h"
#include "xpp_session.h"
#include "xpp_globals.h"
#include "model_switch.h"
#include "plot_data.h"
#include "phase_data.h"
#include "marks_data.h"
#include "ani_data.h"
#include "aniparse.h"
#include "auto_data.h"
#include "auto_settings.h"
#include "numerics_settings.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "load_eqn.h"
#include "graf_par.h"
#include "colormap.h"
#include "xpp_files.h"

/* the core's own globals and functions that have no header of their own */

namespace xpp::json {

ProtocolSession session;

xpp::Session &client() { return xpp::client_session(); }

/* --silent returns the logging owner's error count; an interactive
   session's errors were shown as they came, so its end is always 0. */
static int exit_code(void) { return session.silent ? xpp::log_exit_code() : 0; }

void quit_session(void)
{
    if (session.silent && xpp::log_exit_code() == 0 && !player_finished())
        j_command_error("play", "Playback ended before every recorded step and input played");
    exit(exit_code());
}

/* a client's quit command is a normal end (W112): the bye first, as the
   end of a session says it, so a front end that waits on one (browser mode's
   at_exit) knows the exit was meant; a silent error stays exit 1 without it */
void quit_command(void)
{
    if (exit_code() == 0) send_simple("bye");
    quit_session();
}

/* --silent's internal command list must match the command's dialogue. */
void silent_fail(const char *what, const char *line, const char *ask)
{
    std::string why = what;
    if (ask && ask[0]) why += xpp::format("\n  open question: {}}}", ask);
    xpp::Place at = xpp::command_place();
    if (at.source.empty()) at.source = line;
    xpp::log(XPP_LOG_ERROR, "{}\n  line: {}\n", xpp::Error{"silent commands", why, at}.text(), line);
    exit(1);
}

bool quit_waits(const char *line)
{
    return is_cmd(line, "quit") && (get_int(line, "ask", 0) != 0 || get_int(line, "save", 0) != 0);
}

/* commands that make sense at any moment, even while a prompt is open (a
   quit that asks or saves is a command of its own, in the dispatch table) */
int handle_async(xpp::Session &s, const char *line)
{
    if (is_cmd(line, "quit") && !quit_waits(line)) quit_command();
    if (is_cmd(line, "state")) {
        send_state(s);
        return 1;
    }
    if (is_cmd(line, "browser") && js_find(line, "from")) {
        browser_rows(s, line);
        return 1;
    }
    return play_async(line) ? 1 : 0;
}

/* What a running computation (xpp::job::computing(): an integration, a
   range, Sing pts, a boundary value problem, an AUTO run) takes: the one
   list (docs/protocol.md "Commands during a command"). On the reader
   thread (classify()) and at the computation's checkpoint (j_check_abort,
   for a line queued just before the computation began); it only parses
   the line and logs, allocation-free (short strings).

   - acted on at the computation's next check (Verdict::control): abort and
     quit, the stop keys (Escape, and '/', which ends a range or a shooting
     for good), and what only reads or steers a view: state, browser with
     from, ani pause/fast/slow/speed;
   - kept for its turn (Verdict::normal): an answer (the computation's own
     questions), every other command of the view kind (W95: data, a zoom,
     a window picked, a menu that only shows) and of the setting kind
     (W106: set, values, auto set, a Numerics item's key, whose dialog
     opens then): it runs after the computation's idle, in arrival order
     with the rest, so the run in progress never sees a setting and each
     line's idle comes in the order the client sent it;
   - refused (Verdict::refuse), with one log line: a command of the data
     or computation kind (a key that computes or saves, a file written):
     never run, it gets an error message, state and idle when the core
     takes it, after the computation. A client that enables its actions by
     kind, as the page does (W95), never sends one. */
xpp::inbox::Verdict during_run(const char *line)
{
    std::string c, o; /* Whole words: a longer input must not impersonate a known command. */
    get_string(line, "cmd", c);
    if (c == "abort" || c == "quit" || c == "state") return xpp::inbox::Verdict::control;
    if (c == "browser" && js_find(line, "from")) return xpp::inbox::Verdict::control;
    if (c == "play" && get_string(line, "op", o) && (o == "start" || o == "pause" || o == "step" || o == "speed"))
        return xpp::inbox::Verdict::control;
    if (c == "answer") return xpp::inbox::Verdict::normal;
    if (c == "key" && !js_find(line, "win")) {
        get_string(line, "key", o);
        int k = key_code(o.c_str());
        if (k == ESC || k == '/') return xpp::inbox::Verdict::control;
    } else if (get_string(line, "op", o) && c == "ani"
               && (o == "pause" || o == "fast" || o == "slow" || o == "speed")) {
        return xpp::inbox::Verdict::control;
    }
    const char kind = line_kind(line);
    if (kind == XPP_KIND_VIEW || kind == XPP_KIND_CONTROL || kind == XPP_KIND_SETTING) return xpp::inbox::Verdict::normal;
    xpp::log(XPP_LOG_WARN, "refused during a computation: {}{}{}\n", c.empty() ? "(no cmd)" : c.c_str(),
            o.empty() ? "" : " ", o.c_str());
    return xpp::inbox::Verdict::refuse;
}

void defer_line(const char *line, bool refused, bool applied)
{
    try {
        session.deferred.push_back({line, read_line_seq(), refused, applied});
    } catch (...) {
        xpp::out_of_memory("keeping a command");
    }
}

namespace {

/* The input classifier (xpp_inbox.h), on the reader thread: it only parses
   the line, touches xpp_job's atomics and logs a refused line.

   abort and quit go to the control queue always, and cancel the running
   job (and any not yet begun that came before them) at once: the
   computation sees xpp::job::cancelled() at its next check without reading
   input. Quit then exits when the engine takes the line.

   While a computation runs, during_run() decides: what it acts on is a
   control line, an answer or a view command waits its turn, a data or
   computation command is refused. Once it is stopping (cancelled by an abort or Escape), a line
   is for after it and is classified as below: a command sent after an
   abort runs normally.

   Otherwise, while a job runs (a command in its prompts, or finishing),
   key, set, state, browser with from, and ani pause/fast/slow/speed are
   control lines (the animation's Go acts on them between frames: Escape
   or Pause stop it, a set changes a parameter under it); a prompt takes
   control lines first, and the command loop reads both queues in
   arrival order, so a control line no job consumed runs in its turn, as
   an ordinary command. Sent while no job runs they are normal.

   Everything else is normal: a command sent while a job runs, outside a
   computation, waits for it and runs after the job's idle. */
xpp::inbox::Verdict classify(const char *line, unsigned long seq)
{
    std::string c, o; /* Whole words: a longer input must not impersonate a known command. */
    if (!get_string(line, "cmd", c))
        return xpp::job::computing() && !xpp::job::stopping() ? during_run(line) : xpp::inbox::Verdict::normal;
    /* a quit that asks (W59d), or saves (the page asked, W110), stops a
       computation, then runs as a command of its own, in its turn */
    if (c == "quit" && (get_int(line, "ask", 0) != 0 || get_int(line, "save", 0) != 0)) {
        if (xpp::job::computing() && !xpp::job::stopping()) xpp::job::cancel(seq);
        return xpp::inbox::Verdict::normal;
    }
    if (c == "abort" || c == "quit") {
        xpp::job::cancel(seq);
        return xpp::inbox::Verdict::control;
    }
    if (xpp::job::computing() && !xpp::job::stopping()) return during_run(line);
    if (!xpp::job::running()) return xpp::inbox::Verdict::normal;
    if ((c == "key" && !js_find(line, "win")) || c == "set" || c == "state") return xpp::inbox::Verdict::control;
    if (c == "browser" && js_find(line, "from")) return xpp::inbox::Verdict::control;
    if (c == "ani" && get_string(line, "op", o) && (o == "pause" || o == "fast" || o == "slow" || o == "speed"))
        return xpp::inbox::Verdict::control;
    return xpp::inbox::Verdict::normal;
}

} // namespace

void take_setting(xpp::Session &s, const char *line)
{
    const bool now = !xpp::job::computed() && is_cmd(line, "set");
    if (now) {
        record_setting(line);
        apply_set(s, line);
    }
    defer_line(line, false, now);
}

/* a control line taken by a checkpoint: every kind classify() puts there
   is acted on, none dropped. Returns ESC for abort, the code of a key,
   ANI_PAUSE for the animation's Pause, 64 otherwise. A setting: take_setting(). */
int control_line(xpp::Session &s, const char *line)
{
    std::string k;
    if (handle_async(s, line)) return KEY_NONE;
    if (is_cmd(line, "abort")) return ESC;
    if (is_cmd(line, "key")) {
        get_string(line, "key", k);
        const int code = key_code(k.c_str());
        /* what the job does with it a recording keeps, to hand it the key
           at the same point in a replay: / ends a range; Escape stops the
           animation's Go (during a computation it cancels the job: the
           step's abort) */
        if (code == '/' || (code == ESC && xpp::job::progress().what == xpp::job::Reported::frame)) record_key_read(k);
        return code;
    }
    if (line_kind(line) == XPP_KIND_SETTING) {
        take_setting(s, line);
        return KEY_NONE;
    }
    if (is_cmd(line, "ani")) {
        get_string(line, "op", k);
        record_control(line);
        if (k == "pause") return ANI_PAUSE;
        ani_speed_op(s, k.c_str(), line);
    }
    return KEY_NONE;
}

/* where the running job was when it was cancelled, from what the
   computation reported last (xpp_job.h): the stopped event's `at`, and a
   recorded step's `abort` (json_record.cpp) */
void buf_stopped_at(Buf *b)
{
    xpp::job::Progress p = xpp::job::progress();
    if (p.what == xpp::job::Reported::rows) {
        /* t is a stored single-precision number: 9 digits read back exactly */
        buf_format(b, "{{\"what\":\"integrate\",\"rows\":{:d},\"t\":", p.rows);
        buf_num(b, p.t, 9);
        BUF_LIT(b, "}");
    } else if (p.what == xpp::job::Reported::point) {
        buf_format(b, "{{\"what\":\"auto\",\"branch\":{:d},\"point\":{:d}}}", p.branch, p.point);
    } else if (p.what == xpp::job::Reported::frame) {
        buf_format(b, "{{\"what\":\"ani\",\"frame\":{:d}}}", p.frame);
    } else {
        BUF_LIT(b, "{\"what\":\"other\"}");
    }
}

namespace {

/* {"ev":"stopped","at":AT}: where the running job was when it was
   cancelled (docs/protocol.md "stopped"). A script replays the
   interruption from AT. */
void send_stopped(void)
{
    Buf b;
    BUF_LIT(&b, "{\"ev\":\"stopped\",\"at\":");
    buf_stopped_at(&b);
    BUF_LIT(&b, "}");
    send_buf(&b);
}

} // namespace

bool arm_recorded_stop(const char *at, int key)
{
    std::string what;
    get_string(at, "what", what);
    if (what == "integrate")
        xpp::job::stop_at_rows(static_cast<long>(get_num(at, "rows", -1)));
    else if (what == "auto")
        xpp::job::stop_at_point(get_int(at, "branch", -1), get_int(at, "point", -1));
    else if (what == "ani")
        xpp::job::stop_at_frame(get_int(at, "frame", -1));
    else
        return false; /* an interruption of anything else cannot be placed: the job runs on */
    xpp::job::stop_with_key(key);
    return true;
}

namespace {

void j_exit_program(void)
{
    quit_command();
}

void j_void(void) {}
void j_int(int) {}

/* the running command's first computation began (xpp_job.h): the client
   disables what it may not do until the command's idle */
void send_computing(void) { send_simple("computing"); }

/* the JSON front end's table: assignments, so C++17 needs no designated
   initializers; fields not set stay null, as in the C initializer */
XppUi make_json_ui(void)
{
    XppUi u{};
    u.err_msg = j_err_msg;
    u.command_place = j_command_place;
    u.save_replace = [](bool independent) -> int {
        if (independent && !xpp::ok_or_show(reset_save_decision())) return SAVE_DECLINE;
        record_save_permission();
        const int decision = player_save_replace(session.save_replace);
        return session.silent && !session.generated && decision == SAVE_ASK ? SAVE_REPLACE : decision;
    };
    u.save_decision = record_save_decision;
    u.save_result = [](std::string_view file, bool saved) {
        Buf b;
        buf_format(&b, "{{\"ev\":\"saved\",\"saved\":{},\"file\":", saved ? "true" : "false");
        buf_str(&b, std::string(file).c_str());
        BUF_LIT(&b, "}");
        send_buf(&b);
    };
    u.ping = j_ping;
    u.bottom_msg = j_bottom_msg;
    u.message_box = j_message_box;
    u.kill_message_box = j_kill_message_box;
    u.title_text = j_title_text;
    u.new_string = j_new_string;
    u.yes_no_box = j_yes_no_box;
    u.two_choice = j_two_choice;
    u.respond_box = j_respond_box;
    u.checklist = j_checklist;
    u.string_box = j_string_box;
    u.file_selector = j_file_selector;
    u.dialog = j_dialog;
    u.get_mouse_xy = j_get_mouse_xy;
    u.menu_flash = j_int;
    u.show_menu = j_show_menu;
    u.redraw_menu = j_void;
    u.menu_choose = j_menu_choose;
    u.check_abort = j_check_abort;
    u.progress_begin = j_progress_begin;
    u.progress = j_progress;
    u.flush = json_flush;
    u.redraw_params = j_state_dirty;
    u.param_box_set = j_state_dirty_is;
    u.param_box_redraw = j_state_dirty_i;
    u.ic_box_set = j_state_dirty_is;
    u.ic_box_redraw = j_state_dirty_i;
    u.redraw_ics = j_state_dirty;
    u.redraw_all = j_redraw_all;
    u.redraw_bcs = j_state_dirty;
    u.redraw_delays = j_state_dirty;
    u.redraw_graph = j_redraw_graph;
    u.redraw_screens = j_redraw_screens;
    u.clear_screens = j_clear_screens;
    u.clear_draw_window = [](xpp::Session &s) { clr_scrn(s); };
    u.reset_graphics = j_reset_graphics;
    u.data_changed = j_browser_changed;
    u.rows_stored = j_rows_stored;
    u.live_state = j_live_state;
    u.browser_redraw = j_browser_changed;
    u.activate_graph = j_activate_graph;
    u.create_plot_window = j_create_plot_window;
    u.destroy_plot_window = j_destroy_plot_window;
    u.kill_plot_windows = j_kill_plot_windows;
    u.lower_plot_window = j_void;
    u.gr_col = j_void;
    u.base_col = j_void;
    u.cput_text = j_cput_text;
    u.get_draw_size = j_get_draw_size;
    u.draw_freeze = j_draw_freeze;
    u.blank_draw_window = j_blank_draw_window;
    u.small_base = j_void;
    u.small_gr = j_void;
    u.film_clip = j_film_clip;
    u.reset_film = j_reset_film;
    u.movie_play_back = j_movie_play_back;
    u.movie_auto_play = j_movie_auto_play;
    u.movie_save = j_movie_save;
    u.movie_make_anigif = j_movie_make_anigif;
    u.rubber_band = j_rubber_band;
    u.scroll_window = j_scroll_window;
    u.new_colormap = j_new_colormap;
    u.aplot_make = j_aplot_make;
    u.aplot_redraw = j_aplot_redraw;
    u.aplot_reset_axes = j_aplot_redraw;
    u.aplot_draw_one = j_aplot_draw_one;
    u.auto_make_window = j_auto_make_window;
    u.auto_redraw_menus = j_void;
    u.auto_refresh = j_auto_refresh;
    u.auto_check_abort = j_auto_check_abort;
    u.auto_rubber = j_auto_rubber;
    u.auto_scroll_window = j_auto_scroll_window;
    u.auto_grab_event = j_auto_grab_event;
    u.auto_show_hint = j_auto_show_hint;
    u.auto_diagram = j_auto_diagram;
    u.new_vcr = j_new_vcr;
    u.ani_show = j_ani_show;
    u.ani_slider = j_ani_slider;
    u.init_txtview = j_void;
    u.show_eq_box = j_show_eq_box;
    u.make_txtview = j_make_txtview;
    u.q_calc = j_q_calc;
    u.open_help = j_open_help;
    u.copy_text = j_copy_text;
    u.record_toggle = j_record_toggle;
    u.play_recording = j_play_recording;
    u.recording = j_recording;
    u.save_recording = j_save_recording;
    u.exit_program = j_exit_program;
    return u;
}

const XppUi json_ui = make_json_ui();

/* a key of a window's own layer (menus.h): {"cmd":"key","win":...,"key":k},
   the AUTO, data browser, animation, array plot or equilibrium window; the
   browser's takes the selected "row" too */
void window_key(xpp::Session &s, const std::string &win, int ch, const char *line)
{
    if (win == "auto") auto_key(s, ch);
    else if (win == "browser") browser_key(s, ch, line);
    else if (win == "ani") ani_key(s, ch);
    else if (win == "aplot") aplot_key(s, ch);
    else if (win == "equilibrium") equilibrium_key(s, ch);
    else j_command_error("key", xpp::format("No key layer for the window {}", win));
}

void key_command(xpp::Session &s, const char *line)
{
    std::string k, win, menu, item;
    get_string(line, "key", k);
    if (js_find(line, "menu") || js_find(line, "item")) {
        get_string(line, "menu", menu);
        get_string(line, "item", item);
        const auto action = main_menu_action(menu, item);
        if (!action || js_find(line, "win") || js_find(line, "key")) {
            j_command_error("key", "A menu action needs a known menu and item, without key or win");
            return;
        }
        show_main_menu(s, action->which);
        commander(s, action->key);
        show_main_menu(s, MAIN_MENU);
    }
    else if (get_string(line, "win", win)) window_key(s, win, key_code(k.c_str()), line);
    else commander(s, key_code(k.c_str()));
}

void session_command(xpp::Session &s, const char *line)
{
    std::string o, name;
    get_string(line, "op", o);
    get_string(line, "name", name);
    /* data: in (1), left out (0), or asked above 50 MB (absent) */
    const char *jd = js_find(line, "data");
    if (o == "save") xpp_session_save(s, name.empty() ? nullptr : name.c_str(), jd ? (js_num(jd, 1) != 0) : -1);
    else if (o == "load") xpp_session_load(s, name.empty() ? nullptr : name.c_str());
}

/* the model's folder for a client that cannot reach it (xpp_files.h) */
void file_command(xpp::Session &, const char *line)
{
    std::string o;
    get_string(line, "op", o);
    try {
        auto result = xpp::files::command(o, js_find(line, "name"), js_find(line, "data"));
        if (result.error) {
            /* A replayed command belongs to its recording step; name still identifies the requested file. */
            const xpp::Place step = player_place();
            if (!step.file.empty()) result.error->place = step;
            else if (result.error->place.file.empty()) result.error->place = xpp::command_place();
            if (session.silent) {
                xpp::show_error(*result.error);
                return;
            }
            xpp::log_note_error();
        }
        data_emit(file_event(o, result));
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("in a file command");
    }
}

/* {"cmd":"dfield"|"equilibrium","op":"write","name":...} */
void write_command(xpp::Session &s, const char *line)
{
    std::string o, name;
    get_string(line, "op", o);
    get_string(line, "name", name);
    if (o != "write" || name.empty()) j_command_error("write", "dfield and equilibrium write to a file: op write and a name");
    else if (is_cmd(line, "dfield")) write_dfield(s,name);
    else write_equilibrium(s,name.c_str(), get_int(line, "shoot", 0));
}

/* The protocol's commands (docs/protocol.md "Commands"): the one table of
   what handle_line runs and of each command's kind (menus.h XPP_KIND_*,
   W95), which hello sends and during_run() judges by. An entry with an op
   is for lines with that "op"; the entry after it with none for the
   command's other lines. A key's kind is its menu item's (0 here). A step
   is the user's own action, what a recording records and the page keeps as
   the command a missing file runs again (hello.commands' "step"); the rest
   are the client's own requests (state, the data events, the browser's
   paging, the model's folder), the answers and aborts that belong to the
   step they come in, and the recording's and player's own. */
constexpr bool STEP = true, NOT_STEP = false;

struct CommandInfo {
    const char *cmd;
    const char *op;
    char kind;
    bool step;
    void (*run)(xpp::Session &s, const char *line);
};

constexpr char C = XPP_KIND_CONTROL, V = XPP_KIND_VIEW, S = XPP_KIND_SETTING, D = XPP_KIND_DATA, X = XPP_KIND_COMPUTE;

const CommandInfo commands[] = {
    {"continue", nullptr, X, STEP, [](xpp::Session &s, const char *line) {
        double value;
        const bool extra=js_find(line,"extra")!=nullptr, until=js_find(line,"until")!=nullptr;
        if(extra==until||!js_number(js_find(line,extra?"extra":"until"),&value)||(extra&&value<=0)){
            j_command_error("continue","Needs exactly one numeric end time or positive extra duration");
            return;
        }
        show_main_menu(s, MAIN_MENU);
        xpp::ok_or_show(xpp::continue_to(s,extra?s.data_store.current_time+value:value));
    }},
    {"steady", nullptr, X, STEP, [](xpp::Session &s, const char *line) {
        double decimals, hold, maximum;
        if (!js_number(js_find(line,"decimals"),&decimals) || !std::isfinite(decimals) || decimals<0 || decimals>xpp::MAX_STEADY_DECIMALS || decimals!=static_cast<int>(decimals) ||
            !js_number(js_find(line,"hold"),&hold) || !js_number(js_find(line,"maximum"),&maximum)) {
            j_command_error("steady","Needs integer decimal places, numeric hold and maximum durations");
            return;
        }
        show_main_menu(s, MAIN_MENU);
        xpp::ok_or_show(xpp::run_to_steady_state(s,{static_cast<int>(decimals),hold,maximum}));
    }},
    {"key", nullptr, 0, STEP, key_command},
    {"answer", nullptr, C, NOT_STEP,
     [](xpp::Session &, const char *line) {
         /* Reaching the main dispatch means no ask was pending. */
         if (session.generated) silent_fail("answers a question that was never asked", line, NULL);
         else if (!player_place().file.empty()) j_command_error("play", "Answers a question that was never asked");
     }},
    {"abort", nullptr, C, NOT_STEP, [](xpp::Session &, const char *) {}},
    {"quit", nullptr, C, NOT_STEP,
     [](xpp::Session &s, const char *line) {
         if (!quit_waits(line)) quit_command();
         /* the user's quit (W59d): File/Quit's question, or its Save
            session answered in the page (W110); a recording playing
            waits, the question being the user's */
         player_hold();
         xpp_quit(s, get_int(line, "save", 0) != 0);
     }},
    {"state", nullptr, V, NOT_STEP, [](xpp::Session &s, const char *) { send_state(s); }},
    {"data", nullptr, V, NOT_STEP, data_command},
    {"equations", nullptr, V, NOT_STEP, [](xpp::Session &s, const char *) { send_equations(s); }},
    {"click", nullptr, V, STEP, click_command},
    {"display", nullptr, V, STEP, display_command},
    {"redraw", nullptr, V, NOT_STEP,
     [](xpp::Session &s, const char *) {
         redraw_graph(s);
         auto_redraw_for_client(s);
     }},
    {"plotvars", nullptr, V, STEP, plotvars_command},
    {"aplot", nullptr, V, STEP, aplot_command},
    {"ani", "mouse", X, STEP, ani_command}, /* release can integrate */
    {"ani", nullptr, V, STEP, ani_command},
    {"browser", "write", D, STEP, browser_command},
    {"browser", "load", D, STEP, browser_command},
    {"browser", "postprocess", X, STEP, browser_command},
    {"browser", nullptr, V, NOT_STEP, browser_command}, /* with from: the block shown */
    {"auto", "set", S, STEP, auto_command},
    {"auto", "grab", D, STEP, auto_command},
    {"auto", nullptr, V, STEP, auto_command}, /* display, point, close */
    {"file", "put", D, NOT_STEP, file_command},
    {"file", nullptr, V, NOT_STEP, file_command}, /* list, get */
    {"set", nullptr, S, STEP, apply_set},
    {"default", nullptr, S, STEP, default_command},
    {"slide", nullptr, S, STEP, slide_command},
    {"slider", nullptr, S, STEP, slider_command},
    {"action", nullptr, D, STEP, action_command},
    {"values", "write", D, STEP, values_command},
    {"values", "query", D, STEP, values_command},
    {"values", nullptr, S, STEP, values_command}, /* read, internset: values set */
    {"session", nullptr, D, STEP, session_command},
    {"dfield", nullptr, D, STEP, write_command},
    {"open", nullptr, D, STEP,
     [](xpp::Session &s, const char *line) {
         std::string file;
         get_string(line, "file", file);
         xpp_model_open(s, file.c_str());
     }},
    {"reload", nullptr, D, STEP, [](xpp::Session &s, const char *) { xpp_model_reload(s); }},
    {"record", "note", C, NOT_STEP, record_command},
    {"record", nullptr, D, NOT_STEP, record_command}, /* start, stop */
    {"play", "start", C, NOT_STEP, play_command},
    {"play", "pause", C, NOT_STEP, play_command},
    {"play", "step", C, NOT_STEP, play_command},
    {"play", "speed", C, NOT_STEP, play_command},
    {"play", "close", C, NOT_STEP, play_command},
    {"play", nullptr, D, NOT_STEP, play_command}, /* open, from, note */
    {"equilibrium", nullptr, X, STEP, write_command},
    {"userbut", nullptr, X, STEP,
     [](xpp::Session &s, const char *line) {
         int i = get_int(line, "index", -1);
         if (i >= 0 && i < s.nuserbut) run_the_commands(s, s.userbut[i].com);
     }},
};

/* the table's entry for `line`, NULL for an unknown command; allocation-free */
const CommandInfo *command_of(const char *line)
{
    std::string c, o; /* at most 15 bytes */
    if (!get_string(line, "cmd", c)) return nullptr;
    get_string(line, "op", o);
    for (const CommandInfo &e : commands)
        if (c == e.cmd && (!e.op || o == e.op)) return &e;
    return nullptr;
}

/* hello's "commands": the table, a key's kind left to its menu */
void buf_commands(Buf *b)
{
    BUF_LIT(b, ",\"commands\":[");
    bool first = true;
    for (const CommandInfo &e : commands) {
        if (!first) BUF_LIT(b, ",");
        first = false;
        BUF_LIT(b, "{\"cmd\":");
        buf_str(b, e.cmd);
        if (e.op) {
            BUF_LIT(b, ",\"op\":");
            buf_str(b, e.op);
        }
        /* a key's kind is its menu item's: "" here */
        if (e.kind) buf_format(b, ",\"kind\":\"{}\"", e.kind);
        else BUF_LIT(b, ",\"kind\":\"\"");
        buf_format(b, ",\"step\":{}}}", e.step);
    }
    BUF_LIT(b, "]");
}

} // namespace

bool line_is_step(const char *line)
{
    const CommandInfo *e = command_of(line);
    return e && e->step;
}

char line_kind(const char *line)
{
    const CommandInfo *e = command_of(line);
    if (!e) return 0;
    if (e->run == ani_command && e->kind == XPP_KIND_COMPUTE) {
        std::string what;
        get_string(line, "what", what);
        return what == "up" && session.grab_computes.load(std::memory_order_relaxed)
            ? XPP_KIND_COMPUTE : XPP_KIND_VIEW;
    }
    if (e->kind) return e->kind;
    std::string k, win, menu, item; /* a key: its menu item's kind */
    if (js_find(line, "menu") || js_find(line, "item")) {
        get_string(line, "menu", menu);
        get_string(line, "item", item);
        const auto action = main_menu_action(menu, item);
        if (!action || js_find(line, "win") || js_find(line, "key")) return 0;
        return main_menu_kind(action->which, action->key);
    }
    get_string(line, "key", k);
    const int ch = key_code(k.c_str());
    if (!get_string(line, "win", win)) return main_menu_kind(session.menu.load(std::memory_order_relaxed), ch);
    const XppWindowLayer *l = window_layer(win);
    return l ? menu_kind(l->menu, ch) : 0;
}

xpp::Result<> reset_save_decision()
{
    session.save_replace = SAVE_ASK;
    return read_save_replace(session.reading.source.c_str(), session.save_replace);
}

namespace {

/* one command, run as a job (xpp_job.h) numbered by its line's sequence
   number: an abort cancels it from the reader thread. A line refused
   during a computation (during_run) only says so, and ends as any does;
   a set applied already, under a job that computed nothing
   (control_line), only ends. The Session the command ended in: the one
   it ran in, or the model's it loaded in that one's place. */
xpp::Session &handle_line(const char *line, unsigned long seq, bool refused, bool applied = false)
{
    xpp::job::begin(seq);
    player_begin(line);
    /* A live command is a one-line source; recordings retain their step's
       own Place through player_place. This also locates bad save names. */
    session.reading=xpp::Place{"command",1,0,line};
    const xpp::Result<> permission = reset_save_decision();
    /* the session this command runs in, the client's in the session list
       (session.h): chosen here, once, and passed down (W47d); only a model
       loaded in its place below replaces it */
    xpp::Session *s = &xpp::client_session();
    std::string input_error;
    if (get_string(line, "input_error", input_error)) {
        j_command_error("input", input_error);
    } else if (!permission) {
        xpp::show_error(permission.error());
    } else if (applied) {
    } else if (refused) {
        std::string c;
        get_string(line, "cmd", c);
        j_command_error("command", xpp::format("{}: {} was refused", xpp::job::REFUSED_WHILE_COMPUTING, c));
    } else if (handle_async(*s, line)) {
    } else if (const CommandInfo *e = command_of(line)) {
        record_begin(line);
        e->run(*s, line);
    } else {
        std::string c;
        if (get_string(line, "cmd", c)) j_command_error("command", xpp::format("Unknown command {}", c));
    }
    /* File > Open model or Reload asked for another model: loaded now,
       when nothing of this one's Session is in use any more */
    if (std::optional<xpp::ModelRequest> req = xpp::take_model_request(*s)) {
        /* Every recorded model switch keeps silent outputs in the launch folder. */
        if (session.silent && !session.generated) req->dir = session.output_folder;
        xpp::Session *before = s;
        s = &switch_model(*s, *req);
        player_model_switched(s != before);
    }
    aplot_update(*s);
    browser_update(*s);
    plot_data_update(*s);
    phase_data_update(*s);
    marks_data_update(*s);
    ani_data_update(*s);
    session.grab_computes.store(s->animation.grab_flag && run_now_grab(*s), std::memory_order_relaxed);
    diag_flush(*s, 1);
    auto_data_update(*s, 1);
    auto_view_update(*s);
    auto_settings_update(*s);
    numerics_settings_update(*s);
    json_flush();
    /* a cancelled job says where it stopped; a replayed one must have
       stopped where the recorded session did */
    if (xpp::job::cancelled()) send_stopped();
    if (xpp::job::stop_armed()) player_stop_missed();
    record_end(*s, xpp::job::cancelled());
    player_step_end();
    /* the command is finished; the client may send the next one */
    xpp::job::end();
    send_state(*s);
    send_simple("idle");
    /* Make the next command of a silent model's internal list. */
    if (session.generated) xpp::inbox::generated_advance();
    return *s;
}

} // namespace

} // namespace xpp::json

/* ---- ui_json.h ---- */

namespace xpp {

using namespace json;

xpp::Session &json_ui_handle(const char *line) { return handle_line(line, 0, false); }

void json_ui_loop(void)
{
    DeferredLine next;
    for (;;) {
        /* what the last command's prompts or computation kept for after
           it comes first (defer_line), then the input in arrival order;
           a copy: the command's own prompts read further lines */
        if (!session.deferred.empty()) {
            next = std::move(session.deferred.front());
            session.deferred.pop_front();
        } else {
            /* a recording playing acts when its time comes (json_player.cpp) */
            char *line = read_line(xpp::inbox::From::arrival, player_wait_ms());
            if (!line) {
                player_fire();
                continue;
            }
            try {
                next.line = line;
            } catch (...) {
                xpp::out_of_memory("taking a command");
            }
            next.seq = read_line_seq();
            next.refused = read_line_refused();
            next.applied = false;
        }
        /* an abort did its work when it arrived (classify()): it is no
           command of its own, and gets no state or idle; in a script,
           where nothing ran for it to stop, the next line follows */
        if (!is_cmd(next.line.c_str(), "abort")) handle_line(next.line.c_str(), next.seq, next.refused, next.applied);
        else if (session.generated) xpp::inbox::generated_advance();
    }
}

void json_ui_push_open(const char *path)
{
    /* on the window's thread, called from C (Win32, GTK): nothing may throw */
    try {
        Buf b;
        BUF_LIT(&b, "{\"cmd\":\"open\",\"file\":");
        buf_str(&b, path);
        BUF_LIT(&b, "}");
        xpp::inbox::push(b.s);
    } catch (...) {
        xpp::out_of_memory("opening a model");
    }
}

namespace {

/* the front end: the XppUi table, the data modules' output, the input
   classifier, and the protocol on stdio unless silent (--silent: its
   events go nowhere, and what the core says goes to the log, as with no
   interface at all: xpp_ui.cpp's headless messages) */
void install(bool silent)
{
    if (!silent && !xpp::http::active()) { /* browser mode has taken stdout and stderr */
        open_protocol_stdout();
        if (!xpp::inbox::start_stdin()) {
            xpp::log(XPP_LOG_ERROR, "xppautX: cannot start the input thread\n");
            exit(1);
        }
    }
    windows_init();
    plot_data_init(data_emit);
    phase_data_init(data_emit);
    marks_data_init(data_emit);
    ani_data_init(data_emit);
    xpp::auto_data_init(data_emit, diag_point_of_node);
    xpp::auto_settings_init(data_emit);
    xpp::numerics_settings_init(data_emit);
    xpp::inbox::set_classifier(classify);
    xpp::job::set_compute_hook(send_computing);
    XppUi ui = json_ui;
    if (silent) {
        /* NULL keeps the headless entry (set_ui) */
        ui.err_msg = nullptr;
        ui.respond_box = nullptr;
        ui.copy_text = nullptr;
    }
    set_ui(&ui);
}

} // namespace

void json_ui_install(bool silent)
{
    session.silent = silent;
    if (silent) session.output_folder = xpp::files::working_dir();
    install(silent);
}
void json_ui_terminal_auto(bool automatic) { session.terminal_auto = automatic; }

int json_ui_silent(int argc, char **argv)
{
    /* the model loads as with no interface at all: a model that does not
       load exits 1, its reason logged */
    const xpp::Loaded loaded = xpp::load_model(argc, argv, 1);
    if (!loaded) exit(1);
    xpp::Session &s = **loaded;
    xpp::batch_start(s);
    session.generated = true;
    session.silent = true;
    xpp::inbox::start_generated(silent_script(s));
    install(true);
    xpp::inbox::generated_advance(); /* its first line */
    json_ui_loop(); /* exits when the script ends */
    return 0;
}

void json_ui_load_error(const xpp::Error &e)
{
    /* a script whose model does not load fails, as an error message does */
    xpp::json::send_error("error", e);
}

} // namespace xpp

namespace xpp::json {

/* the first events a client sees, and again for a model loaded in place
   of the one before (json_model.cpp) */
void send_hello(xpp::Session &s)
{
    const xpp::Model &m = s.model();
    Buf b;
    int i;
    const std::string file = xpp::model_title(m);
    const std::string title_text =
        file.size() < 60
            ? xpp::format("XPP Ver {:g}.{:g} >> {}", program.version_major, program.version_minor, file)
            : xpp::format("XPP Version {:g}.{:g}", program.version_major, program.version_minor);
    const char *title = title_text.c_str();
    BUF_LIT(&b, "{\"ev\":\"hello\",\"protocol\":" JSON_UI_STR(JSON_UI_PROTOCOL) ",\"features\":[\"series\",\"plots\",\"nullclines\",\"dfield\",\"marks\",\"ani\",\"autoinfo\",\"autosettings\",\"numerics\",\"player\"],\"title\":");
    buf_str(&b, title);
    BUF_LIT(&b, ",\"file\":");
    buf_str(&b, m.this_file);
    buf_format(&b, ",\"steady\":{{\"max_decimals\":{},\"default_decimals\":{},\"default_hold\":{}}}",
        xpp::MAX_STEADY_DECIMALS,xpp::DEFAULT_STEADY_DECIMALS,xpp::DEFAULT_STEADY_HOLD);
    buf_format(&b, ",\"continue\":{{\"grid_tolerance\":{}}}", xpp::CONTINUE_GRID_TOLERANCE);
    buf_format(&b, ",\"state_inspection\":{{\"tail_intervals\":{}}}", STATE_TAIL_INTERVALS);
    BUF_LIT(&b, ",\"output_names\":{");
    static constexpr struct { std::string_view key, ext, what; } names[]={
        {"par",".par",""},{"ic",".ic",""},{"csv",".csv",""},{"curves",".csv","curves"}};
    for (std::size_t n=0;n<std::size(names);++n){
        if(n)BUF_LIT(&b,",");
        buf_str(&b,names[n].key); BUF_LIT(&b,":");
        buf_str(&b,xpp::files::output_name(m.this_file,names[n].ext,names[n].what));
    }
    BUF_LIT(&b,"}");
    BUF_LIT(&b, ",\"about\":");
    buf_str(&b, xpp_about_text());
    /* File > Quit's question, as the core asks it (model_switch.h): the
       page asks the same itself while a computation runs (W110) */
    BUF_LIT(&b, ",\"quit\":{\"question\":");
    buf_str(&b, xpp::quit_question(false));
    BUF_LIT(&b, ",\"recording\":");
    buf_str(&b, xpp::quit_question(true));
    BUF_LIT(&b, ",\"choices\":[");
    buf_str(&b, xpp::LEAVE_SAVE);
    BUF_LIT(&b, ",");
    buf_str(&b, xpp::LEAVE_DONT_SAVE);
    BUF_LIT(&b, "],\"keys\":");
    buf_str(&b, xpp::LEAVE_KEYS);
    BUF_LIT(&b, "}");
    BUF_LIT(&b, ",\"menus\":{\"main\":");
    buf_str_array(&b, main_menu + 1, MAIN_ENTRIES); /* [0] is the title */
    BUF_LIT(&b, ",\"main_keys\":");
    buf_str(&b, main_menu_keys);
    BUF_LIT(&b, ",\"main_hints\":");
    buf_str_array(&b, main_hint, MAIN_ENTRIES);
    BUF_LIT(&b, ",\"file\":");
    buf_str_array(&b, file_menu + 1, FILE_ENTRIES); /* [0] is the title */
    BUF_LIT(&b, ",\"file_keys\":");
    buf_str(&b, file_menu_keys);
    BUF_LIT(&b, ",\"file_hints\":");
    buf_str_array(&b, file_hint, FILE_ENTRIES);
    BUF_LIT(&b, ",\"num\":");
    buf_str_array(&b, num_menu + 1, NUM_ENTRIES); /* [0] is the title */
    BUF_LIT(&b, ",\"num_keys\":");
    buf_str(&b, num_menu_keys);
    BUF_LIT(&b, ",\"num_hints\":");
    buf_str_array(&b, num_hint, NUM_ENTRIES);
    /* each item's kind (W95, menus.h), parallel to the keys */
    BUF_LIT(&b, ",\"main_kinds\":");
    buf_str(&b, main_menu_kinds);
    BUF_LIT(&b, ",\"file_kinds\":");
    buf_str(&b, file_menu_kinds);
    BUF_LIT(&b, ",\"num_kinds\":");
    buf_str(&b, num_menu_kinds);
    /* the page's names: of each menu, by state's menu number, and of each
       item, parallel to the keys (what the page looks a key up by) */
    BUF_LIT(&b, ",\"names\":");
    buf_str_array(&b, main_menu_names, 3);
    BUF_LIT(&b, ",\"main_ids\":");
    buf_str_array(&b, main_menu_ids, MAIN_ENTRIES);
    BUF_LIT(&b, ",\"file_ids\":");
    buf_str_array(&b, file_menu_ids, FILE_ENTRIES);
    BUF_LIT(&b, ",\"num_ids\":");
    buf_str_array(&b, num_menu_ids, NUM_ENTRIES);
    BUF_LIT(&b, "}");
    /* the limits the page keeps to and the windows' numbers in `window`
       events (plot windows are 1 to plots) */
    buf_format(&b, ",\"limits\":{{\"upload\":{},\"browser_rows\":{},\"browser_cols\":{}}}", XPP_FILES_CAP,
               BROWSER_MAX_ROWS, BROWSER_MAX_COLS);
    BUF_LIT(&b, ",\"upload_error\":");
    buf_str(&b, xpp::files::status_text(XPP_FILES_TOO_LARGE));
    buf_format(&b, ",\"player_speed\":{{\"min\":{},\"max\":{}}}", PLAYER_SPEED_MIN, PLAYER_SPEED_MAX);
    buf_format(&b, ",\"window_ids\":{{\"plots\":{},\"auto\":{},\"ani\":{},\"aplot\":{}}}", MAXPOP, WIN_AUTO,
               WIN_ANI, WIN_APLOT);
    /* the windows' key layers (menus.h window_layers) and the other
       commands' kinds: what the page enables while a computation runs */
    BUF_LIT(&b, ",\"windows\":{");
    for (i = 0; i < XPP_WINDOW_LAYERS; i++) {
        const XppWindowLayer &l = window_layers[i];
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, l.win);
        BUF_LIT(&b, ":{\"items\":");
        buf_str_array(&b, l.menu->items, l.menu->n);
        BUF_LIT(&b, ",\"keys\":");
        buf_str(&b, l.menu->keys);
        BUF_LIT(&b, ",\"kinds\":");
        buf_str(&b, l.menu->kinds);
        BUF_LIT(&b, ",\"ids\":");
        buf_str_array(&b, l.ids, l.menu->n);
        BUF_LIT(&b, ",\"hints\":");
        buf_str_array(&b, l.menu->hints, l.menu->n);
        BUF_LIT(&b, "}");
    }
    BUF_LIT(&b, "}");
    buf_commands(&b);
    /* the lists a form field *n picks from (pop_list.c make_scrbox_lists) */
    BUF_LIT(&b, ",\"lists\":[[\"T\"");
    for (i = 0; i < m.neq; i++) {
        BUF_LIT(&b, ",");
        buf_str(&b, m.uvar_names[i]);
    }
    BUF_LIT(&b, "],[");
    for (i = 0; i < m.node + m.nmarkov; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, m.uvar_names[i]);
    }
    BUF_LIT(&b, "],[");
    for (i = 0; i < m.nupar; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, m.upar_names[i]);
    }
    BUF_LIT(&b, "],[");
    for (i = 0; i < m.node + m.nmarkov + m.nupar; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, i < m.node + m.nmarkov ? m.uvar_names[i] : m.upar_names[i - m.node - m.nmarkov]);
    }
    BUF_LIT(&b, "],[");
    for (i = 0; i <= xpp::LAST_PLOT_COLOR; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, xpp::format("{} {}", i, color_names[i]).c_str());
    }
    BUF_LIT(&b, "],[\"2 Box\",\"3 Diamond\",\"4 Triangle\",\"5 Plus\",\"6 X\",\"7 Circle\"],[");
    for (const xpp::SolverInfo &solver : xpp::solvers()) {
        if (solver.id) BUF_LIT(&b, ",");
        buf_str(&b, xpp::format("{} {}", static_cast<int>(solver.id), solver.name).c_str());
    }
    BUF_LIT(&b, "]]");
    /* @ button name:keys lines of the ODE file ({"cmd":"userbut","index":i}) */
    BUF_LIT(&b, ",\"userbuttons\":[");
    for (i = 0; i < s.nuserbut; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, s.userbut[i].bname);
    }
    /* the model file's values, what `default` restores, in state's order */
    BUF_LIT(&b, "],\"defaults\":{\"pars\":[");
    for (i = 0; i < m.nupar; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_num(&b, m.default_val[i], 16);
    }
    BUF_LIT(&b, "],\"ics\":[");
    for (i = 0; i < m.node + m.nmarkov; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_num(&b, m.default_ic[i], 16);
    }
    BUF_LIT(&b, "]}}");
    send_buf(&b);
    send_main_window(title);
    send_state(s);
}

} // namespace xpp::json
