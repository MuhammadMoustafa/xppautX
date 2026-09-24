/* The protocol front end: an XppUi table that talks line-delimited JSON.

   One JSON object per line in each direction. What the core shows goes out
   as events ("ev"), as data (series, plots, nullclines, marks, the AUTO
   diagram, animation frames: the page draws them); keys, parameter edits
   and the answers to prompts come in as commands ("cmd"). A prompt (menu,
   string box, file name, mouse pick, ...) is an "ask" event with an id; the
   core blocks until the matching {"cmd":"answer","id":N,...} arrives, the
   same way the X11 front end runs a nested event loop. See docs/protocol.md.

   This file holds the XppUi table, the command dispatch, the input
   classifier, script replay, install and hello; ui_json_internal.h names
   the files that hold the rest. */
#include "ui_json.h"
#include "ui_json_internal.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_http.h"
#include "xpp_inbox.h"
#include "xpp_job.h"
#include "xpp_globals.h"
#include "xpp_util.h"
#include "menus.h"
#include "mykeydef.h"
#include "userbut.h"
#include "menudrive.h"
#include "xpp_session.h"
#include "plot_data.h"
#include "phase_data.h"
#include "marks_data.h"
#include "ani_data.h"
#include "auto_data.h"
#include "auto_settings.h"
#include "xpp_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* the core's own globals and functions that have no header of their own */
extern "C" {
extern int NUPAR, NODE, NMarkov, NEQ;
extern char upar_names[MAXPAR][XPP_NAME_MAX+1], uvar_names[MAXODE][XPP_NAME_MAX+1];
extern double default_val[MAXPAR], default_ic[MAXODE];
extern char this_file[];
extern char *color_names[], *auto_hint[];
void commander(int ch); /* commands.c */
}

namespace xpp::json {

Session session;

/* exit 1 for a script that hit an error or an unmatched ask
   (docs/protocol.md "Scripts"), else as always, 0 */
void quit_session(void) { exit(session.script_mode && session.script_error ? 1 : 0); }

/* A line that does not fit a script's dialogue (an answer with no question
   open, or a command where an answer was due) stops the script at once:
   nothing after it can line up. */
void script_fail(const char *what, const char *line, const char *ask)
{
    xpp_log(XPP_LOG_ERROR, "xppautX: script line %d %s\n  line: %s\n", xpp_inbox_script_line(), what, line);
    if (ask && ask[0]) xpp_log(XPP_LOG_ERROR, "  open question: %s}\n", ask);
    exit(1);
}

/* commands that make sense at any moment, even while a prompt is open */
int handle_async(const char *line)
{
    if (is_cmd(line, "quit")) quit_session();
    if (is_cmd(line, "state")) {
        send_state();
        return 1;
    }
    if (is_cmd(line, "browser") && js_find(line, "from")) {
        browser_rows(line);
        return 1;
    }
    return 0;
}

namespace {

/* The input classifier (xpp_inbox.h), on the reader thread: it only parses
   the line and touches xpp_job's atomics.

   abort and quit go to the control queue always, and cancel the running
   job (and any not yet begun that came before them) at once: the
   computation sees xpp_job_cancelled() at its next check without reading
   input. Quit then exits when the engine takes the line.

   key, set, state, browser with from, and ani pause/fast/slow are
   what a computation's checkpoint acts on (Escape stops it, a set changes
   a parameter under it, the animation loop changes speed): they go to the
   control queue only while a job runs, and are meant for that job; a set
   sent then applies at once, ahead of commands queued behind the job, as
   it always has. Sent while no job runs they stay normal: prompts take
   control lines first and checkpoints nothing else, so a control line
   could be taken by the next command's prompt or computation ahead of
   commands sent before it; a normal line runs in its turn. The command
   loop reads both queues in arrival order, so a control line no job
   consumed also runs in its turn, as an ordinary command.

   Everything else is normal: a command sent while a job runs waits for
   it, and runs after the job's idle. */
int classify(const char *line, unsigned long seq)
{
    char c[16], o[16];
    if (!get_str(line, "cmd", c, sizeof c)) return XPP_INBOX_NORMAL;
    if (strcmp(c, "abort") == 0 || strcmp(c, "quit") == 0) {
        xpp_job_cancel(seq);
        return XPP_INBOX_CONTROL;
    }
    if (!xpp_job_running()) return XPP_INBOX_NORMAL;
    if (strcmp(c, "key") == 0 || strcmp(c, "set") == 0 || strcmp(c, "state") == 0)
        return XPP_INBOX_CONTROL;
    if (strcmp(c, "browser") == 0 && js_find(line, "from")) return XPP_INBOX_CONTROL;
    if (strcmp(c, "ani") == 0 && get_str(line, "op", o, sizeof o) &&
        (strcmp(o, "pause") == 0 || strcmp(o, "fast") == 0 || strcmp(o, "slow") == 0 || strcmp(o, "speed") == 0))
        return XPP_INBOX_CONTROL;
    return XPP_INBOX_NORMAL;
}

} // namespace

/* a control line taken by a checkpoint: every kind classify() puts there
   is acted on, none dropped. Returns ESC for abort, the code of a key,
   ANI_PAUSE for the animation's Pause, 64 otherwise. */
int control_line(const char *line)
{
    char k[32];
    if (handle_async(line)) return 64;
    if (is_cmd(line, "abort")) return ESC;
    if (is_cmd(line, "key")) {
        get_str(line, "key", k, sizeof k);
        return key_code(k);
    }
    if (is_cmd(line, "set")) {
        apply_set(line);
        return 64;
    }
    if (is_cmd(line, "ani")) {
        get_str(line, "op", k, sizeof k);
        if (strcmp(k, "pause") == 0) return ANI_PAUSE;
        ani_speed_op(k, line);
    }
    return 64;
}

namespace {

/* {"ev":"stopped","at":AT}: where the running job was when it was
   cancelled (docs/protocol.md "stopped"), from what the computation
   reported last (xpp_job.h). A script replays the interruption from AT. */
void send_stopped(void)
{
    XppJobProgress p = xpp_job_progress();
    Buf b = {0};
    BUF_LIT(&b, "{\"ev\":\"stopped\",\"at\":");
    if (p.what == XPP_JOB_INTEGRATE) {
        /* t is a stored single-precision number: 9 digits read back exactly */
        buf_printf(&b, "{\"what\":\"integrate\",\"rows\":%ld,\"t\":", p.rows);
        if (isfinite(p.t)) buf_printf(&b, "%.9g}", p.t);
        else BUF_LIT(&b, "null}");
    } else if (p.what == XPP_JOB_AUTO) {
        buf_printf(&b, "{\"what\":\"auto\",\"branch\":%d,\"point\":%d}", p.branch, p.point);
    } else {
        BUF_LIT(&b, "{\"what\":\"other\"}");
    }
    BUF_LIT(&b, "}");
    send_buf(&b);
    xpp_free(b.s);
}

/* A recorded interruption (docs/protocol.md "Scripts"): the line after the
   one a script is about to run is {"cmd":"abort","at":AT}. That line is
   dropped, and the job of the line about to run (the running job, for an
   answer) cancels itself at AT (xpp_job_stop_at_rows/point). stop_line and
   stop_at say what was armed, for script_stop_missed(). */
int stop_line;
char stop_at[400];

void script_arm_stop(void)
{
    int no = 0;
    const char *next = xpp_inbox_script_peek(&no), *at, *end;
    char what[16];
    if (!next || !is_cmd(next, "abort") || !(at = js_find(next, "at")) || *at != '{') return;
    end = skip_value(at);
    snprintf(stop_at, sizeof stop_at, "%.*s", (int)(end - at), at);
    stop_line = no;
    get_str(at, "what", what, sizeof what);
    if (strcmp(what, "integrate") == 0)
        xpp_job_stop_at_rows((long)get_num(at, "rows", -1));
    else if (strcmp(what, "auto") == 0)
        xpp_job_stop_at_point((int)get_num(at, "branch", -1), (int)get_num(at, "point", -1));
    /* an interruption of anything else cannot be placed: the job runs on */
    xpp_inbox_script_skip();
}

} // namespace

/* the script's next line to the core (docs/protocol.md "Scripts"), and
   the interruption recorded after it */
void script_next(void)
{
    xpp_inbox_script_advance();
    script_arm_stop();
}

namespace {

/* the job ends with its recorded interruption still armed */
void script_stop_missed(void)
{
    xpp_log(XPP_LOG_ERROR, "xppautX: script line %d: the recorded interruption at %s was never reached\n", stop_line,
            stop_at);
    exit(1);
}

void j_exit_program(void)
{
    send_simple("bye", NULL, NULL);
    exit(0);
}

void j_void(void) {}
void j_int(int i) { (void)i; }

/* the JSON front end's table: assignments, so C++17 needs no designated
   initializers; fields not set stay null, as in the C initializer */
XppUi make_json_ui(void)
{
    XppUi u{};
    u.err_msg = [](char *m) { j_err_msg(m); };
    u.ping = j_ping;
    u.bottom_msg = j_bottom_msg;
    u.message_box = [](char *m) { j_message_box(m); };
    u.kill_message_box = j_kill_message_box;
    u.title_text = j_title_text;
    u.canvas_xy = j_canvas_xy;
    u.new_string = j_new_string;
    u.yes_no_box = j_yes_no_box;
    u.two_choice = j_two_choice;
    u.respond_box = [](char *b, char *m) { j_respond_box(b, m); };
    u.checklist = j_checklist;
    u.string_box = j_string_box;
    u.file_selector = j_file_selector;
    u.dialog = [](char *t, char *n, char *v, char *o, char *c, int m) { return j_dialog(t, n, v, o, c, m); };
    u.edit_box = j_edit_box;
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
    u.clear_draw_window = clr_scrn;
    u.reset_graphics = j_reset_graphics;
    u.data_changed = j_browser_changed;
    u.rows_stored = j_rows_stored;
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
    u.auto_choose_key = j_auto_choose_key;
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
    u.exit_program = j_exit_program;
    return u;
}

const XppUi json_ui = make_json_ui();

/* one command, run as a job (xpp_job.h) numbered by its line's sequence
   number: an abort cancels it from the reader thread */
void handle_line(const char *line, unsigned long seq)
{
    char k[32];
    xpp_job_begin(seq);
    if (handle_async(line)) {
    } else if (is_cmd(line, "key")) {
        get_str(line, "key", k, sizeof k);
        commander(key_code(k));
    } else if (is_cmd(line, "set")) {
        apply_set(line);
    } else if (is_cmd(line, "default")) {
        default_command(line);
    } else if (is_cmd(line, "slide")) {
        slide_command(line);
    } else if (is_cmd(line, "userbut")) {
        int i = (int)get_num(line, "index", -1);
        if (i >= 0 && i < nuserbut) run_the_commands(userbut[i].com);
    } else if (is_cmd(line, "browser")) {
        browser_command(line);
    } else if (is_cmd(line, "aplot")) {
        aplot_command(line);
    } else if (is_cmd(line, "view")) {
        view_command(line);
    } else if (is_cmd(line, "rotate")) {
        rotate_command(line);
    } else if (is_cmd(line, "view3d")) {
        view3d_command(line);
    } else if (is_cmd(line, "plotvars")) {
        plotvars_command(line);
    } else if (is_cmd(line, "eqimport")) {
        eqimport_command();
    } else if (is_cmd(line, "answer")) {
        /* reaching the main dispatch (rather than ask_wait) means no ask
           was pending for it (docs/protocol.md "Scripts") */
        if (session.script_mode) script_fail("answers a question that was never asked", line, NULL);
    } else if (is_cmd(line, "data")) {
        data_command(line);
    } else if (is_cmd(line, "equations")) {
        send_equations();
    } else if (is_cmd(line, "action")) {
        action_command(line);
    } else if (is_cmd(line, "click")) {
        click_command(line);
    } else if (is_cmd(line, "redraw")) {
        j_redraw_graph();
        auto_redraw_for_client();
    } else if (is_cmd(line, "ani")) {
        ani_command(line);
    } else if (is_cmd(line, "auto")) {
        auto_command(line);
    } else if (is_cmd(line, "session")) {
        char o[8], name[XPP_MAX_NAME];
        get_str(line, "op", o, sizeof o);
        get_str(line, "name", name, sizeof name);
        if (strcmp(o, "save") == 0) xpp_session_save(name[0] ? name : NULL);
        else if (strcmp(o, "load") == 0) xpp_session_load(name[0] ? name : NULL);
    } else if (is_cmd(line, "file")) {
        /* the model's folder for a client that cannot reach it (xpp_files.h) */
        char o[8];
        get_str(line, "op", o, sizeof o);
        xpp_files_command(o, js_find(line, "name"), js_find(line, "data"), data_emit);
    }
    apply_deferred_sets();
    aplot_update();
    browser_update();
    plot_data_update();
    phase_data_update();
    marks_data_update();
    ani_data_update();
    diag_flush(1);
    auto_data_update(1);
    auto_settings_update();
    json_flush();
    /* a cancelled job says where it stopped; a replayed one must have
       stopped where the recorded session did */
    if (xpp_job_cancelled()) send_stopped();
    if (session.script_mode && xpp_job_stop_armed()) script_stop_missed();
    /* the command is finished; the client may send the next one */
    xpp_job_end();
    send_state();
    send_simple("idle", NULL, NULL);
    /* a script's next line is the next command (docs/protocol.md
       "Scripts"); this also releases the very first script line, since
       xppautx_main.c's startup "redraw" ends here too */
    if (session.script_mode) script_next();
}

} // namespace

} // namespace xpp::json

/* ---- the C API (ui_json.h) ---- */

using namespace xpp::json;

void json_ui_handle(const char *line) { handle_line(line, 0); }

void json_ui_loop(void)
{
    char *copy = NULL;
    size_t cap = 0;
    for (;;) {
        /* a copy: the command's own prompts read further lines */
        char *line = read_line(XPP_INBOX_ARRIVAL, -1);
        unsigned long seq = read_line_seq();
        size_t n = strlen(line) + 1;
        if (n > cap) {
            cap = n;
            copy = static_cast<char *>(xpp_realloc(copy, cap));
        }
        memcpy(copy, line, n);
        /* an abort did its work when it arrived (classify()): it is no
           command of its own, and gets no state or idle; in a script,
           where nothing ran for it to stop, the next line follows */
        if (!is_cmd(copy, "abort")) handle_line(copy, seq);
        else if (session.script_mode) script_next();
    }
}

int json_ui_set_script(const char *path)
{
    if (!xpp_inbox_start_file(path)) return 0;
    session.script_mode = 1;
    return 1;
}

void json_ui_install(void)
{
    if (!xpp_http_active()) { /* browser mode has taken stdout and stderr */
        open_protocol_stdout();
        /* commands from stdin, read on a thread of their own; a script's
           file (json_ui_set_script(), called before this) is read by the
           core thread itself instead, so no reader thread for it here */
        if (!session.script_mode && !xpp_inbox_start_stdin()) {
            xpp_log(XPP_LOG_ERROR, "xppautX: cannot start the input thread\n");
            exit(1);
        }
    }
    windows_init();
    plot_data_init(data_emit);
    phase_data_init(data_emit);
    marks_data_init(data_emit);
    ani_data_init(data_emit);
    auto_data_init(data_emit, diag_point_of_node);
    auto_settings_init(data_emit);
    xpp_inbox_set_classifier(classify);
    xpp_set_ui(&json_ui);
}

/* the first events a client sees */
void json_ui_hello(char *title)
{
    Buf b = {0};
    int i;
    BUF_LIT(&b, "{\"ev\":\"hello\",\"protocol\":" JSON_UI_STR(JSON_UI_PROTOCOL) ",\"features\":[\"series\",\"plots\",\"nullclines\",\"dfield\",\"marks\",\"ani\",\"autoinfo\",\"autosettings\"],\"title\":");
    buf_str(&b, title);
    BUF_LIT(&b, ",\"file\":");
    buf_str(&b, this_file);
    BUF_LIT(&b, ",\"menus\":{\"main\":");
    buf_str_array(&b, main_menu + 1, MAIN_ENTRIES); /* [0] is the title */
    BUF_LIT(&b, ",\"main_keys\":");
    buf_str(&b, main_menu_keys);
    BUF_LIT(&b, ",\"main_hints\":");
    buf_str_array(&b, main_hint, MAIN_ENTRIES);
    BUF_LIT(&b, ",\"file\":");
    buf_str_array(&b, fileon_menu + 1, FILE_ENTRIES); /* [0] is the title */
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
    BUF_LIT(&b, "}");
    /* the lists a form field *n picks from (pop_list.c make_scrbox_lists) */
    BUF_LIT(&b, ",\"lists\":[[\"T\"");
    for (i = 0; i < NEQ; i++) {
        BUF_LIT(&b, ",");
        buf_str(&b, uvar_names[i]);
    }
    BUF_LIT(&b, "],[");
    for (i = 0; i < NODE + NMarkov; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, uvar_names[i]);
    }
    BUF_LIT(&b, "],[");
    for (i = 0; i < NUPAR; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, upar_names[i]);
    }
    BUF_LIT(&b, "],[");
    for (i = 0; i < NODE + NMarkov + NUPAR; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, i < NODE + NMarkov ? uvar_names[i] : upar_names[i - NODE - NMarkov]);
    }
    BUF_LIT(&b, "],[");
    for (i = 0; i < 11; i++) {
        char item[40];
        snprintf(item, sizeof item, "%d %s", i, color_names[i]);
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, item);
    }
    BUF_LIT(&b, "],[\"2 Box\",\"3 Diamond\",\"4 Triangle\",\"5 Plus\",\"6 X\",\"7 Circle\"],"
                "[\"0 Discrete\",\"1 Euler\",\"2 Mod. Euler\",\"3 Runge-Kutta\",\"4 Adams\",\"5 Gear\","
                "\"6 Volterra\",\"7 BackEul\",\"8 QualRK\",\"9 Stiff\",\"10 CVode\",\"11 DoPri5\","
                "\"12 DoPri8(3)\",\"13 Rosenbrock\",\"14 Symplectic\"]]");
    BUF_LIT(&b, ",\"auto_hints\":");
    buf_str_array(&b, auto_hint, 9);
    /* @ button name:keys lines of the ODE file ({"cmd":"userbut","index":i}) */
    BUF_LIT(&b, ",\"userbuttons\":[");
    for (i = 0; i < nuserbut; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, userbut[i].bname);
    }
    /* @ slider1=name,slider1lo=...: the parameter sliders set in the file */
    BUF_LIT(&b, "],\"sliders\":[");
    {
        int set[3] = {!notAlreadySet.SLIDER1, !notAlreadySet.SLIDER2, !notAlreadySet.SLIDER3};
        int k = 0;
        for (i = 0; i < XPP_NSLIDERS; i++) {
            if (!set[i]) continue;
            if (k++) BUF_LIT(&b, ",");
            BUF_LIT(&b, "{\"name\":");
            buf_str(&b, sliders[i].var);
            buf_printf(&b, ",\"lo\":%.16g,\"hi\":%.16g}", sliders[i].lo, sliders[i].hi);
        }
    }
    /* the model file's values, what `default` restores, in state's order */
    BUF_LIT(&b, "],\"defaults\":{\"pars\":[");
    for (i = 0; i < NUPAR; i++) buf_printf(&b, "%s%.16g", i ? "," : "", default_val[i]);
    BUF_LIT(&b, "],\"ics\":[");
    for (i = 0; i < NODE + NMarkov; i++) buf_printf(&b, "%s%.16g", i ? "," : "", default_ic[i]);
    BUF_LIT(&b, "]}}");
    send_buf(&b);
    xpp_free(b.s);
    send_main_window(title);
    send_state();
}
