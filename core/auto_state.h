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
#include "auto_data.h"
#include "auto_stop.h"
#include "auto_stability.h"
#include "display_state.h"
#include "xAuto.h"
#include "xpp_io.h"

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
     HOME, as upstream (-silent); xppautX sets a private one per session
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

namespace xpp {
struct Session; /* session.h */
}

struct AutoLib {
  /* the Session whose run this is, from go_go_auto until the run ends:
     how the translated routines, which reach this through their
     iap_type's lib, and the model's callbacks they call (autpp.cpp) reach
     the model and its settings */
  xpp::Session *session=nullptr;
  /* the fort.3/7/9 files open during a run (gogoauto.cpp's RunUnits) */
  FILE *fp3=nullptr,*fp7=nullptr,*fp9=nullptr;
  /* fort.8, opened when a run writes its first label (autlib1.cpp) and
     closed by close_auto, or with the Session when a run never got there */
  xpp::UniqueFile fp8;
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
