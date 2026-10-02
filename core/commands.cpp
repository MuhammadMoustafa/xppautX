/* The command layer: the main-window key handler (commander), the M_*
   command switch (run_the_commands) and the pop-up menus that lead to it.
   Moved out of main.c and menudrive.c so that every front end drives XPP
   through the same code. Menus are data (menus.c) and are shown with
   menu_choose(); see xpp_ui.h. */
#include "xpp_ui.h"
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
#include "tutor.h"

#include "adj2.h"
#include "auto_nox.h"
#include "comline.h"
#include "graf_par.h"
#include "graphics.h"
#include "grobs.h"
#include "integrate.h"
#include "load_eqn.h"
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
#include <sys/types.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <unistd.h>
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

void do_tutorial(void)
{
  int tut = 0;
  xpp::log_printf(XPP_LOG_INFO, "Running tutorial!\n");
  while (1) {
    char ans = static_cast<char>(ui.two_choice("Next", "Done", tutorial[tut], "nd",
                                       "Did you know you can..."));
    if (ans != 'n') /* 'd', or a front end that cannot ask */
      break;
    tut++;
    tut = tut % N_TUTORIAL;
  }
}

#ifdef _WIN32
/* no fork on Windows: start the editor/browser and let it run on its own */
void edit_xpprc(void)
{
  const char *ed = getenv("XPPEDITOR");
  const char *home = getenv("USERPROFILE");
  if (ed == NULL || ed[0] == '\0') {
    command_error("editor", "Environment variable XPPEDITOR needs to be set.");
    return;
  }
  std::string cmd = xpp::format("start \"\" \"{}\" \"{}\\.xpprc\"", ed, home ? home : ".");
  if (system(cmd.c_str()) != 0) command_error("editor", "Unable to start the editor.");
}
#else
void edit_xpprc(void)
{
  const char *ed = getenv("XPPEDITOR");

  if (ed == NULL || ed[0] == '\0') {
    command_error("editor", "Environment variable XPPEDITOR needs to be set.");
    return;
  }
  std::string editor = ed;

  pid_t child_pid = fork();
  if (child_pid == 0) {
    const char *home = getenv("HOME");
    std::string rc = xpp::format("{}/.xpprc", home ? home : "");
    char *const args[] = {editor.data(), rc.data(), NULL};
    execvp(editor.c_str(), args);
    int child_status;
    wait(&child_status);
    return;
  }
  if (child_pid == -1)
    command_error("editor", "Unable to fork process for editor.");
}
#endif

/* ---- commands that were in X11 files -------------------------------- */

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
    base = "frame";
    new_string_of("Base file name", base, XPP_FIELD_FILE);
    if (!base.empty())
      ui.movie_save(s,base.c_str(), 2);
    break;
  case 5: ui.movie_make_anigif(s); break;
  case 6: break;
  }
}

static void get_intern_set(xpp::Session &s)
{
  const std::vector<xpp::Model::InternalSet> &sets = s.model().intern_sets;
  int count = static_cast<int>(sets.size());
  if (count <= 0 || count >= MAX_INTERN_SET) return;

  std::vector<std::string> labels;
  std::vector<const char *> items;
  std::string keys;
  labels.reserve(static_cast<std::size_t>(count));
  items.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; i++) {
    char key = static_cast<char>('a' + i);
    labels.push_back(xpp::format("{}: {}", key, sets[i].name));
    keys.push_back(key);
  }
  for (const auto &l : labels) items.push_back(l.c_str());

  XppMenu m = {"param_set", "Param set", count, items.data(), keys.c_str(),
               no_hint, -1};
  int ch = menu_choose(&m, 0);
  /* Esc or a dismissed menu chooses nothing: a cancel is not an error */
  if (ch < 'a' || ch >= 'a' + count) return;
  use_intern_set(s, ch - 'a');
}

void use_intern_set(xpp::Session &s, int j)
{
  const std::vector<xpp::Model::InternalSet> &sets = s.model().intern_sets;
  if (j < 0 || j >= static_cast<int>(sets.size())) {
    command_error("set", "Not a valid set");
    return;
  }
  get_graph(s);
  /* all of it or nothing (W125): a bad item is the error, at the set's line */
  if (const xpp::Result<> r = xpp::extract_internset(s, j); !r) {
    show_error(r.error());
    return;
  }
  xpp::chk_delay(s);
  redraw_params();
  redraw_ics();
  reset_graph(s);
  s.this_internset = "_" + sets[static_cast<std::size_t>(j)].name;
}

/* File/cOpy set line (W67): the current values as a `set` line for the
   .ode, shown for confirmation, then sent to the page's clipboard */
static void copy_set_line(xpp::Session &s)
{
  std::string name = xpp::intern_set_default_name(s.model());
  if (!new_string("Name of the set", name)) return;
  std::string problem = xpp::intern_set_name_problem(s.model(), name);
  if (!problem.empty()) {
    command_error("copy set", problem);
    return;
  }
  std::string line = xpp::intern_set_line(s, name);
  std::string question = xpp::format("Copy this line to paste into the .ode, then reload:\n{}", line);
  if (TwoChoice("Copy", "Cancel", question, "cn") != 'c') return;
  copy_text("set", line);
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
    xpp::cont_integ(s);
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
  if (com >= M_GCN && com <= M_GCU) {
    change_cmap_com(s,com - M_GCN);
    redraw_dfield(s);
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
  if (com >= M_FP && com <= M_FO) {
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
#ifdef AUTO
    do_auto_win(s);
#endif
    break;
  case M_FC: q_calc(s); break;
  case M_FR: xpp::import_xppaut_set_command(s); break;
  case M_FH: open_help("05-commands", "file"); break;
  case M_FX: edit_xpprc(); break;
  case M_FU: do_tutorial(); break;
  case M_FQ: xpp_quit(s); break;
  case M_FL: xpp::clone_ode(s); break;
  case M_FO: copy_set_line(s); break;
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
  menu_run(s, &menu_poincare, s.numerics.poimap);
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
  case 'c':
    j = menu_pick(&menu_colormap, 0);
    if (j >= 0) com = M_GCN + j;
    break;
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
    case 'c': flash(1); xpp::cont_integ(s); flash(1); break;
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
    }
    break;

  case NUM_MENU:
    xpp::get_num_par(s,static_cast<char>(ch));
    break;

  case FILE_MENU:
    switch (ch) {
    case 't': xpp::do_transpose(s); break;
    case 'g': get_intern_set(s); break;
    case 'p': flash(0); make_txtview(s); flash(0); break;
    case 's': flash(2); xpp::file_inf(s); flash(2); break;
    case 'a':
      flash(3);
#ifdef AUTO
      do_auto_win(s);
#endif
      flash(3);
      break;
    case 'c': flash(4); q_calc(s); flash(4); break;
    case 'r': flash(5); xpp::import_xppaut_set_command(s); flash(5); break;
    case 'h': open_help("05-commands", "file"); break;
    case 'q':
      flash(7);
      xpp_quit(s);
      flash(7);
      break;
    case 'l': xpp::clone_ode(s); break;
    case 'o': copy_set_line(s); break;
    case 'm': xpp_model_open(s, nullptr); break;
    case 'e': xpp_model_reload(s); break;
    case 'v': xpp_session_save(s, nullptr, -1); break;
    case 'n': xpp_session_load(s, nullptr); break;
    case 'd': record_toggle(s); break;
    case 'y': play_recording(s,""); break;
    case 'x': edit_xpprc(); break;
    case 'u': do_tutorial(); break;
    }
    show_main_menu(s,MAIN_MENU);
    break;
  }
}

} // namespace xpp
