/* The M_* command switch (run_the_commands): what a model's user buttons (`@ userbut`) name by their
   XPPAUT codes, run through the registry's functions; and the file and movie helpers it calls. */
#include "xpp_files.h"
#include "xpp_ui.h"
#include "image_format.h"
#include "browse.h"
#include "session.h"
#include "command_registry.h"
#include "xpp_session.h"
#include "model_switch.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include "xpp_globals.h"
#include "xpp_util.h"
#include "menus.h"
#include "menudrive.h"

#include "adj2.h"
#include "auto_nox.h"
#include "comline.h"
#include "graf_par.h"
#include "graphics.h"
#include "grobs.h"
#include "integrate.h"
#include "load_eqn.h"
#include "model_save.h"
#include "lunch-new.h"
#include "markov.h"
#include "nullcline.h"
#include "numerics.h"
#include "plot_data.h"
#include "pp_shoot.h"
#include "tabular.h"
#include "torus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "many_pops.h"
#include "kinescope.h"
#include "expr.h"
#include "model.h"

namespace xpp {

void do_movie_com(xpp::Session &s, int c)
{
  XppKinescope &k = s.kinescope;
  std::string base;
  const auto run = [&](const std::string &line) { xpp::ok_or_show(xpp::command::run_button(s, line, xpp::command_place())); };
  switch (c) {
  case 0: run("{\"cmd\":\"kinescope.capture\"}"); break;
  case 1: run("{\"cmd\":\"kinescope.reset\"}"); break;
  case 2: run("{\"cmd\":\"kinescope.play\"}"); break;
  case 3: {
    int cycles = k.cycles, frame_ms = k.frame_ms;
    new_int("Number of cycles", &cycles);
    new_int("Msec between frames", &frame_ms);
    run(xpp::format("{{\"cmd\":\"kinescope.autoplay\",\"cycles\":{},\"frame_ms\":{}}}", cycles, frame_ms));
    break;
  }
  case 4:
    if(!save_ready(s.kinescope.frames>0))return;
    base = xpp::files::output_name(s.model().this_file, ".gif");
    if (!file_selector("Save kinescope frames", base, "*.gif")) return;
    if (!base.empty()) xpp::ok_or_show(ui.movie_save(s, base, IMAGE_FORMAT_GIF));
    break;
  case 5: {
    if(!save_ready(s.kinescope.frames>0))return;
    base = xpp::files::output_name(s.model().this_file, ".gif");
    if (!file_selector("Save kinescope GIF", base, "*.gif")) return;
    xpp::ok_or_show(ui.movie_make_anigif(s, base));
    break;
  }
  }
}

/* a graph for a scope that works on one (all, plot) when more than one is open:
   the user picks it, as a script names it; false when the pick is cancelled
   (Esc or a dismissed menu chooses nothing: a cancel is not an error) */
static bool pick_target_graph(xpp::Session &s, std::string_view scope, std::optional<int> &graph)
{
  if (scope != xpp::set_scope::all && scope != xpp::option_group::plot) return true;
  std::vector<std::string> names;
  std::vector<int> graphs;
  for (int i = 0; i < static_cast<int>(s.plot_windows.graph.size()); i++)
    if (s.plot_windows.graph[i].Use) {
      graphs.push_back(i);
      names.push_back(xpp::format("Window {}{}", s.plot_windows.graph[i].w, i == s.plot_windows.active ? " (active)" : ""));
    }
  if (graphs.size() < 2) return true;
  const int pick = pick_from_list("set_graph", "Window", names);
  if (pick < 0) return false;
  graph = graphs[static_cast<std::size_t>(pick)];
  return true;
}

/* File/Apply set (getparset): one of the model's sets, a scope and a graph
   asked, then applied by xpp::apply_named_set, the one path every front end
   and `applyset` take */
static void get_intern_set(xpp::Session &s)
{
  const std::vector<xpp::Model::InternalSet> &sets = s.model().intern_sets;
  if (sets.empty()) {
    bottom_msg(0, "This model has no named sets: a set line in the model defines one");
    return;
  }
  std::vector<std::string> names;
  for (const xpp::Model::InternalSet &set : sets) names.push_back(set.name);
  const int pick = pick_from_list("param_set", "Param set", names);
  if (pick < 0) return;
  const std::vector<std::string> scopes = xpp::set_scopes();
  const int scope = pick_from_list("set_scope", "Apply", scopes);
  if (scope < 0) return;
  std::optional<int> graph;
  if (!pick_target_graph(s, scopes[static_cast<std::size_t>(scope)], graph)) return;
  xpp::ok_or_show(xpp::apply_named_set(s, names[static_cast<std::size_t>(pick)], scopes[static_cast<std::size_t>(scope)], graph));
}

/* ---- the command switch --------------------------------------------- */

static void do_file_com(xpp::Session &s, int com);

void run_the_commands(xpp::Session &s, MenuCommand com)
{
  if (com < 0) return;
  if (com <= MAX_M_I) {
    xpp::do_init_data(s,com);
    return;
  }
  if (com == M_C) {
    xpp::ok_or_show(xpp::continue_total(s));
    return;
  }
  if (com >= M_SG && com <= M_SC) {
    xpp::find_equilib_com(s,com - M_SG);
    return;
  }
  if (com >= M_NFF && com <= M_NFA) {
    xpp::ok_or_show(xpp::command::nullcline_button(s,com-M_NFF+5,xpp::command_place()));
    return;
  }
  if (com >= M_NN && com <= M_NS) {
    if(com==M_NS-1)return; /* A menu prefix has no operation. */
    xpp::ok_or_show(xpp::command::nullcline_button(s,com==M_NS?4:com-M_NN,xpp::command_place()));
    return;
  }
  if (com >= M_DD && com <= M_DS) {
    xpp::ok_or_show(xpp::command::dirfield_button(s,com-M_DD,xpp::command_place()));
    return;
  }
  if (com >= M_WW && com <= M_WS) {
    window_zoom_com(s,com - M_WW);
    return;
  }
  if (com >= M_AA && com <= M_AC) {
    xpp::do_torus_com(s,com - M_AA);
    return;
  }
  if (com >= M_KC && com <= M_KM) {
    do_movie_com(s, com - M_KC);
    return;
  }
  if (com >= M_GA && com <= M_GC) {
    add_a_curve_com(s,com - M_GA);
    return;
  }
  if (com >= M_GFF && com <= M_GFO) {
    freeze_com(s,com - M_GFF);
    return;
  }
  if (com == M_GFKK || com == M_GFKN) {
    key_frz_com(s,com - M_GFKN);
    return;
  }
  if (com == M_UKE || com == M_UKV) {
    xpp::new_lookup_com(s, com - M_UKE);
    return;
  }
  if (com == M_R) {
    drw_all_scrns(s);
    plot_data_picture(s, 1); /* a data client draws the current data again */
    return;
  }
  if (com == M_EE) {
    xpp::clear_windows(s);
    return;
  }
  if (com == M_X) {
    xi_vs_t(s);
    return;
  }
  if (com == M_3) {
    get_3d_par_com(s);
    return;
  }
  if (com == M_P) {
    xpp::new_parameter(s);
    return;
  }
  if(com>=M_MC && com<=M_MS){
    xpp::ok_or_show(xpp::command::window_button(s,com-M_MC,xpp::command_place()));
    return;
  }
  if (com >= M_FP && com <= M_FG) {
    do_file_com(s, com);
    return;
  }
  if (com >= M_TT && com <= M_TS) {
    do_gr_objs_com(s, com - M_TT);
    return;
  }
  if (com >= M_TEM && com <= M_TED) {
    edit_object_com(s, com - M_TEM);
    return;
  }
  if (com >= M_BR && com <= M_BH) {
    xpp::find_bvp_com(s,com - M_BR);
    return;
  }
  if (com >= M_V2 && com <= M_VT) change_view_com(s,com - M_V2);
  if (com >= M_UAN && com <= M_UAR) xpp::make_adj_com(s,com - M_UAN);
  if (com >= M_UCN && com <= M_UCA) xpp::set_col_par_com(s,com - M_UCN);
  if (com >= M_UPN && com <= M_UPP) xpp::get_pmap_pars_com(s,com - M_UPN);
  if (com >= M_UHN && com <= M_UH2) xpp::do_stochast_com(s,com - M_UHN);
  if(com==M_UN){
    int mesh=s.numerics.nullcline_mesh;
    new_int("Nullcline mesh",&mesh);
    xpp::ok_or_show(xpp::command::run_button(s,xpp::format("{{\"cmd\":\"nullcline.set_mesh\",\"mesh\":{}}}",mesh),xpp::command_place()));
    return;
  }
  if(com==M_UI){
    auto controls=s.numerics.equilibria;
    new_int("Max iterations",&controls.max_iterations);
    if(new_float(s,"Newton tolerance",&controls.newton_tolerance)==-1
       ||new_float(s,"Jacobian epsilon",&controls.jacobian_step)==-1
       ||new_float(s,"Search limit",&controls.search_limit)==-1)return;
    xpp::ok_or_show(xpp::command::run_button(s,xpp::format("{{\"cmd\":\"equilibrium.configure\",\"max_iterates\":{},\"newton_tolerance\":{},\"jacobian_epsilon\":{},\"search_limit\":{}}}",
        controls.max_iterations,controls.newton_tolerance,controls.jacobian_step,controls.search_limit),xpp::command_place()));
    return;
  }
  if (com >= M_UT && com <= M_UC) xpp::quick_num(s, com - M_UT);
}

static void do_file_com(xpp::Session &s, int com)
{
  switch (com) {
  case M_FT: xpp::do_transpose(s); break;
  case M_FG: get_intern_set(s); break;
  case M_FP: make_txtview(s); break;
  case M_FS: xpp::file_inf(s); break;
  case M_FA:
    do_auto_win(s);
    break;
  case M_FC: q_calc(s); break;
  case M_FR: xpp::import_settings_command(s); break;
  case M_FQ: xpp_quit(s); break;
  }
}

} // namespace xpp
