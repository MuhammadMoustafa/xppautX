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

/* Pop up m and return the index of the chosen item, -1 if none. */
static int menu_pick(const XppMenu *m, int def)
{
  return xpp_menu_index(m, menu_choose(m, def));
}

/* Pop up m and run the command of the chosen item in the session s. */
static void menu_run(xpp::Session &s, const XppMenu *m, int def)
{
  int i = menu_pick(m, def);
  if (i >= 0)
    run_the_commands(s, m->first_cmd + i);
}

/* ---- the three main-window menus ------------------------------------ */

void show_main_menu(int which)
{
  xpp_ui.show_menu(which);
  help_menu = which;
}

/* ---- things the File menu runs without a pop-up ---------------------- */

void do_tutorial(void)
{
  int tut = 0;
  xpp_log(XPP_LOG_INFO, "Running tutorial!\n");
  while (1) {
    char ans = static_cast<char>(xpp_ui.two_choice("Next", "Done", tutorial[tut], "nd",
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
    err_msg("Environment variable XPPEDITOR needs to be set.");
    return;
  }
  std::string cmd = xpp::format("start \"\" \"{}\" \"{}\\.xpprc\"", ed, home ? home : ".");
  if (system(cmd.c_str()) != 0) err_msg("Unable to start the editor.");
}
#else
void edit_xpprc(void)
{
  const char *ed = getenv("XPPEDITOR");

  if (ed == NULL || ed[0] == '\0') {
    err_msg("Environment variable XPPEDITOR needs to be set.");
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
    err_msg("Unable to fork process for editor.");
}
#endif

/* ---- commands that were in X11 files -------------------------------- */

int help_menu;

void do_movie_com(XppKinescope &k, int c)
{
  std::string base;
  switch (c) {
  case 0:
    if (xpp_ui.film_clip() == 0)
      respond_box("Okay", "Out of film!");
    break;
  case 1: reset_film(); break;
  case 2: xpp_ui.movie_play_back(); break;
  case 3:
    new_int("Number of cycles", &k.cycles);
    new_int("Msec between frames", &k.frame_ms);
    if (k.frame_ms < 0) k.frame_ms = 0;
    if (k.cycles <= 0) return;
    xpp_ui.movie_auto_play();
    break;
  case 4:
    base = "frame";
    new_string_of("Base file name", base, XPP_FIELD_FILE);
    if (!base.empty())
      xpp_ui.movie_save(base.c_str(), 2);
    break;
  case 5: xpp_ui.movie_make_anigif(); break;
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
    err_msg("Not a valid set");
    return;
  }
  get_graph(s);
  extract_internset(s, j);
  chk_delay();
  redraw_params();
  redraw_ics();
  reset_graph(s);
  s.this_internset = "_" + sets[static_cast<std::size_t>(j)].name;
}

/* File/cOpy set line (W67): the current values as a `set` line for the
   .ode, shown for confirmation, then sent to the page's clipboard */
static void copy_set_line(xpp::Session &s)
{
  std::string name = intern_set_default_name(s.model());
  if (!new_string("Name of the set", name)) return;
  std::string problem = intern_set_name_problem(s.model(), name);
  if (!problem.empty()) {
    err_msg(problem.c_str());
    return;
  }
  std::string line = intern_set_line(s, name);
  std::string question = xpp::format("Copy this line to paste into the .ode, then reload:\n{}", line);
  if (TwoChoice("Copy", "Cancel", question.c_str(), "cn") != 'c') return;
  copy_text("set", line.c_str());
}

/* ---- the command switch --------------------------------------------- */

static void do_file_com(xpp::Session &s, int com);

void run_the_commands(xpp::Session &s, int com)
{
  if (com < 0) return;
  if (com <= MAX_M_I) {
    do_init_data(com);
    return;
  }
  if (com == M_C) {
    cont_integ();
    return;
  }
  if (com >= M_SG && com <= M_SC) {
    find_equilib_com(com - M_SG);
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
    xpp_ui.redraw_graph();
    return;
  }
  if (com >= M_WW && com <= M_WS) {
    window_zoom_com(s,com - M_WW);
    return;
  }
  if (com >= M_AA && com <= M_AC) {
    do_torus_com(com - M_AA);
    return;
  }
  if (com >= M_KC && com <= M_KM) {
    do_movie_com(s.kinescope, com - M_KC);
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
    change_cmap_com(com - M_GCN);
    redraw_dfield(s);
    return;
  }
  if (com == M_GFKK || com == M_GFKN) {
    key_frz_com(s,com - M_GFKN);
    return;
  }
  if (com == M_UKE || com == M_UKV) {
    new_lookup_com(s, com - M_UKE);
    return;
  }
  if (com == M_R) {
    drw_all_scrns();
    plot_data_picture(s, 1); /* a data client draws the current data again */
    return;
  }
  if (com == M_EE) {
    clr_all_scrns();
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
    new_parameter(s);
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
    find_bvp_com(com - M_BR);
    return;
  }
  if (com >= M_V2 && com <= M_VT) change_view_com(s,com - M_V2);
  if (com >= M_UAN && com <= M_UAR) make_adj_com(com - M_UAN);
  if (com >= M_UCN && com <= M_UCA) set_col_par_com(com - M_UCN);
  if (com >= M_UPN && com <= M_UPP) get_pmap_pars_com(com - M_UPN);
  if (com >= M_UHN && com <= M_UH2) do_stochast_com(com - M_UHN);
  if (com >= M_UT && com <= M_UC) quick_num(com - M_UT);
}

static void do_file_com(xpp::Session &s, int com)
{
  switch (com) {
  case M_FT: do_transpose(); break;
  case M_FG: get_intern_set(s); break;
  case M_FP: make_txtview(); break;
  case M_FW: do_lunch(s, 0); break;
  case M_FS: file_inf(s); break;
  case M_FA:
#ifdef AUTO
    do_auto_win();
#endif
    break;
  case M_FC: q_calc(); break;
  case M_FR: do_lunch(s, 1); break;
  case M_FH: open_help("05-commands", "file"); break;
  case M_FX: edit_xpprc(); break;
  case M_FU: do_tutorial(); break;
  case M_FQ:
    if (yes_no_box()) bye_bye();
    break;
  case M_FL: clone_ode(s); break;
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
   and the front end open: entry points of their own, in the current session
   until those modules pass theirs (W47d) */
void froz_cline_stuff(void) { menu_run(xpp::session(), &menu_freeze_cline, 0); }
void do_stochast(void) { menu_run(xpp::session(), &menu_stochastic, 0); }
void set_col_par(void) { menu_run(xpp::session(), &menu_color_code, 0); }
void make_adj(void) { menu_run(xpp::session(), &menu_adjoint, 0); }

void get_pmap_pars(void)
{
  xpp::Session &s = xpp::session();
  menu_run(s, &menu_poincare, s.numerics.poimap);
}

void new_lookup(void)
{
  xpp::Session &s = xpp::session();
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
  switch (help_menu) {
  case MAIN_MENU:
    switch (ch) {
    case 'i': flash(0); ini_data_menu(s); flash(0); break;
    case 'c': flash(1); cont_integ(); flash(1); break;
    case 'n': flash(2); new_clines(s); flash(2); break;
    case 'd': flash(3); direct_field(s); flash(3); break;
    case 'w': flash(4); window_zoom(s); flash(4); break;
    case 'a': flash(5); do_torus(s); flash(5); break;
    case 'k': flash(6); do_movie(s); flash(6); break;
    case 'g': flash(7); flash(7); add_a_curve(s); break;
    case 'u': flash(8); flash(8); show_main_menu(NUM_MENU); break;
    case 'f': flash(9); flash(9); show_main_menu(FILE_MENU); break;
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
    get_num_par(static_cast<char>(ch));
    break;

  case FILE_MENU:
    switch (ch) {
    case 't': do_transpose(); break;
    case 'g': get_intern_set(s); break;
    case 'p': flash(0); make_txtview(); flash(0); break;
    case 'w': flash(1); do_lunch(s, 0); flash(1); break;
    case 's': flash(2); file_inf(s); flash(2); break;
    case 'a':
      flash(3);
#ifdef AUTO
      do_auto_win();
#endif
      flash(3);
      break;
    case 'c': flash(4); q_calc(); flash(4); break;
    case 'r': flash(5); do_lunch(s, 1); flash(5); break;
    case 'h': open_help("05-commands", "file"); break;
    case 'q':
      flash(7);
      if (yes_no_box()) bye_bye();
      flash(7);
      break;
    case 'l': clone_ode(s); break;
    case 'o': copy_set_line(s); break;
    case 'm': xpp_model_open(s, nullptr); break;
    case 'e': xpp_model_reload(s); break;
    case 'v': xpp_session_save(s, nullptr, -1); break;
    case 'n': xpp_session_load(s, nullptr); break;
    case 'x': edit_xpprc(); break;
    case 'u': do_tutorial(); break;
    }
    show_main_menu(MAIN_MENU);
    break;
  }
}
