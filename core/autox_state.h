#pragma once
/* autoX's part of a Session (session.h) and of one run. C++ only.

   AutoxState is the Session's: autox_nox.cpp's autoX window, its settings
   and the parameters it continues, the diagram's views, what the window
   shows (the last runs' stop, the info strip) and the folder the solutions
   are kept in.

   AutoxRun is one run's own (autox_lib1-5, gogoautox, setubv2): its
   parameters, its fort folder and files, its work arrays, why its branches
   ended, its stability values, and the Session whose model and settings it
   reads. do_autox makes one per run and the numerical code reaches the
   Session only through it, so two runs share no state of their own. What a
   run still writes on the Session is what is not a run's: the model's
   evaluation state (Session::parser, the integrator), the diagram its points
   are appended to and what the window shows of them. */
#include <stdio.h>
#include "autox_unknowns.h"
#include "jacobian.h"
#include "autox_nox.h"
#include "autox_data.h"
#include "colormap.h"
#include "autox_stop.h"
#include "autox_stability.h"
#include "autox_codim.h"
#include "autox_parallel.h"
#include "autox_script.h"
#include "display_state.h"
#include "xAutox.h"
#include "xpp_io.h"
#include "xpp_files.h"
#include "xpp_error.h"
#include "xpp_math.h"
#include "xpp_job.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

/* the autoX settings the model's @ options give (autox_ntst=, ...), which
   init_autox_win copies into AutoxState::bifur */
// A nonnegative norm has no useful lower default; the upper bound leaves ordinary branches unrestricted.
inline constexpr double autox_default_norm_min=0.0;
inline constexpr double autox_default_norm_max=1e100;
struct AutoxOptions {
  int ntst=15,nmx=200,npr=50,ncol=4;
  double ds=.02,dsmax=.5,dsmin=.001;
  double rl0=0.0,rl1=2,a0=autox_default_norm_min,a1=autox_default_norm_max;
  double xmax=2.5,xmin=-.5,ymax=3.0,ymin=-3.0;
  double epsl=1e-4,epsu=1e-4,epss=1e-4;
  int var=0;
};

/* one view of the diagram (W50): its axes, and the part of them the page
   shows (its zoom, W65; a zoom belongs to the axes it was made at, other
   ones drop it) */
// autoX starts with one default named view.
inline constexpr const char *initial_diagram_name="diagram_1";

struct AutoxDiagramView {
  xpp::AUTOAX axes{};
  xpp::Zoom zoom;
  bool zoom_seen=false; /* zoom_axes holds the axes the zoom was made at */
  std::array<double,4> zoom_axes{};
  std::string name = initial_diagram_name;
  bool hidden=false; /* window.hide: kept with its axes and zoom, not shown */
};

/* the diagram's marked stretch (the S and E keys in the Grab loop): the
   branch and point numbers of its start and end */
struct AutoxDiagramMark {
  int state=0;        /* 0 nothing, 1 start marked, 2 start and end */
  int start_branch=0,end_branch=0;
  int start_point=0,end_point=0;
};

// Only solver boundaries translate indices; all other parameter lists use Model indices.
struct AutoxParameterLayout {
  int block=xpp::autox_unknown::block_size;
  int model_parameters=0;
  int solver_index(int k) const { return block+k; }
  // What a solver slot is: a model parameter, or one of autoX's unknowns (the stored form of an axis).
  xpp::AutoxAxis axis(int slot) const { const int k=model_index(slot); return k>=0 ? xpp::AutoxAxis::parameter(k) : xpp::AutoxAxis::unknown(slot); }
  int model_index(int index) const { return index>=block && index<size() ? index-block : -1; }
  int size() const { return block+model_parameters; }
  // The solver's array par in its two parts: autoX's own block, then the model's parameters.
  template<class T> std::span<T> unknowns(T *par) const { return {par,static_cast<std::size_t>(block)}; }
  template<class T> std::span<T> parameters(T *par) const { return {par+block,static_cast<std::size_t>(model_parameters)}; }
};

/* a homoclinic run (HomCont, IPS 9): none, a homoclinic orbit (one
   equilibrium) or a heteroclinic one (a left and a right equilibrium) */
enum class HomoclinicRun { None, Homoclinic, Heteroclinic };
/* the equilibrium of a heteroclinic orbit */
enum class HomoclinicSide { Left, Right };
/* the File menu's "Draw labeled" toggle: off, traversing the diagram
   draws each labelled point's orbit, or clears the screens first */
enum class LabeledOrbits { Off, Draw, ClearAndDraw };
/* every codimension-two kind detected, as AUTO-07p does by default */
constexpr std::array<bool, xpp::autox_codim_kinds.size()> all_special_points()
{
  std::array<bool, xpp::autox_codim_kinds.size()> on{};
  on.fill(true);
  return on;
}

/* what a homoclinic or heteroclinic start gives HomCont's boundary conditions
   (autox_run.cpp): the dimensions of the equilibria's unstable and stable
   manifolds, and the kind of orbit (+1 homoclinic, -2 heteroclinic; 0 none) */
struct HomoclinicEquilibria {
  int nunstab=0,nstab=0,iequib=0;
};

/* where a stored orbit first returns to its first row (autox_run.cpp): the time from that row and the position among
   the rows, both interpolated between two */
struct AutoxOrbitReturn {
  double period=0,index=0;
};

struct AutoxState {
  /* the autoX window and its settings */
  xpp::BIFUR bifur{};
  /* the views of the one diagram (W50), at least one, each plotting it at
     its own axes. The active one is the Axes menu's and its zoom's, the
     exports', the info strip's and a run's: its icp1 and icp2 are the
     parameters a run continues in. */
  std::vector<AutoxDiagramView> views=std::vector<AutoxDiagramView>(1);
  int active_view=0;
  xpp::AUTOAX &axes() { return views[static_cast<std::size_t>(active_view)].axes; }
  const xpp::AUTOAX &axes() const { return views[static_cast<std::size_t>(active_view)].axes; }
  /* the advanced numerics */
  xpp::ADVAUTOX advanced{};
  AutoxParameterLayout parameter_layout;
  /* where the solutions of the session's .s file start (autox_nox.h); none until a restart needs it, reset when the file is replaced */
  std::optional<xpp::AutoxSolutionIndex> solutions_index;
  // autoX's augmented formulations allocate up to eight state copies (init's thu).
  static constexpr int state_copies = 8;
  // A foreign XPPAUT .auto alone carries its old selected-parameter mapping.
  std::vector<int> imported_parameters;
  /* Numerics' SuppBP: no branch point detection */
  bool suppress_bp=false;
  /* a run from the current state continues both directions, +DS and -DS, and every branch the jobs find (autox_queue.h)
     as jobs at once (autox_jobs.cpp); off: only the direction the sign of DS gives */
  bool follow_all_branches=true;
  // AUTO-07p enables codimension-two detection by default.
  std::array<bool, xpp::autox_codim_kinds.size()> special_points=all_special_points();
  /* a homoclinic orbit's left and right equilibria (autox_pp.cpp's stpnt),
     and whether a homoclinic run is on */
  std::vector<double> homo_l,homo_r;
  HomoclinicRun homo_flag=HomoclinicRun::None;
  /* which of them the equilibrium window's Import saves next (xpp_util.cpp eq_import) */
  HomoclinicSide homo_side=HomoclinicSide::Left;
  /* a run that starts from an orbit, its period new (autox_pp.cpp's stpnt);
     false: from the steady state of the initial conditions */
  bool new_period_flag=false;
  /* the stored orbit's first return a periodic start found (autox_run.cpp); a homoclinic start takes the whole orbit
     (autox_nox.cpp get_start_period) */
  AutoxOrbitReturn start_return;
  /* a two-parameter run */
  xpp::AutoxCalculation two_param=xpp::no_calculation;
  /* the kind of the last run asked for (autox.run), which the error that offers to go on from an end point repeats */
  xpp::AutoxRunKind run_kind=xpp::AutoxRunKind::equilibrium;
  /* the kind of run each stored point
     records */
  xpp::AutoxCalculation type_of_calc=xpp::no_calculation;
  /* the torus period a two-parameter torus run starts from */
  xpp::ROTCHK blrtn{};
  /* the model's @ options for autoX */
  AutoxOptions options;
  /* the .autox script the model was opened with, and how far it has run (W306) */
  xpp::AutoxScriptState script;
  /* the equilibria of the homoclinic start a run copies (autox_evd.cpp init_autox) */
  HomoclinicEquilibria homo_equilibria;
  /* false until addbif (autox_evd.cpp) fills the diagram's first point
     (start_diagram's) */
  bool diag_flag=false;
  /* autoX's scratch directory (<model>.ode.b/.d/.s). Empty: HOME, as
     upstream (--silent); xppautX sets a private one per session so
     concurrent sessions never share autoX files (xppautx_main.cpp) */
  std::string dir;
  /* <model>.ode under dir (the .b/.d/.s files are it plus their
     extension): the diagram's branches, diagnostics and solutions, which
     a run's fort files (AutoxRun) are moved into when it ends */
  std::string file;
  /* do_autox's own follow-up runs (the run's restart_label) are one
     chain: its depth */
  int depth=0;
  /* the point the Grab loop is on, which a run starts from */
  xpp::GRABPT grabpt{};
  /* the diagram's marked stretch (the S and E keys in the Grab loop) */
  AutoxDiagramMark diagram_mark;
  /* autoX's File menu Redraw toggle: only reported */
  bool redraw=true;
  /* the File menu's "Draw labeled" toggle */
  LabeledOrbits load_all_labeled_orbits=LabeledOrbits::Off;
  /* the two-parameter point the Grab loop picked (storeautoxpoint), for
     setautoxpoint to give the model's parameters */
  double from_autox_x=0,from_autox_y=0;
  bool from_autox_flag=false;
  /* the time shift that best lines a homoclinic orbit up with the one
     before (find_best_homo_shift) */
  double homo_shift=0.0;
  /* the axes a one-parameter and a two-parameter plot had last
     (keep_last_plot, load_last_plot) */
  xpp::AUTOAX old1p{},old2p{};
  /* why the last runs' branches ended: what the runs of a chain (depth)
     left, each run's own AutoxStop merged in when it ends (autox_stop.h) */
  xpp::AutoxStop stop;
  /* what the info strip and the stability circle show (autox_data.h) */
  xpp::AutoxDataShown shown;
};

// Collocation assembly scratch; each worker AutoxRun owns its own buffers.
struct AutoxAssemblyScratch {
  std::vector<double> dfdp, dfdu, uold, f, u, wploc, prm, residual_parameters, wi, wp, wt;
  std::vector<double> dicd, ficd, dbc, fbc, uic, uio, uid, uip, ubc0, ubc1;
  void resize(std::ptrdiff_t ndim, std::ptrdiff_t ncol, std::ptrdiff_t nbc,
              std::ptrdiff_t nint, std::ptrdiff_t parameters);
};

/* Evaluator scratch: size_run_scratch sizes augmented-system and
   FUNI differences once per run. Condensation, quadrature and map storage
   are sized by their owners on first use. Each worker owns its copy.
   Spell autox_f2c.h's types out here: its typedefs clash with CVODE's. */
struct AutoxGlobalScratch {
  std::vector<double> dfu, dfp, uu1, uu2, ff1, ff2;
  std::vector<double> funi_u1, funi_u2, funi_f1, funi_f2;
  std::vector<std::ptrdiff_t> maximum_column;
  std::vector<std::pair<std::ptrdiff_t,std::ptrdiff_t>> conpar_swaps;
  std::vector<double> integral_weights, map_state, map_jacobian, map_parameters, map_chain;
};
struct AutoxGlobalRotations {
  std::ptrdiff_t irtn = 0;
  std::vector<std::ptrdiff_t> nrtn; /* per boundary condition (nbc), sized with the scratch */
};

namespace xpp {
struct Session; /* session.h */
}

/* solvbv's matrices and index arrays, kept from one call to the next: a
   call with *ifst == 1 sizes them with resize, later ones reuse
   what it factored (autox_lib2.cpp). resize states every dimension once
   (autox_lib2.cpp, where the Fortran's NAX, M1AA ... macros were); the four
   matrices setubv fills are handed out as bounded views, made again for each
   call, so none outlives a resize or a copy of the work. */
struct SolvbvWork {
  double pivot=NAN; /* the last reduced linear solve, for a BVP no-convergence diagnostic */
  std::vector<double> a,b,c,d,a1,a2,s1,s2,bb,cc,c2,dd,faa,fcfc;
  std::vector<double> ft,e,fcc,sol,xe;
  std::vector<std::vector<std::pair<std::ptrdiff_t,double>>> dlog;
  std::vector<std::ptrdiff_t> icf,irf,ipr,icf2;
  /* for ndim variables, ncol collocation points, ntst intervals, nbc
     boundary conditions, nint integral constraints and nfpr continuation
     parameters (the run's, not the model's whole set) */
  void resize(std::ptrdiff_t ndim, std::ptrdiff_t ncol, std::ptrdiff_t ntst, std::ptrdiff_t nbc, std::ptrdiff_t nint, std::ptrdiff_t nfpr);
  /* the Fortran A (nca x nra x ntst), B (ncb x nra x ntst), C (nca x nrc x
     ntst) and D (ncb x nrc), whose leading dimensions are those of the call */
  xpp::Matrix3<double> a_matrix(std::ptrdiff_t nca, std::ptrdiff_t nra, std::ptrdiff_t ntst) { return {a, nca, nra, ntst}; }
  xpp::Matrix3<double> b_matrix(std::ptrdiff_t ncb, std::ptrdiff_t nra, std::ptrdiff_t ntst) { return {b, ncb, nra, ntst}; }
  xpp::Matrix3<double> c_matrix(std::ptrdiff_t nca, std::ptrdiff_t nrc, std::ptrdiff_t ntst) { return {c, nca, nrc, ntst}; }
  xpp::Matrix<double> d_matrix(std::ptrdiff_t ncb, std::ptrdiff_t nrc) { return {d, ncb, nrc}; }
};

/* The direction of the branch an equilibrium's branch point lies on, as
   wrtsp8 stores it with the point (AUTO-07p's UDOT): the free parameter
   it was computed for, PAR(ICP(1))-dot and U-dot. readlb reads it; cnrlae's
   switch at the point continues along the null vector perpendicular to it. */
struct AutoxRestartDirection {
  int parameter=0;
  double rldot=0;
  std::vector<double> udot;
};

/* a point a job (autox_jobs.h) computed, held until the jobs are joined:
   the diagram entry, and whether it starts a new drawn segment (addbif's flag) */
struct AutoxBufferedPoint {
  xpp::DIAGRAM point;
  bool new_segment=false;
};

/* One autoX run: do_autox makes it, the translated routines reach it through
   their iap_type's run, and it is gone when the run ends. A collocation
   worker (autox_parallel.cpp) is a run too, with only its scratch. */
struct AutoxRun {
  /* the Session whose run this is: the model and the settings, which a run
     reads and never changes, and what the model's callbacks (autox_pp.cpp)
     evaluate on */
  xpp::Session *session=nullptr;
  /* a collocation worker's: the run it works for; null for a run itself */
  AutoxRun *owner=nullptr;
  /* the run's parameters, filled from the model and settings by do_autox
     (set_autox) before go_go_autox (gogoautox.cpp) reads them */
  XAUTOX params{};
  std::ptrdiff_t parameter_count=0;
  /* the label the continuation started from (Auto.irs), for its first
     point: do_autox sets it, addbif takes it (autox_run_from_take) */
  int run_from=0;
  /* the label a follow-up run (a restart) continues from, 0 for none:
     autox_lib1.cpp's wrtsp8 sets it, do_autox reads it when the run ends */
  int restart_label=0;
  /* why this run's branches ended and the stability values of its points
     (autox_stop.h, autox_stability.h); do_autox merges the stop into the
     Session's when the run ends */
  xpp::AutoxStop stop;
  xpp::AutoxStability stability;
  /* the run's own folder (xpp::files::make_temp_dir) and fort.3/7/8/9 in it
     (open_autox): fort.3 the restart data, fort.7 the branches, fort.8 the
     solutions, fort.9 the diagnostics. close_autox moves them to the
     Session's .b/.d/.s files and the folder goes with the run. Declared
     before fp8, which is closed before the folder is removed. */
  std::optional<xpp::TempDir> folder;
  std::string fort3,fort7,fort8,fort9;
  /* a restart reads the session's own .s file as fort.3 (nothing is copied, the file is only read): close_autox must not remove it */
  bool fort3_is_session=false;
  /* where the solutions of fort.3 start, when it is the run's own file (the session's is the Session's solutions_index) */
  std::optional<xpp::AutoxSolutionIndex> fort3_index;
  /* the fort.3/7/9 files open during a run (gogoautox.cpp's RunUnits) */
  FILE *fp3=nullptr,*fp7=nullptr,*fp9=nullptr;
  /* fort.8, opened when a run writes its first label (autox_lib1.cpp) and
     closed by close_autox, or with the run when it never got there */
  xpp::UniqueFile fp8;
  /* W247: the pool persists across collocation calls. Worker AutoxRuns own
     their evaluation arrays and finite-difference scratch, never a Session's. */
  std::unique_ptr<xpp::AutoxParallel> collocation;
  /* the run evaluates the model on its own rhs_constants and rhs_variables
     (a collocation worker, or a job run at once with others) and never writes
     the Session's parser; autox_constants says which constants a run reads */
  bool pure_rhs=false;
  xpp::AutoxCodim codim;
  std::vector<double> rhs_constants; /* the parser's constants and variables, copied for a worker or a job */
  std::vector<double> rhs_variables;
  /* A job of follow_all_branches (autox_jobs.cpp): addbif keeps its points here
     instead of in the Session's diagram, and the join appends them in the
     jobs' order. Off for an ordinary run. */
  bool job=false;
  std::vector<AutoxBufferedPoint> points;
  /* the job runs on a thread of its own: it never polls the front end or
     reports progress (the main thread does), and never touches the Session
     but to read its model and settings */
  bool on_thread=false;
  /* this job's collocation threads (the thread budget's share when it
     started); 0: the Session's setting */
  unsigned collocation_threads=0;
  /* a job's start already refined by Newton's method, once for all the jobs, before they began
     (autox_jobs.cpp): the start check takes it instead of refining again */
  std::optional<std::vector<double>> refined_start;
  /* the run starts from the integrated orbit (the Session's new_period_flag when it began) rather than from the initial
     conditions: a job that starts at an equilibrium another job found has none, whatever run it belongs to started from */
  bool orbit_start=false;
  /* the queue stops this job (autox_jobs.cpp): another one made it unnecessary; checked where Stop is */
  std::atomic<bool> dropped{false};
  /* the job is stopped alone, as Stop stops every job (a replayed recording's stop: xpp::job::stop_jobs): addbif sets
     it when the job holds one point fewer than stop_after_points, and the next solve ends the branch with an end point */
  std::optional<std::size_t> stop_after_points;
  std::atomic<bool> stopped_alone{false};
  /* the job fails when it holds this many points (XPP_AUTOX_FAIL_CHILD_AT, autox_jobs.cpp: servercheck's failing job) */
  std::optional<std::size_t> fail_after_points;
  /* the error this run ends with is the whole run's (the diagram's limit, memory): the queue stops every job */
  bool run_wide_error=false;
  /* called by addbif with the number of points the job holds each time it holds one more (on the job's thread), so the
     queue can see how the branch leaves its start (autox_jobs.cpp); none for an ordinary run */
  std::function<void(std::size_t)> point_stored;
  /* the largest branch and label autoX's newlab found before this run's own: its numbers start above them
     (autox_lib1.cpp newlab); the join of jobs renumbers by them */
  int base_branch=0,base_label=0;
  AutoxGlobalScratch scratch{};
  AutoxAssemblyScratch assembly;
  xpp::JacobianScratch jacobian_scratch;
  AutoxGlobalRotations rotations{};
  /* while stepbv solves a Newton step (autox_lib1.cpp). A cancelled job
     then stops the collocation setup early (setubv2.cpp), solvbv skips
     the solve and stepbv returns to the last converged point. Other
     solves, such as stdrbv's starting direction and the location of a
     special point (lcspae, lcspbv), always run to the end. */
  bool setubv_stop=false;
  /* from init's "Generating starting data" (a restart that switches
     branch) until the run writes its first label, which becomes the
     follow-up run's restart label (autox_lib1.cpp) */
  bool restart_flag=false;
  /* the direction readlb found stored with the restart label; none for a label that has no direction (autox_lib1.cpp) */
  std::optional<AutoxRestartDirection> restart_direction;
  /* while lcspae/lcspbv locate a special point: their solves run to
     the end (autox_lib1.cpp's Locating) */
  bool locating=false;
  /* the branch of cycles cnrlbv continues started at a Hopf point: its
     first steps' tests read the converged point's Jacobian (autox_lib1.cpp) */
  bool hopf_start=false;
  /* the branch's first point as the same-point rule sees it, and whether the branch has left it (autox_ends.h,
     W604): what a crossing of its parameter value is compared with, by the hidden test of autox_lib1.cpp; set when
     the point is stored, empty before it */
  std::optional<xpp::AutoxBranchStart> branch_start;
  /* the exact Jacobian's failure (W275): a derivative not finite where the
     right-hand side is, recorded by func (autox_pp.cpp) on its own AutoxRun (a
     collocation worker's, merged into its run's by the caller when the
     workers are done) and ended as a Stop ends a run, so the last
     converged point is stored as an end point to restart from; do_autox
     (autox_nox.cpp) shows it with the action that switches to differences */
  std::optional<xpp::Error> jacobian_failure;
  /* solvae's last ntop (its output's paging) */
  std::ptrdiff_t solvae_ntop=0;
  /* solvbv's matrices, kept from one call to the next: a call with
     *ifst == 1 sizes them, later ones reuse what it
     factored (autox_lib2.cpp) */
  SolvbvWork solvbv;
  /* makes solvbv's last factorization (ifst = 1) again, from the inputs it was made from, which solvbv keeps in it
     (autox_lib2.cpp): kilobytes where the factored matrices are megabytes, for a failed location's restore
     (autox_lib1.cpp's LocationState). Empty before the first. */
  std::function<void()> refactor;
  /* the homoclinic continuation's (HomCont's) common blocks
     (autox_lib5.cpp): its settings, the projections, the equilibria's
     eigenvalues and vectors, and their previous values */
  struct {
    struct { std::ptrdiff_t itwist,istart,iequib,nfixed,npsi,nunstab,nstab,nrev; } blhom{};
    struct { std::vector<std::ptrdiff_t> ipsi,ifixed,irev; } blhmp;
    struct { std::vector<double> pu0,pu1; } blhmu;
    struct { std::ptrdiff_t nbcn; } bcnn{};
    struct { double compzero; } blhma{};
    struct { std::vector<double> rr,ri,v,vt,xequib; std::ptrdiff_t ineig=0; } bleig;
    struct { std::vector<double> vrprev; std::vector<std::ptrdiff_t> ieigc; } blhme;
    struct { std::vector<double> cprev; std::vector<std::ptrdiff_t> iflag; } beyn;
  } homcont;
};

/* Stop was pressed or this job was stopped alone: what ends a branch with an end point */
inline bool autox_cancelled(const AutoxRun &run) { return xpp::job::cancelled() || run.stopped_alone; }

namespace xpp {
/* What autoX's numerics throw where they used to exit() (W63a): a
   dimension or setting autoX cannot run with (Ncol above 7, Ntst below
   the number of nodes), a singular solve, a BLAS routine's argument out
   of range. do_autox (autox_nox.cpp), where every run starts, catches it
   and reports its Error; the run ends as a cancelled one does: gogoautox's
   RunUnits closes fort.3/7/9 as the stack unwinds, close_autox saves the
   diagram so far, and the work arrays are std::vectors, so nothing leaks
   and the next run starts clean. */
struct AutoxFailed {
  Error error;
};
[[noreturn]] inline void autox_fail(Error error) { throw AutoxFailed{std::move(error)}; }
/* Numerical failures have no file; do_autox attaches the command's place. */
[[noreturn]] inline void autox_fail(std::string what) { throw AutoxFailed{{"autox", std::move(what)}}; }
} // namespace xpp

