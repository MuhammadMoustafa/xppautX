#ifndef XPP_SESSION_H
#define XPP_SESSION_H
/* xpp::Session: everything a run changes (AGENTS.md "No global state";
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

   A Session is chosen once where work starts (W47d, docs/roadmap.md: a
   protocol command in ui_json.cpp's handle_line, the start of the
   program, a load) and passed down from there as `xpp::Session &s`;
   nothing reads a current one. The one global that holds Sessions is the
   session list (client_session, below). A load builds a new Model and
   Session together (Load, below) and keeps them only when it succeeds. */
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
#include "dae_fun.h"
#include "do_fit.h"
#include "histogram.h"
#include "adj2.h"
#include "markov.h"
#include "nullcline.h"
#include "gear.h"
#include "pp_shoot.h"
#include "graphics.h"
#include "my_ps.h"
#include "grobs.h"
#include "arrayplot.h"
#include "aniparse.h"
#include "userbut.h"
#include "xpp_math.h"
#include "phase_data.h"
#include "marks_data.h"
#include "ani_data.h"
#include "xpp_error.h"
#include "xpp_log.h"
#include "model_switch.h"
#include "value_undo.h"
#include "display_state.h"
#include "xpp_session.h"

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace xpp {

struct Session {
  /* a Session of the Model m (Load) */
  explicit Session(Model &m) noexcept : model_(&m) {}
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
  /* 3D Params' movie settings (graf_par.cpp) */
  Mov3d movie_3d;
  /* what the page displays of each window: earlier runs, zoom (display_state.h) */
  std::array<PlotDisplay,MAXPOP> plot_display;
  /* what each window shows besides its curves since it was last blanked:
     nullclines, direction field and flows (phase_data.cpp), equilibria,
     labels, objects and frozen curves (marks_data.cpp) */
  PhaseShown phase_shown;
  MarksShown marks_shown;
  /* AUTO's hidden branches and zoom */
  AutoView auto_view;
  /* how plots are written to files (graf_par.cpp) */
  XppPlotExport plot_export;

  /* where a file dialog starts (json_prompts.cpp, W151): the folder of the
     file of its kind (its wild pattern) last opened or saved, else `home`,
     the model's own folder -- never the process's current folder, which a
     replay moves into its scratch folder and AUTO's files are not named by */
  struct FileDialogs {
    std::string home;
    std::map<std::string,std::string> last;
  } file_dialogs;

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
  std::vector<XppSlider> sliders = std::vector<XppSlider>(XPP_NSLIDERS);
  /* the options a source has set (model_options.h): the command line
     sets its own first, and the model's may not override them */
  OptionsSet options_set;
  /* how many of the model's options set_internopts (load_eqn.cpp) has
     applied: each call applies those the parser added since the call
     before (xpp::load_model's, after the parse, applies them all;
     set_all_vals' own finds none new) */
  std::size_t options_applied=0;
  /* the last equilibrium the equilibrium window showed, for its Import
     (json_state.cpp) */
  std::vector<double> last_equilibrium;
  /* which menu the main window's keys go to: MAIN_MENU, FILE_MENU or
     NUM_MENU (commands.cpp show_main_menu) */
  int help_menu=0;
  /* --runnow or @ runnow=1: integrate once the front end is up */
  int run_immediately=0;
  /* the command line named a model file */
  int got_file=0;
  /* the colour scale's type (colormap.h's make_cmaps: @ colormap, the
     Colormap menu); 0 the default, so a model without @ colormap gets it */
  int colormap=0;

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
  /* the adjoint, its H function and the transposed data (adj2.cpp) */
  AdjointState adjoint;
  /* stocHast's many-runs state (markov.cpp) */
  StochasticState stochastic;
  /* the random generator its runs, the parser's ran(), normal() and
     poisson() and Monte Carlo draw from (xpp_math.h); a .snapx's
     random.txt saves and restores it */
  Random random;
  /* the nullclines', direction field's and orbit colouring's settings
     (nullcline.cpp) */
  NullclineSettings nullclines;
  /* the columns a batch run writes, the "only" statement's or
     post-processing's (form_ode.cpp's create_plot_list, histogram.cpp);
     none: every column */
  std::vector<int> plot_list;
  /* the integral equations' running state (volterra2.cpp) */
  VolterraState volterra;
  /* Data's Fit settings (do_fit.cpp) */
  FitInfo fit;
  /* the algebraic variables' solver (dae_fun.cpp) */
  DaeState dae;
  /* the nullclines and direction field computed, and the frozen ones */
  NullclineState nullcline_state;
  /* BVP's Range settings (pp_shoot.cpp) */
  ShootRange shoot_range;
  /* Text,etc's marker settings (grobs.cpp) */
  MarkInfo marker;
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
  /* the animation's frames as the events' text (ani_data.cpp) */
  AniShown ani_shown;
  /* the model's buttons (@ button=name:keys; userbut.cpp) */
  int nuserbut=0;
  std::array<USERBUT,USERBUTMAX> userbut{};

  /* File > Open model's or Reload's model to load once the command that
     asked for it has returned (model_switch.h) */
  std::optional<ModelRequest> model_request;
  /* the session file this session was last saved to or opened from
     (xpp_session.h) */
  SavedSession saved_session;
  /* the Values edits that can be undone and redone (value_undo.h) */
  ValueUndo value_undo;

private:
  Model *model_;
};

/* The session list, the only global that holds a Session (AGENTS.md "No
   global state"): one Session per client. The process serves one client
   (the page or the desktop window, --server's stdin, --silent's script, a
   unit test), so the list holds one, its client's: the first made on
   first use (of an empty Model, before any load), then each load's (Load,
   below). It is read only where a Session is chosen, and passed down
   from there: a protocol command (ui_json.cpp's handle_line) and the
   front end's own asks and checkpoints, which core code reaches through
   the XppUi seam with no Session (ui_json.cpp's client()), and the
   program's start and exit (xppautx_main.cpp). tools/sessioncheck.sh
   fails a read anywhere else in core/. */
Session &client_session();

/* what model_failed throws while a Load is in progress: the model
   cannot be loaded (a parse or compile error), what is wrong and where */
struct LoadFailed {
  Error error;
};

/* A load in progress: it builds a fresh Model and Session, which the
   parser and the load's set-up are handed (session(), model()) and fill;
   while it lives they are the client's in the session list too, for the
   front end's asks during the load (the core is single-threaded: nothing
   else sees them meanwhile); commit() keeps them
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
  /* the fresh Model and Session the load fills: the readers and the
     load's set-up are handed these (W47d3) */
  Session &session() noexcept { return *session_; }
  Model &model() noexcept { return session_->model(); }
  /* a Load is in progress */
  static bool running() noexcept;
  /* e.place.source, when empty, the line e.place.line of e.place.file as
     the model of the Load in progress reads it (model_files.h); nothing
     when no Load is in progress */
  static void add_source(Error &e);
  /* The load is at line (and column) of file: the model's readers and
     builder say where they are, so that a problem is reported there;
     line 0, the model as a whole. The ERROR and WARN messages logged
     while a Load is in progress are kept (LogCapture) and written with
     the place they are about (Error::text()) once the load moves on from
     it (here), commits or ends; the ones about the place a load fails at
     are its error's what, written once by load_model. Nothing when no
     Load is in progress. */
  static void at(std::string_view file, int line=0, int col=0);
  /* what went wrong where the load is (a Load is in progress): the place
     at() gave, and as what failed the ERROR and WARN messages logged
     since (no longer kept to be written), then also */
  static Error error(std::string_view also={});
  /* where the load is (at()'s place); empty when no Load is in progress */
  static Place place();
private:
  /* the messages kept since at(), without the blank lines around them,
     each line without the blanks at its end (a caret line keeps those in
     front) */
  std::string kept() const;
  /* the messages kept, written at the place they are about */
  void write_kept();
  /* the client's before the load (none at the first), until commit();
     the Session declared last, so that it goes before its Model */
  std::unique_ptr<Model> previous_model;
  std::unique_ptr<Session> previous_session;
  Session *session_;
  bool committed=false;
  Place where;
  LogCapture messages;
};

}

#endif
