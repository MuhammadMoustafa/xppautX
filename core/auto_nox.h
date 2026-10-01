#ifndef _auto_nox_h_
#define _auto_nox_h_

#include <stdio.h>
#include <string>
#include <string_view>
#include "xpplim.h"
#include "autlim.h"
#include "xpp_error.h"

#define MAX_AUT_PER 10

namespace xpp {

struct Session; /* session.h */

typedef struct {
  double torper;
} ROTCHK;

typedef struct  {

  int exist;
  int ntst,nmx,npr;
  double ds,dsmax,dsmin,rl0,rl1,a0,a1;
  /* the active view's last point, where its next line starts (the
     axes are the views', AutoState::views) */
  double lastx,lasty;
  int wid,hgt,x0,y0;
  int nfpar,nbc;
  int ips,irs,ilp,isp,isw,itp;
  int icp3,icp4,icp5;
  int nper;
  std::string hinttxt;
  double period[MAX_AUT_PER];
  int uzrpar[MAX_AUT_PER];
  double epsl,epsu,epss;
  int ncol;
}BIFUR;

typedef struct {
  int iad;
  int mxbf;
  int iid;
  int itmx;
  int itnw;
  int nwtn;
  int iads;

} ADVAUTO;

typedef struct {
  int ibr,itp,lab;
  double uhi[NAUTO],ulo[NAUTO],u0[NAUTO],ubar[NAUTO];
  double par[20],per,torper;
  int nfpar;
  int flag;
} GRABPT;

typedef struct diagram {
  int ibr,ntot,itp,lab,calc;
  double norm,*uhi,*ulo,*u0,*ubar,*evr,*evi;
  double par[20],per,torper;
  int index,nfpar;
  int icp1,icp2,icp3,icp4,flag2;
  int from; /* the label its run started from, on a run's first point (not saved in files) */
} DIAGRAM;

/* a view of the diagram's axes (W50, AutoState::views): what it plots
   (Auto.plot: HI_P ... AV_P; the variable var; icp1 the x axis' parameter
   and, in a two-parameter view, icp2 the y axis', both indices into
   AutoState::par) and the ranges. The active view's icp1 and icp2 are also
   the parameters a run continues in. */
typedef struct {
  int plot,var,icp1,icp2;
  double xmin,ymin,xmax,ymax;
}  AUTOAX;

void colset(Session &s, int type);
void pscolset2(Session &s, int flag2);
void colset2(int flag2);
void draw_export_axes(Session &s);
void draw_bif_axes(Session &s);
int IXVal(const Session &s, double x);
int IYVal(const Session &s, double y);
int chk_auto_bnds(const Session &s, int ix, int iy);
void close_auto(Session &s, int flag);
void open_auto(Session &s, int flag);
/* the path of AUTO's fort.<unit> (3, 7, 8 or 9) open_auto set, "" for
   another unit: gogoauto.cpp opens them for a run */
const char *auto_fort_path(const Session &s, int unit);
void do_auto(Session &s, int iold, int isave, int itp);
void set_auto(Session &s);
/* AUTO's index of the period among its parameters (PAR(10), named T) */
#define AUTO_PERIOD_INDEX 10
/* the model's name of AUTO's parameter index k (AutoPar[k]), NULL when k
   is not one of AUTO's NAutoPar parameters: the one lookup the forms,
   the settings event, the CSV export and the branch-end text share */
const char *auto_par_name(const Session &s, int k);
#define AUTO_COL_W 14
void auto_per_par(Session &s);
void auto_params(Session &s);
void auto_num_par(Session &s);
void auto_plot_par(Session &s);
void auto_fit(Session &s);
void auto_default(Session &s);
/* the diagram's views (W50, AutoState::views). New view: one more, a copy
   of the active one's axes, made active. Close view k: 0 when it is the
   last one (it stays) or there is no view k. Activate view k: the one the
   Axes menu, zoom, the exports and a run's parameters then use; 0 when
   there is no view k. Each draws the diagram again when AUTO's window is
   open. */
void auto_new_view(Session &s);
int auto_close_view(Session &s, int k);
int auto_activate_view(Session &s, int k);
void auto_zoom_in(Session &s, int i1, int j1, int i2, int j2);
void auto_zoom_out(Session &s, int i1, int j1, int i2, int j2);
/* where view ax plots a point: x, and y1 and y2 (the maximum and minimum,
   or y1 twice) */
void auto_xy_plot(const AUTOAX *ax, double *x, double *y1, double *y2, double par1, double par2, double per, double *uhigh, double *ulow, double *ubar, double a);
/* the stored point d into the PostScript or SVG picture being written
   (type its get_bif_type, flag 0 for a branch's first point) */
void add_ps_point(Session &s, const DIAGRAM &d, int type, int flag);
void auto_line(Session &s, double x1i, double y1i, double x2i, double y2i);
/* the label the running continuation started from, once: for its first point */
int auto_run_from_take(Session &s);
/* the stored point d (its branch, point and type, its entry in the
   diagram list, the label its run started from, its stability) plotted in
   every view: the diagram's data and the active view's drawing. par the
   parameters it was computed at (AUTO's own during a run, d's stored ones
   in a redraw), type its get_bif_type, flag 0 for a branch's first point */
void add_point(Session &s, const DIAGRAM &d, const double *par, int type, int flag);
/* the two-letter symbol of AUTO's point type itp (BP, LP, HB, UZ, PD, TR,
   EP, MX), two blanks for any other */
const char *auto_bif_sym(int itp);
void info_header(Session &s, int flag2, int icp1, int icp2);
/* the info strip's line for the stored point d */
void new_info(Session &s, const DIAGRAM &d);
void traverse_out(Session &s, DIAGRAM *d, int *ix, int *iy, int dodraw);
void do_auto_win(Session &s);
void load_last_plot(Session &s, int flag);
void keep_last_plot(Session &s, int flag);
void init_auto_win(Session &s);
int yes_reset_auto(Session &s);
int reset_auto(Session &s);
void auto_grab(Session &s);

void auto_start_diff_ss(Session &s);
void auto_start_at_bvp(Session &s);
void auto_start_at_per(Session &s);
void find_best_homo_shift(Session &s, int n);
void get_start_period(Session &s, double *p);
void get_start_orbit(Session &s, double *u, double t, double p, int n);
void get_shifted_orbit(Session &s, double *u, double t, double p, int n);
void auto_new_ss(Session &s);
void auto_new_discrete(Session &s);
void auto_extend_ss(Session &s);
void auto_start_choice(Session &s);
void torus_choice(Session &s);
void per_doub_choice(Session &s);
void periodic_choice(Session &s);
void hopf_choice(Session &s);
void auto_new_per(Session &s);
void auto_start_at_homoclinic(Session &s);
int get_homo_info(Session &s, int flg,int *nun,int *nst,double *ul, double *ur);
void auto_extend_homoclinic(Session &s);
void auto_extend_bvp(Session &s);
void auto_switch_per(Session &s);
void auto_switch_bvp(Session &s);
void auto_switch_ss(Session &s);
void auto_2p_limit(Session &s, int ips);
void auto_twopar_double(Session &s);
void auto_torus(Session &s);
void auto_2p_branch(Session &s, int ips);
void auto_branch_choice(Session &s, int ibr, int ips);
void auto_homo_choice(Session &s, int itp);
void auto_2p_fixper(Session &s);
void auto_2p_hopf(Session &s);
void auto_period_double(Session &s);
void auto_run(Session &s);
void load_auto_orbit(Session &s);
/* File/Save diagram and Load diagram: an .autox (autox.h), or an XPPAUT
   .auto imported */
void save_auto(Session &s);
void load_auto(Session &s);
/* the parts of an XPPAUT .auto file, read (import_auto_file reads them
   all) */
void load_auto_numerics(Session &s, FILE *fp);
void load_auto_graph(Session &s, FILE *fp);
int import_auto_file(Session &s, FILE *fp);
int move_to_label(int mylab, int *nrow, int *ndim, FILE *fp);
void get_a_row(double *u, double *t, int n, FILE *fp);
void auto_file(Session &s);
int check_plot_type(const Session &s, int flag2,int icp1, int icp2);

/* grabbing and marking points on the diagram (from auto_x11.c) */
void traverse_diagram(Session &s);
/* the scriptable grabs: the same outcome as an interactive grab ending
   with Return on that point (docs/protocol.md "Grab by point"). 1 on
   success, 0 (nothing changed) when there is no such point. */
int auto_grab_label(Session &s, int lab);
int auto_grab_type_index(Session &s, const char *type, int index);
/* asks which kind of special point: its symbol (auto_bif_sym's), or NULL */
const char *query_special(const char *title);
void MarkAuto(int x, int y);
void clear_msg(Session &s);
void auto_update_view(Session &s, float xlo, float xhi, float ylo, float yhi);
void auto_motion_xy(Session &s, int i, int j);
/* a click on the diagram at (x, y) in its axis quantities: shown, and kept
   as the point AUTO's File/sElect 2par pt uses in a two-parameter plot */
void auto_point_xy(Session &s, double x, double y);

void DLINE(Session &s, double a, double b, double c, double d);
void find_point(Session &s, int ibr, int pt);
void auto_set_mark(Session &s, int i);
void do_auto_range(Session &s);

/* orbit lab of the solution file into the data (a labelled point's, when
   a grab or a diagram point asks for it); what failed otherwise, for the
   command to show */
Result<> load_auto_orbitx(Session &s, int ibr,int flag, int lab, double per);
/* the .auto's copy of the solution file, written beside this model's */
Result<> make_q_file(const Session &s, FILE *fp);

/* the number of points of the marked stretch of the diagram (*n) and its
   first parameter's name (pname); both unchanged when none is marked */
void auto_get_info(Session &s, int *n, std::string &pname);
/* AUTO's printed column heading col in the user's own names (screen
   only), AUTO_COL_W wide */
std::string auto_screen_col(const Session &s, const std::string &col);
/* AUTO's index of parameter or period name s (10 for the period, T),
   -1 when it is not one of AUTO's parameters */
int auto_name_to_index(const Session &s, std::string_view name);

/* AUTO's solution file (fort.8 after a run, the orbits of the labelled
   points a grab restarts from): <HOME or the model's folder>/<model>.s */
std::string auto_solutions_file(const Session &s);

/* view ax's axis labels as the axes show them: a name, name_bar or
   "Frequency" each (json_auto.cpp's diagram events) */
void get_auto_str(const Session &s, const AUTOAX &ax, std::string &xlabel, std::string &ylabel);

} // namespace xpp
#endif
