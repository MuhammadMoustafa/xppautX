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
#include "expr.h"
#include "arrayplot.h"
#include "xpp_session.h"
#include "plot_data.h"
#include "phase_data.h"
#include "marks_data.h"
#include "ani_data.h"
#include "auto_data.h"
#include "auto_settings.h"
#include "numerics_settings.h"
#include "lunch-new.h"
#include "histogram.h"
#include "menus.h"
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

void send_state(xpp::Session &s)
{
    const xpp::Model &m = s.model();
    Buf b;
    int i;
    double z;
    state_dirty = 0;
    evaluate_derived();
    BUF_LIT(&b, "{\"ev\":\"state\",\"pars\":[");
    for (i = 0; i < m.nupar; i++) {
        get_val(m.upar_names[i], &z);
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, m.upar_names[i]);
        BUF_LIT(&b, ",");
        buf_num(&b, z, 16);
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "],\"ics\":[");
    for (i = 0; i < m.node + m.nmarkov; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, m.uvar_names[i]);
        BUF_LIT(&b, ",");
        buf_num(&b, s.last_ic[i], 16);
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "]");
    /* where the last run ended (MyData, what Initialconds/Last starts from) */
    if (s.numerics.inflag) {
        BUF_LIT(&b, ",\"now\":[");
        for (i = 0; i < m.node + m.nmarkov; i++) {
            if (i) BUF_LIT(&b, ",");
            buf_num(&b, s.data_store.current[i], 16);
        }
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, ",\"bcs\":[");
    for (i = 0; m.bc_defined > 0 && i < m.node; i++) {
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
        for (i = 0; i < m.node; i++) {
            if (i) BUF_LIT(&b, ",");
            BUF_LIT(&b, "[");
            buf_str(&b, m.uvar_names[i]);
            BUF_LIT(&b, ",");
            buf_str(&b, s.delay_string[i].c_str());
            BUF_LIT(&b, "]");
        }
        BUF_LIT(&b, "]");
    }
    /* pixel to plot coordinates of the active window and the AUTO diagram,
       for the x,y readout under the mouse (scale_to_real, auto_motion_xy) */
    get_draw_area(s);
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
                   "\"ymin\":{:g},\"ymax\":{:g}}}", s.auto_state.bifur.x0, s.auto_state.bifur.y0, s.auto_state.bifur.wid, s.auto_state.bifur.hgt, s.auto_state.axes().xmin, s.auto_state.axes().xmax,
                   s.auto_state.axes().ymin, s.auto_state.axes().ymax);
    buf_format(&b, ",\"rows\":{:d},\"menu\":{:d},\"win\":{:d}", s.browser.view.maxrow, help_menu, s.plot_windows.draw_win);
    if (s.numerics.last_seed) buf_format(&b, ",\"seed\":{:d}", *s.numerics.last_seed);
    const SavedSession &saved = s.saved_session;
    if (!saved.file.empty()) {
        BUF_LIT(&b, ",\"session\":{\"file\":");
        buf_str(&b, saved.file.c_str());
        BUF_LIT(&b, "}");
    }
    buf_recording(&b);
    buf_player(&b);
    BUF_LIT(&b, "}");
    send_buf(&b);
}

void j_state_dirty(void) { state_dirty = 1; }

void send_state_if_dirty(void)
{
    /* at a flush (json_io.cpp), from anywhere: the current session */
    if (state_dirty) send_state(xpp::session());
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

void send_browser(const xpp::Session &s)
{
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
        buf_str(&b, browse_column_name(j));
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
void browser_rows(const xpp::Session &s, const char *line)
{
    br_from = get_int(line, "from", 0);
    br_count = get_int(line, "count", 100);
    br_col = get_int(line, "col", 1);
    br_ncol = get_int(line, "ncol", 20);
    if (br_count > 2000) br_count = 2000;
    if (br_ncol > 500) br_ncol = 500;
    send_browser(s);
}

void j_browser_changed(int)
{
    plot_data_changed();
    state_dirty = 1;
    browser_dirty = 1;
    aplot_changed(); /* X11 redraws an auto-redrawn array plot on expose */
}

/* {"cmd":"browser","op":"load"|"write",...}: Save data's and Load's
   choices given, each asked for when not given (the keys l and w of
   menu_browser_window ask them all); "postprocess": the model's
   @ postprocess on the data (histogram.cpp) */
void browser_command(xpp::Session &s, const char *line)
{
    std::string o, what, format, name;
    get_string(line, "op", o, 16);
    get_string(line, "what", what, 8);
    get_string(line, "format", format, 16);
    get_string(line, "name", name, XPP_MAX_NAME);
    if (o == "load") data_read(s, &s.browser.view, format, name);
    else if (o == "write") data_write(s, &s.browser.view, what, format, name, get_int(line, "replace", 0) != 0);
    else if (o == "postprocess") post_process_stuff(s);
    else j_err_msg(xpp::format("Unknown browser op {}", o).c_str());
    browser_dirty = 1;
}

/* a key of the data browser window (menu_browser_window), on the selected
   row: {"cmd":"key","win":"browser","key":k,"row":r} */
void browser_key(xpp::Session &s, int ch, const char *line)
{
    int row = get_int(line, "row", -1);
    if (row >= 0 && row < s.browser.view.maxrow) s.browser.view.row0 = row;
    switch (xpp_menu_index(&menu_browser_window, ch)) {
    case BK_FIND: data_find(s, &s.browser.view); break;
    case BK_GET: data_get(s, &s.browser.view); break;
    case BK_REPLACE: data_replace(s, &s.browser.view); break;
    case BK_UNREPLACE: data_unreplace(s); break;
    case BK_TABLE: data_table(s, &s.browser.view); break;
    case BK_FIRST: data_first(&s.browser.view); break;
    case BK_LAST: data_last(&s.browser.view); break;
    case BK_RESTORE: data_restore(s,&s.browser.view); break;
    case BK_ADDCOL: data_add_col(s, &s.browser.view); break;
    case BK_DELCOL: data_del_col(s, &s.browser.view); break;
    case BK_LOAD: data_read(s, &s.browser.view, "", ""); break;
    case BK_WRITE: data_write(s, &s.browser.view, "", "", ""); break;
    }
    browser_dirty = 1;
}

/* the block the client sees, when the data changed */
void browser_update(const xpp::Session &s)
{
    if (browser_dirty && br_count) send_browser(s);
}

/* the ICs box's xvst (0) and pp (1) buttons: {"cmd":"plotvars","how":0,"names":[...]} */
void plotvars_command(xpp::Session &s, const char *line)
{
    const xpp::Model &m = s.model();
    std::array<int, MAXODE> isck{};
    int i, n = m.node + m.nmarkov;
    const char *arr = js_find(line, "names");
    std::string name;
    for (i = 0; arr && js_elem(arr, i); i++) {
        if (!js_string(js_elem(arr, i), name)) continue;
        for (int k = 0; k < n; k++)
            if (xpp::equal_ignoring_case(m.uvar_names[k], name)) isck[k] = 1;
    }
    if (get_int(line, "how", 0) == 2) {
        /* arry: the array of variables from the first to the second checked */
        std::array<int, 2> list;
        int k = 0;
        for (i = 0; i < n && k < 2; i++)
            if (isck[i]) list[k++] = i + 1;
        if (k == 2) optimize_aplot(s, list.data());
        return;
    }
    plot_checked_vars(s, get_int(line, "how", 0), isck.data(), n);
}

/* {"cmd":"data","events":["series","plots","nullclines","dfield","marks","ani","autoinfo","autosettings","numerics"],"enc":"f32"}:
   the data events the client wants from now on (an empty list stops them);
   each is sent at the end of this command, which is what a client that
   (re)connects needs. hello.features lists the names known here. "enc":"f32"
   sends the events' value arrays as base64 of little-endian float32,
   anything else as JSON numbers. */
void data_command(xpp::Session &s, const char *line)
{
    const char *arr = js_find(line, "events");
    std::string name, enc;
    int i, series = 0, plots = 0, nullclines = 0, dfield = 0, marks = 0, ani = 0, autoinfo = 0, autosettings = 0,
        numerics = 0, f32;
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
        else if (name == "numerics") numerics = 1;
    }
    f32 = get_string(line, "enc", enc, 8) && enc == "f32";
    plot_data_subscribe(series, plots, f32);
    phase_data_subscribe(nullclines, dfield, f32);
    marks_data_subscribe(marks, f32);
    ani_data_subscribe(ani);
    auto_data_subscribe(autoinfo);
    auto_view_subscribe(autoinfo);
    auto_settings_subscribe(s, autosettings);
    numerics_settings_subscribe(s, numerics);
}

/* the equations window: one "dX/dT=..." line per equation (eig_list.c) */
void send_equations(const xpp::Session &s)
{
    const xpp::Model &m = s.model();
    Buf b, line;
    int i;
    BUF_LIT(&b, "{\"ev\":\"equations\",\"lines\":[");
    for (i = 0; i < m.neq; i++) {
        const std::string &name = m.uvar_names[i];
        const char *rhs = m.formulas[i].c_str();
        line.s.clear();
        if (i < m.node && m.eq_type[i] != 1 && s.numerics.method > 0) BUF_LIT(&line, "d");
        buf_add(&line, name.data(), name.size());
        if (i < m.node && m.eq_type[i] == 1) BUF_LIT(&line, "(t)");
        else if (i < m.node && s.numerics.method == 0) BUF_LIT(&line, "(n+1)");
        else if (i < m.node) BUF_LIT(&line, "/dT");
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

/* one value of a "set": {"kind":"par|ic|bc|delay|num","name":...,"value":number
   or "text":...}. Text is what the user would type in the X11 box: a number
   or %formula for parameters and ICs, an expression for BCs and delays; for
   the numerics (num, W106) a number, or a method's name, the field named by
   its key (numerics_settings.h). 0 when set (or nothing to set), -1 on a
   formula that does not evaluate or a numerics value refused. */
int apply_value(xpp::Session &s, const char *line)
{
    const xpp::Model &m = s.model();
    std::string kind, name, text;
    double z;
    int type, i, n, index = -1;
    get_string(line, "kind", kind, 16);
    get_string(line, "name", name);
    if (!get_string(line, "text", text)) text = xpp::format("{:.16g}", get_num(line, "value", 0));
    if (kind == "par") type = 1;        /* PARAMBOX */
    else if (kind == "ic") type = 2;    /* ICBOX */
    else if (kind == "delay") type = 3; /* DELAYBOX */
    else if (kind == "bc") type = 4;    /* BCBOX */
    else if (kind == "num") {
        std::string why;
        if (numerics_settings_set(s, name, text, why) == 0) return 0;
        j_err_msg(xpp::format("Numerics: {}", why).c_str());
        return -1;
    } else return 0;
    n = type == 1 ? m.nupar : type == 2 ? m.node + m.nmarkov : m.node;
    /* BC names are not unique ("0="): those come by index */
    index = get_int(line, "index", -1);
    if (index >= n) index = -1;
    for (i = 0; index < 0 && i < n; i++) {
        const char *bc = type == 4 ? s.bcs[i].name.data() : nullptr;
        if (type == 4 ? bc && xpp::equal_ignoring_case(bc, name)
                      : xpp::equal_ignoring_case(type == 1 ? m.upar_names[i] : m.uvar_names[i], name))
            index = i;
    }
    state_dirty = 1;
    if (index < 0) return 0;
    if (box_set_value(s, type, index, text.c_str(), &z) == -1) {
        j_err_msg("Bad formula");
        return -1;
    }
    box_values_loaded(s, type);
    return 0;
}

} // namespace

/* {"cmd":"set", one value's members (apply_value), or "values":[{...}...]
   to set several in one command. Never runs anything itself (W69). A
   setting (W106): sent during a computation it applies when that ends,
   never to the run in progress (ui_json.cpp control_line). */
void apply_set(xpp::Session &s, const char *line)
{
    const char *values = js_find(line, "values");
    int i;
    if (values)
        for (i = 0; js_elem(values, i); i++) apply_value(s, js_elem(values, i));
    else
        apply_value(s, line);
}

/* {"cmd":"default","kind":"par|ic"}: the model file's values */
void default_command(xpp::Session &s, const char *line)
{
    std::string kind;
    get_string(line, "kind", kind, 16);
    if (kind == "par") set_default_params(s);
    else set_default_ics(s);
}

/* a parameter slider moved: {"cmd":"slide","name":...,"value":v} (W69: sets
   only, like `set`; the page sends it as part of the next `set` before the
   next computation, never on its own any more, but the command still just
   sets the value for a client that does) */
void slide_command(xpp::Session &s, const char *line)
{
    std::string name;
    int type, index;
    get_string(line, "name", name);
    if (find_par_or_var(name.c_str(), &type, &index)) {
        set_par_or_var(s, name.c_str(), type, index, get_num(line, "value", 0));
        state_dirty = 1;
    }
}

/* {"cmd":"values","op":"write"|"read","kind":"par"|"ic","name":...}: the
   values panel's Save/Load of XPP's own .par/.ic file (docs/protocol.md
   "values"), through lunch-new.cpp's save_parameter_file/save_ic_file/
   load_parameter_file/load_ic_file, which write/read the model's folder
   the way `browser`'s `write` (Save data) does; `name` given skips the
   file ask, empty asks like Save data. */
void values_command(xpp::Session &s, const char *line)
{
    std::string o, kind, name;
    get_string(line, "op", o, 16);
    get_string(line, "kind", kind, 8);
    get_string(line, "name", name, XPP_MAX_NAME);
    if (o == "internset") { /* File/Get par set, the set given by index or name */
        const std::vector<xpp::Model::InternalSet> &sets = s.model().intern_sets;
        int j = get_int(line, "index", -1);
        for (std::size_t i = 0; j < 0 && i < sets.size(); i++)
            if (sets[i].name == name) j = static_cast<int>(i);
        if (j < 0) j_err_msg(xpp::format("No internal set {}", name).c_str());
        else use_intern_set(s, j);
        state_dirty = 1;
        return;
    }
    if (o == "query") {
        if (name.empty()) j_err_msg("values query needs a name");
        else write_values_query(s, name.c_str(), get_int(line, "sets", 0), get_int(line, "pars", 0), get_int(line, "ics", 0));
        return;
    }
    if (kind != "par" && kind != "ic") {
        j_err_msg(xpp::format("values writes or reads par or ic, not {}", kind).c_str());
        return;
    }
    if (o == "write") {
        if (kind == "par") save_parameter_file(name);
        else save_ic_file(name);
        state_dirty = 1;
    } else if (o == "read") {
        if (kind == "par") load_parameter_file(name);
        else load_ic_file(name);
        state_dirty = 1;
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
    plot_data_rows_stored(xpp::session(), nrows); /* an XppUi callback: an entry point (W47d6) */
}

/* ---- equilibria, source ------------------------------------------------------ */

namespace {

/* the last equilibrium shown, for its Import button */
double last_eq[MAXODE];
int last_eq_n;

} // namespace

void state_forget(void)
{
    last_eq_n = 0;
    state_dirty = 1;
    browser_dirty = 1;
}

void j_show_eq_box(int cp, int cm, int rp, int rm, int im, double *y, double *ev, int n)
{
    const xpp::Model &m = xpp::model(); /* an XppUi entry point */
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
        buf_str(&b, m.uvar_names[i]);
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

/* a key of the equilibrium window (menu_equilibrium_window): Import */
void equilibrium_key(xpp::Session &s, int ch)
{
    if (xpp_menu_index(&menu_equilibrium_window, ch) == EK_IMPORT && last_eq_n) eq_import(s, last_eq, last_eq_n);
}

void j_make_txtview(void)
{
    Buf b;
    const xpp::Model &m = xpp::model(); /* an XppUi entry point */
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
void action_command(xpp::Session &s, const char *line)
{
    int i = get_int(line, "index", -1);
    const std::vector<xpp::Model::Comment> &comments = s.model().comments;
    if (i >= 0 && i < static_cast<int>(comments.size()) && comments[i].aflag > 0)
        do_txt_action(s, comments[i].action.c_str());
}

} // namespace xpp::json
