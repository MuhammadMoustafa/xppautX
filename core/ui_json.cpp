/* The protocol front end: an XppUi table that talks line-delimited JSON.

   One JSON object per line in each direction. What the core shows goes out
   as events ("ev"), as data (series, plots, nullclines, marks, the AUTO
   diagram, animation frames: the page draws them); keys, parameter edits
   and the answers to prompts come in as commands ("cmd"). A prompt (menu,
   string box, file name, mouse pick, ...) is an "ask" event with an id; the
   core blocks until the matching {"cmd":"answer","id":N,...} arrives.
   See docs/protocol.md.

   This file holds the XppUi table, the command dispatch, the input
   classifier, install and hello; ui_json_internal.h names
   the files that hold the rest. */
#include "model.h"
#include <cmath>
#include "session.h"
#include "grobs.h"
#include "ui_json.h"
#include "json_files.h"
#include "xpp_about.h"
#include "form_ode.h"
#include "xpp_batch.h"
#include "integrate.h"
#include "nullcline.h"
#include "ui_json_internal.h"
#include "auto_run.h"
#include "picture.h"
#include "json_error.h"
#include "xpp_log.h"
#include "xpp_http.h"
#include "xpp_inbox.h"
#include "xpp_job.h"
#include "xpp_util.h"
#include "many_pops.h"
#include "command_table.h"
#include "command_registry.h"
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
#include "xpp_examples.h"
#include "xpp_recent.h"

/* the core's own globals and functions that have no header of their own */

namespace xpp::json {

ProtocolSession session;

xpp::Session &client() { return xpp::client_session(); }

/* --silent returns the logging owner's error count; an interactive
   session's errors were shown as they came, so its end is always 0. */
static int exit_code(void) { return session.silent ? xpp::log_exit_code() : 0; }

namespace {

/* {"ev":"stopped","at":AT}: where the running job was when it was
   cancelled (docs/protocol.md "stopped"). A script replays the
   interruption from AT. */
void send_stopped(void)
{
    std::string b;
    BUF_LIT(b, "{\"ev\":\"stopped\",\"at\":");
    buf_stopped_at(b);
    BUF_LIT(b, "}");
    send_buf(b);
}

} // namespace

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

/* the command that stops the running computation: the reader thread acts on it the moment it arrives, in JSON or in server text */
constexpr std::string_view ABORT_COMMAND = "run.abort";
bool is_abort(const char *line) { return xpp::command::name(line) == ABORT_COMMAND; }

/* the parameters' edit: acted on at once under a job that computes nothing (a prompt open, the animation playing),
   as a `set` was, so a slider moves a parameter under the animation */
constexpr std::string_view PARAMETERS_COMMAND = "value.set_parameters";
bool is_parameter_edit(const char *line) { return xpp::command::name(line) == PARAMETERS_COMMAND; }

/* the table's paging, answered at once, even during a computation or a prompt: it only reads (and steers the view) */
constexpr std::string_view PAGE_COMMAND = "browser.page";
bool is_page(const char *line) { return xpp::command::name(line) == PAGE_COMMAND; }

/* browser.page taken as a control line (a computation or a prompt is running), run now on the core's thread: the
   block's event only, as run.abort has no done of its own while another command's is due; a refusal is said
   by its done, the one thing the client must not miss */
void answer_page(xpp::Session &s, const char *line)
{
    const xpp::Place place = j_command_place();
    xpp::Result<std::string> request = std::string(line);
    if (*skip_ws(line) != '{' && !session.silent && !xpp::http::active()) request = xpp::command::tokenize(line, place);
    const xpp::Result<std::string> result = request ? xpp::command::run(s, request->c_str(), place, false) : std::unexpected<xpp::Error>(request.error());
    browser_update(s);
    if (result) return;
    xpp::log_note_error();
    std::string event = xpp::command::done(request ? request->c_str() : "{}", result, false);
    send_buf(event);
}

constexpr std::string_view PAUSE_COMMAND = "animation.pause";

bool quit_waits(const char *line)
{
    return is_cmd(line, "quit") && (get_int(line, "ask", 0) != 0 || get_int(line, "save", 0) != 0);
}

/* commands that make sense at any moment, even while a prompt is open (a
   quit that asks or saves is a command of its own, in the dispatch table) */
int handle_async(xpp::Session &s, const char *line)
{
    if (is_cmd(line, "quit") && !quit_waits(line)) {
        /* read by a computation's own checkpoint, the quit ends the command it cancelled: that command
           says where it stopped, as when the computation sees the cancel first and the quit comes after */
        if (xpp::job::computing()) send_stopped();
        quit_command();
    }
    if (is_cmd(line, "state")) {
        send_state(s);
        return 1;
    }
    if (is_page(line)) {
        answer_page(s, line);
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
    if (is_abort(line) || is_page(line) || c == "quit" || c == "state") return xpp::inbox::Verdict::control;
    if (c == "play" && get_string(line, "op", o) && (o == "start" || o == "pause" || o == "step" || o == "speed"))
        return xpp::inbox::Verdict::control;
    if (c == "answer") return xpp::inbox::Verdict::normal;
    if (c == "key" && !js_find(line, "win")) {
        get_string(line, "key", o);
        int k = key_code(o.c_str());
        if (k == ESC || k == '/') return xpp::inbox::Verdict::control;
    } else if (is_steering(line)) {
        return xpp::inbox::Verdict::control;
    }
    const char kind = line_kind(line);
    if (kind == XPP_KIND_VIEW || kind == XPP_KIND_CONTROL || kind == XPP_KIND_SETTING) return xpp::inbox::Verdict::normal;
    xpp::log(XPP_LOG_WARN, "refused during a computation: {}{}{}\n", c.empty() ? "(no cmd)" : c.c_str(),
            o.empty() ? "" : " ", o.c_str());
    return xpp::inbox::Verdict::refuse;
}

/* the lines the command loop keeps count against the inbox's bound (xpp_inbox.h) */
void count_deferred()
{
    size_t bytes = 0;
    for (const DeferredLine &d : session.deferred) bytes += d.line.size();
    xpp::inbox::set_kept(session.deferred.size(), bytes);
}

void defer_line(const char *line, bool refused, bool applied, std::optional<xpp::Result<std::string>> taken)
{
    try {
        session.deferred.push_back({line, read_line_seq(), refused, applied, std::move(taken)});
    } catch (...) {
        xpp::out_of_memory("keeping a command");
    }
    count_deferred();
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
/* The inbox's busy hook (xpp_inbox.h), on the pushing thread: the error
   event for a normal line the full queue refused, naming the command (its
   `cmd`, and its `id` when it has one) and carrying it, cut short, as the
   error's source. It touches no core state: the command's place is the
   line itself, not command_place(), which belongs to the core's thread.
   The client sends it again later (docs/protocol.md "Commands during a
   command"). With a reply the caller wants it (HTTP's 503 body), else it
   goes out as any event. */
void busy(const char *line, std::string *reply)
{
    constexpr size_t SOURCE_SHOWN = 200; /* the refused line is echoed cut short, not all of its megabytes */
    std::string c;
    get_string(line, "cmd", c);
    std::string what = xpp::format("{}: {}{} was refused, the server already holds {} waiting commands; send it again later",
                                   xpp::inbox::BUSY, c.empty() ? "a command" : c,
                                   js_find(line, "id") ? xpp::format(" (id {})", get_int(line, "id", 0)) : "",
                                   xpp::inbox::MAX_PENDING_LINES);
    xpp::Place place;
    place.source.assign(line, std::min(std::strlen(line), SOURCE_SHOWN));
    std::string event = error_event("message", xpp::Error{"command", std::move(what), std::move(place)});
    if (reply) *reply = std::move(event);
    else {
        out_line(event.data(), event.size());
        out_flush();
    }
}

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
    if (is_abort(line) || c == "quit") {
        xpp::job::cancel(seq);
        return xpp::inbox::Verdict::control;
    }
    if (xpp::job::computing() && !xpp::job::stopping()) return during_run(line);
    if (!xpp::job::running()) return xpp::inbox::Verdict::normal;
    if ((c == "key" && !js_find(line, "win")) || c == "set" || c == "state" || is_page(line) || is_parameter_edit(line))
        return xpp::inbox::Verdict::control;
    if (is_steering(line)) return xpp::inbox::Verdict::control;
    return xpp::inbox::Verdict::normal;
}

} // namespace

/* what steers a view while a job runs, acted on at its next check: the animation's Pause, speed, faster and
   slower (a running Go reads them between frames) */
bool is_steering(const char *line)
{
    constexpr std::array<std::string_view, 4> STEERING_COMMANDS = {PAUSE_COMMAND, "animation.set_speed", "animation.faster", "animation.slower"};
    return std::ranges::find(STEERING_COMMANDS, xpp::command::name(line)) != STEERING_COMMANDS.end();
}

void take_setting(xpp::Session &s, const char *line)
{
    const bool parameters = is_parameter_edit(line);
    const bool now = !xpp::job::computed() && (is_cmd(line, "set") || parameters);
    std::optional<xpp::Result<std::string>> taken;
    if (now) {
        record_setting(line);
        if (parameters) taken = xpp::command::run(s, line, j_command_place(), false);
        else apply_set(s, line);
    }
    defer_line(line, false, now, std::move(taken));
}

/* a control line taken by a checkpoint: every kind classify() puts there
   is acted on, none dropped. Returns ESC for abort, the code of a key,
   ANI_PAUSE for the animation's Pause, 64 otherwise. A setting: take_setting(). */
int control_line(xpp::Session &s, const char *line)
{
    std::string k;
    if (handle_async(s, line)) return KEY_NONE;
    if (is_abort(line)) return ESC;
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
    if (is_steering(line)) {
        /* done now; its own `done` comes with the running command's end, as a setting's */
        const xpp::Place place = j_command_place();
        const bool text = *skip_ws(line) != '{' && !session.silent && !xpp::http::active();
        const xpp::Result<std::string> request = text ? xpp::command::tokenize(line, place) : xpp::Result<std::string>(line);
        const xpp::Result<std::string> result = request ? xpp::command::run(s, request->c_str(), place, false) : std::unexpected(request.error());
        if (request) record_control(request->c_str());
        std::optional<xpp::Result<std::string>> taken;
        if (result) taken = result;
        defer_line(line, false, taken.has_value(), std::move(taken));
        if (xpp::command::name(line) == PAUSE_COMMAND) return ANI_PAUSE;
    }
    return KEY_NONE;
}

/* where the running job was when it was cancelled, from what the
   computation reported last (xpp_job.h): the stopped event's `at`, and a
   recorded step's `abort` (json_record.cpp) */
void buf_stopped_at(std::string &b)
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
static QuestionPolicy j_question_policy(bool independent)
{
    return answer_policy(session.silent, !independent && (session.generated || player_place().line > 0));
}

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
    u.write_folders = []() { return &client().write_folders; };
    u.save_result = [](std::string_view file, bool saved) {
        std::string b;
        buf_format(b, "{{\"ev\":\"saved\",\"saved\":{},\"file\":", saved ? "true" : "false");
        buf_str(b, std::string(file).c_str());
        BUF_LIT(b, "}");
        send_buf(b);
    };
    u.ping = j_ping;
    u.bottom_msg = j_bottom_msg;
    u.message_box = j_message_box;
    u.kill_message_box = j_kill_message_box;
    u.title_text = j_title_text;
    u.question_policy = j_question_policy;
    u.new_string = j_new_string;
    u.choose = j_choose;
    u.checklist = j_checklist;
    u.string_box = j_string_box;
    u.typed_form = j_typed_form;
    u.grid_box = j_grid_box;
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
    u.create_plot_window = j_create_plot_window;
    u.destroy_plot_window = j_destroy_plot_window;
    u.window_renamed = j_window_renamed;
    u.auto_closed = j_auto_closed;
    u.tool_window_closed = j_tool_window_closed;
    u.redraw_diagram_for_client = auto_redraw_for_client;
    u.lower_plot_window = j_void;
    u.cput_text = j_cput_text;
    u.get_draw_size = j_get_draw_size;
    u.blank_draw_window = j_blank_draw_window;
    u.film_clip = j_film_clip;
    u.reset_film = j_reset_film;
    u.movie_play_back = j_movie_play_back;
    u.movie_auto_play = j_movie_auto_play;
    u.movie_save = j_movie_save;
    u.movie_make_anigif = j_movie_make_anigif;
    u.rubber_band = j_rubber_band;
    u.scroll_window = j_scroll_window;
    u.aplot_make = j_aplot_make;
    u.aplot_redraw = j_aplot_redraw;
    u.aplot_reset_axes = j_aplot_redraw;
    u.aplot_draw_one = j_aplot_draw_one;
    u.aplot_gif = j_aplot_gif;
    u.auto_make_window = j_auto_make_window;
    u.auto_refresh = j_auto_refresh;
    u.auto_check_abort = j_auto_check_abort;
    u.auto_rubber = j_auto_rubber;
    u.auto_scroll_window = j_auto_scroll_window;
    u.auto_grab_event = j_auto_grab_event;
    u.auto_show_hint = j_auto_show_hint;
    u.auto_diagram = j_auto_diagram;
    u.auto_points_held = j_auto_points_held;
    u.new_vcr = j_new_vcr;
    u.ani_show = j_ani_show;
    u.ani_slider = j_ani_slider;
    u.ani_go = j_ani_go;
    u.init_txtview = j_void;
    u.show_eq_box = j_show_eq_box;
    u.make_txtview = j_make_txtview;
    u.q_calc = j_q_calc;
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
        if(!action){
            j_command_error("key",xpp::format("Unknown command key menu {} item {}",menu,item));
            return;
        }
        if (js_find(line, "win") || js_find(line, "key")) {
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
    if (o == "save") xpp_session_save(s, name.empty() ? nullptr : name.c_str(), jd ? (get_num(line, "data", 1) != 0) : -1);
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
    {"key", nullptr, 0, STEP, key_command},
    {"answer", nullptr, C, NOT_STEP,
     [](xpp::Session &, const char *line) {
         /* Reaching the main dispatch means no ask was pending. */
         if (session.generated) silent_fail("answers a question that was never asked", line, NULL);
         else if (!player_place().file.empty()) j_command_error("play", "Answers a question that was never asked");
     }},
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
    {"keymap", nullptr, C, NOT_STEP, keymap_command}, /* the user's settings, not the model's: any time */
    {"file", "put", D, NOT_STEP, file_command},
    {"file", nullptr, V, NOT_STEP, file_command}, /* list, get */
    {"set", nullptr, S, STEP, apply_set},
    {"action", nullptr, D, STEP, action_command},
    {"session", nullptr, D, STEP, session_command},
    {"open", nullptr, D, STEP,
     [](xpp::Session &s, const char *line) {
         std::string file, example;
         get_string(line, "file", file);
         if (get_string(line, "example", example)) xpp_model_open_example(s, example);
         else xpp_model_open(s, file.c_str());
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
    {"userbut", nullptr, X, STEP,
     [](xpp::Session &s, const char *line) {
         int i = get_int(line, "index", -1);
         if (i >= 0 && i < s.nuserbut) run_the_commands(s, static_cast<MenuCommand>(s.userbut[i].com));
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
/* hello's "point_types": each point type's name, label and mark, the one table of core/auto_codes.h (Pending is run-time only) */
void buf_point_types(std::string &b)
{
    BUF_LIT(b, ",\"point_types\":[");
    bool first = true;
    for (const xpp::AutoPointName &p : xpp::auto_point_names) {
        if (p.kind == xpp::AutoPointKind::Pending) continue;
        buf_format(b, "{}{{\"name\":", first ? "" : ",");
        buf_str(b, p.name);
        BUF_LIT(b, ",\"label\":");
        buf_str(b, p.label);
        BUF_LIT(b, ",\"mark\":");
        buf_str(b, p.mark);
        BUF_LIT(b, "}");
        first = false;
    }
    BUF_LIT(b, "]");
}

void buf_commands(std::string &b)
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
    xpp::command::append_commands(b);
    BUF_LIT(b, "]");
}

/* how the page names the Esc key of the Numerics layer's last item (the others are their letters) */
constexpr std::string_view ESC_KEY_NAME = "Esc";

/* a legacy key as shown: its capital, or Esc */
std::string key_name(char key)
{
    return key == '\033' ? std::string(ESC_KEY_NAME) : std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(key))));
}

/* one row of hello's "command_table" (core/command_table.h); `legacy_keys` is the XPPAUT sequence: the
   shortcut layer's key, then the item's */
void buf_command_row(std::string &b, const CommandRow &row)
{
    BUF_LIT(b, "{\"menu\":");
    buf_str(b, MENU_NAMES[static_cast<std::size_t>(row.menu)]);
    BUF_LIT(b, ",\"id\":");
    buf_str(b, row.id);
    BUF_LIT(b, ",\"key\":");
    buf_str(b, row.key == PAGE_KEY ? std::string_view() : std::string_view(&row.key, 1));
    BUF_LIT(b, ",\"label\":");
    buf_str(b, row.label);
    BUF_LIT(b, ",\"description\":");
    buf_str(b, row.description);
    buf_format(b, ",\"kind\":\"{}\"", row.kind);
    BUF_LIT(b, ",\"category\":");
    buf_str(b, category_id(row.category));
    buf_format(b, ",\"pinnable\":{},\"primary\":{},\"default_keys\":[", row.pinnable, row.primary);
    bool first_key = true;
    for (std::string_view key : row.default_keys) {
        if (key.empty()) continue;
        if (!first_key) BUF_LIT(b, ",");
        first_key = false;
        buf_str(b, key);
    }
    BUF_LIT(b, "],\"legacy_keys\":[");
    if (row.key != PAGE_KEY) {
        if (const CommandRow *layer = layer_entry(row.menu)) {
            buf_str(b, key_name(layer->key));
            BUF_LIT(b, ",");
        }
        buf_str(b, key_name(row.key));
    }
    BUF_LIT(b, "]}");
}

/* hello's "menu_names", "command_categories" and "command_table" (docs/protocol.md "The command table"):
   the one list of commands in the sidebar's order, with the categories it groups them in */
void buf_command_table(std::string &b)
{
    BUF_LIT(b, ",\"menu_names\":[");
    for (std::size_t i = 0; i < MENU_NAMES.size(); ++i) {
        if (i) BUF_LIT(b, ",");
        buf_str(b, MENU_NAMES[i]);
    }
    BUF_LIT(b, "],\"command_categories\":[");
    for (std::size_t i = 0; i < COMMAND_CATEGORIES.size(); ++i) {
        const CategoryInfo &c = COMMAND_CATEGORIES[i];
        if (i) BUF_LIT(b, ",");
        BUF_LIT(b, "{\"id\":");
        buf_str(b, c.id);
        BUF_LIT(b, ",\"label\":");
        buf_str(b, c.label);
        buf_format(b, ",\"listed\":{},\"expanded\":{}}}", c.listed, c.expanded);
    }
    BUF_LIT(b, "],\"command_table\":[");
    for (std::size_t i = 0; i < COMMANDS.size(); ++i) {
        if (i) BUF_LIT(b, ",");
        buf_command_row(b, COMMANDS[i]);
    }
    BUF_LIT(b, "]");
}

} // namespace

bool line_is_step(const char *line)
{
    const CommandInfo *e = command_of(line);
    return xpp::command::is_step(line) || (e && e->step);
}

char line_kind(const char *line)
{
    if (const char kind = xpp::command::kind(line)) return kind;
    const CommandInfo *e = command_of(line);
    if (!e) return 0;
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

/* the commands a session with no model runs (the start screen, W232):
   Open model (the `open` command, or File > Open model's key), leaving,
   answering Open model's questions, and the user's settings; `data` is the
   page asking for events a model would send, which there are none of.
   Every other command needs a model and says so. */
bool runs_without_model(const char *line)
{
    if (is_abort(line)) return true;
    for (const char *cmd : {"open", "quit", "answer", "keymap", "data"})
        if (is_cmd(line, cmd)) return true;
    /* File > Open model by its stable id, as the page's menu and Ctrl+O send it */
    std::string menu, item;
    return is_cmd(line, "key") && get_string(line, "menu", menu) && get_string(line, "item", item) &&
           menu == xpp::MENU_NAMES[FILE_MENU] && item == "openmodel";
}

/* one command, run as a job (xpp_job.h) numbered by its line's sequence
   number: an abort cancels it from the reader thread. A line refused
   during a computation (during_run) only says so, and ends as any does;
   a set applied already, under a job that computed nothing
   (control_line), only ends. The Session the command ended in: the one
   it ran in, or the model's it loaded in that one's place. */
xpp::Session &handle_line(const char *line, unsigned long seq, bool refused, bool applied = false,
                          const xpp::Result<std::string> *taken = nullptr)
{
    xpp::job::begin(seq);
    const bool played = player_begin(line);
    /* A live command is a one-line source; recordings retain their step's
       own Place through player_place. This also locates bad save names. */
    session.reading=xpp::Place{"command",1,0,line};
    /* the session this command runs in, the client's in the session list
       (session.h): chosen here, once, and passed down (W47d); only a model
       loaded in its place below replaces it */
    xpp::Session *s = &xpp::client_session();
    const bool text = *skip_ws(line) != '{' && !session.silent && !xpp::http::active();
    const bool registered=text || !xpp::command::name(line).empty();
    xpp::Result<std::string> request=std::string(line),result=std::string{};
    if (registered) {
        const xpp::Place place=j_command_place();
        if(text)request=xpp::command::tokenize(line,place);
        if(request && applied && taken) {
            /* done when it was taken (take_setting): it only ends here, with the result it had */
            result=*taken;
        } else if(request) {
            record_begin(request->c_str());
            const auto permission=reset_save_decision();
            /* --silent's own commands replace the output files they write, as its key lines' "replace":1 said */
            if(session.generated)session.save_replace=SAVE_REPLACE;
            if(refused)result=xpp::fail("command",xpp::format("{}: {} was refused",xpp::job::REFUSED_WHILE_COMPUTING,xpp::command::name(request->c_str())),place);
            else result=!permission?std::unexpected<xpp::Error>(permission.error()):!s->model().loaded() && xpp::command::is_step(request->c_str()) ?
                xpp::fail("command","No model is open",place) : xpp::command::run(*s,request->c_str(),place,!played && !session.silent && !xpp::http::active());
        } else result=std::unexpected<xpp::Error>(request.error());
        if(!result) {
            if(session.silent)xpp::log(XPP_LOG_ERROR,"{}\n",result.error().text());
            else xpp::log_note_error();
        }
    } else {
    const xpp::Result<> permission = reset_save_decision();
    std::string input_error;
    const char *wrong_at = nullptr;
    const char *wrong = js_problem(line, wrong_at);
    try {
        const ArgumentCheck check;
        if (wrong) {
            j_command_error("command", xpp::format("the command line {} (at character {})", wrong, wrong_at - line + 1));
        } else if (get_string(line, "input_error", input_error)) {
            j_command_error("input", input_error);
        } else if (!permission) {
            xpp::show_error(permission.error());
        } else if (applied) {
        } else if (refused) {
            std::string c;
            get_string(line, "cmd", c);
            j_command_error("command", xpp::format("{}: {} was refused", xpp::job::REFUSED_WHILE_COMPUTING, c));
        } else if (!s->model().loaded() && !runs_without_model(line)) {
            j_command_error("command", "No model is open: open one first (Open model)");
        } else if (is_cmd(line, "data") && !s->model().loaded()) {
            /* no model: no events to send, nothing to subscribe */
        } else if (handle_async(*s, line)) {
        } else if (const CommandInfo *e = command_of(line)) {
            record_begin(line);
            e->run(*s, line);
            /* an evaluation's failure no caller took (W387: user functions
               nested too deep in the flow, a plot, a formula) is shown once,
               and the next command starts clean */
            if (s->integrator.step_error) xpp::show_error(xpp::take_step_error(*s).error());
        } else {
            std::string c;
            if (get_string(line, "cmd", c)) j_command_error("command", xpp::format("Unknown command {}", c));
        }
    } catch (const BadArgument &bad) {
        std::string cmd;
        get_string(line, "cmd", cmd);
        j_command_error(cmd.empty() ? "command" : cmd,
                        xpp::format("{}: {} must be {}, not {}", cmd.empty() ? "command" : cmd,
                                    bad.key.empty() ? std::string("an element") : xpp::format("argument \"{}\"", bad.key), bad.expected, bad.given));
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
    }
    const bool stopped=xpp::job::cancelled();
    if (!s->model().loaded()) {
        /* still the start screen (a cancelled Open model): nothing of a model to send */
        json_flush();
        xpp::job::end();
        if(registered){std::string event=xpp::command::done(request?request->c_str():"{}",result,stopped);send_buf(event);}
        else send_simple("idle");
        return *s;
    }
    aplot_update(*s);
    browser_update(*s);
    plot_data_update(*s);
    phase_data_update(*s);
    marks_data_update(*s);
    ani_data_update(*s);
    diag_flush(*s, 1);
    auto_data_update(*s, 1);
    auto_view_update(*s);
    session.windows.update(*s);
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
    if(registered){std::string event=xpp::command::done(request?request->c_str():"{}",result,stopped);send_buf(event);}
    else send_simple("idle");
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
            count_deferred();
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
            next.taken.reset();
        }
        /* an abort did its work when it arrived (classify()): it is no
           command of its own, and gets no state or idle; in a script,
           where nothing ran for it to stop, the next line follows */
        if (!is_abort(next.line.c_str()))
            handle_line(next.line.c_str(), next.seq, next.refused, next.applied, next.taken ? &*next.taken : nullptr);
        else if (session.generated) xpp::inbox::generated_advance();
    }
}

void json_ui_push_open(const char *path)
{
    /* on the window's thread, called from C (Win32, GTK): nothing may throw */
    try {
        std::string b;
        BUF_LIT(b, "{\"cmd\":\"open\",\"file\":");
        buf_str(b, path);
        BUF_LIT(b, "}");
        xpp::inbox::push_own(b);
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
    plot_data_init(data_emit);
    phase_data_init(data_emit);
    marks_data_init(data_emit);
    ani_data_init(data_emit);
    session.autoinfo.init(data_emit);
    session.autosettings.init(data_emit);
    session.autoview.init(data_emit);
    session.windows.init(data_emit);
    xpp::numerics_settings_init(data_emit);
    xpp::inbox::set_hooks(classify, busy);
    xpp::job::set_compute_hook(send_computing);
    XppUi ui = json_ui;
    if (silent) {
        /* NULL keeps the headless entry (set_ui) */
        ui.err_msg = nullptr;
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

int json_ui_silent(CommandLine cl)
{
    /* the model loads as with no interface at all: a model that does not
       load exits 1, its reason logged */
    const xpp::Loaded loaded = xpp::load_model(std::move(cl), 1);
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

void json_ui_start_screen()
{
    program.interactive = 1;
    send_hello(client());
    send_simple("idle");
    json_flush();
    /* the one Open model: the same command the page's button and File > Open model send */
    json_ui_push_open("");
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
    std::string b;
    int i;
    const std::string file = xpp::model_title(m);
    const std::string title_text =
        !m.loaded() ? std::string("xppautX")
        : file.size() < 60
            ? xpp::format("XPP Ver {:g}.{:g} >> {}", program.version_major, program.version_minor, file)
            : xpp::format("XPP Version {:g}.{:g}", program.version_major, program.version_minor);
    const char *title = title_text.c_str();
    BUF_LIT(b, "{\"ev\":\"hello\",\"protocol\":" JSON_UI_STR(JSON_UI_PROTOCOL) ",\"features\":[\"series\",\"plots\",\"nullclines\",\"dfield\",\"marks\",\"ani\",\"autoinfo\",\"autosettings\",\"numerics\",\"windows\",\"player\"],\"title\":");
    buf_str(b, title);
    BUF_LIT(b, ",\"file\":");
    buf_str(b, m.this_file);
    buf_format(b, ",\"steady\":{{\"max_decimals\":{},\"default_decimals\":{},\"default_hold\":{}}}",
        xpp::MAX_STEADY_DECIMALS,xpp::DEFAULT_STEADY_DECIMALS,xpp::DEFAULT_STEADY_HOLD);
    buf_format(b, ",\"continue\":{{\"grid_tolerance\":{}}}", xpp::CONTINUE_GRID_TOLERANCE);
    buf_format(b, ",\"state_inspection\":{{\"tail_intervals\":{}}}", STATE_TAIL_INTERVALS);
    BUF_LIT(b, ",\"output_names\":{");
    static constexpr struct { std::string_view key, ext, what; } names[]={
        {"par",".par",""},{"ic",".ic",""},{"csv",".csv",""},{"curves",".csv","curves"}};
    for (std::size_t n=0;n<std::size(names);++n){
        if(n)BUF_LIT(b,",");
        buf_str(b,names[n].key); BUF_LIT(b,":");
        buf_str(b,xpp::files::output_name(m.this_file,names[n].ext,names[n].what));
    }
    BUF_LIT(b,"}");
    BUF_LIT(b, ",\"about\":");
    BUF_LIT(b, "[");
    bool first_line = true;
    for (const AboutLine &line : xpp_about_lines()) {
        if (!first_line) BUF_LIT(b, ",");
        first_line = false;
        BUF_LIT(b, "[");
        for (std::size_t k = 0; k < line.parts.size(); ++k) {
            if (k) BUF_LIT(b, ",");
            BUF_LIT(b, "{\"text\":");
            buf_str(b, line.parts[k].text);
            if (!line.parts[k].url.empty()) {
                BUF_LIT(b, ",\"url\":");
                buf_str(b, line.parts[k].url);
            }
            BUF_LIT(b, "}");
        }
        BUF_LIT(b, "]");
    }
    BUF_LIT(b, "]");
    /* File > Quit's question, as the core asks it (model_switch.h): the
       page asks the same itself while a computation runs (W110) */
    BUF_LIT(b, ",\"quit\":{\"question\":");
    buf_str(b, xpp::quit_question(false));
    BUF_LIT(b, ",\"recording\":");
    buf_str(b, xpp::quit_question(true));
    BUF_LIT(b, ",\"choices\":[");
    buf_str(b, xpp::LEAVE_SAVE);
    BUF_LIT(b, ",");
    buf_str(b, xpp::LEAVE_DONT_SAVE);
    BUF_LIT(b, "],\"keys\":");
    buf_str(b, xpp::LEAVE_KEYS);
    BUF_LIT(b, "}");
    buf_command_table(b);
    BUF_LIT(b, ",\"keymap\":");
    const std::string keymap = hello_keymap();
    buf_add(b, keymap.data(), keymap.size());
    /* a session with no model: what the start screen shows (xpp_recent.h) */
    if (!m.loaded()) {
        BUF_LIT(b, ",\"start\":");
        const std::string start = xpp::recent::start_json(xpp::recent::load_recent(), ",\"examples\":" + xpp::examples::json());
        buf_add(b, start.data(), start.size());
    }
    /* the limits the page keeps to and the windows' numbers in `window`
       events (plot windows are 1 to plots) */
    buf_format(b, ",\"limits\":{{\"upload\":{},\"browser_rows\":{},\"browser_cols\":{}}}", XPP_FILES_CAP,
               BROWSER_MAX_ROWS, BROWSER_MAX_COLS);
    BUF_LIT(b, ",\"upload_error\":");
    buf_str(b, xpp::files::status_text(XPP_FILES_TOO_LARGE));
    buf_format(b, ",\"player_speed\":{{\"min\":{},\"max\":{}}}", PLAYER_SPEED_MIN, PLAYER_SPEED_MAX);
    buf_format(b, ",\"text\":{{\"max_length\":{},\"size_max\":{},\"default_size\":{}}}", xpp::LABEL_TEXT_MAX, xpp::LABEL_SIZE_MAX,
               xpp::LABEL_SIZE_DEFAULT);
    BUF_LIT(b, ",\"text_styles\":");
    buf_str_array(b, xpp::text_style_names.data(), static_cast<int>(xpp::text_style_names.size()));
    BUF_LIT(b, ",\"colormaps\":");
    {
        std::vector<std::string> names(xpp::colormap_names.begin(), xpp::colormap_names.end());
        buf_str_array(b, names);
    }
    const std::string line_styles = xpp::line_styles_json();
    const std::string glyphs = xpp::annotation_glyphs_json();
    BUF_LIT(b, ",\"annotation_glyphs\":");
    buf_add(b, glyphs.data(), glyphs.size());
    BUF_LIT(b, ",\"line_styles\":");
    buf_add(b, line_styles.data(), line_styles.size());
    BUF_LIT(b,",\"window_names\":{\"main\":");buf_str(b,s.plot_windows.graph[0].name);
    BUF_LIT(b,",\"auto\":");buf_str(b,xpp::window_name(s,WIN_AUTO));
    BUF_LIT(b,",\"ani\":");buf_str(b,xpp::window_name(s,WIN_ANI));
    BUF_LIT(b,",\"aplot\":");buf_str(b,xpp::window_name(s,WIN_APLOT));BUF_LIT(b,",\"browser\":");buf_str(b,std::string(xpp::BROWSER_WINDOW));BUF_LIT(b,"}");
    /* the windows' key layers (menus.h window_layers) and the other
       commands' kinds: what the page enables while a computation runs */
    BUF_LIT(b, ",\"windows\":{");
    for (i = 0; i < XPP_WINDOW_LAYERS; i++) {
        const XppWindowLayer &l = window_layers[i];
        if (i) BUF_LIT(b, ",");
        buf_str(b, l.win);
        BUF_LIT(b, ":{\"items\":");
        buf_str_array(b, l.menu->items, l.menu->n);
        BUF_LIT(b, ",\"keys\":");
        buf_str(b, l.menu->keys);
        BUF_LIT(b, ",\"kinds\":");
        buf_str(b, l.menu->kinds);
        BUF_LIT(b, ",\"ids\":");
        buf_str_array(b, l.ids, l.menu->n);
        BUF_LIT(b, ",\"hints\":");
        buf_str_array(b, l.menu->hints, l.menu->n);
        BUF_LIT(b, "}");
    }
    BUF_LIT(b, "}");
    buf_commands(b);
    buf_point_types(b);
    BUF_LIT(b, ",\"auto_run\":");
    b += xpp::auto_run_table_json();
    /* the lists a form field *n picks from */
    BUF_LIT(b, ",\"lists\":[[\"T\"");
    for (i = 0; i < m.neq; i++) {
        BUF_LIT(b, ",");
        buf_str(b, m.uvar_names[i]);
    }
    BUF_LIT(b, "],[");
    for (i = 0; i < m.node + m.nmarkov; i++) {
        if (i) BUF_LIT(b, ",");
        buf_str(b, m.uvar_names[i]);
    }
    BUF_LIT(b, "],[");
    for (i = 0; i < m.nupar; i++) {
        if (i) BUF_LIT(b, ",");
        buf_str(b, m.upar_names[i]);
    }
    BUF_LIT(b, "],[");
    for (i = 0; i < m.node + m.nmarkov + m.nupar; i++) {
        if (i) BUF_LIT(b, ",");
        buf_str(b, i < m.node + m.nmarkov ? m.uvar_names[i] : m.upar_names[i - m.node - m.nmarkov]);
    }
    BUF_LIT(b, "],[\"2 Box\",\"3 Diamond\",\"4 Triangle\",\"5 Plus\",\"6 X\",\"7 Circle\"],[");
    for (const xpp::SolverInfo &solver : xpp::solvers()) {
        if (solver.id) BUF_LIT(b, ",");
        buf_str(b, xpp::format("{} {}", static_cast<int>(solver.id), solver.name).c_str());
    }
    BUF_LIT(b, "]]");
    /* @ button name:keys lines of the ODE file ({"cmd":"userbut","index":i}) */
    BUF_LIT(b, ",\"userbuttons\":[");
    for (i = 0; i < s.nuserbut; i++) {
        if (i) BUF_LIT(b, ",");
        buf_str(b, s.userbut[i].bname);
    }
    /* the model file's values, what `default` restores, in state's order */
    BUF_LIT(b, "],\"defaults\":{\"pars\":[");
    for (i = 0; i < m.nupar; i++) {
        if (i) BUF_LIT(b, ",");
        buf_num(b, m.default_val[i], 16);
    }
    BUF_LIT(b, "],\"ics\":[");
    for (i = 0; i < m.node + m.nmarkov; i++) {
        if (i) BUF_LIT(b, ",");
        buf_num(b, m.default_ic[i], 16);
    }
    BUF_LIT(b, "]}}");
    send_buf(b);
    /* the start screen has no plot window and no state */
    if (m.loaded()) {
        send_main_window(s,title);
        send_state(s);
    }
}

} // namespace xpp::json
