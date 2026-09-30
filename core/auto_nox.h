#ifndef _auto_nox_h_
#define _auto_nox_h_

#include <stdio.h>
#ifdef __cplusplus
#include <string>
#endif
#include "xpplim.h"
#include "autlim.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_AUT_PER 10

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
#ifdef __cplusplus
}

namespace xpp {
struct Session; /* session.h */
}
#endif

void colset(xpp::Session &s, int type);
void pscolset2(xpp::Session &s, int flag2);
void colset2(int flag2);
void draw_export_axes(xpp::Session &s);
void draw_bif_axes(xpp::Session &s);
int IXVal(const xpp::Session &s, double x);
int IYVal(const xpp::Session &s, double y);
int chk_auto_bnds(const xpp::Session &s, int ix, int iy);
void close_auto(xpp::Session &s, int flag);
void open_auto(xpp::Session &s, int flag);
/* the path of AUTO's fort.<unit> (3, 7, 8 or 9) open_auto set, "" for
   another unit: gogoauto.cpp opens them for a run */
const char *auto_fort_path(int unit);
void do_auto(xpp::Session &s, int iold, int isave, int itp);
void set_auto(xpp::Session &s);
/* AUTO's index of the period among its parameters (PAR(10), named T) */
#define AUTO_PERIOD_INDEX 10
/* the model's name of AUTO's parameter index k (AutoPar[k]), NULL when k
   is not one of AUTO's NAutoPar parameters: the one lookup the forms,
   the settings event, the CSV export and the branch-end text share */
const char *auto_par_name(const xpp::Session &s, int k);
#define AUTO_COL_W 14
void auto_per_par(xpp::Session &s);
void auto_params(xpp::Session &s);
void auto_num_par(xpp::Session &s);
void auto_plot_par(xpp::Session &s);
void auto_fit(xpp::Session &s);
void auto_default(xpp::Session &s);
/* the diagram's views (W50, AutoState::views). New view: one more, a copy
   of the active one's axes, made active. Close view k: 0 when it is the
   last one (it stays) or there is no view k. Activate view k: the one the
   Axes menu, zoom, the exports and a run's parameters then use; 0 when
   there is no view k. Each draws the diagram again when AUTO's window is
   open. */
void auto_new_view(xpp::Session &s);
int auto_close_view(xpp::Session &s, int k);
int auto_activate_view(xpp::Session &s, int k);
void auto_zoom_in(xpp::Session &s, int i1, int j1, int i2, int j2);
void auto_zoom_out(xpp::Session &s, int i1, int j1, int i2, int j2);
/* where view ax plots a point: x, and y1 and y2 (the maximum and minimum,
   or y1 twice) */
void auto_xy_plot(const AUTOAX *ax, double *x, double *y1, double *y2, double par1, double par2, double per, double *uhigh, double *ulow, double *ubar, double a);
void add_ps_point(xpp::Session &s, double *par, double per, double *uhigh, double *ulow, double *ubar, double a, int type, int flag, int lab, int npar, int icp1, int icp2, int flag2, double *evr, double *evi);
void auto_line(xpp::Session &s, double x1i, double y1i, double x2i, double y2i);
/* who the next add_point() is: AUTO's branch, point and type, its entry in
   the diagram list (DIAGRAM.index) and the label its run started from */
void auto_point_id(int ibr, int ntot, int itp, int node, int from);
/* the label the running continuation started from, once: for its first point */
int auto_run_from_take(void);
void add_point(xpp::Session &s, double *par, double per, double *uhigh, double *ulow, double *ubar, double a, int type, int flag, int lab, int npar, int icp1, int icp2, int flag2,int icp3, int icp4, double *evr, double *evi);
/* the two-letter symbol of AUTO's point type itp (BP, LP, HB, UZ, PD, TR,
   EP, MX), two blanks for any other */
const char *auto_bif_sym(int itp);
void info_header(xpp::Session &s, int flag2, int icp1, int icp2);
void new_info(xpp::Session &s, int ibr, int pt, const char *ty, int lab, double *par, double norm, double u0, double per, int flag2, int icp1, int icp2);
void traverse_out(xpp::Session &s, DIAGRAM *d, int *ix, int *iy, int dodraw);
void do_auto_win(xpp::Session &s);
void load_last_plot(xpp::Session &s, int flag);
void keep_last_plot(xpp::Session &s, int flag);
void init_auto_win(xpp::Session &s);
int yes_reset_auto(xpp::Session &s);
int reset_auto(xpp::Session &s);
void auto_grab(xpp::Session &s);

void auto_start_diff_ss(xpp::Session &s);
void auto_start_at_bvp(xpp::Session &s);
void auto_start_at_per(xpp::Session &s);
void find_best_homo_shift(xpp::Session &s, int n);
void get_start_period(xpp::Session &s, double *p);
void get_start_orbit(xpp::Session &s, double *u, double t, double p, int n);
void get_shifted_orbit(xpp::Session &s, double *u, double t, double p, int n);
void auto_new_ss(xpp::Session &s);
void auto_new_discrete(xpp::Session &s);
void auto_extend_ss(xpp::Session &s);
void auto_start_choice(xpp::Session &s);
void torus_choice(xpp::Session &s);
void per_doub_choice(xpp::Session &s);
void periodic_choice(xpp::Session &s);
void hopf_choice(xpp::Session &s);
void auto_new_per(xpp::Session &s);
void auto_start_at_homoclinic(xpp::Session &s);
int get_homo_info(xpp::Session &s, int flg,int *nun,int *nst,double *ul, double *ur);
void auto_extend_homoclinic(xpp::Session &s);
void auto_extend_bvp(xpp::Session &s);
void auto_switch_per(xpp::Session &s);
void auto_switch_bvp(xpp::Session &s);
void auto_switch_ss(xpp::Session &s);
void auto_2p_limit(xpp::Session &s, int ips);
void auto_twopar_double(xpp::Session &s);
void auto_torus(xpp::Session &s);
void auto_2p_branch(xpp::Session &s, int ips);
void auto_branch_choice(xpp::Session &s, int ibr, int ips);
void auto_homo_choice(xpp::Session &s, int itp);
void auto_2p_fixper(xpp::Session &s);
void auto_2p_hopf(xpp::Session &s);
void auto_period_double(xpp::Session &s);
void auto_run(xpp::Session &s);
void load_auto_orbit(xpp::Session &s);
/* File/Save diagram and Load diagram: an .autox (autox.h), or an XPPAUT
   .auto imported */
void save_auto(xpp::Session &s);
void load_auto(xpp::Session &s);
/* the parts of an XPPAUT .auto file, read (import_auto_file reads them
   all) */
void load_auto_numerics(xpp::Session &s, FILE *fp);
void load_auto_graph(xpp::Session &s, FILE *fp);
int import_auto_file(xpp::Session &s, FILE *fp);
int move_to_label(int mylab, int *nrow, int *ndim, FILE *fp);
void get_a_row(double *u, double *t, int n, FILE *fp);
void auto_file(xpp::Session &s);
int check_plot_type(const xpp::Session &s, int flag2,int icp1, int icp2);

/* grabbing and marking points on the diagram (from auto_x11.c) */
void traverse_diagram(xpp::Session &s);
/* the scriptable grabs: the same outcome as an interactive grab ending
   with Return on that point (docs/protocol.md "Grab by point"). 1 on
   success, 0 (nothing changed) when there is no such point. */
int auto_grab_label(xpp::Session &s, int lab);
int auto_grab_type_index(xpp::Session &s, const char *type, int index);
/* asks which kind of special point: its symbol (auto_bif_sym's), or NULL */
const char *query_special(const char *title);
void MarkAuto(int x, int y);
void clear_msg(xpp::Session &s);
void auto_update_view(xpp::Session &s, float xlo, float xhi, float ylo, float yhi);
void auto_motion_xy(xpp::Session &s, int i, int j);
/* a click on the diagram at (x, y) in its axis quantities: shown, and kept
   as the point AUTO's File/sElect 2par pt uses in a two-parameter plot */
void auto_point_xy(xpp::Session &s, double x, double y);

void DLINE(xpp::Session &s, double a, double b, double c, double d);
void find_point(xpp::Session &s, int ibr, int pt);
void auto_set_mark(xpp::Session &s, int i);
void do_auto_range(xpp::Session &s);

#ifdef __cplusplus
#include <string>
#include "xpp_error.h"

/* orbit lab of the solution file into the data (a labelled point's, when
   a grab or a diagram point asks for it); what failed otherwise, for the
   command to show */
xpp::Result<> load_auto_orbitx(xpp::Session &s, int ibr,int flag, int lab, double per);
/* the .auto's copy of the solution file, written beside this model's */
xpp::Result<> make_q_file(FILE *fp);

/* the number of points of the marked stretch of the diagram (*n) and its
   first parameter's name (pname); both unchanged when none is marked */
void auto_get_info(xpp::Session &s, int *n, std::string &pname);
/* AUTO's printed column heading col in the user's own names (screen
   only), AUTO_COL_W wide */
std::string auto_screen_col(const xpp::Session &s, const std::string &col);
/* AUTO's index of parameter or period name s (10 for the period, T),
   -1 when it is not one of AUTO's parameters */
int auto_name_to_index(const xpp::Session &s, std::string_view name);

/* AUTO's solution file (fort.8 after a run, the orbits of the labelled
   points a grab restarts from): <HOME or the model's folder>/<model>.s */
std::string auto_solutions_file();

/* view ax's axis labels as the axes show them: a name, name_bar or
   "Frequency" each (json_auto.cpp's diagram events) */
void get_auto_str(const xpp::Session &s, const AUTOAX &ax, std::string &xlabel, std::string &ylabel);
#endif
#endif
