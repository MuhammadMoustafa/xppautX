#pragma once
/* RunParts: what the integrators take of a Session. C++ only. */
#include <string>
#include <vector>

namespace xpp {
struct Session; /* session.h */
struct Model; /* model.h */
struct NumericsSettings; /* numerics_settings.h */
struct IntegratorState; /* integrator_state.h */
struct DataStore; /* storage.h */
struct SolverWork; /* storage.h */
struct JacobianScratch; /* jacobian.h */
struct DelayState; /* delay_state.h */
struct VolterraState; /* volterra.h */
struct DaeState; /* dae_fun.h */
struct ParserState; /* expr.h */
struct XppPlotWindows; /* many_pops.h */
struct AnimationState; /* ani_state.h */
struct TABULAR; /* tabular.h */
struct NetworkValues; /* simplenet.h */
class Random; /* xpp_math.h */
struct SessionParts; /* model_options.h */

/* The parts of a Session the integrators read and write: the solvers, the right-hand side, the delays, the integral equations,
   the flags and the run's driver take them (a `const RunParts &s`) where they take the Session of old, so none of their files
   includes session.h. The Session holds one, made with it by `xpp::run_parts` (session.cpp), and converts to it where it is passed
   (a function that takes the parts is called with a Session as before). The members are the Session's own parts, not copies, and
   keep the Session's names, so a solver reads `s.numerics->total_time`. `session` is only a handle, handed on, unread, to the
   XppUi seam's callbacks and to the functions of other modules that still take a Session. Constness is as shallow as a Session's own
   pointers: a function that only reads takes the parts const as well. */
struct RunParts {
  Session &session;
  Model &model_;
  NumericsSettings *numerics;
  IntegratorState *integrator;
  DataStore *data_store;
  SolverWork *solver_work;
  JacobianScratch *jacobian_scratch;
  DelayState *delay;
  VolterraState *volterra;
  DaeState *dae;
  ParserState &parser;
  XppPlotWindows *plot_windows;
  AnimationState *animation;
  std::vector<double> &last_ic;
  std::vector<std::string> &delay_string;
  std::vector<int> &itor;
  std::vector<TABULAR> &tables;
  std::vector<NetworkValues> &networks;
  Random &random;
  const SessionParts &session_parts; /* the settings the option table reads, named in what a run reports */
  Model &model() const { return model_; }
};

} // namespace xpp
