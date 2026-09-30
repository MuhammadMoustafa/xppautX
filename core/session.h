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

   A Session runs one Model, the one its load built: model() reaches it,
   so a function that takes a Session& needs no Model& beside it.

   The current Session is chosen once where work starts (W47d,
   docs/roadmap.md: a protocol command in ui_json.cpp's handle_line, the
   start of the program, a load) and passed down from there as
   `xpp::Session &s`; xpp::session() and xpp::model() are for the entry
   points the stages of W47d have not reached yet. A hot loop takes the
   Session once. A load builds a new Model and Session together (Load,
   below) and keeps them only when it succeeds. */
#include "model.h"
#include "storage.h"
#include "many_pops.h"
#include "graf_par.h"
#include "integrate.h"
#include "auto_state.h"
#include "diagram.h"
#include "browse.h"
#include "kinescope.h"
#include "load_eqn.h"
#include "expr.h"
#include "delay_handle.h"
#include "histogram.h"
#include "markov.h"
#include "nullcline.h"
#include "gear.h"
#include "graphics.h"
#include "my_ps.h"
#include "grobs.h"
#include "arrayplot.h"
#include "aniparse.h"
#include "userbut.h"
#include "diagnostic.h"
#include "xpp_log.h"
#include "model_switch.h"
#include "display_state.h"
#include "xpp_session.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace xpp {

struct Session {
  /* a Session of the Model m (Load) */
  explicit Session(Model &m) noexcept : model_(&m) {}
  /* the first one, made on first use before any load: of the first Model */
  Session() : Session(detail::current<Model>()) {}
  Session(const Session &)=delete;
  Session &operator=(const Session &)=delete;

  /* the Model this session runs */
  Model &model() noexcept { return *model_; }
  const Model &model() const noexcept { return *model_; }

  /* the rows a run stores and where the last run ended (storage.cpp) */
  DataStore data_store;
  /* the plot windows, the active one, Simulplot (xpp_util.cpp) */
  XppPlotWindows plot_windows{};
  /* the frozen curves of every window (graf_par.cpp) */
  XppFrozenCurves frozen_curves{};
  /* what the page displays of each window: earlier runs, zoom (display_state.h) */
  std::array<PlotDisplay,MAXPOP> plot_display;
  /* AUTO's hidden branches and zoom */
  AutoView auto_view;
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

  /* the numerics and the main plot's settings in use (load_eqn.cpp) */
  NumericsSettings numerics;
  PlotSettings plot_settings;
  /* the expression engine's constants, variables, counts, symbol table
     and evaluation stacks (expr.h) */
  ParserState parser;
  /* the parameter sliders the model sets up (@ s1=name, slo1=, shi1=,
     likewise 2 and 3) */
  std::array<XppSlider,XPP_NSLIDERS> sliders;
  /* 1 while an option may still be set: the command line and .xpprc set
     theirs first, and the model's may not override them */
  OptionsSet not_already_set{};
  /* -runnow or @ runnow=1: integrate once the front end is up */
  int run_immediately=0;
  /* the command line named a model file */
  int got_file=0;

  /* what each network computed last and its work space (simplenet.cpp;
     their definitions are Model::networks) */
  std::array<NetworkValues,MAXNET> networks;

  /* the tables (tabular.cpp), ntable of them: a function table's values
     are computed again when a parameter it reads changes (autoeval,
     redo_all_fun_tables), and a file table can be read again from the
     Numerics menu */
  int ntable=0;
  std::array<TABULAR,MAX_TAB> tables;

  /* the boundary conditions in use, one per ODE: the Model's, copied at
     the end of a load, then changed by a `set` or a .set
     (set_bc_formula) and compiled when a shooting starts (pp_shoot.cpp) */
  std::array<xpp::Model::BoundaryCondition,MAXODE> bcs;

  /* "_<name>" of the internal set applied last (use_intern_set), "" when
     none: a batch run's plot files are named after it (batch_plot_name) */
  std::string this_internset;

  /* the delay equations' state (delay_handle.cpp) */
  DelayState delay;
  /* the histogram and spectrum settings and results (histogram.cpp) */
  HistogramState histogram;
  /* stocHast's many-runs state (markov.cpp) */
  StochasticState stochastic;
  /* the nullclines', direction field's and orbit colouring's settings
     (nullcline.cpp) */
  NullclineSettings nullclines;
  /* Sing pts' shooting (gear.cpp) */
  ManifoldShots manifolds;

  /* the drawing state and a picture file's (graphics.cpp, axes2.cpp,
     my_ps.cpp, my_svg.cpp) */
  DrawingState drawing;
  PlotFileState plot_file;
  /* the text labels and graphic objects on the plot windows (grobs.cpp) */
  std::array<LABEL,MAXLAB> labels{};
  std::array<GROB,MAXGROB> grobs{};
  /* the array plot (arrayplot.cpp) and the animator (aniparse.cpp) */
  ArrayPlotState array_plot;
  AnimationState animation;
  /* the model's buttons (@ button=name:keys; userbut.cpp) */
  int nuserbut=0;
  std::array<USERBUT,USERBUTMAX> userbut{};

  /* File > Open model's or Reload's model to load once the command that
     asked for it has returned (model_switch.h) */
  std::optional<ModelRequest> model_request;
  /* the session file this session was last saved to or opened from
     (xpp_session.h) */
  SavedSession saved_session;

private:
  Model *model_;
};

/* the current Session (xpp_current.h) */
inline Session &session()
{
  return detail::current<Session>();
}

/* what xpp_model_failed throws while a Load is in progress: the model
   cannot be loaded (a parse or compile error, already logged), what is
   wrong and where */
struct LoadFailed {
  Diagnostic diagnostic;
};

/* A load in progress: while it lives the current Model and Session are
   fresh ones, which the parser and the load's set-up fill (the core is
   single-threaded: nothing else sees them meanwhile); commit() keeps them
   and drops the ones before, and a load that never commits (it failed:
   LoadFailed) puts the ones before back, untouched. The fresh Session
   keeps the process's AUTO scratch folder. Model and Session memory is
   never freed while a pointer into it may be held: the old ones go at
   commit, when the new ones have replaced every use. */
class Load {
public:
  Load();
  ~Load();
  Load(const Load &)=delete;
  Load &operator=(const Load &)=delete;
  void commit();
  /* a Load is in progress */
  static bool running() noexcept { return detail::current_slot<Load>()!=nullptr; }
  /* The load is at line (and column) of file: the model's readers and
     builder say where they are, so that a problem is reported there;
     line 0, the model as a whole. The messages logged before are no
     longer about it. Nothing when no Load is in progress. */
  static void at(std::string_view file, int line=0, int col=0);
  /* what went wrong where the load is (a Load is in progress): the place
     at() gave, and as the cause the ERROR and WARN messages logged since */
  static Diagnostic diagnostic();
private:
  Model *previous_model;
  Session *previous_session;
  bool committed=false;
  Diagnostic where;
  LogCapture messages;
};

}

#endif
