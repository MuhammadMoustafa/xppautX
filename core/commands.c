/* The command layer: the main-window key handler (commander), the M_*
   command switch (run_the_commands) and the pop-up menus that lead to it.
   Moved out of main.c and menudrive.c so that every front end drives XPP
   through the same code. Menus are data (menus.c) and are shown with
   menu_choose(); see xpp_ui.h. */
#include "xpp_ui.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_globals.h"
#include "xpp_util.h"
#include "edit_rhs.h"
#include "menus.h"
#include "menudrive.h"
#include "tutor.h"

#include "adj2.h"
#include "auto_nox.h"
#include "comline.h"
#include "extra.h"
#include "graf_par.h"
#include "graphics.h"
#include "grobs.h"
#include "integrate.h"
#include "load_eqn.h"
#include "lunch-new.h"
#include "markov.h"
#include "nullcline.h"
#include "numerics.h"
#include "pp_shoot.h"
#include "tabular.h"
#include "torus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <unistd.h>

extern int DF_FLAG, NTable, POIMAP, TORUS;
extern int Nintern_set;
extern INTERN_SET intern_set[MAX_INTERN_SET];
extern char *no_hint[];

int status;

/* Pop up m and return the index of the chosen item, -1 if none. */
static int menu_pick(const XppMenu *m, int def)
{
  int i;
  char ch = (char)menu_choose(m, def);
  for (i = 0; i < m->n; i++)
    if (ch == m->keys[i])
      return i;
  return -1;
}

/* Pop up m and run the command of the chosen item. */
static void menu_run(const XppMenu *m, int def)
{
  int i = menu_pick(m, def);
  if (i >= 0)
    run_the_commands(m->first_cmd + i);
}

/* ---- the three main-window menus ------------------------------------ */

void help(void)
{
  xpp_ui.show_menu(MAIN_MENU);
  help_menu = MAIN_MENU;
}

void help_num(void)
{
  xpp_ui.show_menu(NUM_MENU);
  help_menu = NUM_MENU;
}

void help_file(void)
{
  xpp_ui.show_menu(FILE_MENU);
  help_menu = FILE_MENU;
}

/* ---- things the File menu runs without a pop-up ---------------------- */

void do_tutorial(void)
{
  int tut = 0;
  xpp_log(XPP_LOG_INFO, "Running tutorial!\n");
  while (1) {
    char ans = (char)xpp_ui.two_choice("Next", "Done", tutorial[tut], "nd",
                                       "Did you know you can...");
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
  char cmd[600];
  char *ed = getenv("XPPEDITOR");
  char *home = getenv("USERPROFILE");
  if ((ed == NULL) || (strlen(ed) == 0)) {
    err_msg("Environment variable XPPEDITOR needs to be set.");
    return;
  }
  snprintf(cmd, sizeof(cmd), "start \"\" \"%s\" \"%s\\.xpprc\"", ed, home ? home : ".");
  if (system(cmd) != 0) err_msg("Unable to start the editor.");
}

void xpp_hlp(void)
{
  char cmd[600];
  if (getenv("XPPHELP") == NULL) {
    err_msg("Environment variable XPPHELP undefined.");
    return;
  }
  snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", getenv("XPPHELP"));
  if (system(cmd) != 0) err_msg("Unable to open the help.");
}
#else
void edit_xpprc(void)
{
  pid_t child_pid;
  char rc[256];
  char editor[256];
  int child_status;
  char *ed = getenv("XPPEDITOR");

  if ((ed == NULL) || (strlen(ed) == 0)) {
    err_msg("Environment variable XPPEDITOR needs to be set.");
    return;
  }
  snprintf(editor, sizeof(editor), "%s", ed);

  child_pid = fork();
  if (child_pid == 0) {
    sprintf(rc, "%s/.xpprc", getenv("HOME"));
    {
      char *const args[] = {editor, rc, NULL};
      execvp(editor, args);
    }
    wait(&child_status);
    return;
  }
  if (child_pid == -1)
    err_msg("Unable to fork process for editor.");
}

void xpp_hlp(void)
{
  char cmd[256];

  if (getenv("XPPHELP") == NULL) {
    err_msg("Environment variable XPPHELP undefined.");
    return;
  }
  if (getenv("XPPBROWSER") == NULL) {
    err_msg("Environment variable XPPBROWSER undefined.");
    return;
  }
  sprintf(cmd, "file:///%s", getenv("XPPHELP"));
  if (fork() == 0) {
    execlp(getenv("XPPBROWSER"), getenv("XPPHELP"), cmd, (char *)0);
    perror("Unable to open browser. Check your XPPBROWSER and XPPHELP environement variables.");
    exit(1);
  } else {
    wait(&status);
  }
}

#endif

/* ---- commands that were in X11 files -------------------------------- */

void do_movie_com(int c)
{
  char base[128];
  switch (c) {
  case 0:
    if (xpp_ui.film_clip() == 0)
      respond_box("Okay", "Out of film!");
    break;
  case 1: reset_film(); break;
  case 2: xpp_ui.movie_play_back(); break;
  case 3:
    new_int("Number of cycles", &ks_ncycle);
    new_int("Msec between frames", &ks_speed);
    if (ks_speed < 0) ks_speed = 0;
    if (ks_ncycle <= 0) return;
    xpp_ui.movie_auto_play();
    break;
  case 4:
    sprintf(base, "frame");
    new_string("Base file name", base);
    if (strlen(base) > 0)
      xpp_ui.movie_save(base, 2);
    break;
  case 5: xpp_ui.movie_make_anigif(); break;
  case 6: break;
  }
}

/* debug stress test on the 'y' key: half a million random lines */
void draw_many_lines(void)
{
  int NLINE = 500000;
  int i;
  for (i = 0; i < NLINE; i++)
    xpp_ui.draw_line(rand() % 200, rand() % 200, rand() % 200, rand() % 200);
  xpp_log(XPP_LOG_INFO, "Done\n");
}

void get_intern_set(void)
{
  char *n[MAX_INTERN_SET], key[MAX_INTERN_SET], ch;
  int i, j;
  int count = Nintern_set;
  XppMenu m = {"param_set", "Param set", 0, NULL, NULL, NULL, -1, 12, -1};
  if (count <= 0 || count >= MAX_INTERN_SET) return;
  for (i = 0; i < Nintern_set; i++) {
    n[i] = (char *)xpp_malloc(256);
    key[i] = 'a' + i;
    sprintf(n[i], "%c: %s", key[i], intern_set[i].name);
  }
  key[count] = 0;
  m.n = count; m.items = n; m.keys = key; m.hints = no_hint;
  ch = (char)menu_choose(&m, 0);
  for (i = 0; i < count; i++) xpp_free(n[i]);
  j = (int)(ch - 'a');
  if (j < 0 || j >= Nintern_set) {
    err_msg("Not a valid set");
    return;
  }
  get_graph();
  extract_internset(j);
  chk_delay();
  redraw_params();
  redraw_ics();
  reset_graph();
}

/* ---- the command switch --------------------------------------------- */

void run_the_commands(int com)
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
    froz_cline_stuff_com(com - M_NFF);
    return;
  }
  if (com >= M_NN && com <= M_NS) {
    new_clines_com(com - M_NN);
    return;
  }
  if (com >= M_DD && com <= M_DS) {
    direct_field_com(com - M_DD);
    if ((com - M_DD) == 1)
      return;
    create_new_cline();
    xpp_ui.redraw_graph();
    return;
  }
  if (com >= M_WW && com <= M_WS) {
    window_zoom_com(com - M_WW);
    return;
  }
  if (com >= M_AA && com <= M_AC) {
    do_torus_com(com - M_AA);
    return;
  }
  if (com >= M_KC && com <= M_KM) {
    do_movie_com(com - M_KC);
    return;
  }
  if (com >= M_GA && com <= M_GC) {
    add_a_curve_com(com - M_GA);
    return;
  }
  if (com >= M_GFF && com <= M_GFO) {
    freeze_com(com - M_GFF);
    return;
  }
  if (com >= M_GCN && com <= M_GCU) {
    change_cmap_com(com - M_GCN);
    redraw_dfield();
    return;
  }
  if (com == M_GFKK || com == M_GFKN) {
    key_frz_com(com - M_GFKN);
    return;
  }
  if (com == M_UKE || com == M_UKV) {
    new_lookup_com(com - M_UKE);
    return;
  }
  if (com == M_R) {
    drw_all_scrns();
    return;
  }
  if (com == M_EE) {
    clr_all_scrns();
    DF_FLAG = 0;
    return;
  }
  if (com == M_X) {
    xi_vs_t();
    return;
  }
  if (com == M_3) {
    get_3d_par_com();
    return;
  }
  if (com == M_P) {
    new_parameter();
    return;
  }
  if (com >= M_MC && com <= M_MS) {
    do_windows_com(com - M_MC);
    return;
  }
  if (com >= M_FP && com <= M_FL) {
    do_file_com(com);
    return;
  }
  if (com >= M_TT && com <= M_TS) {
    do_gr_objs_com(com - M_TT);
    return;
  }
  if (com >= M_TEM && com <= M_TED) {
    edit_object_com(com - M_TEM);
    return;
  }
  if (com >= M_BR && com <= M_BH) {
    find_bvp_com(com - M_BR);
    return;
  }
  if (com >= M_V2 && com <= M_VT) change_view_com(com - M_V2);
  if (com >= M_UAN && com <= M_UAR) make_adj_com(com - M_UAN);
  if (com >= M_UCN && com <= M_UCA) set_col_par_com(com - M_UCN);
  if (com >= M_UPN && com <= M_UPP) get_pmap_pars_com(com - M_UPN);
  if (com >= M_UHN && com <= M_UH2) do_stochast_com(com - M_UHN);
  if (com >= M_UT && com <= M_UC) quick_num(com - M_UT);
}

void do_file_com(int com)
{
  switch (com) {
  case M_FT: do_transpose(); break;
  case M_FG: get_intern_set(); break;
  case M_FI: TipsFlag = 1 - TipsFlag; break;
  case M_FP: make_txtview(); break;
  case M_FW: do_lunch(0); break;
  case M_FS: file_inf(); break;
  case M_FA:
#ifdef AUTO
    do_auto_win();
#endif
    break;
  case M_FC: q_calc(); break;
  case M_FR: do_lunch(1); break;
  case M_FB: tfBell = 1 - tfBell; break;
  case M_FH: break;
  case M_FX: edit_xpprc(); break;
  case M_FU: do_tutorial(); break;
  case M_FQ:
    if (yes_no_box()) bye_bye();
    break;
  case M_FER: edit_rhs(); break;
  case M_FEF: edit_functions(); break;
  case M_FES: save_as(); break;
  case M_FEL: load_new_dll(); break;
  case M_FL: clone_ode(); break;
  }
}

/* ---- pop-up menus --------------------------------------------------- */

void ini_data_menu(void) { menu_run(&menu_integrate, 3); }
void new_clines(void) { menu_run(&menu_nullclines, 0); }
void froz_cline_stuff(void) { menu_run(&menu_freeze_cline, 0); }
void direct_field(void) { menu_run(&menu_dirfield, 0); }
void window_zoom(void) { menu_run(&menu_window, 0); }
void do_torus(void) { menu_run(&menu_torus, 1 - TORUS); }
void do_movie(void) { menu_run(&menu_kinescope, 0); }
void find_equilibrium(void) { menu_run(&menu_equilibria, 1); }
void change_view(void) { menu_run(&menu_view, 0); }
void find_bvp(void) { menu_run(&menu_bvp, 1); }
void do_stochast(void) { menu_run(&menu_stochastic, 0); }
void get_pmap_pars(void) { menu_run(&menu_poincare, POIMAP); }
void set_col_par(void) { menu_run(&menu_color_code, 0); }
void make_adj(void) { menu_run(&menu_adjoint, 0); }

void new_lookup(void)
{
  if (NTable == 0) return;
  menu_run(&menu_lookup, 1);
}

void do_windows(void)
{
  menu_run(SimulPlotFlag == 0 ? &menu_windows : &menu_windows_simoff, 0);
}

void do_gr_objs(void)
{
  int i = menu_pick(&menu_text, 0);
  if (i < 0) return;
  if (menu_text.keys[i] == 'e') {
    menu_run(&menu_text_edit, 0);
    return;
  }
  run_the_commands(M_TT + i);
}

void add_a_curve(void)
{
  int com = -1;
  int i = menu_pick(&menu_curves, 0), j;

  if (i < 0) return;
  switch (menu_curves.keys[i]) {
  case 'f':
    j = menu_pick(AutoFreezeFlag == 0 ? &menu_freeze : &menu_freeze_off, 0);
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
  run_the_commands(com);
}

void edit_menu(void)
{
  int i = menu_pick(&menu_edit_rhs, 0);
  if (i >= 0) run_the_commands(M_FER + i);
}

void new_param(void) { run_the_commands(M_P); }
void clear_screens(void) { run_the_commands(M_EE); }
void x_vs_t(void) { run_the_commands(M_X); }
void redraw_them_all(void) { run_the_commands(M_R); }
void get_3d_par(void) { run_the_commands(M_3); }

/* ---- the key handler ------------------------------------------------ */

void commander(int ch)
{
  switch (help_menu) {
  case MAIN_MENU:
    switch (ch) {
    case 'i': flash(0); ini_data_menu(); flash(0); break;
    case 'c': flash(1); cont_integ(); flash(1); break;
    case 'n': flash(2); new_clines(); flash(2); break;
    case 'd': flash(3); direct_field(); flash(3); break;
    case 'w': flash(4); window_zoom(); flash(4); break;
    case 'a': flash(5); do_torus(); flash(5); break;
    case 'k': flash(6); do_movie(); flash(6); break;
    case 'g': flash(7); flash(7); add_a_curve(); break;
    case 'u': flash(8); flash(8); help_num(); break;
    case 'f': flash(9); flash(9); help_file(); break;
    case 'p': flash(10); new_param(); flash(10); break;
    case 'e': flash(11); clear_screens(); flash(11); break;
    case 'h':
    case 'm': do_windows(); flash(12); break;
    case 't': flash(13); do_gr_objs(); flash(13); break;
    case 's': flash(14); find_equilibrium(); flash(14); break;
    case 'v': flash(15); change_view(); flash(15); break;
    case 'b': find_bvp(); break;
    case 'x': flash(16); x_vs_t(); flash(16); break;
    case 'r': flash(17); redraw_them_all(); flash(17); break;
    case '3': get_3d_par(); break;
    case 'y': draw_many_lines(); break;
    }
    break;

  case NUM_MENU:
    get_num_par((char)ch);
    break;

  case FILE_MENU:
    switch (ch) {
    case 't': do_transpose(); break;
    case 'g': get_intern_set(); break;
    case 'i': TipsFlag = 1 - TipsFlag; break;
    case 'p': flash(0); make_txtview(); flash(0); break;
    case 'w': flash(1); do_lunch(0); flash(1); break;
    case 's': flash(2); file_inf(); flash(2); break;
    case 'a':
      flash(3);
#ifdef AUTO
      do_auto_win();
#endif
      flash(3);
      break;
    case 'c': flash(4); q_calc(); flash(4); break;
    case 'r': flash(5); do_lunch(1); flash(5); break;
    case 'e': flash(6); edit_menu(); flash(6); break;
    case 'b': tfBell = 1 - tfBell; break;
    case 'h': xpp_hlp(); break;
    case 'q':
      flash(7);
      if (yes_no_box()) bye_bye();
      flash(7);
      break;
    case 'l': clone_ode(); break;
    case 'x': edit_xpprc(); break;
    case 'u': do_tutorial(); break;
    }
    help();
    break;
  }
}
