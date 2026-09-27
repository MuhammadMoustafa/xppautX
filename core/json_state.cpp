/* The model's state as the client sees it: the state event (parameters,
   ICs, BCs, delays, the view), the data browser, the edits that change
   them (set, default, slide), the data events a client subscribes to, and
   the equations, source and equilibrium windows. */
#include "model.h"
#include "session.h"
#include "ui_json_internal.h"
#include "load_eqn.h"
#include "storage.h"
#include "xpp_util.h"
#include "graphics.h"
#include "integrate.h"
#include "browse.h"
#include "auto_nox.h"
#include "derived.h"
#include "form_ode.h"
#include "parserslow.h"
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
#include "menudrive.h"
#include "delay_handle.h"
#include <array>
#include <string>

/* the core's own globals and functions that have no header of their own */
extern "C" {
}

namespace xpp::json {

namespace {

int state_dirty;

} // namespace

/* ---- state ------------------------------------------------------------------ */

void send_state(void)
{
    xpp::Session &s=xpp::session();
    Buf b;
    int i;
    double z;
    state_dirty = 0;
    evaluate_derived();
    BUF_LIT(&b, "{\"ev\":\"state\",\"pars\":[");
    for (i = 0; i < xpp::model().nupar; i++) {
        get_val(xpp::model().upar_names[i], &z);
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, xpp::model().upar_names[i]);
        BUF_LIT(&b, ",");
        buf_num(&b, z, 16);
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "],\"ics\":[");
    for (i = 0; i < xpp::model().node + xpp::model().nmarkov; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, xpp::model().uvar_names[i]);
        BUF_LIT(&b, ",");
        buf_num(&b, s.last_ic[i], 16);
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "]");
    /* where the last run ended (MyData, what Initialconds/Last starts from) */
    if (s.numerics.inflag) {
        BUF_LIT(&b, ",\"now\":[");
        for (i = 0; i < xpp::model().node + xpp::model().nmarkov; i++) {
            if (i) BUF_LIT(&b, ",");
            buf_num(&b, s.data_store.current[i], 16);
        }
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, ",\"bcs\":[");
    for (i = 0; i < xpp::model().node; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, s.bcs[i].name.data());
        BUF_LIT(&b, ",");
        buf_str(&b, s.bcs[i].string.data());
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "]");
    if (s.delay.flag) {
        BUF_LIT(&b, ",\"delays\":[");
        for (i = 0; i < xpp::model().node; i++) {
            if (i) BUF_LIT(&b, ",");
            BUF_LIT(&b, "[");
            buf_str(&b, xpp::model().uvar_names[i]);
            BUF_LIT(&b, ",");
            buf_str(&b, s.delay_string[i].c_str());
            BUF_LIT(&b, "]");
        }
        BUF_LIT(&b, "]");
    }
    /* pixel to plot coordinates of the active window and the AUTO diagram,
       for the x,y readout under the mouse (scale_to_real, auto_motion_xy) */
    get_draw_area();
    buf_format(&b, ",\"view\":{{\"win\":{:d},\"left\":{:d},\"right\":{:d},\"top\":{:d},\"bottom\":{:d},"
               "\"xlo\":{:g},\"xhi\":{:g},\"ylo\":{:g},\"yhi\":{:g},\"three\":{:d}",
               s.plot_windows.draw_win, s.drawing.d_left, s.drawing.d_right, s.drawing.d_top, s.drawing.d_bottom, s.plot_windows.current->xlo, s.plot_windows.current->xhi,
               s.plot_windows.current->ylo, s.plot_windows.current->yhi, s.plot_windows.current->ThreeDFlag);
    /* a 3D window's angles (view3d); only then are they set at all */
    if (s.plot_windows.current->ThreeDFlag && isfinite(s.plot_windows.current->Theta) && isfinite(s.plot_windows.current->Phi))
        buf_format(&b, ",\"theta\":{:g},\"phi\":{:g}", s.plot_windows.current->Theta, s.plot_windows.current->Phi);
    BUF_LIT(&b, "}");
    if (s.auto_state.bifur.exist)
        buf_format(&b, ",\"auto\":{{\"x0\":{:d},\"y0\":{:d},\"wid\":{:d},\"hgt\":{:d},\"xmin\":{:g},\"xmax\":{:g},"
                   "\"ymin\":{:g},\"ymax\":{:g}}}", s.auto_state.bifur.x0, s.auto_state.bifur.y0, s.auto_state.bifur.wid, s.auto_state.bifur.hgt, s.auto_state.bifur.xmin, s.auto_state.bifur.xmax,
                   s.auto_state.bifur.ymin, s.auto_state.bifur.ymax);
    buf_format(&b, ",\"rows\":{:d},\"menu\":{:d},\"win\":{:d}", s.browser.view.maxrow, help_menu, s.plot_windows.draw_win);
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
    else buf_format(b, "{:.{}g}", z, digits);
}

void send_browser(void)
{
    xpp::Session &s=xpp::session();
    Buf b;
    int i, j, last, maxcol = s.browser.view.maxcol;
    browser_dirty = 0;
    if (br_from > s.browser.view.maxrow - 1) br_from = s.browser.view.maxrow > 0 ? s.browser.view.maxrow - 1 : 0;
    if (br_from < 0) br_from = 0;
    if (br_col > maxcol - 1) br_col = maxcol - 1;
    if (br_col < 1) br_col = 1;
    buf_format(&b, "{{\"ev\":\"browser\",\"rows\":{:d},\"row0\":{:d},\"start\":{:d},\"end\":{:d},\"cols\":[\"T\"",
               s.browser.view.dataflag ? s.browser.view.maxrow : 0, s.browser.view.row0, s.browser.view.istart, s.browser.view.iend);
    for (j = 1; j < maxcol; j++) {
        BUF_LIT(&b, ",");
        buf_str(&b, xpp::model().uvar_names[j - 1]);
    }
    buf_format(&b, "],\"from\":{:d},\"col\":{:d},\"data\":[", br_from, br_col);
    last = s.browser.view.dataflag ? br_from + br_count : br_from;
    if (last > s.browser.view.maxrow) last = s.browser.view.maxrow;
    for (i = br_from; i < last; i++) {
        if (i > br_from) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        /* 9 significant digits read back as exactly the stored floats (as series) */
        buf_float(&b, s.browser.view.data[0][i], 9);
        for (j = br_col; j < br_col + br_ncol && j < maxcol; j++) {
            BUF_LIT(&b, ",");
            buf_float(&b, s.browser.view.data[j][i], 9);
        }
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "]}");
    send_buf(&b);
}

} // namespace

/* {"cmd":"browser","from":row,"count":n,"col":first column,"ncol":n}: the
   block the client can see; answered at once, even during a prompt */
void browser_rows(const char *line)
{
    br_from = get_int(line, "from", 0);
    br_count = get_int(line, "count", 100);
    br_col = get_int(line, "col", 1);
    br_ncol = get_int(line, "ncol", 20);
    if (br_count > 2000) br_count = 2000;
    if (br_ncol > 500) br_ncol = 500;
    send_browser();
}

void j_browser_changed(int)
{
    plot_data_changed();
    state_dirty = 1;
    browser_dirty = 1;
    aplot_changed(); /* X11 redraws an auto-redrawn array plot on expose */
}

/* {"cmd":"browser","op":...,"row":selected row} */
void browser_command(const char *line)
{
    xpp::Session &s=xpp::session();
    std::string o;
    int row = get_int(line, "row", -1);
    get_string(line, "op", o, 16);
    if (row >= 0 && row < s.browser.view.maxrow) s.browser.view.row0 = row;
    if (o == "find") data_find(&s.browser.view);
    else if (o == "get") data_get(&s.browser.view);
    else if (o == "replace") data_replace(&s.browser.view);
    else if (o == "unreplace") data_unreplace(&s.browser.view);
    else if (o == "table") data_table(&s.browser.view);
    else if (o == "load" || o == "write") {
        /* Save data's and Load's choices, each asked for when not given */
        std::string what, format, name;
        get_string(line, "what", what, 8);
        get_string(line, "format", format, 16);
        get_string(line, "name", name, XPP_MAX_NAME);
        if (o == "load") data_read(&s.browser.view, format, name);
        else data_write(&s.browser.view, what, format, name);
    }
    else if (o == "first") data_first(&s.browser.view);
    else if (o == "last") data_last(&s.browser.view);
    else if (o == "restore") data_restore(&s.browser.view);
    else if (o == "addcol") data_add_col(&s.browser.view);
    else if (o == "delcol") data_del_col(&s.browser.view);
    else if (o == "close") br_count = 0;
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
    std::array<int, MAXODE> isck{};
    int i, n = xpp::model().node + xpp::model().nmarkov;
    const char *arr = js_find(line, "names");
    std::string name;
    for (i = 0; arr && js_elem(arr, i); i++) {
        if (!js_string(js_elem(arr, i), name, NAME_IN)) continue;
        for (int k = 0; k < n; k++)
            if (xpp::equal_ignoring_case(xpp::model().uvar_names[k], name)) isck[k] = 1;
    }
    if (get_int(line, "how", 0) == 2) {
        /* arry: the array of variables from the first to the second checked */
        std::array<int, 2> list;
        int k = 0;
        for (i = 0; i < n && k < 2; i++)
            if (isck[i]) list[k++] = i + 1;
        if (k == 2) optimize_aplot(list.data());
        return;
    }
    plot_checked_vars(get_int(line, "how", 0), isck.data(), n);
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
    std::string name, enc;
    int i, series = 0, plots = 0, nullclines = 0, dfield = 0, marks = 0, ani = 0, autoinfo = 0, autosettings = 0, f32;
    for (i = 0; arr && js_elem(arr, i); i++) {
        if (!js_string(js_elem(arr, i), name, 32)) continue;
        if (name == "series") series = 1;
        else if (name == "plots") plots = 1;
        else if (name == "nullclines") nullclines = 1;
        else if (name == "dfield") dfield = 1;
        else if (name == "marks") marks = 1;
        else if (name == "ani") ani = 1;
        else if (name == "autoinfo") autoinfo = 1;
        else if (name == "autosettings") autosettings = 1;
    }
    f32 = get_string(line, "enc", enc, 8) && enc == "f32";
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
    Buf b, line;
    int i;
    BUF_LIT(&b, "{\"ev\":\"equations\",\"lines\":[");
    for (i = 0; i < xpp::model().neq; i++) {
        const std::string &name = xpp::model().uvar_names[i];
        const char *rhs = xpp::model().formulas[i].c_str();
        line.s.clear();
        if (i < xpp::model().node && xpp::model().eq_type[i] != 1 && xpp::session().numerics.method > 0) BUF_LIT(&line, "d");
        buf_add(&line, name.data(), name.size());
        if (i < xpp::model().node && xpp::model().eq_type[i] == 1) BUF_LIT(&line, "(t)");
        else if (i < xpp::model().node && xpp::session().numerics.method == 0) BUF_LIT(&line, "(n+1)");
        else if (i < xpp::model().node) BUF_LIT(&line, "/dT");
        BUF_LIT(&line, "=");
        buf_add(&line, rhs, strlen(rhs));
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, line.s.c_str());
    }
    BUF_LIT(&b, "]}");
    send_buf(&b);
}
void j_state_dirty_i(int) { state_dirty = 1; }
void j_state_dirty_is(int, const char *) { state_dirty = 1; }

/* ---- values ---------------------------------------------------------------- */

namespace {

/* one value of a "set": {"kind":"par|ic|bc|delay","name":...,"value":number
   or "text":...}. Text is what the user would type in the X11 box: a number
   or %formula for parameters and ICs, an expression for BCs and delays.
   0 when set (or nothing to set), -1 on a formula that does not evaluate. */
int apply_value(const char *line)
{
    std::string kind, name, text;
    double z;
    int type, i, n, index = -1;
    get_string(line, "kind", kind, 16);
    get_string(line, "name", name, NAME_IN);
    if (!get_string(line, "text", text, 256)) text = xpp::format("{:.16g}", get_num(line, "value", 0));
    if (kind == "par") type = 1;        /* PARAMBOX */
    else if (kind == "ic") type = 2;    /* ICBOX */
    else if (kind == "delay") type = 3; /* DELAYBOX */
    else if (kind == "bc") type = 4;    /* BCBOX */
    else return 0;
    n = type == 1 ? xpp::model().nupar : type == 2 ? xpp::model().node + xpp::model().nmarkov : xpp::model().node;
    /* BC names are not unique ("0="): those come by index */
    index = get_int(line, "index", -1);
    if (index >= n) index = -1;
    for (i = 0; index < 0 && i < n; i++) {
        const char *bc = type == 4 ? xpp::session().bcs[i].name.data() : nullptr;
        if (type == 4 ? bc && xpp::equal_ignoring_case(bc, name)
                      : xpp::equal_ignoring_case(type == 1 ? xpp::model().upar_names[i] : xpp::model().uvar_names[i], name))
            index = i;
    }
    state_dirty = 1;
    if (index < 0) return 0;
    if (box_set_value(type, index, text.c_str(), &z) == -1) {
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
    std::string from;
    int i, bad = 0;
    if (get_string(line, "from", from, 8)) {
        if (from != "last") return;
        if (!xpp::session().numerics.inflag) {
            j_err_msg("No prior solution");
            return;
        }
        get_ic(0, xpp::session().data_store.current); /* integrate.c do_init_data M_IL: last_ic = the current state */
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
    std::string kind;
    get_string(line, "kind", kind, 16);
    if (kind == "par") set_default_params();
    else set_default_ics();
    if (get_num(line, "rerun", 0)) slider_rerun();
}

/* a parameter slider moved: {"cmd":"slide","name":...,"value":v,"rerun":1} */
void slide_command(const char *line)
{
    std::string name;
    int type, index;
    get_string(line, "name", name, NAME_IN);
    if (find_par_or_var(name.c_str(), &type, &index)) {
        set_par_or_var(name.c_str(), type, index, get_num(line, "value", 0));
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
    Buf b;
    int i;
    redraw_ics();
    for (i = 0; i < n && i < MAXODE; i++) last_eq[i] = y[i];
    last_eq_n = n < MAXODE ? n : MAXODE;
    buf_format(&b, "{{\"ev\":\"equilibrium\",\"type\":\"{}\",\"cplus\":{:d},\"cminus\":{:d},"
               "\"im\":{:d},\"rplus\":{:d},\"rminus\":{:d},\"values\":[",
               eq_stability(cp, rp, im), cp, cm, im, rp, rm);
    for (i = 0; i < n; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, xpp::model().uvar_names[i]);
        BUF_LIT(&b, ",");
        buf_num(&b, y[i], 16);
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "]");
    if (ev) { /* the Jacobian's eigenvalues, (re, im) pairs (gear.c eigen) */
        BUF_LIT(&b, ",\"eigenvalues\":[");
        for (i = 0; i < n; i++) {
            if (i) BUF_LIT(&b, ",");
            BUF_LIT(&b, "[");
            buf_num(&b, ev[2 * i], 16);
            BUF_LIT(&b, ",");
            buf_num(&b, ev[2 * i + 1], 16);
            BUF_LIT(&b, "]");
        }
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "}");
    send_buf(&b);
}

/* the equilibrium window's Import */
void eqimport_command(void)
{
    if (last_eq_n) eq_import(last_eq, last_eq_n);
}

void j_make_txtview(void)
{
    Buf b;
    const xpp::Model &m = xpp::model();
    BUF_LIT(&b, "{\"ev\":\"source\",\"lines\":");
    buf_str_array(&b, m.source);
    /* comments; one with an action runs it when picked ({"cmd":"action"}) */
    BUF_LIT(&b, ",\"comments\":[");
    for (std::size_t i = 0; i < m.comments.size(); i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, m.comments[i].text);
        buf_format(&b, ",{:d}]", m.comments[i].aflag > 0);
    }
    BUF_LIT(&b, "]}");
    send_buf(&b);
}

/* a comment of the source window picked: {"cmd":"action","index":i} runs
   its action */
void action_command(const char *line)
{
    int i = get_int(line, "index", -1);
    const std::vector<xpp::Model::Comment> &comments = xpp::model().comments;
    if (i >= 0 && i < static_cast<int>(comments.size()) && comments[i].aflag > 0)
        do_txt_action(comments[i].action.c_str());
}

} // namespace xpp::json
