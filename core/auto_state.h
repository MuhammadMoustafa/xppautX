#ifndef XPP_AUTO_STATE_H
#define XPP_AUTO_STATE_H
/* AUTO's part of a Session (session.h): its settings, the run's own
   parameters, the diagram and the scratch folder. C++ only.

   AutoState is auto_nox.cpp's (the AUTO window, its settings and the
   parameters it continues) with autevd.cpp's run parameters beside it;
   AutoLib is the AUTO library's own (autlib1-5, gogoauto, setubv2: the
   fort files open during a run and its work arrays). */
#include <stdio.h>
#include "auto_nox.h"
#include "display_state.h"
#include "xAuto.h"

#include <array>
#include <cstddef>
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
  AUTOAX axes{};
  xpp::Zoom zoom;
  bool zoom_seen=false; /* zoom_axes holds the axes the zoom was made at */
  std::array<double,4> zoom_axes{};
};

struct AutoState {
  /* the AUTO window and its settings */
  BIFUR bifur{};
  /* the views of the one diagram (W50), at least one, each plotting it at
     its own axes. The active one is the Axes menu's and its zoom's, the
     exports', the info strip's and a run's: its icp1 and icp2 are the
     parameters a run continues in. */
  std::vector<AutoDiagramView> views=std::vector<AutoDiagramView>(1);
  int active_view=0;
  AUTOAX &axes() { return views[static_cast<std::size_t>(active_view)].axes; }
  const AUTOAX &axes() const { return views[static_cast<std::size_t>(active_view)].axes; }
  /* the advanced numerics */
  ADVAUTO advanced{};
  /* the parameters AUTO continues: the first npar of par, model
     parameter indices, and their constants[] indices (par_index) */
  int npar=8;
  std::array<int,8> par{};
  std::array<int,8> par_index{};
  /* the Mark values: nuzr of uzr_par (auto_f2c.h's integer) and
     uzr_period */
  int nuzr=0;
  std::array<long,20> uzr_par{};
  std::array<double,20> uzr_period{};
  /* Numerics' SuppBP */
  int suppress_bp=0;
  /* a homoclinic orbit's left and right equilibria (autpp.cpp's stpnt),
     and whether a homoclinic run is on */
  std::array<double,100> homo_l{},homo_r{};
  int homo_flag=0;
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
  ROTCHK blrtn{};
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
     HOME, as upstream (-silent); xppautX sets a private one per session
     so concurrent sessions never share AUTO files (xppautx_main.cpp) */
  std::string dir;
};

/* AUTO's work arrays (autlib1's allocate_global_memory owns their
   storage; autlib3 and autlib5 read them). auto_f2c.h's doublereal and
   integer spelt out (double, long): the header is not included here, as
   its typedefs clash with CVODE's (llnltyps.h) in a file that includes
   both through session.h. */
typedef struct {
  double *dfu, *dfp, *uu1, *uu2, *ff1, *ff2;
} AutoGlobalScratch;
typedef struct {
  long irtn;
  long *nrtn;
} AutoGlobalRotations;

struct AutoLib {
  /* the fort.3/7/8/9 files open during a run */
  FILE *fp3=nullptr,*fp7=nullptr,*fp8=nullptr,*fp9=nullptr;
  int fp8_is_open=0;
  AutoGlobalScratch scratch{};
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
};

namespace xpp {
/* What AUTO's numerics throw where they used to exit() (W63a): a
   dimension or setting AUTO cannot run with (Ncol above 7, Ntst below
   the number of nodes), a singular solve, a BLAS routine's argument out
   of range. do_auto (auto_nox.cpp), where every run starts, catches it
   and reports `what`; the run ends as a cancelled one does: gogoauto's
   RunUnits closes fort.3/7/9 as the stack unwinds, close_auto saves the
   diagram so far, and the work arrays are std::vectors, so nothing leaks
   and the next run starts clean. */
struct AutoFailed {
  std::string what;
};
[[noreturn]] inline void auto_fail(std::string what) { throw AutoFailed{std::move(what)}; }
} // namespace xpp

#endif
