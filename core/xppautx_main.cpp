/* xppautX: xppaut without a window system. It loads the ODE file exactly as
   xppaut does and then, like xppaut, picks what to do from the command line:

     xppautX model.ode              the desktop window (the default): the
                                    compiled-in page, served by xpp_http.cpp
                                    on 127.0.0.1, in the system's own web
                                    view (xpp_window.h); a build without one
                                    (Linux without WebKitGTK), or a system
                                    where it cannot open, uses the browser
     xppautX --browser model.ode    the same page in the default browser,
                                    with its address printed (--web is the
                                    same; --no-open prints it only)
     xppautX --server model.ode     the same session over the line-delimited
                                    JSON protocol of ui_json.cpp on stdin and
                                    stdout, for a front end that embeds it
                                    (the VS Code webview, a test script)
     xppautX model.ode --silent      a headless batch run that writes
                                    output.dat, as upstream xppaut --silent:
                                    a built-in script (json_silent.cpp) of
                                    the protocol's commands, with its
                                    events going nowhere
     xppautX name.recx              a recording (W59c): its model starts,
                                    then it opens in the player; --silent
                                    plays every step and exits
     xppautX name.snapx             a session file (xpp_session.h) in any mode but --silent:
                                    the model saved in it, loaded from its
                                    saved files, then the session as it was saved

   usage: xppautX [--browser|--server] [--port N] [--no-open]
                  [--verbose|--debug] file.ode [xppaut options]
          xppautX --version | --help
   Supported model options use GNU spelling; ours have to come first. --verbose
   and --debug raise the core/xpp_log.h threshold (default: warnings and
   errors only) so the banner, parser stats and integrator chatter show
   on stderr too; xpp::log_parse_arg() also recognizes them
   among xppaut's own options, so they work with --silent as well. */
#include "model.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_globals.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include "xpp_ui.h"
#include "ui_json.h"
#include "colormap.h"
#include "graphics.h"
#include "graf_par.h"
#include "grobs.h"
#include "browse.h"
#include "aniparse.h"
#include "comline.h"
#include "load_eqn.h"
#include "recx.h"
#include "snapx.h"
#include "menudrive.h"
#include "xpp_http.h"
#include "xpp_util.h"
#include "xpp_files.h"
#include "xpp_about.h"
#include "xpp_window.h"
#include "xpp_win32.h"
#include "odex.h"
#include "xpp_session.h"
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "many_pops.h"
#include "axes2.h"
#include "nullcline.h"

/* AUTO's files in a directory of this session's own, removed at exit
   (issue #11); a model's --silent command list runs no AUTO */
static void start_auto_dir(void)
{
    /* issue #32: the folders of runs that ended without their atexit (a
       kill, Ctrl+C, the watchdog's _exit) go at the next start: cleaning up
       in a signal handler or from another thread is neither safe nor needed */
    xpp::files::cleanup_stale_temp_dirs();
    std::string &dir = xpp::client_session().auto_state.dir;
    dir = xpp::files::make_temp_dir();
    /* at exit the folder is the session list's client's: every load hands
       it on to the Session it makes (session.cpp) */
    if (!dir.empty()) atexit([] { xpp::cleanup_auto_dir(xpp::client_session()); });
}

/* the desktop window's Help > About: the same text hello sends the page
   (xpp_about.h) */
/* --help: the modes, then the options that go with them */
static const char *const usage_head =
    "usage: xppautX [MODE] [--port N] [--no-open] [--verbose|--debug] file.ode [xppaut options]\n"
    "       xppautX --version | --help\n"
    "Our options come first; the supported model options follow GNU spelling after them.\n"
    "Modes:\n"
    "  (none)           ";
static const char *const usage_window = "a window of its own: the page in the system's web view\n";
static const char *const usage_no_window = "the browser, as --browser (this build has no window of its own)\n";
static std::string usage_tail()
{
    return xpp::format(
    "  --browser        the page in the default browser; prints its address (XPP: http://...)\n"
    "  --web            the same as --browser\n"
    "  --no-open        browser mode, printing the address without opening a browser\n"
    "  --server         the JSON protocol on stdin and stdout (docs/protocol.md)\n"
    "  --convert        write model.odex from model.ode (docs/odex.md); asks about\n"
    "                   names .odex reserves (--auto takes the suggested names)\n"
    "  --silent          (an xppaut option) a headless run that writes output.dat\n"
    "A session file (name.snapx, File/Save session or AUTO's Save diagram)\n"
    "opens its saved model and whole session. A recording (name.recx)\n"
    "opens in the player; name.recx --silent plays it without an interface.\n"
    "Options:\n"
    "  --port N         the page's port on 127.0.0.1 (default {}; 0: any free port)\n"
    "  --verbose        the log at INFO, --debug at DEBUG (default: warnings and errors)\n"
    "  --setfile FILE   import an XPPAUT set; --parfile FILE / --icfile FILE load values\n"
    "  --outfile FILE   batch output; --noout suppresses rows\n"
    "  --include FILE   include text; --anifile FILE loads animation code\n"
    "  --logfile FILE   console log; --quiet 0|1 controls INFO logging\n"
    "  --newseed        randomize seed; --runnow runs on startup\n"
    "  --internset 0|1 / --uset NAME / --rset NAME   select internal sets\n"
    "  --qsets / --qpars / --qics   query sets / parameters / initial conditions\n"
    "  --mkplot / --plotfmt svg|ps   batch plot and format\n"
    "  --dfdraw 1-5 / --ncdraw 1|2   batch fields / nullclines\n"
    "  --readset FILE / --with TEXT   internal-set settings\n"
    "  --equil 0|1      batch equilibria (1 also writes manifolds)\n"
    "  --auto           answer terminal questions: suggested names; replace files\n"
    "  -h / --help      help; --version prints the version\n"
    "Word options require two dashes. X11 options are removed.\n", xpp::http::DEFAULT_PORT);
}

/* what xppautX does with the session */
enum { MODE_WINDOW, MODE_BROWSER, MODE_SERVER };

/* the session: load the model and serve it until Quit (the core exits);
   on the main thread (on macOS beside the window's: xpp_window.h) */
static int session_argc;
static char **session_argv;

static void run_session(void)
{
    /* a monospace font the client can match: small 7x13, big 9x15 */
    xpp::text_metrics.small_width = 7; xpp::text_metrics.small_height = 13;
    xpp::text_metrics.big_width = 9; xpp::text_metrics.big_height = 15;

    bool silent = false;
    for (int i = 1; i < session_argc; ++i)
        if (strcmp(session_argv[i], "--silent") == 0) silent = true;
    xpp::json_ui_install(silent);
#ifdef __APPLE__
    /* a document Finder or `open` gave xppautX.app when it launched it (an
       Apple Event, not an argument: xpp_window.h) is the model, loaded from
       its own folder as File > Open model loads one; without one, an app
       Finder started (in "/") starts in the home folder, where the Open
       dialog the load then shows begins */
    static std::string launch_name;
    static std::vector<char *> launch_argv;
    if (const char *doc = xpp::window::launch_document()) {
        const std::pair<std::string, std::string> where = xpp::files::split_path(doc);
        if (!where.first.empty()) xpp::files::change_dir(where.first.c_str());
        launch_name = where.second;
        launch_argv.assign(session_argv, session_argv + session_argc);
        launch_argv.push_back(launch_name.data());
        launch_argv.push_back(nullptr);
        session_argc = static_cast<int>(launch_argv.size()) - 1;
        session_argv = launch_argv.data();
    } else if (xpp::files::working_dir() == "/") {
        const char *home = getenv("HOME");
        if (home && *home) xpp::files::change_dir(home);
    }
#endif
    /* a file that carries a model on the command line (W103: an AUTO
       file or a session file): its saved model is loaded, from the file's
       own folder as File > Open model loads one, then what the file adds */
    std::optional<SavedFile> saved;
    std::optional<xpp::RecordingLaunch> recording; /* a .recx: its model starts the session, the player opens it after */
    std::vector<std::string> args;
    std::vector<char *> argv;
    args.assign(session_argv, session_argv + session_argc);
    if (silent) std::erase(args, "--silent");
    for (size_t i = 1; i < args.size(); i++) {
        if (xpp::snapx::has_extension(args[i], xpp::recx::extension)) {
            recording = xpp::json_ui_recording_launch(args[i]);
            if (!recording) exit(1); /* the error said why */
            xpp::files::change_dir(recording->folder.c_str());
            args[i] = recording->model;
            break;
        }
        if (!xpp_saved_file_name(args[i])) continue;
        saved = xpp_saved_read(args[i]);
        if (!saved) exit(1); /* the error message said why, at the file */
        xpp::files::change_dir(xpp::files::split_path(saved->path).first.c_str());
        const std::vector<std::string> model = xpp_saved_args(*saved);
        args.erase(args.begin() + static_cast<std::ptrdiff_t>(i));
        args.insert(args.begin() + static_cast<std::ptrdiff_t>(i), model.begin(), model.end());
        break;
    }
    for (std::string &a : args) argv.push_back(a.data());
    argv.push_back(nullptr);
    /* a saved file's members are read before the load keeps its model */
    std::function<std::optional<xpp::Error>(xpp::Session &)> check;
    if (saved) check = [&saved](xpp::Session &fresh) { return xpp_saved_check(fresh, *saved); };
    const xpp::Loaded loaded = xpp::load_model(static_cast<int>(args.size()), argv.data(), 0,
                                               saved ? &saved->model : recording ? &recording->saved : nullptr, check);
    if (!loaded) {
        if (!silent) json_ui_load_error(loaded.error()); /* load_model already logged the failure */
        exit(1);
    }
    /* the session the load made, the program's until the redraw (a protocol
       command chooses its own, ui_json.cpp handle_line) */
    xpp::Session &loaded_session = **loaded;
    json_ui_start_model(loaded_session);
    if (saved) xpp_saved_restore(loaded_session, *saved);
    if (recording) json_ui_play_launched(loaded_session, recording->saved.in); /* loaded by the redraw below */
    /* the redraw loads a recording's model in the place of this one: the
       session from here is the one it ends in */
    xpp::Session &s = xpp::json_ui_handle("{\"cmd\":\"redraw\"}");
    /* Tutorial and --runnow, after opening the window. */
    if (program.tutorial == 1 || s.run_immediately == 1) {
        if (program.tutorial == 1) xpp::do_tutorial();
        xpp::json_ui_queue_runnow(s);
    }
    xpp::json_ui_loop();
}

int main(int argc, char **argv)
{
    int mode = MODE_WINDOW, batch = 0, port = xpp::http::DEFAULT_PORT, open_browser = 1, convert = 0, convert_auto = 0, i, k;
#ifdef _WIN32
    /* xppautX links -mwindows (no console from Explorer or a file
       association): reattach to a real console before any output, for
       --version/--help and every command-line mode; a no-op when stdio is
       already a real pipe or file, or there is no parent console */
    xpp::win32::attach_console();
#endif
    /* our options come first; the rest are xppaut's */
    for (i = k = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            /* the release tag (v1.2.0); the VS Code extension compares it with the latest release */
            printf("xppautX %s\n", xpp_version_string());
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("%s%s%s", usage_head, xpp::window::supported() ? usage_window : usage_no_window, usage_tail().c_str());
            return 0;
        }
        if (strcmp(argv[i], "--web") == 0 || strcmp(argv[i], "--browser") == 0) mode = MODE_BROWSER;
        else if (strcmp(argv[i], "--server") == 0) mode = MODE_SERVER;
        else if (strcmp(argv[i], "--no-open") == 0) open_browser = 0;
        else if (strcmp(argv[i], "--convert") == 0) convert = 1;
        else if (strcmp(argv[i], "--auto") == 0) convert_auto = 1;
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        else if (xpp::log_parse_arg(argv[i])) { /* --verbose / --debug: xpp_log.h */ }
#ifdef __APPLE__
        /* the process serial number an older macOS gave an app Finder
           started, which xppaut's options would take for a bad one */
        else if (strncmp(argv[i], "-psn_", 5) == 0) {}
#endif
        else {
            /* xppaut's own switch for a run with no interface at all */
            if (strcmp(argv[i], "--silent") == 0) batch = 1;
            argv[k++] = argv[i];
        }
    }
    argc = k;
    argv[argc] = NULL;
    if (const xpp::Result<> r = xpp::check_command_line(argc, argv); !r) {
        xpp::log(XPP_LOG_ERROR, "{}\n", r.error().text());
        return 2;
    }
    /* --convert [--auto] model.ode: model.odex beside it (odex.h) */
    if (convert) {
        if (argc != 2) {
            xpp::log(XPP_LOG_ERROR, "usage: xppautX --convert [--auto] model.ode\n");
            return 2;
        }
        return xpp::odex::convert_file(argv[1], convert_auto != 0, xpp::ask_terminal);
    }
    bool recording_batch = false;
    xpp::json_ui_terminal_auto(convert_auto != 0);
    for (i = 1; i < argc; ++i)
        if (xpp::snapx::has_extension(argv[i], xpp::recx::extension)) recording_batch = batch != 0;
    if (batch && !recording_batch) {
        for (i = 1; i < argc; i++)
            if (xpp_saved_file_name(argv[i])) {
                xpp::log(XPP_LOG_ERROR, "{}\n",
                         xpp::Error{"xppautX", "an AUTO or session file opens in the window, the browser or --server, not with --silent",
                                    xpp::Place{argv[i]}}.text());
                return 2;
            }
        return xpp::json_ui_silent(argc, argv);
    }
    /* --no-open is browser mode (the VS Code extension, tools/cdp.mjs), and
       so is a build without a window */
    if (mode == MODE_WINDOW && (!open_browser || !xpp::window::supported())) mode = MODE_BROWSER;
    start_auto_dir();
    if (recording_batch) mode = MODE_SERVER;
    if (mode != MODE_SERVER) {
        /* the window navigates to the address itself: shown nowhere */
        if (!xpp::http::start(port, mode == MODE_BROWSER, mode == MODE_BROWSER && open_browser)) return 1;
        /* the AUTO window's Output panel shows AUTO's table from the log */
        xpp::log_set_auto_echo(1);
    }
    session_argc = argc;
    session_argv = argv;
    /* the window runs the session itself; when it cannot open, the browser */
    if (mode == MODE_WINDOW && !xpp::window::run(run_session, xpp_about_text().c_str())) xpp::http::show(true);
    run_session();
    return 0;
}
