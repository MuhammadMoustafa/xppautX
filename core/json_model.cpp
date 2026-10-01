/* The model the front end serves: its set-up after a load, at the start
   (xppautx_main.cpp) and after File > Open model or Reload (W61,
   model_switch.h), whose load runs here once the command that asked for
   it has returned (handle_line). The page sees a new hello, as it does on
   a reconnection, after the windows of the model before went. */
#include "ui_json.h"
#include "ui_json_internal.h"
#include "model_switch.h"
#include "session.h"
#include "model.h"
#include "xpp_globals.h"
#include "xpp_session.h"
#include "xpp_util.h"
#include "xpp_window.h"
#include "colormap.h"
#include "comline.h"
#include "browse.h"
#include "aniparse.h"
#include "graphics.h"
#include "graf_par.h"
#include "nullcline.h"
#include "menudrive.h"
#include "plot_data.h"
#include "phase_data.h"
#include "marks_data.h"
#include "ani_data.h"
#include "auto_data.h"
#include <vector>

namespace xpp::json {

namespace {

/* init_grafs() without the window: graph 0 is client window 1 */
void init_main_graph(xpp::Session &s)
{
    int i;
    for (i = 0; i < MAXLAB; i++) {
        s.labels[i].use = 0;
        s.labels[i].w = 0;
    }
    for (i = 0; i < MAXGROB; i++) {
        s.grobs[i].w = 0;
        s.grobs[i].use = 0;
    }
    init_bd();
    for (i = 0; i < MAXFRZ; i++) s.frozen_curves.curve[i].use = 0;
    for (i = 0; i < MAXPOP; i++) s.plot_windows.graph[i].Use = 0;
    s.plot_windows.open[0] = 0;
    init_all_graph(s);
    s.plot_windows.graph[0].w = 1;
    s.plot_windows.graph[0].Use = 1;
    s.plot_windows.graph[0].Nullrestore = 1;
    s.plot_windows.count = 1;
    s.plot_windows.draw_win = s.plot_windows.graph[0].w;
    s.plot_windows.active = 0;
    get_draw_area(s);
}

/* the model just loaded: the front end's set-up (what main.c did after
   opening its window), then Reload's kept values, hello, and -anifile's
   animation */
void start_model(xpp::Session &s, const xpp::KeptValues *kept)
{
    xpp::window::set_model(xpp::model_title(s.model()).c_str());
    program.interactive = 1;
    color_table.enabled = 1;                    /* init_X on a colour display */
    s.drawing.axis_var_labels = 1; /* a plot without axis names is hard to read */
    xpp_build_colormap(s.colormap);
    init_main_graph(s);
    init_browser(s);
    ani_zero(s);
    set_extra_graphs(s);
    set_colorization_stuff(s);
    load_command_line_values(s);
    default_window(s);
    if (kept) xpp::restore_values(s, *kept);
    send_hello(s);
    if (s.animation.options.use_file) {
        new_vcr(s);
        get_ani_file(s,s.animation.options.file.c_str());
    }
}

/* the windows besides the main one the page has of the current model */
std::vector<unsigned long> shown_windows(const xpp::Session &s)
{
    std::vector<unsigned long> w;
    for (int i = 1; i < MAXPOP; i++)
        if (s.plot_windows.graph[i].Use) w.push_back(s.plot_windows.graph[i].w);
    if (s.auto_state.bifur.exist) w.push_back(WIN_AUTO);
    if (s.animation.vcr.iexist) w.push_back(WIN_ANI);
    if (s.array_plot.plot.alive) w.push_back(WIN_APLOT);
    return w;
}

} // namespace

xpp::Session &switch_model(xpp::Session &before, const xpp::ModelRequest &req)
{
    const std::vector<unsigned long> shown = shown_windows(before);
    xpp::KeptValues kept;
    if (req.keep_values) kept = xpp::keep_values(before);
    xpp::Session *loaded = xpp::load_requested(before, req);
    if (!loaded) return before; /* the model before goes on */
    /* before is gone: the new model's session from here on */
    xpp::Session &s = *loaded;
    for (unsigned long w : shown) send_window("destroy", w, 0, 0);
    /* what the data modules and this front end recorded of the model before */
    for (int i = 0; i < MAXPOP; i++) {
        phase_data_cleared(i);
        marks_data_cleared(i);
    }
    ani_data_forget();
    auto_data_forget();
    diag_forget();
    plot_data_changed();
    state_forget();
    renew_auto_dir(s);
    start_model(s, req.keep_values ? &kept : nullptr);
    if (req.restore) xpp_saved_restore(s, *req.restore); /* an AUTO or session file */
    redraw_graph(s);
    auto_redraw_for_client(s);
    /* @ runnow=1, as at the start */
    if (s.run_immediately == 1) {
        run_the_commands(s, 4);
        s.run_immediately = 0;
    }
    return s;
}

} // namespace xpp::json

namespace xpp {
void json_ui_start_model(Session &s) { json::start_model(s, nullptr); }
} // namespace xpp
