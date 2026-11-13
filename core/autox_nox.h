#pragma once

#include <stdio.h>
#include <string>
#include <string_view>
#include <array>
#include <span>
#include <vector>
#include <functional>
#include <optional>
#include <utility>
#include <memory>
#include "xpp_error.h"
#include "autox_codes.h"
#include "autox_axis.h"


struct AutoxRun; /* autox_state.h */

namespace xpp {

struct Session; /* session.h */
class TokenReader;
/* The twelve fort.8 header fields, read and bounded once for every autoX
   restart/orbit reader and the session's untrusted solution member. */
struct AutoxSolutionHeader {
    long ibr{}, ntot{}, lab{}, nfpr{}, isw{}, ntpl{}, nar{}, nskip{}, ntst{}, ncol{}, npar{};
    AutoxPointCode itp; /* decoded when the header is read: an unknown code is a bad header */
    /* an equilibrium's record with the branch's direction after the point (AUTO-07p's NTST=1, no mesh); no orbit has no collocation points */
    bool equilibrium_direction() const { return ntst == 1 && ncol == 0; }
};
/* the first four fields of a point's row in fort.7 (.b): branch, point, type and label, as the writers print them and the
   join of jobs (autox_jobs.cpp) prints them again once renumbered */
/* a header line of fort.8 (.s), its newline included, from its twelve fields */
std::string autox_solution_header_text(const AutoxSolutionHeader &header);
/* the integer (a sign and digits) that starts at `at` after blanks and ends at a blank, a comma or the end, and where it ends */
std::optional<std::pair<long,std::size_t>> autox_integer_at(std::string_view line, std::size_t at);
/* a point's row of the table, on the screen and in fort.9 (.d): branch, point, the type's mark and the label */
std::string autox_point_row_key(long branch, long point, std::string_view mark, long label);
/* A line of the table or of fort.9 with the numbers of its branch and label moved by the callers' maps: a point's row
   (autox_point_row_key), a line that starts "{:4} {:5}" (branch, point: the iteration and note lines), an iteration
   line's " Branch {:2} N=", and the stop lines, warnings and notes that say "Branch N", "Br N" or "branch N". Parsed by the writers' own layout, whatever the widths a large number makes of it; any
   other line as it is. */
std::string autox_renumbered_line(std::string_view line, const std::function<int(int)> &branch, const std::function<int(int)> &label);
std::string autox_branch_row_key(long branch, long point, long type, long label);
bool read_autox_solution_header(TokenReader &reader, AutoxSolutionHeader &h, long parameters);
/* Where each solution of a fort.8/.s file starts, found by one pass over it and kept: autoX's start label is looked up here
   (find_restart_label, new_restart_label, move_to_label) instead of by reading every solution of the session's accumulated .s file again for each run (W705).
   A record is a header line: its position in the file as tell_stream gives it, and the header fields the start needs. */
struct AutoxSolutionRecord {
    long position{}, ibr{}, lab{}, nfpr{};
    AutoxPointCode itp;
};
struct AutoxSolutionIndex {
    std::vector<AutoxSolutionRecord> records; /* in file order: the newest run's first, as close_autox prepends */
    long bytes{};                              /* the file's length */
    long max_branch{}, max_label{};            /* the largest of the records', 0 for none */
};
/* the index of the file at `path` (one pass; a header that cannot be read ends it, as it ends autoX's search); parameters: the layout's size */
Result<AutoxSolutionIndex> autox_index_solutions(const std::string &path, long parameters);
/* the session's .s file's index, made on first use and kept up to date by close_autox, a loaded session and a cleared diagram */
Result<const AutoxSolutionIndex *> autox_session_solutions(Session &s);
/* the index of the run's fort.3: the session's, when the run restarts from it, else its own file's, made on first use; a failure stops the run */
const AutoxSolutionIndex &autox_run_solutions(AutoxRun &run);
// Read a collocation payload into the formulation's explicit target columns.
// tm selects orbit rows (which carry a time); null selects tangent rows.
// A line-bounded source retains the old wrapped-line skips. The callback
// initializes formulation-specific columns after each interior node is read.
bool read_autox_solution_rows(TokenReader &reader, std::ptrdiff_t ndim, std::ptrdiff_t ntst, std::ptrdiff_t ncol,
                             std::ptrdiff_t stride, std::ptrdiff_t first, std::ptrdiff_t count, double *values,
                             double *tm, bool line_bounded, std::ptrdiff_t extra_lines,
                             const std::function<void(std::ptrdiff_t, std::ptrdiff_t, double)> &complete);
bool read_autox_solution_parameters(TokenReader &reader, const AutoxSolutionHeader &header,
                                   const AutoxRun &run, double *parameters);
Result<> check_autox_solutions(std::string_view text, std::string file, int variables, int parameters, bool foreign, int hopf_dimension = 0);

typedef struct {
  double torper;
} ROTCHK;

/* a Mark the user set (the autoX window's Usr period): a model parameter's
   index, or the period, and the value autoX labels (UZ) a point at; a run
   turns each into autoX's slot, ::AutoxMark (xAutox.h) */
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
     axes are the views', AutoxState::views) */
  double lastx,lasty;
  int wid,hgt,x0,y0;
  int nfpar,nbc;
  AutoxProblem ips=AutoxProblem::Algebraic;
  int irs,ilp,isp,isw;
  AutoxPointCode itp;
  int icp3,icp4,icp5;
  std::string hinttxt;
  // User marks belong to the session, independently of the model dimension.
  std::vector<UserMark> marks;
  double epsl,epsu,epss;
  int ncol;
};

struct ADVAUTOX {
  int iad = 3;
  /* autoX's MXBF is one signed number: the count, negative for one direction only (W531 keeps them apart) */
  int max_branch_switches = 5;
  bool switch_one_direction = false;
  int iid = 2; /* an index of autox_output_detail_names */
  int itmx = 8;
  int itnw = 7;
  int nwtn = 3;
  int iads = 1;
};

struct GRABPT {
  int ibr,lab;
  AutoxPointCode itp;
  std::vector<double> uhi,ulo,u0,ubar,par;
  double per,torper;
  int nfpar;
  int flag;
  AutoxCalculation curve=no_calculation; // diagram curve kind, including the neutral-saddle extension
  AutoxCalculation calc=no_calculation; // the calculation that made the point (its branch's problem)
};

struct DiagramBranch {
  std::vector<double> parameters; /* every model parameter, once per computed branch */
};

typedef struct diagram {
  int ibr=0,ntot=0,lab=0;
  AutoxCalculation calc=no_calculation;
  AutoxPointCode itp;
  double norm=0;
  /* the point's n variables' extremes, solution, norm and stability values
     (autox_stability.h), all n long (size_variables) */
  std::vector<double> uhi,ulo,u0,ubar,evr,evi;
  std::shared_ptr<const DiagramBranch> branch;
  std::vector<int> parameter_indices;
  std::vector<double> par; /* only the continued parameters */
  double per=0,torper=0;
  int index=0,nfpar=0;
  AutoxAxis icp1,icp2,icp3,icp4; /* what the point continued in (autox_axis.h) */
  AutoxCalculation flag2=no_calculation;
  int from=0; /* the label its run started from, on a run's first point (not saved in files) */
  int job=0; /* the branch job of a follow_all_branches run that computed it, counted in the order of the join; 0: none (not saved in files) */
  int jacobian=0; /* how the run that computed it formed the Jacobian: JACOBIAN_DIFFERENCE or JACOBIAN_EXACT (jacobian.h, W278) */
  /* the six arrays n long, zeroed: the one place a point's dimension is set */
  void size_variables(std::size_t n){
    for(std::vector<double> *v:{&uhi,&ulo,&u0,&ubar,&evr,&evi})v->assign(n,0.0);
  }
} DIAGRAM;

/* a view of the diagram's axes (W50, AutoxState::views): what it plots
   (its projection; the variable var; icp1 the x axis' parameter
   and, in a two-parameter view, icp2 the y axis', both indices into
   Model::upar_names) and the ranges. The active view's icp1 and icp2 are also
   the parameters a run continues in. */
typedef struct {
  AutoxProjection plot;
  int var,icp1,icp2;
  double xmin,ymin,xmax,ymax;
}  AUTOAX;

void draw_export_axes(Session &s);
void draw_bif_axes(Session &s);
int IXVal(const Session &s, double x);
int IYVal(const Session &s, double y);
/* the run's end: its fort.7, .9 and .8 replace the session's .b, .d and .s
   files (flag OVERWRITE) or go in front of them (APPEND), and its fort.3 and
   the rest go; the fort files are the run's own (open_autox) */
void close_autox(Session &s, AutoxRun &run, int flag);
/* the run's own scratch folder and fort.3/7/8/9 in it; flag OPEN_3 copies the session's .s
   file to fort.3 for a restart. An error when the folder cannot be made */
Result<> open_autox(Session &s, AutoxRun &run, int flag);
/* the path of the run's fort.<unit> (3, 7, 8 or 9) open_autox set, "" for
   another unit: gogoautox.cpp opens them for a run */
const char *autox_fort_path(const AutoxRun &run, int unit);
/* do_autox's iold: autoX's fort.3 is opened for a restart (OPEN_3) or not; isave: the run's solutions replace the .s file or
   are added to it */
inline constexpr int OPEN_3 = 1, NO_OPEN_3 = 0, OVERWRITE = 0, APPEND = 1;
/* what every start of a run checks and prepares first: the model has a parameter, and isave and the parameters
   a restart label holds are settled; the error is what is wrong */
Result<> autox_run_prologue(Session &s, int &isave);
/* one run's depth, however it ends: the outermost clears why the last runs' branches ended (autox_stop.h) */
struct AutoxDepth {
  int &depth;
  explicit AutoxDepth(Session &s);
  ~AutoxDepth() { depth--; }
  AutoxDepth(const AutoxDepth &)=delete;
  AutoxDepth &operator=(const AutoxDepth &)=delete;
};
/* where the run's solutions are kept: the dir may have changed since the last run */
void create_autox_file_name(Session &s);
/* A job that starts at what another job found (autox_jobs.cpp): the problem it continues (ips, isw, isp as the start of such a
   run sets them), and, when it restarts from the point itself (label above 0), the finding job's label, the point as that job
   stored it (its stability), which is not in the diagram yet, and its solution as the finding job's fort.8 holds it, which
   is this job's fort.3. Label 0: a run from the initial conditions, with no point and no solution */
struct AutoxFoundStart {
  AutoxProblem ips;
  int isw,isp,label;
  const DIAGRAM *point;
  std::string solution;
};
/* a run made ready (the Session's settings into run.params, its folder and fort files, where it started from and its
   first point's stability); the error is what is wrong. After create_autox_file_name. */
Result<> autox_set_up_run(Session &s, AutoxRun &run, int iold, const AutoxFoundStart *found = nullptr);
/* a run that left a model parameter not finite (a diverged Newton step) puts the value it had before, `before` (the model's
   nupar parameters), back, with a warning (QA SCI-001) */
void autox_restore_finite_pars(Session &s, const double *before);
/* the numerics of one run, which ends early with the Error it failed with */
std::optional<Error> autox_numerics(AutoxRun &run);
/* what a run that failed shows: its Error as the command returns it ("autoX stopped: ..."), also written to the autoX
   window's Output, with the command's place when it has none */
Error autox_shown_failure(Error failed);
/* the end of a run (or of one job of several, once it is joined): why its branches ended is the Session's, its fort files
   the session's .b/.d/.s (isave, as close_autox), and what failed is shown and returned for the command */
std::optional<Error> autox_end_run(Session &s, AutoxRun &run, std::optional<Error> failed, int isave);
/* a run set up (set_autox) and made; the error is what went wrong, shown by the command that ran it */
Result<> do_autox(Session &s, int iold, int isave, AutoxPointCode itp);
/* goes on from the point labelled `label` with the settings of the run
   before (W275: the action of an exact Jacobian's failure); an error, and
   nothing run, for a label no point has */
/* Restore model slots from the label's branch before a restart solver uses
   them. Non-continued constants were deliberately absent from autoX's PAR. */
void autox_label_parameters(const AutoxRun &run, int label, double *parameters);
/* the model parameter k's name, NULL when k is outside the model: the one lookup the forms,
   the settings event, the CSV export and the branch-end text share */
const char *autox_par_name(const Session &s, int k);
void autox_fit(Session &s);
void autox_default(Session &s);
/* the diagram's views (W50, AutoxState::views). New view: one more, a copy
   of the active one's axes, made active. Close view k: 0 when it is the
   last one (it stays) or there is no view k. Activate view k: the one the
   Axes menu, zoom, the exports and a run's parameters then use; 0 when
   there is no view k. Each draws the diagram again when autoX's window is
   open. */
void autox_new_view(Session &s);
int autox_close_view(Session &s, int k);
int autox_activate_view(Session &s, int k);
/* the active view shows this box of its quantities, x1 below x2 and y1 below y2, or the box a zoom out shrinks it into
   (autox_zoom_out_to), or is moved by dx, dy; the caller draws the diagram again (redraw_diagram) */
/* Clear: the branches so far become the earlier ones, hidden until shown again */
void autox_hide_earlier(Session &s);
void autox_zoom_to(Session &s, double x1, double x2, double y1, double y2);
void autox_zoom_out_to(Session &s, double x1, double x2, double y1, double y2);
void autox_scroll_by(Session &s, double dx, double dy);
/* where view ax plots a point: x, and y1 and y2 (the maximum and minimum,
   or y1 twice) */
void autox_xy_plot(const AUTOAX *ax, double *x, double *y1, double *y2, double par1, double par2, double per, std::span<const double> uhigh, std::span<const double> ulow, std::span<const double> ubar, double a);
/* the stored point d into the PDF or SVG picture being written
   (type its get_bif_type, flag 0 for a branch's first point) */
void add_ps_point(Session &s, const DIAGRAM &d, DiagramPointType type, int flag);
/* the label the running continuation started from, once: for its first point */
int autox_run_from_take(AutoxRun &run);
/* the stored point d (its branch, point and type, its entry in the
   diagram list, the label its run started from, its stability) plotted in
   every view: the diagram's data and the active view's drawing, resolving
   parameters from its stored branch. type is its get_bif_type, flag 0 for
   a branch's first point. */
void add_point(Session &s, const DIAGRAM &d, DiagramPointType type, int flag);
/* the point d of a running job (autox_jobs.cpp) in every view's data as it comes, where add_point puts a stored one: it is in no
   diagram entry, leaves the info strip and the circle alone, and is the job `live`'s (XppDiagPoint.live) */
void add_live_point(Session &s, const DIAGRAM &d, int live, DiagramPointType type, int flag);
/* the info strip's line for the stored point d */
void traverse_out(Session &s, DIAGRAM *d, int *ix, int *iy, int dodraw);
void do_autox_win(Session &s);
/* the axes a one-parameter and a two-parameter view had last
   (AutoxState::old1p, old2p): the Axes menu's last 1 par and last 2 par */
enum class LastPlot { OneParameter, TwoParameter };
void load_last_plot(Session &s, LastPlot which);
void keep_last_plot(Session &s, LastPlot which);
/* the active view's axes kept as the last of their kind (a two-parameter
   view's, or a one-parameter one's; XPPAUT keeps no frequency or average) */
void keep_axes_as_last(Session &s);
void init_autox_win(Session &s);
int yes_reset_autox(Session &s);
void autox_grab(Session &s);

void find_best_homo_shift(Session &s, int n);
void get_start_period(Session &s, double *p);
void get_start_orbit(Session &s, double *u, double t, double p, int n);
/* File's Import orbit: the stored orbit of point `label` (none: the grabbed point's) into the data table */
Result<> load_autox_orbit(Session &s, std::optional<int> label);
/* the diagram as CSV files (csv_export.h), and a foreign XPPAUT diagram (.auto), to or from `file`, or asked for */
Result<> export_autox_csv(Session &s, const std::optional<std::string> &file);
Result<> import_autox(Session &s, const std::optional<std::string> &file);
// XPPAUT stores eight selected parameters before its numerical slots.
inline constexpr int xppaut_parameter_prefix = 8;
/* Foreign XPPAUT import: validate whole, then replace; errors belong to the caller. */
Result<> import_autox_file(Session &s, FILE *fp, const std::string &file);
int move_to_label(Session &s, int mylab, int *nrow, int *ndim, FILE *fp);
/* the solution labelled `label` in a fort.8 or .s file (its header line and its rows, as written, each with its newline), for
   a restart of another run (autox_jobs.cpp); none when the file has no such label or is damaged. parameters: the layout's size */
std::optional<std::string> autox_solution_block(const std::string &path, int label, long parameters);
void get_a_row(double *u, double *t, int n, FILE *fp);
int check_plot_type(const Session &s, AutoxCalculation flag2, AutoxAxis icp1, AutoxAxis icp2);

/* grabbing and marking points on the diagram (from autox_x11.c) */
void traverse_diagram(Session &s);
/* the scriptable grabs: the same outcome as an interactive grab ending
   with Return on that point (docs/protocol.md "Grab by point"). 1 on
   success, 0 (nothing changed) when there is no such point. */
int autox_grab_label(Session &s, int lab);
int autox_grab_type_index(Session &s, AutoxPointKind type, int index);
/* asks which point type the grab-by-type menu picks, or none */
std::optional<AutoxPointKind> query_special(const char *title);
void clear_msg(Session &s);
/* a click on the diagram at (x, y) in its axis quantities: shown, and kept
   as the point autoX's File/sElect 2par pt uses in a two-parameter plot */
void autox_point_xy(Session &s, double x, double y);
/* the two parameters set to the point autox_point_xy stored (File/sElect 2par pt) */
void setautoxpoint(Session &s);

void find_point(Session &s, int ibr, int pt);
void autox_set_mark(Session &s, int i);
void do_autox_range(Session &s);

/* orbit lab of the solution file into the data (a labelled point's, when
   a grab or a diagram point asks for it); what failed otherwise, for the
   command to show */
Result<> load_autox_orbitx(Session &s, int ibr,int flag, int lab, double per);

/* the number of points of the marked stretch of the diagram (*n) and its
   first parameter's name (pname); both unchanged when none is marked */
void autox_get_info(Session &s, int *n, std::string &pname);
/* autoX's printed column heading col in the user's own names (screen
   only), autox_column_width (autox_c.h) wide */
std::string autox_screen_col(const Session &s, const std::string &col);

/* autoX's solution file (fort.8 after a run, the orbits of the labelled
   points a grab restarts from): <HOME or the model's folder>/<model>.s */
std::string autox_solutions_file(const Session &s);

/* view ax's axis labels as the axes show them: a name, name_bar or
   "Frequency" each (json_autox.cpp's diagram events) */
void get_autox_str(const Session &s, const AUTOAX &ax, std::string &xlabel, std::string &ylabel);

/* at exit (xppautx_main.cpp's atexit hook): removes s's autoX scratch
   folder (autox_state.h) if it is set, and clears it */
void cleanup_autox_dir(Session &s);

/* another model is loaded (File > Open model, Reload): a new, empty autoX
   scratch folder in place of the Session s's, whose files (<model>.s, .b,
   .d) were the model before's */
void renew_autox_dir(Session &s);

} // namespace xpp
