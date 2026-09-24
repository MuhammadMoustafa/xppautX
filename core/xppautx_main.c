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
     xppautX --script FILE model.ode  the same protocol, played from FILE
                                    instead of stdin, one command per line;
                                    exits when FILE ends (docs/protocol.md
                                    "Scripts")
     xppautX model.ode -silent      a headless batch run that writes
                                    output.dat, as upstream xppaut -silent

   usage: xppautX [--browser|--server|--script FILE] [--port N] [--no-open]
                  [--verbose|--debug] file.ode [xppaut options]
          xppautX --version | --help
   Every xppaut option still applies; ours have to come first. --verbose
   and --debug raise the core/xpp_log.h threshold (default: warnings and
   errors only) so the banner, parser stats and integrator chatter show
   on stderr too; xpp_log_parse_arg() also recognizes xpp_batch_main's
   own argv, so they work with -silent as well. */
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
#include "menudrive.h"
#include "xpp_http.h"
#include "xpp_util.h"
#include "xpp_window.h"
#include "xpp_win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "many_pops.h"

/* AUTO's files in a directory of this session's own, removed at exit
   (issue #11); -silent runs no AUTO */
static void start_auto_dir(void)
{
    xpp_auto_dir = xpp_make_temp_dir();
    if (xpp_auto_dir != NULL) atexit(xpp_cleanup_auto_dir);
}

/* What --version prints. The Makefile passes the release tag when there is
   one (XPP_VERSION in release.yml, else git describe); a build from a tree with
   no tags and no git says "dev". */
#ifndef XPPAUTX_VERSION
#define XPPAUTX_VERSION "dev"
#endif
/* the commit it was built from, for the window's About (git rev-parse) */
#ifndef XPPAUTX_COMMIT
#define XPPAUTX_COMMIT "unknown"
#endif
#if defined(__clang__)
#define XPPAUTX_COMPILER "clang " __clang_version__
#elif defined(__GNUC__)
#define XPPAUTX_COMPILER "gcc " __VERSION__
#else
#define XPPAUTX_COMPILER "unknown"
#endif

/* the desktop window's Help > About */
static const char about_text[] =
    "xppautX " XPPAUTX_VERSION "\n"
    "Commit " XPPAUTX_COMMIT "\n"
    "Compiler: " XPPAUTX_COMPILER "\n"
    "Protocol " JSON_UI_STR(JSON_UI_PROTOCOL) "\n\n"
    "XPPAUT is by Bard Ermentrout; xppautX is its modernised fork.\n"
    "GPL v2, as XPPAUT; no warranty (see LICENSE).\n\n"
    "https://github.com/MuhammadMoustafa/xppautX";

/* --help: the modes, then the options that go with them */
static const char usage_head[] =
    "usage: xppautX [MODE] [--port N] [--no-open] [--verbose|--debug] file.ode [xppaut options]\n"
    "       xppautX --version | --help\n"
    "Our options come first; every xppaut option still applies after them.\n"
    "Modes:\n"
    "  (none)           ";
static const char usage_window[] = "a window of its own: the page in the system's web view\n";
static const char usage_no_window[] = "the browser, as --browser (this build has no window of its own)\n";
static const char usage_tail[] =
    "  --browser        the page in the default browser; prints its address (XPP: http://...)\n"
    "  --web            the same as --browser\n"
    "  --no-open        browser mode, printing the address without opening a browser\n"
    "  --server         the JSON protocol on stdin and stdout (docs/protocol.md)\n"
    "  --script FILE    the protocol played from FILE, one command per line\n"
    "  -silent          (an xppaut option) a headless run that writes output.dat\n"
    "Options:\n"
    "  --port N         the page's port on 127.0.0.1 (default 8765; 0: any free port)\n"
    "  --verbose        the log at INFO, --debug at DEBUG (default: warnings and errors)\n";

/* what xppautX does with the session */
enum { MODE_WINDOW, MODE_BROWSER, MODE_SERVER };

void set_colorization_stuff(void);
extern char this_file[XPP_MAX_NAME];
extern int RunImmediately;
int SCALEX, SCALEY;

/* init_grafs() without the window: graph 0 is client window 1 */
static void init_main_graph(void)
{
    int i;
    for (i = 0; i < MAXLAB; i++) {
        lb[i].use = 0;
        lb[i].w = 0;
    }
    for (i = 0; i < MAXGROB; i++) {
        grob[i].w = 0;
        grob[i].use = 0;
    }
    init_bd();
    for (i = 0; i < MAXFRZ; i++) frozen_curves.curve[i].use = 0;
    for (i = 0; i < MAXPOP; i++) plot_windows.graph[i].Use = 0;
    plot_windows.open[0] = 0;
    init_all_graph();
    plot_windows.graph[0].w = 1;
    plot_windows.graph[0].Use = 1;
    plot_windows.graph[0].Restore = 1;
    plot_windows.graph[0].Nullrestore = 1;
    plot_windows.graph[0].x0 = 0;
    plot_windows.graph[0].y0 = 0;
    plot_windows.graph[0].Width = 640;
    plot_windows.graph[0].Height = 480;
    plot_windows.count = 1;
    plot_windows.draw_win = plot_windows.graph[0].w;
    plot_windows.active = 0;
    get_draw_area();
}

/* the session: load the model and serve it until Quit (the core exits);
   on the main thread (on macOS beside the window's: xpp_window.h) */
static int session_argc;
static char **session_argv;

static void run_session(void)
{
    char title[128];
    /* a monospace font the client can match: small 7x13, big 9x15 */
    text_metrics.small_width = 7; text_metrics.small_height = 13;
    text_metrics.big_width = 9; text_metrics.big_height = 15;
    SCALEX = 640; SCALEY = 480;

    json_ui_install();
    xpp_load_model(session_argc, session_argv, 0);
    xpp_window_set_model(this_file);

    if (strlen(this_file) < 60)
        XPP_SPRINTF(title, "XPP Ver %g.%g >> %s", xppvermaj, xppvermin, this_file);
    else
        XPP_SPRINTF(title, "XPP Version %g.%g", xppvermaj, xppvermin);
    Xup = 1;
    color_table.enabled = 1;     /* init_X on a colour display */
    periodic = 1;
    AxisVarLabels = 1; /* a plot without axis names is hard to read */
    xpp_build_colormap();
    init_main_graph();
    init_browser();
    ani_zero();
    set_extra_graphs();
    set_colorization_stuff();
    if_needed_load_set();
    if_needed_load_par();
    if_needed_load_ic();
    if_needed_load_ext_options();
    default_window();

    json_ui_hello(title);
    if (ani_options.use_file) {
        new_vcr();
        get_ani_file(ani_options.file);
    }
    json_ui_handle("{\"cmd\":\"redraw\"}");
    /* -tutorial and -runnow, as main.c does after opening its window */
    if (DoTutorial == 1 || RunImmediately == 1) {
        if (DoTutorial == 1) do_tutorial();
        if (RunImmediately == 1) run_the_commands(4);
        RunImmediately = 0;
        json_ui_handle("{\"cmd\":\"state\"}");
    }
    json_ui_loop();
}

int main(int argc, char **argv)
{
    int mode = MODE_WINDOW, batch = 0, port = 8765, open_browser = 1, i, k;
    char *script = NULL;
#ifdef _WIN32
    /* xppautX links -mwindows (no console from Explorer or a file
       association): reattach to a real console before any output, for
       --version/--help and every command-line mode; a no-op when stdio is
       already a real pipe or file, or there is no parent console */
    xpp_win32_attach_console();
#endif
    /* our options come first; the rest are xppaut's */
    for (i = k = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            /* the release tag (v1.2.0); the VS Code extension compares it with the latest release */
            printf("xppautX %s\n", XPPAUTX_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("%s%s%s", usage_head, xpp_window_supported() ? usage_window : usage_no_window, usage_tail);
            return 0;
        }
        if (strcmp(argv[i], "--web") == 0 || strcmp(argv[i], "--browser") == 0) mode = MODE_BROWSER;
        else if (strcmp(argv[i], "--server") == 0) mode = MODE_SERVER;
        else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            mode = MODE_SERVER;
            script = argv[++i];
        }
        else if (strcmp(argv[i], "--no-open") == 0) open_browser = 0;
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        else if (xpp_log_parse_arg(argv[i])) { /* --verbose / --debug: xpp_log.h */ }
        else {
            /* xppaut's own switch for a run with no interface at all */
            if (strcmp(argv[i], "-silent") == 0) batch = 1;
            argv[k++] = argv[i];
        }
    }
    argc = k;
    argv[argc] = NULL;
    if (batch) return xpp_batch_main(argc, argv);
    /* --no-open is browser mode (the VS Code extension, tools/cdp.mjs), and
       so is a build without a window */
    if (mode == MODE_WINDOW && (!open_browser || !xpp_window_supported())) mode = MODE_BROWSER;
    start_auto_dir();
    if (script && !json_ui_set_script(script)) {
        xpp_log(XPP_LOG_ERROR, "xppautX: cannot open script %s\n", script);
        return 1;
    }
    if (mode != MODE_SERVER) {
        /* the window navigates to the address itself: shown nowhere */
        int flags = mode == MODE_BROWSER ? XPP_HTTP_SHOW | (open_browser ? XPP_HTTP_OPEN : 0) : 0;
        if (!xpp_http_start(port, flags)) return 1;
        /* the AUTO window's Output panel shows AUTO's table from the log */
        xpp_log_set_auto_echo(1);
    }
    session_argc = argc;
    session_argv = argv;
    /* the window runs the session itself; when it cannot open, the browser */
    if (mode == MODE_WINDOW && !xpp_window_run(run_session, about_text)) xpp_http_show(1);
    run_session();
    return 0;
}
