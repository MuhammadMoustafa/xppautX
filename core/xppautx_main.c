/* xppautX: xppaut without a window system. It loads the ODE file exactly as
   xppaut does and then, like xppaut, picks what to do from the command line:

     xppautX model.ode              the front end in a browser (the default):
                                    xpp_http.c serves the compiled-in page on
                                    127.0.0.1 and opens it
     xppautX --server model.ode     the same session over the line-delimited
                                    JSON protocol of ui_json.c on stdin and
                                    stdout, for a front end that embeds it
                                    (the VS Code webview, a test script)
     xppautX model.ode -silent      a headless batch run that writes
                                    output.dat, as upstream xppaut -silent

   usage: xppautX [--server|--web] [--port N] [--no-open] file.ode [xppaut options]
          xppautX --version
   Every xppaut option still applies; ours have to come first. */
#include "xpp_batch.h"
#include "xpp_globals.h"
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* What --version prints. The Makefile passes the release tag when there is
   one (GITHUB_REF_NAME in CI, else git describe); a build from a tree with
   no tags and no git says "dev". */
#ifndef XPPAUTX_VERSION
#define XPPAUTX_VERSION "dev"
#endif

void set_colorization_stuff(void);
extern char this_file[XPP_MAX_NAME];
extern int use_ani_file;
extern char anifile[];
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
    for (i = 0; i < MAXFRZ; i++) frz[i].use = 0;
    for (i = 0; i < MAXPOP; i++) graph[i].Use = 0;
    ActiveWinList[0] = 0;
    init_all_graph();
    graph[0].w = 1;
    graph[0].Use = 1;
    graph[0].Restore = 1;
    graph[0].Nullrestore = 1;
    graph[0].x0 = 0;
    graph[0].y0 = 0;
    graph[0].Width = 640;
    graph[0].Height = 480;
    num_pops = 1;
    draw_win = graph[0].w;
    current_pop = 0;
    get_draw_area();
}

int main(int argc, char **argv)
{
    char title[128];
    int web = 1, batch = 0, port = 8765, open_browser = 1, i, k;
    /* our options come first; the rest are xppaut's */
    for (i = k = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            /* the release tag (v1.2.0); the VS Code extension compares it with the latest release */
            printf("xppautX %s\n", XPPAUTX_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--web") == 0) web = 1;
        else if (strcmp(argv[i], "--server") == 0) web = 0;
        else if (strcmp(argv[i], "--no-open") == 0) open_browser = 0;
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        else {
            /* xppaut's own switch for a run with no interface at all */
            if (strcmp(argv[i], "-silent") == 0) batch = 1;
            argv[k++] = argv[i];
        }
    }
    argc = k;
    argv[argc] = NULL;
    if (batch) return xpp_batch_main(argc, argv);
    if (web && !xpp_http_start(port, open_browser)) return 1;
    /* a monospace font the client can match: small 7x13, big 9x15 */
    DCURXs = 7; DCURYs = 13; CURY_OFFs = 10;
    DCURXb = 9; DCURYb = 15; CURY_OFFb = 12;
    DCURX = DCURXb; DCURY = DCURYb; CURY_OFF = CURY_OFFb;
    SCALEX = 640; SCALEY = 480;

    json_ui_install();
    xpp_load_model(argc, argv, 0);

    if (strlen(this_file) < 60)
        sprintf(title, "XPP Ver %g.%g >> %s", xppvermaj, xppvermin, this_file);
    else
        sprintf(title, "XPP Version %g.%g", xppvermaj, xppvermin);
    Xup = 1;
    COLOR = 1;     /* init_X on a colour display */
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
    if (use_ani_file) {
        new_vcr();
        get_ani_file(anifile);
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
    return 0;
}
