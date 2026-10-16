#ifndef XPP_AUTO_STATE_H
#define XPP_AUTO_STATE_H
/* AUTO's part of a Session (session.h): its settings, the run's own
   parameters, the diagram and the scratch folder. C++ only.

   AutoState is auto_nox.cpp's (the AUTO window, its settings and the
   parameters it continues) with autevd.cpp's run parameters beside it;
   AutoLib is the AUTO library's own (autlib1-5, gogoauto, setubv2: the
   fort files open during a run and its work arrays). */
#include <stdio.h>
#include "auto_unknowns.h"
#include "jacobian.h"
#include "auto_nox.h"
#include "auto_data.h"
#include "auto_stop.h"
#include "auto_stability.h"
#include "auto_codim.h"
#include "auto_parallel.h"
#include "xpplim.h"
#include "display_state.h"
#include "xAuto.h"
#include "xpp_io.h"
#include "xpp_error.h"
#include "xpp_math.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

/* the AUTO settings the model's @ options give (auto_ntst=, ...), which
   init_auto_win copies into AutoState::bifur */
struct AutoOptions {
  int ntst=15,nmx=200,npr=50,ncol=4;
  double ds=.02,dsmax=.5,dsmin=.001;
  double rl0=0.0,rl1=2,a0=0.0,a1=1000.;
  double xmax=2.5,xmin=-.5,ymax=3.0,ymin=-3.0;
  double epsl=1e-4,epsu=1e-4,epss=1e-4;
  int var=0;
};

/* one view of the diagram (W50): its axes, and the part of them the page
   shows (its zoom, W65; a zoom belongs to the axes it was made at, other
   ones drop it) */
struct AutoDiagramView {
  xpp::AUTOAX axes{};
  xpp::Zoom zoom;
  bool zoom_seen=false; /* zoom_axes holds the axes the zoom was made at */
  std::array<double,4> zoom_axes{};
};

/* the diagram's marked stretch (the S and E keys in the Grab loop): the
   branch and point numbers of its start and end */
struct AutoDiagramMark {
  int state=0;        /* 0 nothing, 1 start marked, 2 start and end */
  int start_branch=0,end_branch=0;
  int start_point=0,end_point=0;
};

// Only solver boundaries translate indices; all other parameter lists use Model indices.
struct AutoParameterLayout {
  int block=xpp::auto_unknown::block_size;
  int model_parameters=0;
  int solver_index(int k) const { return block+k; }
  // What a solver slot is: a model parameter, or one of AUTO's unknowns (the stored form of an axis).
  xpp::AutoAxis axis(int slot) const { const int k=model_index(slot); return k>=0 ? xpp::AutoAxis::parameter(k) : xpp::AutoAxis::unknown(slot); }
  int model_index(int index) const { return index>=block && index<size() ? index-block : -1; }
  int size() const { return block+model_parameters; }
  // The solver's array par in its two parts: AUTO's own block, then the model's parameters.
  template<class T> std::span<T> unknowns(T *par) const { return {par,static_cast<std::size_t>(block)}; }
  template<class T> std::span<T> parameters(T *par) const { return {par+block,static_cast<std::size_t>(model_parameters)}; }
};

struct AutoState {
  /* the AUTO window and its settings */
  xpp::BIFUR bifur{};
  /* the views of the one diagram (W50), at least one, each plotting it at
     its own axes. The active one is the Axes menu's and its zoom's, the
     exports', the info strip's and a run's: its icp1 and icp2 are the
     parameters a run continues in. */
  std::vector<AutoDiagramView> views=std::vector<AutoDiagramView>(1);
  int active_view=0;
  xpp::AUTOAX &axes() { return views[static_cast<std::size_t>(active_view)].axes; }
  const xpp::AUTOAX &axes() const { return views[static_cast<std::size_t>(active_view)].axes; }
  /* the advanced numerics */
  xpp::ADVAUTO advanced{};
  AutoParameterLayout parameter_layout;
  // AUTO's augmented formulations allocate up to eight state copies (init's thu).
  static constexpr int state_copies = 8;
  // A foreign XPPAUT .auto alone carries its old selected-parameter mapping.
  std::vector<int> imported_parameters;
  /* AUTO's thread count, a program preference (xpp_prefs.h, W267) kept
     here for the run: prefs::AUTO_THREADS_AUTO, or the count */
  unsigned threads=0;
  /* Numerics' SuppBP */
  int suppress_bp=0;
  // AUTO-07p enables equilibrium codimension-two detection by default.
  std::array<bool, xpp::auto_codim_kinds.size()> special_points{true,true,true,true,true,true,true,true};
  /* a homoclinic orbit's left and right equilibria (autpp.cpp's stpnt),
     and whether a homoclinic run is on */
  std::vector<double> homo_l,homo_r;
  int homo_flag=0;
  /* which of them the equilibrium window's Import saves next: 0 the
     left, 1 the right (xpp_util.cpp eq_import) */
  int homo_side=0;
  /* a periodic run starts from a new period (autpp.cpp's stpnt) */
  int new_period_flag=0;
  /* a two-parameter run */
  int two_param=0;
  /* the label do_auto's follow-up run (a restart) continues from, 0 for
     none */
  int restart_label=0;
  /* the kind of run (LPE2, HB2, ...; 0 one parameter) each stored point
     records */
  int type_of_calc=0;
  /* the torus period a two-parameter torus run starts from */
  xpp::ROTCHK blrtn{};
  /* the colours of stable and unstable equilibria and periodic orbits
     (@ sec=, uec=, spc=, upc=) */
  int stable_eq_color=20,unstable_eq_color=0;
  int stable_po_color=26,unstable_po_color=28;
  /* the model's @ options for AUTO */
  AutoOptions options;
  /* the AUTO run's own parameters, filled from the model and settings
     (auto_nox.cpp) before go_go_auto() (gogoauto.cpp) reads them */
  XAUTO run{};
  /* 0 until addbif (autevd.cpp) fills the diagram's first point
     (start_diagram's), then 1 */
  int diag_flag=0;
  /* AUTO's scratch directory (fort.3/7/8/9, <model>.ode.b/.d/.s). Empty:
     HOME, as upstream (--silent); xppautX sets a private one per session
     so concurrent sessions never share AUTO files (xppautx_main.cpp) */
  std::string dir;
  /* <model>.ode under dir (the .b/.d/.s files are it plus their
     extension), and AUTO's unit files there (open_auto): fort.3 the
     restart data, fort.7 the branches, fort.8 the solutions, fort.9 the
     diagnostics */
  std::string file;
  std::string fort3,fort7,fort8,fort9;
  /* the label the running continuation started from (Auto.irs), for its
     first point: do_auto sets it, addbif takes it (auto_run_from_take) */
  int run_from=0;
  /* do_auto's own follow-up runs (restart_label) are one run: its depth */
  int depth=0;
  /* the point the Grab loop is on, which a run starts from */
  xpp::GRABPT grabpt{};
  /* the diagram's marked stretch (the S and E keys in the Grab loop) */
  AutoDiagramMark diagram_mark;
  /* AUTO's File menu Redraw toggle: only reported */
  int redraw=1;
  /* the File menu's "Draw labeled" toggle: 0 off, 1 traversing the
     diagram draws each labelled point's orbit, 2 clearing the screens
     first */
  int load_all_labeled_orbits=0;
  /* the two-parameter point the Grab loop picked (storeautopoint), for
     setautopoint to give the model's parameters */
  double from_auto_x=0,from_auto_y=0;
  int from_auto_flag=0;
  /* the time shift that best lines a homoclinic orbit up with the one
     before (find_best_homo_shift) */
  double homo_shift=0.0;
  /* the axes a one-parameter and a two-parameter plot had last
     (keep_last_plot, load_last_plot) */
  xpp::AUTOAX old1p{},old2p{};
  /* why the runs' branches ended (auto_stop.h) */
  xpp::AutoStop stop;
  /* the stability values of the run's points (auto_stability.h) */
  xpp::AutoStability stability;
  /* what the info strip and the stability circle show (auto_data.h) */
  xpp::AutoDataShown shown;
};

// Collocation assembly scratch; each worker AutoLib owns its own buffers.
struct AutoAssemblyScratch {
  std::vector<double> dfdp, dfdu, uold, f, u, wploc, prm, residual_parameters, wi, wp, wt;
  std::vector<double> dicd, ficd, dbc, fbc, uic, uio, uid, uip, ubc0, ubc1;
  void resize(std::ptrdiff_t ndim, std::ptrdiff_t ncol, std::ptrdiff_t nbc,
              std::ptrdiff_t nint, std::ptrdiff_t parameters);
};

/* Evaluator scratch: allocate_global_memory sizes augmented-system and
   FUNI differences once per run. Condensation, quadrature and map storage
   are sized by their owners on first use. Each worker owns its copy.
   Spell auto_f2c.h's types out here: its typedefs clash with CVODE's. */
struct AutoGlobalScratch {
  std::vector<double> dfu, dfp, uu1, uu2, ff1, ff2;
  std::vector<double> funi_u1, funi_u2, funi_f1, funi_f2;
  std::vector<std::ptrdiff_t> maximum_column;
  std::vector<std::pair<std::ptrdiff_t,std::ptrdiff_t>> conpar_swaps;
  std::vector<double> integral_weights, map_state, map_jacobian, map_parameters, map_chain;
};
struct AutoGlobalRotations {
  std::ptrdiff_t irtn = 0;
  std::vector<std::ptrdiff_t> nrtn; /* per boundary condition (nbc), sized with the scratch */
};

namespace xpp {
struct Session; /* session.h */
}

/* solvbv's matrices and index arrays, kept from one call to the next: a
   call with *ifst == 1 sizes them with resize, later ones reuse
   what it factored (autlib2.cpp). resize states every dimension once
   (autlib2.cpp, where the Fortran's NAX, M1AA ... macros were); the four
   matrices setubv fills are handed out as bounded views, made again for each
   call, so none outlives a resize or a copy of the work. */
struct SolvbvWork {
  std::vector<double> a,b,c,d,a1,a2,s1,s2,bb,cc,faa;
  std::vector<double> ft,e,fcc,sol1,sol2,sol3,xe;
  std::vector<std::vector<std::pair<std::ptrdiff_t,double>>> dlog;
  std::vector<std::ptrdiff_t> icf,irf,ipr,icf1,icf2;
  /* for ndim variables, ncol collocation points, ntst intervals, nbc
     boundary conditions, nint integral constraints and up to parameters
     continuation parameters */
  void resize(std::ptrdiff_t ndim, std::ptrdiff_t ncol, std::ptrdiff_t ntst, std::ptrdiff_t nbc, std::ptrdiff_t nint, std::ptrdiff_t parameters);
  /* the Fortran A (nca x nra x ntst), B (ncb x nra x ntst), C (nca x nrc x
     ntst) and D (ncb x nrc), whose leading dimensions are those of the call */
  xpp::Matrix3<double> a_matrix(std::ptrdiff_t nca, std::ptrdiff_t nra, std::ptrdiff_t ntst) { return {a, nca, nra, ntst}; }
  xpp::Matrix3<double> b_matrix(std::ptrdiff_t ncb, std::ptrdiff_t nra, std::ptrdiff_t ntst) { return {b, ncb, nra, ntst}; }
  xpp::Matrix3<double> c_matrix(std::ptrdiff_t nca, std::ptrdiff_t nrc, std::ptrdiff_t ntst) { return {c, nca, nrc, ntst}; }
  xpp::Matrix<double> d_matrix(std::ptrdiff_t ncb, std::ptrdiff_t nrc) { return {d, ncb, nrc}; }
};

struct AutoLib {
  /* the Session whose run this is, from go_go_auto until the run ends:
     how the translated routines, which reach this through their
     iap_type's lib, and the model's callbacks they call (autpp.cpp) reach
     the model and its settings */
  xpp::Session *session=nullptr;
  std::ptrdiff_t parameter_count=0;
  /* the fort.3/7/9 files open during a run (gogoauto.cpp's RunUnits) */
  FILE *fp3=nullptr,*fp7=nullptr,*fp9=nullptr;
  /* fort.8, opened when a run writes its first label (autlib1.cpp) and
     closed by close_auto, or with the Session when a run never got there */
  xpp::UniqueFile fp8;
  /* W247: the pool persists across collocation calls. Worker AutoLibs own
     their evaluation arrays and finite-difference scratch, never a Session's. */
  std::unique_ptr<xpp::AutoParallel> collocation;
  bool pure_rhs=false;
  xpp::AutoCodim codim;
  std::vector<double> rhs_constants; /* the parser's constants and variables, copied for a worker */
  std::vector<double> rhs_variables;
  AutoGlobalScratch scratch{};
  AutoAssemblyScratch assembly;
  xpp::JacobianScratch jacobian_scratch;
  AutoGlobalRotations rotations{};
  /* 1 while stepbv solves a Newton step (autlib1.cpp). A cancelled job
     then stops the collocation setup early (setubv2.cpp), solvbv skips
     the solve and stepbv returns to the last converged point. Other
     solves, such as stdrbv's starting direction and the location of a
     special point (lcspae, lcspbv), always run to the end. */
  int setubv_stop=0;
  /* 1 from init's "Generating starting data" (a restart that switches
     branch) until the run writes its first label, which becomes the
     follow-up run's restart label (autlib1.cpp); a failed run clears it */
  int restart_flag=0;
  /* 1 while lcspae/lcspbv locate a special point: their solves run to
     the end (autlib1.cpp's Locating) */
  int locating=0;
  /* the exact Jacobian's failure (W275): a derivative not finite where the
     right-hand side is, recorded by func (autpp.cpp) on its own AutoLib (a
     collocation worker's, merged into the Session's by the caller when the
     workers are done) and ended as a Stop ends a run, so the last
     converged point is stored as an end point to restart from; do_auto
     (auto_nox.cpp) shows it with the action that switches to differences */
  std::optional<xpp::Error> jacobian_failure;
  /* solvae's last ntop (its output's paging) */
  std::ptrdiff_t solvae_ntop=0;
  /* solvbv's matrices, kept from one call to the next: a call with
     *ifst == 1 sizes them, later ones reuse what it
     factored (autlib2.cpp) */
  SolvbvWork solvbv;
  /* the homoclinic continuation's (HomCont's) common blocks
     (autlib5.cpp): its settings, the projections, the equilibria's
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

namespace xpp {
/* What AUTO's numerics throw where they used to exit() (W63a): a
   dimension or setting AUTO cannot run with (Ncol above 7, Ntst below
   the number of nodes), a singular solve, a BLAS routine's argument out
   of range. do_auto (auto_nox.cpp), where every run starts, catches it
   and reports its Error; the run ends as a cancelled one does: gogoauto's
   RunUnits closes fort.3/7/9 as the stack unwinds, close_auto saves the
   diagram so far, and the work arrays are std::vectors, so nothing leaks
   and the next run starts clean. */
struct AutoFailed {
  Error error;
};
[[noreturn]] inline void auto_fail(Error error) { throw AutoFailed{std::move(error)}; }
/* Numerical failures have no file; do_auto attaches the command's place. */
[[noreturn]] inline void auto_fail(std::string what) { throw AutoFailed{{"auto", std::move(what)}}; }
} // namespace xpp

#endif
