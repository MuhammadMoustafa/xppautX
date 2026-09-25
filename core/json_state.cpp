/* The model's state as the client sees it: the state event (parameters,
   ICs, BCs, delays, the view), the data browser, the edits that change
   them (set, default, slide), the data events a client subscribes to, and
   the equations, source and equilibrium windows. */
#include "ui_json_internal.h"
#include "xpp_mem.h"
#include "xpp_globals.h"
#include "xpp_util.h"
#include "graphics.h"
#include "integrate.h"
#include "browse.h"
#include "auto_nox.h"
#include "derived.h"
#include "parserslow.h"
#include "txtread.h"
#include "shoot.h"
#include "arrayplot.h"
#include "xpp_session.h"
#include "plot_data.h"
#include "phase_data.h"
#include "marks_data.h"
#include "ani_data.h"
#include "auto_data.h"
#include "auto_settings.h"
#include <strings.h>
#include <climits>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "many_pops.h"
#include "menudrive.h"

/* the core's own globals and functions that have no header of their own */
extern "C" {
extern int NUPAR, NODE, NMarkov, NEQ;
extern char upar_names[MAXPAR][XPP_NAME_MAX+1], uvar_names[MAXODE][XPP_NAME_MAX+1];
extern double last_ic[MAXODE], MyData[MAXODE];
extern int INFLAG;
extern BROWSER my_browser;
extern BIFUR Auto;
extern BC_STRUCT my_bc[MAXODE];
extern char delay_string[MAXODE][80];
extern int DelayFlag, METHOD, EqType[];
extern char *ode_names[];
typedef struct {
    char *text, *action;
    int aflag;
} ACTION; /* form_ode.c */
extern ACTION comments[];
extern int n_comments;
extern char *save_eqn[];
extern int NLINES;
extern int DLeft, DRight, DTop, DBottom;
}

namespace xpp::json {

namespace {

int state_dirty;

} // namespace

/* ---- state ------------------------------------------------------------------ */

void send_state(void)
{
    Buf b = {0};
    int i;
    double z;
    state_dirty = 0;
    evaluate_derived();
    BUF_LIT(&b, "{\"ev\":\"state\",\"pars\":[");
    for (i = 0; i < NUPAR; i++) {
        get_val(upar_names[i], &z);
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, upar_names[i]);
        buf_printf(&b, ",%.16g]", z);
    }
    BUF_LIT(&b, "],\"ics\":[");
    for (i = 0; i < NODE + NMarkov; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, uvar_names[i]);
        buf_printf(&b, ",%.16g]", last_ic[i]);
    }
    BUF_LIT(&b, "]");
    /* where the last run ended (MyData, what Initialconds/Last starts from) */
    if (INFLAG) {
        BUF_LIT(&b, ",\"now\":[");
        for (i = 0; i < NODE + NMarkov; i++) buf_printf(&b, "%s%.16g", i ? "," : "", MyData[i]);
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, ",\"bcs\":[");
    for (i = 0; i < NODE; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, my_bc[i].name);
        BUF_LIT(&b, ",");
        buf_str(&b, my_bc[i].string);
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "]");
    if (DelayFlag) {
        BUF_LIT(&b, ",\"delays\":[");
        for (i = 0; i < NODE; i++) {
            if (i) BUF_LIT(&b, ",");
            BUF_LIT(&b, "[");
            buf_str(&b, uvar_names[i]);
            BUF_LIT(&b, ",");
            buf_str(&b, delay_string[i]);
            BUF_LIT(&b, "]");
        }
        BUF_LIT(&b, "]");
    }
    /* pixel to plot coordinates of the active window and the AUTO diagram,
       for the x,y readout under the mouse (scale_to_real, auto_motion_xy) */
    get_draw_area();
    buf_printf(&b, ",\"view\":{\"win\":%lu,\"left\":%d,\"right\":%d,\"top\":%d,\"bottom\":%d,"
               "\"xlo\":%g,\"xhi\":%g,\"ylo\":%g,\"yhi\":%g,\"three\":%d",
               (unsigned long)plot_windows.draw_win, DLeft, DRight, DTop, DBottom, plot_windows.current->xlo, plot_windows.current->xhi,
               plot_windows.current->ylo, plot_windows.current->yhi, plot_windows.current->ThreeDFlag);
    /* a 3D window's angles (view3d); only then are they set at all */
    if (plot_windows.current->ThreeDFlag && isfinite(plot_windows.current->Theta) && isfinite(plot_windows.current->Phi))
        buf_printf(&b, ",\"theta\":%g,\"phi\":%g", plot_windows.current->Theta, plot_windows.current->Phi);
    BUF_LIT(&b, "}");
    if (Auto.exist)
        buf_printf(&b, ",\"auto\":{\"x0\":%d,\"y0\":%d,\"wid\":%d,\"hgt\":%d,\"xmin\":%g,\"xmax\":%g,"
                   "\"ymin\":%g,\"ymax\":%g}", Auto.x0, Auto.y0, Auto.wid, Auto.hgt, Auto.xmin, Auto.xmax,
                   Auto.ymin, Auto.ymax);
    buf_printf(&b, ",\"rows\":%d,\"menu\":%d,\"win\":%lu", my_browser.maxrow, help_menu,
               (unsigned long)plot_windows.draw_win);
    if (xpp_session_set_file()[0]) {
        BUF_LIT(&b, ",\"session\":{\"set\":");
        buf_str(&b, xpp_session_set_file());
        if (xpp_session_auto_file()[0]) {
            BUF_LIT(&b, ",\"auto\":");
            buf_str(&b, xpp_session_auto_file());
        }
        BUF_LIT(&b, "}");
    }
    BUF_LIT(&b, "}");
    send_buf(&b);
    xpp_free(b.s);
}

void j_state_dirty(void) { state_dirty = 1; }

void send_state_if_dirty(void)
{
    if (state_dirty) send_state();
}

/* ---- data browser ---------------------------------------------------------------
   The client shows a scrolling table and asks for the block of rows and
   columns it can see; my_browser.row0 is the selected row the core's
   browser commands (Get, First, Last, Find) use. */

namespace {

int br_from, br_count, br_col = 1, br_ncol = 1;
int browser_dirty;

void buf_float(Buf *b, double z, int digits)
{
    if (z != z || z > 1e300 || z < -1e300) BUF_LIT(b, "null"); /* not JSON numbers */
    else buf_printf(b, "%.*g", digits, z);
}

void send_browser(void)
{
    Buf b = {0};
    int i, j, last, maxcol = my_browser.maxcol;
    browser_dirty = 0;
    if (br_from > my_browser.maxrow - 1) br_from = my_browser.maxrow > 0 ? my_browser.maxrow - 1 : 0;
    if (br_from < 0) br_from = 0;
    if (br_col > maxcol - 1) br_col = maxcol - 1;
    if (br_col < 1) br_col = 1;
    buf_printf(&b, "{\"ev\":\"browser\",\"rows\":%d,\"row0\":%d,\"start\":%d,\"end\":%d,\"cols\":[\"T\"",
               my_browser.dataflag ? my_browser.maxrow : 0, my_browser.row0, my_browser.istart, my_browser.iend);
    for (j = 1; j < maxcol; j++) {
        BUF_LIT(&b, ",");
        buf_str(&b, uvar_names[j - 1]);
    }
    buf_printf(&b, "],\"from\":%d,\"col\":%d,\"data\":[", br_from, br_col);
    last = my_browser.dataflag ? br_from + br_count : br_from;
    if (last > my_browser.maxrow) last = my_browser.maxrow;
    for (i = br_from; i < last; i++) {
        if (i > br_from) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        /* 9 significant digits read back as exactly the stored floats (as series) */
        buf_float(&b, my_browser.data[0][i], 9);
        for (j = br_col; j < br_col + br_ncol && j < maxcol; j++) {
            BUF_LIT(&b, ",");
            buf_float(&b, my_browser.data[j][i], 9);
        }
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "]}");
    send_buf(&b);
    xpp_free(b.s);
}

} // namespace

/* {"cmd":"browser","from":row,"count":n,"col":first column,"ncol":n}: the
   block the client can see; answered at once, even during a prompt */
void browser_rows(const char *line)
{
    br_from = (int)get_num(line, "from", 0);
    br_count = (int)get_num(line, "count", 100);
    br_col = (int)get_num(line, "col", 1);
    br_ncol = (int)get_num(line, "ncol", 20);
    if (br_count > 2000) br_count = 2000;
    if (br_ncol > 500) br_ncol = 500;
    send_browser();
}

void j_browser_changed(int i)
{
    (void)i;
    plot_data_changed();
    state_dirty = 1;
    browser_dirty = 1;
    aplot_changed(); /* X11 redraws an auto-redrawn array plot on expose */
}

/* {"cmd":"browser","op":...,"row":selected row} */
void browser_command(const char *line)
{
    char o[16];
    int row = (int)get_num(line, "row", -1);
    get_str(line, "op", o, sizeof o);
    if (row >= 0 && row < my_browser.maxrow) my_browser.row0 = row;
    if (strcmp(o, "find") == 0) data_find(&my_browser);
    else if (strcmp(o, "get") == 0) data_get(&my_browser);
    else if (strcmp(o, "replace") == 0) data_replace(&my_browser);
    else if (strcmp(o, "unreplace") == 0) data_unreplace(&my_browser);
    else if (strcmp(o, "table") == 0) data_table(&my_browser);
    else if (strcmp(o, "load") == 0) data_read(&my_browser);
    else if (strcmp(o, "write") == 0) data_write(&my_browser);
    else if (strcmp(o, "first") == 0) data_first(&my_browser);
    else if (strcmp(o, "last") == 0) data_last(&my_browser);
    else if (strcmp(o, "restore") == 0) data_restore(&my_browser);
    else if (strcmp(o, "addcol") == 0) data_add_col(&my_browser);
    else if (strcmp(o, "delcol") == 0) data_del_col(&my_browser);
    else if (strcmp(o, "close") == 0) br_count = 0;
    browser_dirty = 1;
}

/* the block the client sees, when the data changed */
void browser_update(void)
{
    if (browser_dirty && br_count) send_browser();
}

/* the ICs box's xvst (0) and pp (1) buttons: {"cmd":"plotvars","how":0,"names":[...]} */
void plotvars_command(const char *line)
{
    int isck[MAXODE], i, n = NODE + NMarkov;
    const char *arr = js_find(line, "names");
    char name[NAME_IN];
    memset(isck, 0, sizeof isck);
    for (i = 0; arr && js_elem(arr, i); i++) {
        int k;
        if (!js_string(js_elem(arr, i), name, sizeof name)) continue;
        for (k = 0; k < n; k++)
            if (strcasecmp(uvar_names[k], name) == 0) isck[k] = 1;
    }
    if ((int)get_num(line, "how", 0) == 2) {
        /* arry: the array of variables from the first to the second checked */
        int list[2], k = 0;
        for (i = 0; i < n && k < 2; i++)
            if (isck[i]) list[k++] = i + 1;
        if (k == 2) optimize_aplot(list);
        return;
    }
    plot_checked_vars((int)get_num(line, "how", 0), isck, n);
}

/* {"cmd":"data","events":["series","plots","nullclines","dfield","marks","ani","autoinfo","autosettings"],"enc":"f32"}:
   the data events the client wants from now on (an empty list stops them);
   each is sent at the end of this command, which is what a client that
   (re)connects needs. hello.features lists the names known here. "enc":"f32"
   sends the events' value arrays as base64 of little-endian float32,
   anything else as JSON numbers. */
void data_command(const char *line)
{
    const char *arr = js_find(line, "events");
    char name[32], enc[8];
    int i, series = 0, plots = 0, nullclines = 0, dfield = 0, marks = 0, ani = 0, autoinfo = 0, autosettings = 0, f32;
    for (i = 0; arr && js_elem(arr, i); i++) {
        if (!js_string(js_elem(arr, i), name, sizeof name)) continue;
        if (strcmp(name, "series") == 0) series = 1;
        else if (strcmp(name, "plots") == 0) plots = 1;
        else if (strcmp(name, "nullclines") == 0) nullclines = 1;
        else if (strcmp(name, "dfield") == 0) dfield = 1;
        else if (strcmp(name, "marks") == 0) marks = 1;
        else if (strcmp(name, "ani") == 0) ani = 1;
        else if (strcmp(name, "autoinfo") == 0) autoinfo = 1;
        else if (strcmp(name, "autosettings") == 0) autosettings = 1;
    }
    f32 = get_str(line, "enc", enc, sizeof enc) && strcmp(enc, "f32") == 0;
    plot_data_subscribe(series, plots, f32);
    phase_data_subscribe(nullclines, dfield, f32);
    marks_data_subscribe(marks, f32);
    ani_data_subscribe(ani);
    auto_data_subscribe(autoinfo);
    auto_settings_subscribe(autosettings);
}

/* the equations window: one "dX/dT=..." line per equation (eig_list.c) */
void send_equations(void)
{
    Buf b = {0}, line = {0};
    int i;
    BUF_LIT(&b, "{\"ev\":\"equations\",\"lines\":[");
    for (i = 0; i < NEQ; i++) {
        const char *name = uvar_names[i], *rhs = ode_names[i] ? ode_names[i] : "";
        line.len = 0;
        if (i < NODE && EqType[i] != 1 && METHOD > 0) BUF_LIT(&line, "d");
        buf_add(&line, name, strlen(name));
        if (i < NODE && EqType[i] == 1) BUF_LIT(&line, "(t)");
        else if (i < NODE && METHOD == 0) BUF_LIT(&line, "(n+1)");
        else if (i < NODE) BUF_LIT(&line, "/dT");
        BUF_LIT(&line, "=");
        buf_add(&line, rhs, strlen(rhs));
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, line.s);
    }
    BUF_LIT(&b, "]}");
    send_buf(&b);
    xpp_free(b.s);
    xpp_free(line.s);
}
void j_state_dirty_i(int i) { (void)i; state_dirty = 1; }
void j_state_dirty_is(int i, const char *s) { (void)i; (void)s; state_dirty = 1; }

/* ---- values ---------------------------------------------------------------- */

namespace {

/* one value of a "set": {"kind":"par|ic|bc|delay","name":...,"value":number
   or "text":...}. Text is what the user would type in the X11 box: a number
   or %formula for parameters and ICs, an expression for BCs and delays.
   0 when set (or nothing to set), -1 on a formula that does not evaluate. */
int apply_value(const char *line)
{
    char kind[16], name[NAME_IN], text[256];
    double z;
    int type, i, n, index = -1;
    get_str(line, "kind", kind, sizeof kind);
    get_str(line, "name", name, sizeof name);
    if (!get_str(line, "text", text, sizeof text))
        snprintf(text, sizeof text, "%.16g", get_num(line, "value", 0));
    if (strcmp(kind, "par") == 0) type = 1;       /* PARAMBOX */
    else if (strcmp(kind, "ic") == 0) type = 2;   /* ICBOX */
    else if (strcmp(kind, "delay") == 0) type = 3; /* DELAYBOX */
    else if (strcmp(kind, "bc") == 0) type = 4;   /* BCBOX */
    else return 0;
    n = type == 1 ? NUPAR : type == 2 ? NODE + NMarkov : NODE;
    /* BC names are not unique ("0="): those come by index */
    index = (int)get_num(line, "index", -1);
    if (index >= n) index = -1;
    for (i = 0; index < 0 && i < n; i++) {
        const char *s = type == 1 ? upar_names[i] : type == 4 ? my_bc[i].name : uvar_names[i];
        if (s && strcasecmp(s, name) == 0) index = i;
    }
    state_dirty = 1;
    if (index < 0) return 0;
    if (box_set_value(type, index, text, &z) == -1) {
        j_err_msg("Bad formula");
        return -1;
    }
    box_values_loaded(type);
    return 0;
}

} // namespace

/* {"cmd":"set", one value's members (apply_value), or "values":[{...}...]
   to set several in one command, "rerun":1 to integrate again afterwards
   as a slider does (only when every value was set), or "kind":"ic",
   "from":"last" for the initial conditions from where the last run ended,
   what Initialconds/Last starts from, without a run} */
void apply_set(const char *line)
{
    const char *values = js_find(line, "values");
    char from[8];
    int i, bad = 0;
    if (get_str(line, "from", from, sizeof from)) {
        if (strcmp(from, "last") != 0) return;
        if (!INFLAG) {
            j_err_msg("No prior solution");
            return;
        }
        get_ic(0, MyData); /* integrate.c do_init_data M_IL: last_ic = the current state */
        state_dirty = 1;
        return;
    }
    if (values)
        for (i = 0; js_elem(values, i); i++) bad |= apply_value(js_elem(values, i));
    else
        bad = apply_value(line);
    if (!bad && get_num(line, "rerun", 0)) slider_rerun();
}

/* {"cmd":"default","kind":"par|ic","rerun":1}: the model file's values */
void default_command(const char *line)
{
    char kind[16];
    get_str(line, "kind", kind, sizeof kind);
    if (strcmp(kind, "par") == 0) set_default_params();
    else set_default_ics();
    if (get_num(line, "rerun", 0)) slider_rerun();
}

/* a parameter slider moved: {"cmd":"slide","name":...,"value":v,"rerun":1} */
void slide_command(const char *line)
{
    char name[NAME_IN];
    int type, index;
    get_str(line, "name", name, sizeof name);
    if (find_par_or_var(name, &type, &index)) {
        set_par_or_var(name, type, index, get_num(line, "value", 0));
        state_dirty = 1;
        if (get_num(line, "rerun", 1)) slider_rerun();
    }
}

/* a stored row (integrate.c row_stored): the data events' appends, and at
   the start of a run (storage starting again) the state, so a client shows
   the initial conditions the run starts from (Initialconds/Last changed
   them) while it runs, not after */
void j_rows_stored(int nrows)
{
    static int last = INT_MAX;
    if (nrows <= last) {
        state_dirty = 1;
        json_flush();
    }
    last = nrows;
    plot_data_rows_stored(nrows);
}

/* ---- equilibria, source ------------------------------------------------------ */

namespace {

/* the last equilibrium shown, for its Import button */
double last_eq[MAXODE];
int last_eq_n;

} // namespace

void j_show_eq_box(int cp, int cm, int rp, int rm, int im, double *y, double *ev, int n)
{
    Buf b = {0};
    int i;
    redraw_ics();
    for (i = 0; i < n && i < MAXODE; i++) last_eq[i] = y[i];
    last_eq_n = n < MAXODE ? n : MAXODE;
    buf_printf(&b, "{\"ev\":\"equilibrium\",\"type\":\"%s\",\"cplus\":%d,\"cminus\":%d,"
               "\"im\":%d,\"rplus\":%d,\"rminus\":%d,\"values\":[",
               eq_stability(cp, rp, im), cp, cm, im, rp, rm);
    for (i = 0; i < n; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, uvar_names[i]);
        buf_printf(&b, ",%.16g]", y[i]);
    }
    BUF_LIT(&b, "]");
    if (ev) { /* the Jacobian's eigenvalues, (re, im) pairs (gear.c eigen) */
        BUF_LIT(&b, ",\"eigenvalues\":[");
        for (i = 0; i < n; i++) buf_printf(&b, "%s[%.16g,%.16g]", i ? "," : "", ev[2 * i], ev[2 * i + 1]);
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "}");
    send_buf(&b);
    xpp_free(b.s);
}

/* the equilibrium window's Import */
void eqimport_command(void)
{
    if (last_eq_n) eq_import(last_eq, last_eq_n);
}

void j_make_txtview(void)
{
    Buf b = {0};
    int i;
    BUF_LIT(&b, "{\"ev\":\"source\",\"lines\":");
    buf_str_array(&b, save_eqn, NLINES);
    /* comments; one with an action runs it when picked ({"cmd":"action"}) */
    BUF_LIT(&b, ",\"comments\":[");
    for (i = 0; i < n_comments; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, comments[i].text);
        buf_printf(&b, ",%d]", comments[i].aflag > 0);
    }
    BUF_LIT(&b, "]}");
    send_buf(&b);
    xpp_free(b.s);
}

/* a comment of the source window picked: {"cmd":"action","index":i} runs
   its action */
void action_command(const char *line)
{
    int i = (int)get_num(line, "index", -1);
    if (i >= 0 && i < n_comments && comments[i].aflag > 0) do_txt_action(comments[i].action);
}

} // namespace xpp::json
