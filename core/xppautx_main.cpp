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
   on stderr too (comline.cpp reads every option once, ours and xppaut's). */
#include "model.h"
#include "model_options.h"
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
#include "setx_file.h"
#include "menudrive.h"
#include "xpp_http.h"
#include "xpp_util.h"
#include "auto_nox.h"
#include "xpp_files.h"
#include "xpp_about.h"
#include "xpp_window.h"
#include "xpp_win32.h"
#include "odex.h"
#include "xpp_session.h"
#include "xpp_recent.h"
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
    "                   names .odex reserves (--yes takes the suggested names)\n"
    "                   or write drawing.anix from drawing.ani and drawing.odex;\n"
    "                   --convert model.odex --anifile drawing.ani names another model\n"
    "  --check          validate a model or .anix in memory; JSON diagnostics, no files or window\n"
    "  --options-schema print the JSON Schema of a .odex's options block\n"
    "  --setx-schema    print the JSON Schema of a .setx settings file\n"
    "  --no-compile     run the equations in the interpreter, not as compiled code\n"
    "  --silent          (an xppaut option) a headless run that writes output.dat\n"
    "A session file (name.snapx, File/Save session or AUTO's Save diagram)\n"
    "opens its saved model and whole session. A recording (name.recx)\n"
    "opens in the player; name.recx --silent plays it without an interface.\n"
    "Options:\n"
    "  -y, --yes        answer every terminal question yes (suggested names, replacing\n"
    "                   files), printing each answer taken\n"
    "  --port N        the page's port on 127.0.0.1 (default {}; 0: any free port)\n"
    "  --verbose        the log at INFO, --debug at DEBUG (default: warnings and errors)\n"
    "  --setfile FILE   import an XPPAUT set; --parfile FILE / --icfile FILE load values\n"
    "  --outfile FILE   batch output; --noout suppresses rows\n"
    "  --include FILE   include text; --anifile FILE loads animation code\n"
    "  --logfile FILE   console log; --quiet 0|1 controls INFO logging\n"
    "  --newseed        randomize seed\n"
    "  --internset 0|1 / --uset NAME / --rset NAME   select internal sets\n"
    "  --qsets / --qpars / --qics   query sets / parameters / initial conditions\n"
    "  --mkplot / --plotfmt svg|ps   batch plot and format\n"
    "  --dfdraw 1-5 / --ncdraw 1|2   post-run fields / nullclines\n"
    "  --readset FILE / --with TEXT   internal-set settings\n"
    "  --equil 0|1      batch equilibria (1 also writes manifolds)\n"
    "  --auto           answer terminal questions: suggested names; replace files\n"
    "  -h / --help      help; --version prints the version\n"
    "Word options require two dashes. X11 options are removed.\n", xpp::http::DEFAULT_PORT);
}


/* the session: load the model and serve it until Quit (the core exits);
   on the main thread (on macOS beside the window's: xpp_window.h) */
static xpp::CommandLine startup;

static void run_session(void)
{
    /* a monospace font the client can match: small 7x13, big 9x15 */
    xpp::text_metrics.small_width = 7; xpp::text_metrics.small_height = 13;
    xpp::text_metrics.big_width = 9; xpp::text_metrics.big_height = 15;

    const bool silent = startup.silent;
    xpp::json_ui_install(silent);
    /* what the command line says, changed below only where a file in its place
       or the system adds an argument: then the changed arguments are read again */
    std::vector<std::string> args = startup.arguments;
    bool changed = false;
#ifdef __APPLE__
    /* a document Finder or `open` gave xppautX.app when it launched it (an
       Apple Event, not an argument: xpp_window.h) is the model, loaded from
       its own folder as File > Open model loads one; without one, an app
       Finder started (in "/") starts in the home folder, where the
       start screen's Open model dialog begins */
    if (const char *doc = xpp::window::launch_document()) {
        const std::pair<std::string, std::string> where = xpp::files::split_path(doc);
        if (!where.first.empty()) xpp::files::change_dir(where.first.c_str());
        args.push_back(where.second);
        changed = true;
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
    std::string opened; /* the model file the user opened, as an absolute path: the newest recent one */
    const std::string &model_file = changed ? args.back() : startup.model_file;
    const size_t i = changed ? args.size() - 1 : static_cast<size_t>(startup.model_argument);
    if (!model_file.empty()) {
        if (xpp::files::has_extension(model_file, xpp::recx::extension)) {
            recording = xpp::json_ui_recording_launch(model_file);
            if (!recording) exit(1); /* the error said why */
            opened = xpp::files::absolute(model_file);
            xpp::files::change_dir(recording->folder.c_str());
            args[i] = recording->model;
            changed = true;
        } else if (xpp_saved_file_name(model_file)) {
            saved = xpp_saved_read(model_file);
            if (!saved) exit(1); /* the error message said why, at the file */
            opened = xpp::files::absolute(model_file);
            xpp::files::change_dir(xpp::files::split_path(saved->path).first.c_str());
            const std::vector<std::string> model = xpp_saved_args(*saved);
            args.erase(args.begin() + static_cast<std::ptrdiff_t>(i));
            args.insert(args.begin() + static_cast<std::ptrdiff_t>(i), model.begin(), model.end());
            changed = true;
        }
    }
    if (silent) { /* a recording played through the protocol */
        std::erase(args, "--silent");
        changed = true;
    }
    xpp::CommandLine cl = startup;
    if (changed) {
        xpp::Result<xpp::CommandLine> reread = xpp::parse_command_line(std::move(args));
        if (!reread) {
            xpp::log(XPP_LOG_ERROR, "{}\n", reread.error().text());
            exit(2);
        }
        cl = std::move(*reread);
    }
    /* a saved file's members are read before the load keeps its model */
    std::function<std::optional<xpp::Error>(xpp::Session &)> check;
    if (saved) check = [&saved](xpp::Session &fresh) { return xpp_saved_check(fresh, *saved); };
    const xpp::Loaded loaded = xpp::load_model(std::move(cl), 0,
                                               saved ? &saved->model : recording ? &recording->saved : nullptr, check);
    if (!loaded && !silent && xpp::is_no_model(loaded.error())) {
        /* no file named: the start screen, whose Open model is File > Open model's */
        xpp::json_ui_start_screen();
        xpp::json_ui_loop();
    }
    if (!loaded) {
        if (!silent) json_ui_load_error(loaded.error()); /* load_model already logged the failure */
        else if (xpp::is_no_model(loaded.error())) xpp::log(XPP_LOG_ERROR, "{}\n", loaded.error().text()); /* --silent has nothing to open */
        exit(1);
    }
    /* a model named on the command line is a recent one like any opened (a .ode's converted .odex is what is loaded) */
    if (!silent) {
        const xpp::Model &m = (**loaded).model();
        const auto noted = xpp::recent::note(opened.empty() ? xpp::files::absolute(m.this_file, m.load_dir) : opened);
        if (!noted) xpp::log(XPP_LOG_WARN, "{}\n", noted.error().text());
    }
    /* the session the load made, the program's until the redraw (a protocol
       command chooses its own, ui_json.cpp handle_line) */
    xpp::Session &loaded_session = **loaded;
    json_ui_start_model(loaded_session);
    if (saved) xpp_saved_restore(loaded_session, *saved);
    if (recording) json_ui_play_launched(loaded_session, recording->saved.in); /* loaded by the command below */
    /* this first command loads a recording's model in the place of this one */
    xpp::json_ui_handle("{\"cmd\":\"state\"}");
    xpp::json_ui_loop();
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    /* xppautX links -mwindows (no console from Explorer or a file
       association): reattach to a real console before any output, for
       --version/--help and every command-line mode; a no-op when stdio is
       already a real pipe or file, or there is no parent console */
    xpp::win32::attach_console();
#endif
    /* the whole command line, read once: ours and xppaut's */
    xpp::Result<xpp::CommandLine> parsed = xpp::parse_command_line(std::vector<std::string>(argv, argv + argc));
    if (!parsed) {
        xpp::log(XPP_LOG_ERROR, "{}\n", parsed.error().text());
        return 2;
    }
    startup = std::move(*parsed);
    const xpp::CommandLine &cl = startup;
    if (cl.log_threshold) xpp::log_set_threshold(*cl.log_threshold);
    if (cl.version) {
        /* the release tag (v1.2.0); the VS Code extension compares it with the latest release */
        printf("xppautX %s\n", xpp_version_string());
        return 0;
    }
    if (cl.options_schema) {
        /* the JSON Schema of a .odex's options block, from the option table (docs/odex.md) */
        printf("%s", xpp::option_schema().c_str());
        return 0;
    }
    if (cl.setx_schema) {
        /* the JSON Schema of a .setx (docs/odex.md), from the description the reader and writer share */
        printf("%s", xpp::setx_schema().c_str());
        return 0;
    }
    if (cl.help) {
        printf("%s%s%s", usage_head, xpp::window::supported() ? usage_window : usage_no_window, usage_tail().c_str());
        return 0;
    }
    if (cl.check && !cl.convert && !cl.model_file.empty() && cl.load_options == 0) {
        xpp::batch_options.check = true;
        return xpp::odex::check_file(cl.model_file);
    }
    if (cl.check) {
        xpp::log(XPP_LOG_ERROR, "usage: xppautX --check model.ode (or model.odex)\n");
        return 2;
    }
    xpp::batch_options.yes = cl.yes;
    /* --convert [--yes] model.ode: model.odex beside it (odex.h) */
    if (cl.convert) {
        if (cl.model_file.empty() || cl.load_options != (cl.ani_file ? 1U : 0U)) {
            xpp::log(XPP_LOG_ERROR, "usage: xppautX --convert [--yes] model.ode\n");
            return 2;
        }
        if (cl.ani_file) return xpp::convert_animation_file(*cl.ani_file, cl.model_file);
        if (xpp::files::has_extension(cl.model_file, ".ani")) return xpp::convert_animation_file(cl.model_file);
        return xpp::odex::convert_file(cl.model_file);
    }
    const bool recording_batch = cl.silent && xpp::files::has_extension(cl.model_file, xpp::recx::extension);
    if (cl.silent && !recording_batch) {
        if (xpp_saved_file_name(cl.model_file)) {
            xpp::log(XPP_LOG_ERROR, "{}\n",
                     xpp::Error{"xppautX", "an AUTO or session file opens in the window, the browser or --server, not with --silent",
                                xpp::Place{cl.model_file}}.text());
            return 2;
        }
        return xpp::json_ui_silent(startup);
    }
    /* --no-open is browser mode (the VS Code extension, tools/cdp.mjs), and
       so is a build without a window */
    xpp::Interface interface = cl.interface.value_or(xpp::Interface::window);
    if (interface == xpp::Interface::window && (cl.no_open || !xpp::window::supported())) interface = xpp::Interface::browser;
    start_auto_dir();
    if (recording_batch) interface = xpp::Interface::server;
    if (interface != xpp::Interface::server) {
        const bool browser = interface == xpp::Interface::browser;
        /* the window navigates to the address itself: shown nowhere */
        if (!xpp::http::start(cl.port.value_or(xpp::http::DEFAULT_PORT), browser, browser && !cl.no_open)) return 1;
        /* the AUTO window's Output panel shows AUTO's table from the log */
        xpp::log_set_auto_echo(1);
    }
    /* the window runs the session itself; when it cannot open, the browser */
    if (interface == xpp::Interface::window && !xpp::window::run(run_session, xpp_about_markup().c_str())) xpp::http::show(true);
    run_session();
    return 0;
}
