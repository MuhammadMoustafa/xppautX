#ifndef _load_eqn_h_
#define _load_eqn_h_

#include <stdio.h>
#include "xpplim.h"
#include "model_options.h"
#include "odex.h"
#include "odex_write.h"
#include "xpp_error.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vector>

/*
The acutual max filename length is determined by the 
FILENAME_MAX (see <stdio.h>), and usually 4096 -- but
this is huge and usually overkill.  On the otherhand 
the old Xpp default string buffer size of 100 is a bit
restricitive for lengths of filenames. You could also 
set this define in the Makefile or at compile time to 
override the below definition.
*/

#ifndef XPP_MAX_NAME
#define XPP_MAX_NAME 300
#if (XPP_MAX_NAME > FILENAME_MAX)
	#undef XPP_MAX_NAME
	#define XPP_MAX_NAME FILENAME_MAX
#endif
#endif


namespace xpp {

struct Model;   /* model.h */
struct Session; /* session.h */


/* s2 starts with s1 */
int msc(std::string_view s1, std::string_view s2);
/* ~/.xpprc's @ lines, each whole (none when it is not there), for
   set_internopts_xpprc_and_comline */
std::vector<std::string> check_for_xpprc();

/* The parameter sliders the ODE file sets up (@ s1=name, slo1=, shi1=,
   likewise 2 and 3); options_set has the s1.. rows once slider n was set. */
struct XppSlider {
    std::string var;           /* the parameter it moves */
    double lo = 0.0, hi = 1.0; /* its range */
    double step = 0.0;         /* 0: the page picks one */
};

/* The numerics settings in use (the nUmerics menu, the model's @ options,
   the command line), a Session's (session.h) */
struct NumericsSettings {
  /* the time span: from start_time for total_time, step delta_t, every store_every-th step
     stored once |t| is past the transient_time */
  double start_time = 0, total_time = 0, delta_t = 0, transient_time = 0;
  int store_every = 0;
  /* the method (numerics.cpp's numbers) and its step bounds, tolerances,
     the bound on the variables and the delays' maximum */
  int method = 0;
  double min_step = 0, max_step = 0, tolerance = 0, abs_tolerance = 0, bound = 0, delay = 0;
  /* the Volterra equations' memory, in points */
  int max_points = 0;
  /* equilibria: the Newton and eigenvector tolerances and iterations */
  double singpt_newton_tolerance = 0, singpt_jacobian_epsilon = 0;
  int singpt_max_iterates = 0;
  /* the boundary value solver */
  double bvp_epsilon = 0, bvp_tolerance = 0;
  int bvp_max_iterates = 0, bvp_flag = 0;
  /* the backward Euler's Newton solve */
  double eul_tol = 0;
  int max_eul_iter = 0;
  /* the nullclines' mesh */
  int nullcline_mesh = 0;
  /* the Poincare map: its kind, the section's variable, sign and plane,
     extrema, stop on section */
  int poincare_map = 0, poincare_variable = 0, poincare_sign = 0, poiext = 0, poincare_stop = 0;
  double poincare_plane = 0;
  /* the torus and its period (each variable's flag is Session::itor) */
  int torus = 0;
  double torus_period = 6.2831853071795864770;
  /* the random numbers' seed: rand_seed is the NEXT run's (shown/set by
     "@ seed=", Stochastic > New seed, --newseed; W71's "a seed per run"),
     applied and logged when that run starts (xpp_math.cpp Random::seed); the
     seed a run actually used is last_seed, empty before any run, for the
     protocol's state and a saved data file's header/metadata. Once used,
     rand_seed is redrawn from a separate seed stream seeded by it
     (xpp::next_seed), so an untouched field still gives fresh noise next
     time. */
  int rand_seed = 12345678;
  std::optional<int> last_seed;
  /* CVODE's banded Jacobian (the nUmerics menu's Stiff settings) */
  int cv_bandflag = 0, cv_bandupper = 1, cv_bandlower = 1;
  /* how every Jacobian of the model is formed: JACOBIAN_DIFFERENCE or
     JACOBIAN_EXACT (jacobian.h, W260) */
  int jacobian = 0;
  /* the global flags' crossing tolerance */
  double stol = 1.e-10;
  /* recompute the tables and kernels when a parameter changes
     (Numerics' AutoEval, @ autoeval=) */
  int auto_evaluate = 0;
  /* a run's state: data were stored (inflag), storing is on (storflag),
     a range stops (endsing), pauses
     between runs (pauser), shoots (shoot) or follows its parameter
     (par_fol); the browser shows a Fourier transform (fft) or a
     histogram (hist); nullclines were computed (null_here) */
  int inflag = 0, storflag = 0, endsing = 0, pauser = 0;
  int shoot = 0, par_fol = 0, fft = 0, hist = 0, null_here = 0;
};

/* The main plot's settings in use (the model's @ options, the Viewaxes
   dialogs), a Session's (session.h): the plotted variables, the axes'
   kind, the 2D window and the 3D box, and the extra curves (@ xp2=,
   ..., npltv of them) with the windows @ nplot opens (multi_win) */
struct PlotSettings {
  int ixplt = 0, iyplt = 0, izplt = 0;
  int axes = 0, timplot = 0, plot_3d = 0;
  double my_xlo = 0, my_ylo = 0, my_xhi = 0, my_yhi = 0;
  std::array<double,2> x_3d{}, y_3d{}, z_3d{};
  std::array<int,10> ix_plt{}, iy_plt{}, iz_plt{};
  int npltv = 0, multi_win = 0;
  std::array<double,10> x_lo{}, y_lo{}, x_hi{}, y_hi{};
  int start_line_type = 1;
};

/* the name=value items of an @ line as set_internopts reads them, or
   (set) of an internal set's "$ ..." as extract_action does: each
   item without a name or a value left out (@ total = 1 sets nothing) */
std::vector<std::pair<std::string, std::string>> option_items(std::string_view line, bool set,
                                                              std::vector<std::string> *ignored = nullptr);

/* The load (xpp_batch.cpp) hands the Model and Session it builds to what
   reads the model and its options (W47d3): */
/* the model's file (Model::this_file) named on the command line is kept;
   none (or a folder) is the error xpp::is_no_model (xpp_batch.h): the
   start screen's Open model asks, not the load */
void choose_model_file(Session &s);
void load_eqn(Session &s);
/* the defaults of the options no source set (set_option_defaults), then
   the storage, the solver and the initial values the model gives */
void set_all_vals(Session &s);
/* an @ line of the model (an .ode's, the .ode reader) kept in m's options,
   which set_internopts applies; -1 (logged, not kept) when it sets
   dll_lib or dll_fun */
int add_model_option(Model &m, std::string_view s1);
/* a .odex's options block, its entries kept in m's options, each at its
   own place (the file names the entries' Pos::file in files); a load error
   past the limit of options a model may set */
void add_model_options(Model &m, std::span<const odex::Option> entries, const std::vector<std::string> &files);
/* the model's options not yet applied (mask: the options unset before
   the model's, which its lines may set again), then .xpprc's and the
   command line's, each through set_option (model_options.h) */
void set_internopts(Session &s, const OptionsSet *mask);
void set_internopts_xpprc_and_comline(Session &s, std::span<const std::string> interopt);
/* the torus settings read from or written to a set file (lunch-new.cpp) */
/* the model's internal sets: one added (set name {does}), one's
   "name=value ..." settings applied to s (extract_action for any such
   text, a comment's action), one setting */
void add_intern_set(Model &m, std::string_view name, std::string_view does,
                    std::vector<std::pair<std::string, std::string>> settings = {});
/* the model's sets and options lines again from the statements of its file
   read anew, once the file was edited (model_save.h): every set of the
   statements, an import's included, at its own place; the options block's
   entries (the sets and the options of the model before are dropped) */
void replace_intern_sets(Model &m, const odex::Parsed &parsed);
void replace_model_options(Model &m, const odex::Parsed &parsed);
/* the limits the builder keeps that the parser does not (the options a model
   sets, its sets): a file past them would not load again */
Result<> check_model_limits(const odex::Parsed &parsed);
/* all or nothing: every item is checked first, and the first that cannot
   be applied is the error, at where (the set's line in the model), with
   nothing applied. xppaut_names: XPPAUT's words for the options count too
   (the command line's settings); a model's own set and action do not take them */
Result<> extract_action(Session &s, std::string_view ptr, const Place &where, bool xppaut_names);
/* The scopes of a set's apply and of restoring defaults: all, the parameters,
   the initial values (the states), or one of the option groups a set may hold
   (set_groups, model_options.h) */
namespace set_scope {
inline constexpr std::string_view all = "all", parameters = "parameters", initial = "initial";
}
std::vector<std::string> set_scopes();
/* why scope is none of them (naming them), "" when it is */
std::string set_scope_problem(std::string_view scope);
/* the index of the model's set called name (the spelling it was written in),
   -1 when it has none */
int intern_set_index(const Model &m, std::string_view name);
/* Apply set: a patch over the current values, the model's set `name` limited
   to the entries of `scope` (the parameters, the initial values, one group of
   settings, or all); the plot settings go to graph (the active one when
   none). All or nothing: a bad entry, a set or a graph that does not exist is
   the error (at the set's line, or the command's place) and nothing changed.
   A parameter-only apply does not touch the initial values, whose formulas are
   not evaluated again. One undo step. */
Result<> apply_named_set(Session &s, std::string_view name, std::string_view scope, std::optional<int> graph);
/* Restore defaults: the model's own values back, for scope (as above) and, with
   a field, only that parameter, state or setting of it. A setting returns to
   the built-in value with the model's `options` lines over it. All or nothing,
   one undo step. */
Result<> restore_defaults(Session &s, std::string_view scope, std::string_view field, std::optional<int> graph);
/* every set of the model checked as apply_named_set would apply it, none
   applied: the first that cannot be is the error, at its line (a load, so a
   bad set stops it) */
Result<> check_intern_sets(Session &s);
void do_intern_set(Session &s, std::string_view name1, std::string_view value);
/* Save values' name for a new set: the first of set1, set2, ... not yet a set
   of the model */
std::string intern_set_default_name(const Model &m);
/* the current values of scope (as apply_named_set's) as a save writes them: every
   parameter, every state (the last state reached, stored, when last_state), and
   every setting of the groups a set holds that the scope covers, from graph (the
   active one when none) for the plot settings. The error when a scope or graph is
   none, or the last state was asked and nothing was run. */
Result<odex::Values> captured_values(Session &s, std::string_view scope, std::optional<int> graph, bool last_state);
/* what a key of a set is in m: a parameter, a state, or neither */
odex::ValueKind value_kind(const Model &m, std::string_view name);

} // namespace xpp
#endif
