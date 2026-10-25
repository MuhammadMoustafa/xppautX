/* The command layer: the main-window key handler (commander), the M_*
   command switch (run_the_commands) and the pop-up menus that lead to it.
   Every front end drives xppautX through this one code. Menus are data
   (menus.cpp) and are shown with menu_choose(); see xpp_ui.h. */
#include "xpp_files.h"
#include "xpp_ui.h"
#include "image_format.h"
#include "browse.h"
#include "session.h"
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

/* Pop up m and return the index of the chosen item, -1 if none. */
static int menu_pick(const XppMenu *m, int def)
{
  return menu_index(m, menu_choose(m, def));
}

/* Pop up m and run the command of the chosen item in the session s. */
static void menu_run(xpp::Session &s, const XppMenu *m, int def)
{
  int i = menu_pick(m, def);
  if (i >= 0)
    run_the_commands(s, m->first_cmd + i);
}

/* ---- the three main-window menus ------------------------------------ */

void show_main_menu(xpp::Session &s, int which)
{
  ui.show_menu(which);
  s.help_menu = which;
}

/* ---- things the File menu runs without a pop-up ---------------------- */

/* ---- commands the menus run through the command switch --------------- */

void do_movie_com(xpp::Session &s, int c)
{
  XppKinescope &k = s.kinescope;
  std::string base;
  switch (c) {
  case 0:
    if (const auto r = ui.film_clip(s); !r) show_error(r.error());
    break;
  case 1: reset_film(s); break;
  case 2: ui.movie_play_back(s); break;
  case 3:
    new_int("Number of cycles", &k.cycles);
    new_int("Msec between frames", &k.frame_ms);
    if (k.frame_ms < 0) k.frame_ms = 0;
    if (k.cycles <= 0) return;
    ui.movie_auto_play(s);
    break;
  case 4:
    if(!save_ready(s.kinescope.frames>0))return;
    base = xpp::files::output_name(s.model().this_file, ".gif");
    if (!file_selector("Save kinescope frames", base, "*.gif")) return;
    if (!base.empty())
      ui.movie_save(s,base.c_str(), IMAGE_FORMAT_GIF);
    break;
  case 5: ui.movie_make_anigif(s); break;
  case 6: break;
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

/* File/Restore defaults (restoredefaults): all, or one section, back to the
   model's values (xpp::restore_defaults) */
static void restore_defaults_menu(xpp::Session &s)
{
  const std::vector<std::string> scopes = xpp::set_scopes();
  const int scope = pick_from_list("restore_scope", "Restore defaults of", scopes);
  if (scope < 0) return;
  std::optional<int> graph;
  if (!pick_target_graph(s, scopes[static_cast<std::size_t>(scope)], graph)) return;
  xpp::ok_or_show(xpp::restore_defaults(s, scopes[static_cast<std::size_t>(scope)], "", graph));
}

/* what a save asks after the kind: the scope (all or one section), the graph for
   the plot settings, and, when the initial values go in, whether they are the
   initial values or the last state reached; false when any is cancelled */
struct SaveChoice {
  std::string scope;
  std::optional<int> graph;
  bool last_state = false;
};

static bool ask_save_choice(xpp::Session &s, std::string_view title, SaveChoice &choice)
{
  const std::vector<std::string> scopes = xpp::set_scopes();
  const int scope = pick_from_list("save_scope", title, scopes);
  if (scope < 0) return false;
  choice.scope = scopes[static_cast<std::size_t>(scope)];
  if (!pick_target_graph(s, choice.scope, choice.graph)) return false;
  const bool states = choice.scope == xpp::set_scope::all || choice.scope == xpp::set_scope::initial;
  if (states && s.data_store.rows > 0) {
    const int answer = TwoChoice("Initial values", "Last state", "Save the initial values, or the last state reached?", "il");
    if (answer == 0) return false;
    choice.last_state = answer == 'l';
  }
  return true;
}

/* one of the model file's own sets, picked; empty when none or cancelled */
static std::string pick_own_set(xpp::Session &s, std::string_view title)
{
  const std::vector<std::string> own = xpp::own_set_names(s);
  if (own.empty()) {
    bottom_msg(0, "This model file has no set of its own");
    return "";
  }
  const int pick = pick_from_list("own_set", title, own);
  return pick < 0 ? "" : own[static_cast<std::size_t>(pick)];
}

/* File/Save values (savevalues): the current values, a new set or the part of a set
   of this model file, written by xpp::save_new_set / save_into_set */
static void save_values_menu(xpp::Session &s)
{
  const std::vector<std::string> modes{"New set", "Replace in set"};
  const int mode = pick_from_list("save_mode", "Save values", modes);
  if (mode < 0) return;
  std::string name;
  if (mode == 0) {
    name = xpp::intern_set_default_name(s.model());
    if (!new_string("Name of the set", name)) return;
  } else {
    name = pick_own_set(s, "Replace in set");
    if (name.empty()) return;
  }
  SaveChoice choice;
  if (!ask_save_choice(s, mode == 0 ? "Save into the new set" : "Replace in the set", choice)) return;
  if (mode == 1 && !confirm(xpp::format("Replace the {} values of set {} in {}? Its other parts stay as they are.", choice.scope == xpp::set_scope::all ? "all" : choice.scope, name, s.model().this_file))) return;
  xpp::ok_or_show(mode == 0 ? xpp::save_new_set(s, name, choice.scope, choice.graph, choice.last_state)
                            : xpp::save_into_set(s, name, choice.scope, choice.graph, choice.last_state));
}

/* File/Save as defaults (savedefaults): the current values as the model's par and init lines and options groups */
static void save_defaults_menu(xpp::Session &s)
{
  SaveChoice choice;
  if (!ask_save_choice(s, "Save as defaults", choice)) return;
  xpp::ok_or_show(xpp::save_defaults(s, choice.scope, choice.graph, choice.last_state));
}

/* File/Export values (exportvalues): the current values as a set of another .odex file */
static void export_values_menu(xpp::Session &s)
{
  SaveChoice choice;
  if (!ask_save_choice(s, "Export", choice)) return;
  std::string name = xpp::intern_set_default_name(s.model());
  if (!new_string("Name of the set", name)) return;
  std::string file = xpp::files::output_name(s.model().this_file, xpp::odex::extension, "values");
  if (!file_selector("Export to", file, xpp::format("*{}", xpp::odex::extension))) return;
  xpp::ok_or_show(xpp::export_values(s, name, choice.scope, file, choice.graph, choice.last_state));
}

/* File/Rename set (renameset) */
static void rename_set_menu(xpp::Session &s)
{
  const std::string from = pick_own_set(s, "Rename set");
  if (from.empty()) return;
  std::string to = from;
  if (!new_string("New name", to)) return;
  xpp::ok_or_show(xpp::rename_intern_set(s, from, to));
}

/* File/Delete set (deleteset) */
static void delete_set_menu(xpp::Session &s)
{
  const std::string name = pick_own_set(s, "Delete set");
  if (name.empty()) return;
  if (!confirm(xpp::format("Delete the set {} from {}?", name, s.model().this_file))) return;
  xpp::ok_or_show(xpp::delete_intern_set(s, name));
}

/* ---- the command switch --------------------------------------------- */

static void do_file_com(xpp::Session &s, int com);

void run_the_commands(xpp::Session &s, int com)
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
    froz_cline_stuff_com(s,com - M_NFF);
    return;
  }
  if (com >= M_NN && com <= M_NS) {
    new_clines_com(s,com - M_NN);
    return;
  }
  if (com >= M_DD && com <= M_DS) {
    direct_field_com(s,com - M_DD);
    if ((com - M_DD) == 1)
      return;
    create_new_cline(s);
    ui.redraw_graph(s);
    return;
  }
  if (com >= M_WW && com <= M_WS) {
    window_zoom_com(s,com - M_WW);
    return;
  }
  if (com >= M_AA && com <= M_AC) {
    do_torus_com(s,com - M_AA);
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
    clr_all_scrns(s);
    plot_data_picture(s, 0); /* and blanks its picture */
    s.nullclines.df_flag = 0;
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
  if (com >= M_MC && com <= M_MS) {
    do_windows_com(s, com - M_MC);
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

/* ---- pop-up menus --------------------------------------------------- */

/* the main menu's, reached from its keys (commander) */
static void ini_data_menu(xpp::Session &s) { menu_run(s, &menu_integrate, 3); }
static void new_clines(xpp::Session &s) { menu_run(s, &menu_nullclines, 0); }
static void direct_field(xpp::Session &s) { menu_run(s, &menu_dirfield, 0); }
static void window_zoom(xpp::Session &s) { menu_run(s, &menu_window, 0); }
static void do_torus(xpp::Session &s) { menu_run(s, &menu_torus, 1 - s.numerics.torus); }
static void do_movie(xpp::Session &s) { menu_run(s, &menu_kinescope, 0); }
static void find_equilibrium(xpp::Session &s) { menu_run(s, &menu_equilibria, 1); }
static void change_view(xpp::Session &s) { menu_run(s, &menu_view, 0); }
static void find_bvp(xpp::Session &s) { menu_run(s, &menu_bvp, 1); }

/* the ones the Numerics menu (numerics.cpp), the nullclines' (nullcline.cpp)
   and the front end open, in the Session they pass */
void froz_cline_stuff(xpp::Session &s) { menu_run(s, &menu_freeze_cline, 0); }
void do_stochast(xpp::Session &s) { menu_run(s, &menu_stochastic, 0); }
void set_col_par(xpp::Session &s) { menu_run(s, &menu_color_code, 0); }
void make_adj(xpp::Session &s) { menu_run(s, &menu_adjoint, 0); }

void get_pmap_pars(xpp::Session &s)
{
  menu_run(s, &menu_poincare, static_cast<int>(s.numerics.poincare_map));
}

void new_lookup(xpp::Session &s)
{
  if (s.ntable == 0) return;
  menu_run(s, &menu_lookup, 1);
}

static void do_windows(xpp::Session &s)
{
  menu_run(s, s.plot_windows.simul == 0 ? &menu_windows : &menu_windows_simoff, 0);
}

static void do_gr_objs(xpp::Session &s)
{
  int i = menu_pick(&menu_text, 0);
  if (i < 0) return;
  if (menu_text.keys[i] == 'e') {
    menu_run(s, &menu_text_edit, 0);
    return;
  }
  run_the_commands(s, M_TT + i);
}

static void add_a_curve(xpp::Session &s)
{
  int com = -1;
  int i = menu_pick(&menu_curves, 0), j;

  if (i < 0) return;
  switch (menu_curves.keys[i]) {
  case 'f':
    j = menu_pick(s.frozen_curves.auto_freeze == 0 ? &menu_freeze : &menu_freeze_off, 0);
    if (j < 0) break;
    if (menu_freeze.keys[j] == 'k') {
      j = menu_pick(&menu_freeze_key, 0);
      if (j >= 0) com = M_GFKN + j;
    } else {
      com = M_GFF + j;
    }
    break;
  case 'c': {
    /* a scale by its name, from all of them */
    const std::vector<std::string> names(colormap_names.begin(), colormap_names.end());
    j = pick_from_list("colormap", "Colormap", names);
    if (j >= 0) {
      change_cmap_com(s, j);
      redraw_dfield(s);
    }
    return;
  }
  default:
    com = M_GA + i;
  }
  run_the_commands(s, com);
}

/* ---- the key handler ------------------------------------------------ */

void commander(xpp::Session &s, int ch)
{
  switch (s.help_menu) {
  case MAIN_MENU:
    switch (ch) {
    case 'i': flash(0); ini_data_menu(s); flash(0); break;
    case 'c': flash(1); xpp::ok_or_show(xpp::continue_total(s)); flash(1); break;
    case 'n': flash(2); new_clines(s); flash(2); break;
    case 'd': flash(3); direct_field(s); flash(3); break;
    case 'w': flash(4); window_zoom(s); flash(4); break;
    case 'a': flash(5); do_torus(s); flash(5); break;
    case 'k': flash(6); do_movie(s); flash(6); break;
    case 'g': flash(7); flash(7); add_a_curve(s); break;
    case 'u': flash(8); flash(8); show_main_menu(s,NUM_MENU); break;
    case 'f': flash(9); flash(9); show_main_menu(s,FILE_MENU); break;
    case 'p': flash(10); run_the_commands(s, M_P); flash(10); break;
    case 'e': flash(11); run_the_commands(s, M_EE); flash(11); break;
    case 'm': do_windows(s); flash(12); break;
    case 't': flash(13); do_gr_objs(s); flash(13); break;
    case 's': flash(14); find_equilibrium(s); flash(14); break;
    case 'v': flash(15); change_view(s); flash(15); break;
    case 'b': find_bvp(s); break;
    case 'x': flash(16); run_the_commands(s, M_X); flash(16); break;
    case 'r': flash(17); run_the_commands(s, M_R); flash(17); break;
    case '3': run_the_commands(s, M_3); break;
    case 'z': if (!xpp::undo_values(s)) command_error("undo", "No value edit to undo"); break;
    case 'y': if (!xpp::redo_values(s)) command_error("redo", "No value edit to redo"); break;
    }
    break;

  case NUM_MENU:
    xpp::get_num_par(s,static_cast<char>(ch));
    break;

  case FILE_MENU:
    switch (ch) {
    case 't': xpp::do_transpose(s); break;
    case 'g': get_intern_set(s); break;
    case 'k': restore_defaults_menu(s); break;
    case 'p': flash(0); make_txtview(s); flash(0); break;
    case 's': flash(2); xpp::file_inf(s); flash(2); break;
    case 'a':
      flash(3);
      do_auto_win(s);
      flash(3);
      break;
    case 'c': flash(4); q_calc(s); flash(4); break;
    case 'r': flash(5); xpp::import_settings_command(s); flash(5); break;
    case 'q':
      flash(7);
      xpp_quit(s);
      flash(7);
      break;
    case 'o': save_values_menu(s); break;
    case 'j': save_defaults_menu(s); break;
    case 'i': export_values_menu(s); break;
    case 'z': rename_set_menu(s); break;
    case 'f': delete_set_menu(s); break;
    case 'm': xpp_model_open(s, nullptr); break;
    case 'e': xpp_model_reload(s); break;
    case 'v': xpp_session_save_here(s); break;
    case 'w': xpp_session_save(s, nullptr, -1); break;
    case 'b': xpp_session_save(s, nullptr, -1, true); break;
    case 'n': xpp_session_load(s, nullptr); break;
    case 'd': record_toggle(s); break;
    case 'y': play_recording(s,""); break;
    }
    show_main_menu(s,MAIN_MENU);
    break;
  }
}

} // namespace xpp
