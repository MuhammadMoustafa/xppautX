#pragma once
/* AnalysisParts: what the analyses take of a Session. C++ only. */
#include <memory>
#include <string>
#include <vector>

#include "run_parts.h"
#include "xpp_globals.h"


namespace xpp {
struct Colors; /* colormap.h */
struct HistogramState; /* histogram_state.h */
struct AdjointState; /* adjoint_state.h */
struct StochasticState; /* stochastic.h */
struct FitInfo; /* do_fit.h */
struct ManifoldShots; /* equilibria.h */
struct ShootRange; /* model_analysis.h */
struct DataState; /* data_state.h */
struct MarksShown; /* plot_state.h */
struct BoundaryCondition; /* model_parts.h */
struct Place; /* xpp_error.h */
struct SweepWorker; /* sweep_runs.h */

/* The parts of a Session the analyses read and write beside the integrators': the histogram, the adjoint, the stochastic runs, the
   fit, the manifolds, the shooting's range, the data table and what a run's output names. The analyses take it (a `const AnalysisParts &s`)
   where they took the Session, so none of their files includes session.h; it is a RunParts too, so they hand it to the integrators
   as it is. The Session holds one, made with it, and converts to it where it is passed (session.cpp), as it does to RunParts. The members
   are the Session's own parts, not copies, and keep the Session's names. `make_worker` is the Session module's (a sweep's worker is a
   Session of the same Model: make_sweep_worker, session.cpp): the analyses call it and never name a Session's construction.
   `threads` is the program setting's value as the Session holds it. */
struct AnalysisParts : RunParts {
  HistogramState *histogram;
  AdjointState *adjoint;
  StochasticState *stochastic;
  FitInfo *fit;
  ManifoldShots *manifolds;
  ShootRange *shoot_range;
  DataState *data;
  MarksShown *marks_shown;
  Colors &colors;
  std::vector<int> &plot_list;
  std::vector<BoundaryCondition> &bcs;
  Place &output_place;
  std::string &batch_out_file;
  std::vector<double> &last_equilibrium, &last_equilibrium_eigenvalues;
  std::string &last_equilibrium_stability;
  int &ntable;
  const unsigned &threads;
  std::unique_ptr<SweepWorker> (*make_worker)(const AnalysisParts &s, int rows);
  unsigned thread_budget() const { return program.thread_budget(threads); }
};

} // namespace xpp
