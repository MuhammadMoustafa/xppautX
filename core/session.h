#ifndef XPP_SESSION_H
#define XPP_SESSION_H
/* xpp::Session: everything a run changes (CLAUDE.md "No global state";
   docs/roadmap.md W47c). C++ only.

   A Model (model.h) is what loading a .ode file produces and stays as the
   load left it; a Session is what a person does with it: the data a run
   stores, the plot windows and what is drawn in them, the numerics
   settings in use, the integrator's state, AUTO's, the browser's and the
   kinescope's. Each part's type is defined by the module that owns it
   (storage.h's DataStore, many_pops.h's XppPlotWindows, ...), and the
   Session holds one of each.

   For now the current Session is reached through xpp::session(), like
   xpp::model(); W47d passes it explicitly. A hot loop takes
   `xpp::Session &s=xpp::session()` once. */
#include "model.h"
#include "storage.h"
#include "many_pops.h"
#include "graf_par.h"
#include "integrate.h"
#include "auto_state.h"
#include "diagram.h"
#include "browse.h"
#include "kinescope.h"

#include <array>
#include <string>

namespace xpp {

struct Session {
  /* the rows a run stores and where the last run ended (storage.cpp) */
  DataStore data_store;
  /* the plot windows, the active one, Simulplot (xpp_util.cpp) */
  XppPlotWindows plot_windows{};
  /* the frozen curves of every window (graf_par.cpp) */
  XppFrozenCurves frozen_curves{};
  /* how plots are written to files (graf_par.cpp) */
  XppPlotExport plot_export;

  /* the integrator's state (integrate.cpp) */
  IntegratorState integrator;
  /* its state vector and the solvers' scratch space (storage.cpp) */
  SolverWork solver_work;
  /* the initial data the next run starts from (the model's are
     Model::default_ic) */
  std::array<double,MAXODE> last_ic{};
  /* each variable's initial data as typed (a delay equation's history:
     "0.0" when it has none) */
  std::array<std::string,MAXODE> delay_string;
  /* each variable's torus flag: 1 when it is taken modulo the torus
     period (the nUmerics menu's Torus) */
  std::array<int,MAXODE> itor{};

  /* AUTO's settings, run parameters and scratch folder (auto_nox.cpp,
     autevd.cpp), the AUTO library's files and work arrays (autlib1.cpp,
     gogoauto.cpp) and the diagram (diagram.cpp) */
  AutoState auto_state;
  AutoLib auto_lib;
  AutoDiagram diagram;

  /* the data browser (browse_data.cpp) */
  BrowserState browser;
  /* the kinescope (commands.cpp, json_windows.cpp) */
  XppKinescope kinescope;
};

/* the current Session (xpp_current.h) */
inline Session &session()
{
  return detail::current<Session>();
}

}

#endif
