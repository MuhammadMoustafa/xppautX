#pragma once

#include <stdio.h>
#include <string>
#include <string_view>
#include <array>
#include <span>
#include <vector>
#include <memory>
#include <functional>
#include "xpp_error.h"
#include "auto_codes.h"
#include "auto_axis.h"


namespace xpp {

struct Session; /* session.h */
class TokenReader;
/* The twelve fort.8 header fields, read and bounded once for every AUTO
   restart/orbit reader and the session's untrusted solution member. */
struct AutoSolutionHeader {
    long ibr{}, ntot{}, lab{}, nfpr{}, isw{}, ntpl{}, nar{}, nskip{}, ntst{}, ncol{}, npar{};
    AutoPointCode itp; /* decoded when the header is read: an unknown code is a bad header */
};
bool read_auto_solution_header(TokenReader &reader, AutoSolutionHeader &h, long parameters);
// Read a collocation payload into the formulation's explicit target columns.
// tm selects orbit rows (which carry a time); null selects tangent rows.
// A line-bounded source retains the old wrapped-line skips. The callback
// initializes formulation-specific columns after each interior node is read.
bool read_auto_solution_rows(TokenReader &reader, std::ptrdiff_t ndim, std::ptrdiff_t ntst, std::ptrdiff_t ncol,
                             std::ptrdiff_t stride, std::ptrdiff_t first, std::ptrdiff_t count, double *values,
                             double *tm, bool line_bounded, std::ptrdiff_t extra_lines,
                             const std::function<void(std::ptrdiff_t, std::ptrdiff_t, double)> &complete);
bool read_auto_solution_parameters(TokenReader &reader, const AutoSolutionHeader &header,
                                   const Session &session, double *parameters);
Result<> check_auto_solutions(std::string_view text, std::string file, int variables, int parameters, bool foreign, int hopf_dimension = 0);

typedef struct {
  double torper;
} ROTCHK;

/* a Mark the user set (the AUTO window's Usr period): a model parameter's
   index, or the period, and the value AUTO labels (UZ) a point at; a run
   turns each into AUTO's slot, ::AutoMark (xAuto.h) */
struct UserMark {
  int parameter;
  bool period;
  double value;
  bool operator==(const UserMark &) const = default;
};

struct BIFUR {

  int exist;
  int ntst,nmx,npr;
  double ds,dsmax,dsmin,rl0,rl1,a0,a1;
  /* the active view's last point, where its next line starts (the
     axes are the views', AutoState::views) */
  double lastx,lasty;
  int wid,hgt,x0,y0;
  int nfpar,nbc;
  AutoProblem ips=AutoProblem::Algebraic;
  int irs,ilp,isp,isw;
  AutoPointCode itp;
  int icp3,icp4,icp5;
  std::string hinttxt;
  // User marks belong to the session, independently of the model dimension.
  std::vector<UserMark> marks;
  double epsl,epsu,epss;
  int ncol;
};

struct ADVAUTO {
  int iad = 3;
  /* AUTO's MXBF is one signed number: the count, negative for one direction only (W531 keeps them apart) */
  int max_branch_switches = 5;
  bool switch_one_direction = false;
  int iid = 2; /* an index of auto_output_detail_names */
  int itmx = 8;
  int itnw = 7;
  int nwtn = 3;
  int iads = 1;
};

struct GRABPT {
  int ibr,lab;
  AutoPointCode itp;
  std::vector<double> uhi,ulo,u0,ubar,par;
  double per,torper;
  int nfpar;
  int flag;
  AutoCalculation curve=no_calculation; // diagram curve kind, including the neutral-saddle extension
  AutoCalculation calc=no_calculation; // the calculation that made the point (its branch's problem)
};

struct DiagramBranch {
  std::vector<double> parameters; /* every model parameter, once per computed branch */
};

typedef struct diagram {
  int ibr=0,ntot=0,lab=0;
  AutoCalculation calc=no_calculation;
  AutoPointCode itp;
  double norm=0;
  /* the point's n variables' extremes, solution, norm and stability values
     (auto_stability.h), all n long (size_variables) */
  std::vector<double> uhi,ulo,u0,ubar,evr,evi;
  std::shared_ptr<const DiagramBranch> branch;
  std::vector<int> parameter_indices;
  std::vector<double> par; /* only the continued parameters */
  double per=0,torper=0;
  int index=0,nfpar=0;
  AutoAxis icp1,icp2,icp3,icp4; /* what the point continued in (auto_axis.h) */
  AutoCalculation flag2=no_calculation;
  int from=0; /* the label its run started from, on a run's first point (not saved in files) */
  int jacobian=0; /* how the run that computed it formed the Jacobian: JACOBIAN_DIFFERENCE or JACOBIAN_EXACT (jacobian.h, W278) */
  /* the six arrays n long, zeroed: the one place a point's dimension is set */
  void size_variables(std::size_t n){
    for(std::vector<double> *v:{&uhi,&ulo,&u0,&ubar,&evr,&evi})v->assign(n,0.0);
  }
} DIAGRAM;

/* a view of the diagram's axes (W50, AutoState::views): what it plots
   (its projection; the variable var; icp1 the x axis' parameter
   and, in a two-parameter view, icp2 the y axis', both indices into
   Model::upar_names) and the ranges. The active view's icp1 and icp2 are also
   the parameters a run continues in. */
typedef struct {
  AutoProjection plot;
  int var,icp1,icp2;
  double xmin,ymin,xmax,ymax;
}  AUTOAX;

void draw_export_axes(Session &s);
void draw_bif_axes(Session &s);
int IXVal(const Session &s, double x);
int IYVal(const Session &s, double y);
void close_auto(Session &s, int flag);
void open_auto(Session &s, int flag);
/* the path of AUTO's fort.<unit> (3, 7, 8 or 9) open_auto set, "" for
   another unit: gogoauto.cpp opens them for a run */
const char *auto_fort_path(const Session &s, int unit);
/* do_auto's iold: AUTO's fort.3 is opened for a restart (OPEN_3) or not; isave: the run's solutions replace the .s file or
   are added to it */
inline constexpr int OPEN_3 = 1, NO_OPEN_3 = 0, OVERWRITE = 0, APPEND = 1;
/* a run set up (set_auto) and made; the error is what went wrong, shown by the command that ran it */
Result<> do_auto(Session &s, int iold, int isave, AutoPointCode itp);
/* goes on from the point labelled `label` with the settings of the run
   before (W275: the action of an exact Jacobian's failure); an error, and
   nothing run, for a label no point has */
/* Restore model slots from the label's branch before a restart solver uses
   them. Non-continued constants were deliberately absent from AUTO's PAR. */
void auto_label_parameters(const Session &s, int label, double *parameters);
void set_auto(Session &s);
/* the model parameter k's name, NULL when k is outside the model: the one lookup the forms,
   the settings event, the CSV export and the branch-end text share */
const char *auto_par_name(const Session &s, int k);
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
/* the active view shows this box of its quantities, x1 below x2 and y1 below y2, or the box a zoom out shrinks it into
   (auto_zoom_out_to), or is moved by dx, dy; the caller draws the diagram again (redraw_diagram) */
/* Clear: the branches so far become the earlier ones, hidden until shown again */
void auto_hide_earlier(Session &s);
void auto_zoom_to(Session &s, double x1, double x2, double y1, double y2);
void auto_zoom_out_to(Session &s, double x1, double x2, double y1, double y2);
void auto_scroll_by(Session &s, double dx, double dy);
/* where view ax plots a point: x, and y1 and y2 (the maximum and minimum,
   or y1 twice) */
void auto_xy_plot(const AUTOAX *ax, double *x, double *y1, double *y2, double par1, double par2, double per, std::span<const double> uhigh, std::span<const double> ulow, std::span<const double> ubar, double a);
/* the stored point d into the PostScript or SVG picture being written
   (type its get_bif_type, flag 0 for a branch's first point) */
void add_ps_point(Session &s, const DIAGRAM &d, DiagramPointType type, int flag);
/* the label the running continuation started from, once: for its first point */
int auto_run_from_take(Session &s);
/* the stored point d (its branch, point and type, its entry in the
   diagram list, the label its run started from, its stability) plotted in
   every view: the diagram's data and the active view's drawing, resolving
   parameters from its stored branch. type is its get_bif_type, flag 0 for
   a branch's first point. */
void add_point(Session &s, const DIAGRAM &d, DiagramPointType type, int flag);
/* the info strip's line for the stored point d */
void traverse_out(Session &s, DIAGRAM *d, int *ix, int *iy, int dodraw);
void do_auto_win(Session &s);
/* the axes a one-parameter and a two-parameter view had last
   (AutoState::old1p, old2p): the Axes menu's last 1 par and last 2 par */
enum class LastPlot { OneParameter, TwoParameter };
void load_last_plot(Session &s, LastPlot which);
void keep_last_plot(Session &s, LastPlot which);
/* the active view's axes kept as the last of their kind (a two-parameter
   view's, or a one-parameter one's; XPPAUT keeps no frequency or average) */
void keep_axes_as_last(Session &s);
void init_auto_win(Session &s);
int yes_reset_auto(Session &s);
void auto_grab(Session &s);

void find_best_homo_shift(Session &s, int n);
void get_start_period(Session &s, double *p);
void get_start_orbit(Session &s, double *u, double t, double p, int n);
/* File's Import orbit: the stored orbit of point `label` (none: the grabbed point's) into the data browser */
Result<> load_auto_orbit(Session &s, std::optional<int> label);
/* the diagram as CSV files (csv_export.h), and a foreign XPPAUT diagram (.auto), to or from `file`, or asked for */
Result<> export_auto_csv(Session &s, const std::optional<std::string> &file);
Result<> import_auto(Session &s, const std::optional<std::string> &file);
// XPPAUT stores eight selected parameters before its numerical slots.
inline constexpr int xppaut_parameter_prefix = 8;
/* Foreign XPPAUT import: validate whole, then replace; errors belong to the caller. */
Result<> import_auto_file(Session &s, FILE *fp, const std::string &file);
int move_to_label(const Session &s, int mylab, int *nrow, int *ndim, FILE *fp);
void get_a_row(double *u, double *t, int n, FILE *fp);
int check_plot_type(const Session &s, AutoCalculation flag2, AutoAxis icp1, AutoAxis icp2);

/* grabbing and marking points on the diagram (from auto_x11.c) */
void traverse_diagram(Session &s);
/* the scriptable grabs: the same outcome as an interactive grab ending
   with Return on that point (docs/protocol.md "Grab by point"). 1 on
   success, 0 (nothing changed) when there is no such point. */
int auto_grab_label(Session &s, int lab);
int auto_grab_type_index(Session &s, AutoPointKind type, int index);
/* asks which point type the grab-by-type menu picks, or none */
std::optional<AutoPointKind> query_special(const char *title);
void clear_msg(Session &s);
/* a click on the diagram at (x, y) in its axis quantities: shown, and kept
   as the point AUTO's File/sElect 2par pt uses in a two-parameter plot */
void auto_point_xy(Session &s, double x, double y);
/* the two parameters set to the point auto_point_xy stored (File/sElect 2par pt) */
void setautopoint(Session &s);

void find_point(Session &s, int ibr, int pt);
void auto_set_mark(Session &s, int i);
void do_auto_range(Session &s);

/* orbit lab of the solution file into the data (a labelled point's, when
   a grab or a diagram point asks for it); what failed otherwise, for the
   command to show */
Result<> load_auto_orbitx(Session &s, int ibr,int flag, int lab, double per);

/* the number of points of the marked stretch of the diagram (*n) and its
   first parameter's name (pname); both unchanged when none is marked */
void auto_get_info(Session &s, int *n, std::string &pname);
/* AUTO's printed column heading col in the user's own names (screen
   only), auto_column_width (auto_c.h) wide */
std::string auto_screen_col(const Session &s, const std::string &col);

/* AUTO's solution file (fort.8 after a run, the orbits of the labelled
   points a grab restarts from): <HOME or the model's folder>/<model>.s */
std::string auto_solutions_file(const Session &s);

/* view ax's axis labels as the axes show them: a name, name_bar or
   "Frequency" each (json_auto.cpp's diagram events) */
void get_auto_str(const Session &s, const AUTOAX &ax, std::string &xlabel, std::string &ylabel);

/* at exit (xppautx_main.cpp's atexit hook): removes s's AUTO scratch
   folder (auto_state.h) if it is set, and clears it */
void cleanup_auto_dir(Session &s);

/* another model is loaded (File > Open model, Reload): a new, empty AUTO
   scratch folder in place of the Session s's, whose files (<model>.s, .b,
   .d) were the model before's */
void renew_auto_dir(Session &s);

} // namespace xpp
