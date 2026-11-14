#pragma once
/* xpp::Session: everything a run changes (AGENTS.md "No global state";
   docs/roadmap.md W47c). C++ only.

   A Model (model.h) is what loading a .ode file produces and stays as the
   load left it; a Session is what a person does with it: the data a run
   stores, the plot windows and what is drawn in them, the numerics
   settings in use, the integrator's state, autoX's, the data table's and the
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
#include "colormap.h"
#include "expr.h"
#include "xpp_math.h"
#include "xpp_error.h"
#include "xpp_files.h"
#include "xpp_log.h"
#include "model_options.h"
#include "xpp_cpu.h"
#include "model_parts.h" /* IWYU pragma: keep (a std::vector of its BoundaryCondition is destroyed by Session, which needs it complete) */

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>
namespace xpp { struct TABULAR; struct UserButton; }

/* global types, as their owners define them */
struct LABEL; /* struct.h: the text labels a Session holds in a vector */
struct ArrayPlotState; /* arrayplot.h */
struct AutoxState; /* autox_state.h */
struct AutoxDiagram; /* diagram.h */
struct XppKinescope; /* kinescope.h */
struct SavedSession; /* xpp_session.h */

namespace xpp {

struct Model; /* model.h */

/* The parts a Session holds through a std::unique_ptr, so that a change to
   one's header rebuilds only the files that use that part, never every file
   that reads session.h; each is defined by its owner's header. A file that
   uses `s.part` includes that header beside session.h. */
struct DataStore; /* storage.h */
struct XppPlotWindows; /* many_pops.h */
struct ToolWindowsHidden; /* many_pops.h */
struct XppFrozenCurves; /* graf_par.h */
struct Mov3d; /* graf_par.h */
struct PhaseShown; /* phase_data.h */
struct MarksShown; /* marks_data.h */
struct AutoxView; /* display_state.h */
struct IntegratorState; /* integrate.h */
struct SolverWork; /* storage.h */
struct JacobianScratch; /* jacobian.h */
struct DataState; /* browse.h */
struct NumericsSettings; /* numerics_settings.h */
struct PlotSettings; /* plot_settings.h */
struct DelayState; /* delay_handle.h */
struct HistogramState; /* histogram.h */
struct AdjointState; /* adj2.h */
struct StochasticState; /* markov.h */
struct NullclineSettings; /* nullcline.h */
struct VolterraState; /* volterra.h */
struct FitInfo; /* do_fit.h */
struct DaeState; /* dae_fun.h */
struct NullclineState; /* nullcline.h */
struct ShootRange; /* pp_shoot.h */
struct MarkInfo; /* grobs.h */
struct ManifoldShots; /* equilibria.h */
struct DrawingState; /* graphics.h */
struct PlotFileState; /* picture.h */
struct AnimationState; /* aniparse.h */
struct AniShown; /* ani_data.h */
struct ValueUndo; /* value_undo.h */
struct ModelRequest; /* model_switch.h */
/* element types of the vectors a Session holds: the vector's type may be
   incomplete here, as Session's constructor and destructor are in session.cpp */
struct PlotDisplay; /* display_state.h */
struct NetworkValues; /* simplenet.h */
struct BoundaryCondition; /* model_parts.h */
struct GROB; /* grobs.h */
struct XppSlider; /* model_options.h */

struct Session {
  /* a Session of the Model m (Load) */
  explicit Session(Model &m);
  ~Session();
  Session(const Session &)=delete;
  Session &operator=(const Session &)=delete;

  /* Room for n variables, fixed variables and aux quantities in the
     Session's per-variable tables (last_ic, delay_string, itor, bcs) and
     the Model's (Model::size_variables); the builder calls it as the file
     declares its variables. The new entries are 0, "0.0" (no history), 0
     (no torus) and no boundary condition. */
  void size_variables(int n);

  /* the Model this session runs */
  Model &model() noexcept { return *model_; }
  const Model &model() const noexcept { return *model_; }

  /* the rows a run stores and where the last run ended (storage.cpp) */
  std::unique_ptr<DataStore> data_store;
  /* the plot windows, the active one, Simulplot (many_pops.cpp) */
  std::unique_ptr<XppPlotWindows> plot_windows;
  std::string selected_window; // The first graph's name, set by the constructor; the target of an omitted window argument.
  std::unique_ptr<ToolWindowsHidden> tools_hidden; // The data table, animation and array plot windows hidden (graphs and views keep their own).
  /* the frozen curves of every window (graf_par.cpp) */
  std::unique_ptr<XppFrozenCurves> frozen_curves;
  /* 3D Params' movie settings (graf_par.cpp) */
  std::unique_ptr<Mov3d> movie_3d;
  /* what the page displays of each window: earlier runs, zoom (display_state.h) */
  std::vector<PlotDisplay> plot_display; /* one per window; the constructor makes the first */
  /* what each window shows besides its curves since it was last blanked:
     nullclines, direction field and flows (phase_data.cpp), equilibria,
     labels, objects and frozen curves (marks_data.cpp) */
  std::unique_ptr<PhaseShown> phase_shown;
  std::unique_ptr<MarksShown> marks_shown;
  /* autoX's hidden branches and zoom */
  std::unique_ptr<AutoxView> autox_view;
  /* how plots are written to files (graf_par.cpp) */

  /* where a file dialog starts (json_prompts.cpp, W151): the folder of the
     file of its kind (its wild pattern) last opened or saved, else `home`,
     the model's own folder -- never the process's current folder, which a
     replay moves into its scratch folder and autoX's files are not named by */
  struct FileDialogs {
    std::string home;
    std::map<std::string,std::string> last;
  } file_dialogs;
  files::WriteFolders write_folders; /* Folder answers belong to this session, never to a global. */
  Place output_place; /* The model option that named a batch output, kept through its run. */
  /* a run without an interface: the data file it writes
     (--outfile, @ output=, else a set's name: json_silent.cpp changes it per
     internal set). The Session's, so a load that fails or another model
     opened later never inherits them. */
  std::string batch_out_file = "output.dat";
  /* the picture a batch run writes (output.picture_file): "" for none, else a path whose extension picks the
     format (image_format.h's picture_format_of_file); json_silent.cpp names it after the set, like batch_out_file */
  std::string picture_file;
  /* the run_set options: the sets a batch run runs, by name (none: every set) */
  std::vector<std::string> run_sets;
  /* the file a batch run writes what its after_run analysis made to (the histogram, the spectrum), so batch_out_file
     keeps the trajectory; empty: the analysis replaces the table batch_out_file is written from (XPPAUT's way) */
  std::string after_run_file;

  /* the integrator's state (integrate.cpp) */
  std::unique_ptr<IntegratorState> integrator;
  /* its state vector and the solvers' scratch space (storage.cpp) */
  std::unique_ptr<SolverWork> solver_work;
  std::unique_ptr<JacobianScratch> jacobian_scratch;
  /* the initial data the next run starts from (the model's are
     Model::default_ic) */
  std::vector<double> last_ic;
  /* each variable's initial data as typed (a delay equation's history:
     "0.0" when it has none) */
  std::vector<std::string> delay_string;
  /* each variable's torus flag: 1 when it is taken modulo the torus
     period (the nUmerics menu's Torus) */
  std::vector<int> itor;

  /* the thread count of autoX's collocation and of a sweep's runs (W267, W434), a program preference
     (xpp_prefs.h) kept here for the run: cpu::THREADS_AUTO, or the count */
  unsigned threads = cpu::THREADS_AUTO;
  /* the colors the diagram, the nullclines and the manifolds are drawn in, a program preference like the thread count */
  Colors colors;
  /* the threads a set of jobs or runs shares: the count, or all the physical cores when automatic */
  unsigned thread_budget() const { return threads != cpu::THREADS_AUTO ? threads : cpu::physical_threads(); }

  /* autoX's settings, what its window shows and its scratch folder
     (autox_nox.cpp), and the diagram (diagram.cpp). A run's own state, its
     parameters, fort files and work arrays, is an AutoxRun (autox_state.h)
     do_autox makes: none is kept here. */
  std::unique_ptr<AutoxState> autox_state;
  std::unique_ptr<AutoxDiagram> diagram;

  /* the data table (browse_data.cpp) */
  std::unique_ptr<DataState> data;
  /* the kinescope (commands.cpp, json_windows.cpp) */
  std::unique_ptr<XppKinescope> kinescope;

  /* the numerics and the main plot's settings in use (load_eqn.cpp) */
  std::unique_ptr<NumericsSettings> numerics;
  std::unique_ptr<PlotSettings> plot_settings;
  /* the expression engine's constants, variables, counts, symbol table
     and evaluation stacks (expr.h) */
  ParserState parser;
  /* The native preset list; foreign @ S1-S3 options populate it on import. */
  std::vector<XppSlider> sliders;
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
  std::vector<double> last_equilibrium, last_equilibrium_eigenvalues;
  std::string last_equilibrium_stability;
  bool equilibrium_append=false, equilibrium_shoot=false;
  /* the command line named a model file */
  int got_file=0;
  /* the color scale: its name (@ colormap, the Colormap menu; viridis the
     default of a model of ours, jet the one an XPPAUT .ode without @ colormap
     is converted with) and its 256 colors (colormap.h's make_ramp: made once for each name it has) */
  Colormap colormap=Colormap::viridis;
  const Ramp &ramp() const
  {
    if(ramp_of_!=colormap){
      ramp_=make_ramp(colormap);
      ramp_of_=colormap;
    }
    return ramp_;
  }

  /* the colormap's ramp: ramp()'s cache */
  mutable Ramp ramp_=make_ramp(Colormap::viridis);
  mutable Colormap ramp_of_=Colormap::viridis;

  /* what each network computed last and its work space (simplenet.cpp;
     their definitions are Model::networks) */
  std::vector<NetworkValues> networks; /* one per Model network (add_net_name) */

  /* the tables (tabular.cpp), ntable of them: a function table's values
     are computed again when a parameter it reads changes (autoeval,
     redo_all_fun_tables), and a file table can be read again from the
     Numerics menu */
  int ntable=0;
  std::vector<TABULAR> tables; /* set_table_name makes room for each */

  /* the boundary conditions in use, one per ODE: the Model's, copied at
     the end of a load, then changed by a `set` or a .set
     (set_bc_formula) and compiled when a shooting starts (pp_shoot.cpp) */
  std::vector<xpp::BoundaryCondition> bcs;

  /* the delay equations' state (delay_handle.cpp) */
  std::unique_ptr<DelayState> delay;
  /* the histogram and spectrum settings and results (histogram.cpp) */
  std::unique_ptr<HistogramState> histogram;
  /* the adjoint, its H function and the transposed data (adj2.cpp) */
  std::unique_ptr<AdjointState> adjoint;
  /* stocHast's many-runs state (markov.cpp) */
  std::unique_ptr<StochasticState> stochastic;
  /* the random generator its runs, the parser's ran(), normal() and
     poisson() and Monte Carlo draw from (xpp_math.h); a .snapx's
     the random group of a .snapx's session.odex saves and restores it */
  Random random;
  /* the nullclines', direction field's and orbit coloring's settings
     (nullcline.cpp) */
  std::unique_ptr<NullclineSettings> nullclines;
  /* the columns a batch run writes, the "only" statement's or
     post-processing's (form_ode.cpp's create_plot_list, histogram.cpp);
     none: every column */
  std::vector<int> plot_list;
  /* the integral equations' running state (volterra2.cpp) */
  std::unique_ptr<VolterraState> volterra;
  /* Data's Fit settings (do_fit.cpp) */
  std::unique_ptr<FitInfo> fit;
  /* the algebraic variables' solver (dae_fun.cpp) */
  std::unique_ptr<DaeState> dae;
  /* the nullclines and direction field computed, and the frozen ones */
  std::unique_ptr<NullclineState> nullcline_state;
  /* BVP's Range settings (pp_shoot.cpp) */
  std::unique_ptr<ShootRange> shoot_range;
  /* Text,etc's marker settings (grobs.cpp) */
  std::unique_ptr<MarkInfo> marker;
  /* Sing pts' shooting (equilibria.cpp) */
  std::unique_ptr<ManifoldShots> manifolds;

  /* the drawing state and a picture file's (graphics.cpp, axes2.cpp,
     picture.cpp) */
  std::unique_ptr<DrawingState> drawing;
  std::unique_ptr<PlotFileState> plot_file;
  /* the text labels and graphic objects on the plot windows (grobs.cpp) */
  std::vector<LABEL> labels;
  std::vector<GROB> grobs;
  int next_annotation_id = 0; /* the id of the next text, arrow, pointer or marker added (grobs.h) */
  /* the array plot (arrayplot.cpp) and the animator (aniparse.cpp) */
  std::unique_ptr<ArrayPlotState> array_plot;
  std::unique_ptr<AnimationState> animation;
  /* the animation's frames as the events' text (ani_data.cpp) */
  std::unique_ptr<AniShown> ani_shown;
  /* the model's buttons (userbut.h) */
  std::vector<UserButton> user_buttons;

  /* File > Open model's or Reload's model to load once the command that
     asked for it has returned (model_switch.h) */
  std::unique_ptr<ModelRequest> model_request;
  /* the session file this session was last saved to or opened from
     (xpp_session.h) */
  std::unique_ptr<SavedSession> saved_session;
  /* the Values edits that can be undone and redone (value_undo.h) */
  std::unique_ptr<ValueUndo> value_undo;

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
   keeps the process's autoX scratch folder. Model and Session memory is
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
