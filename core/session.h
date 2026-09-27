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
};

/* the current Session (xpp_current.h) */
inline Session &session()
{
  return detail::current<Session>();
}

}

#endif
