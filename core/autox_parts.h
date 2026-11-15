#pragma once
/* AutoxParts: what autoX takes of a Session. C++ only. */
#include "analysis_parts.h"
#include <string>
#include <vector>

struct AutoxState; /* autox_state.h */
struct AutoxDiagram; /* diagram.h */
namespace xpp {
struct Model; /* model.h */
struct NumericsSettings; /* numerics_settings.h */
struct IntegratorState; /* integrate.h */
struct DataStore; /* storage.h */
struct ParserState; /* expr.h */
struct Colors; /* colormap.h */
struct AutoxView; /* autox_state.h */
struct SessionParts; /* model_options.h */
struct RunParts; /* run_parts.h */
}

namespace xpp {

/* The parts of a Session that autoX reads and writes, handed in where an autoX operation starts (xpp::autox_parts(Session &), made by
   the Session module: session.h) and held by the run (AutoxRun::parts), so autoX's files never include session.h. The references are
   the Session's own parts, not copies; `threads` is the program preference's value when it was made, which a command never changes.
   `analysis` is what the integrators and the analyses take (a RunParts too); its `session` is only a handle, handed on to the XppUi
   seam's callbacks and to the functions of other modules that still take a Session; autoX itself reads nothing through it.
   Constness is as shallow as a Session's own pointers: a function that only reads takes it const. */
struct AutoxParts {
  AnalysisParts &analysis;
  Model &model_;
  AutoxState *autox_state;
  AutoxDiagram *diagram;
  AutoxView *autox_view;
  NumericsSettings *numerics;
  IntegratorState *integrator;
  DataStore *data_store;
  ParserState &parser;
  Colors &colors;
  std::vector<double> &last_ic;
  std::string &selected_window;
  const SessionParts &session_parts; /* the settings the option table reads, named in what autoX reports */
  unsigned threads;
  Model &model() const { return model_; }
};

} // namespace xpp
