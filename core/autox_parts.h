#pragma once
/* AutoxParts: what autoX takes of a Session. C++ only. */
#include <concepts>
#include <string>
#include <type_traits>
#include <vector>

struct AutoxState; /* autox_state.h */
struct AutoxDiagram; /* diagram.h */
namespace xpp {
struct Session; /* session.h */
struct Model; /* model.h */
struct NumericsSettings; /* numerics_settings.h */
struct IntegratorState; /* integrate.h */
struct DataStore; /* storage.h */
struct ParserState; /* expr.h */
struct Colors; /* colormap.h */
struct AutoxView; /* autox_state.h */
}

namespace xpp {

/* The parts of a Session that autoX reads and writes, handed in where an autoX operation starts (a Session converts to it
   where it is passed, session.cpp) and held by the run (AutoxRun::parts), so autoX's files never include session.h. The
   references are the Session's own parts, not copies; `threads` is the program preference's value
   when it was made, which a command never changes. `session` is only a handle, handed on to the functions of other
   modules that still take one (the solvers', the analyses', the XppUi seam's); autoX itself reads nothing through it.
   Constness is as shallow as a Session's own pointers: a const Session converts too. */
struct AutoxParts {
  /* implicit: callers outside autoX pass their Session (a const one converts too: a function that takes it const only
     reads the parts). A template, so the conversion is made in the caller, which has session.h: autoX's files never
     name a Session's members and no link reference to the Session's module is left in them */
  template<class S> requires std::same_as<std::remove_const_t<S>, Session>
  AutoxParts(S &s) : AutoxParts(const_cast<std::remove_const_t<S> &>(s), 0) {}
  Session &session;
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
  unsigned threads;
  Model &model() const { return model_; }
 private:
  template<class S> AutoxParts(S &s, int)
    : session(s), model_(s.model()), autox_state(s.autox_state.get()), diagram(s.diagram.get()), autox_view(s.autox_view.get()),
      numerics(s.numerics.get()), integrator(s.integrator.get()), data_store(s.data_store.get()), parser(s.parser),
      colors(s.colors), last_ic(s.last_ic), selected_window(s.selected_window), threads(s.threads) {}
};

} // namespace xpp
