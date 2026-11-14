#pragma once

#include <array>
#include <optional>
#include <string_view>

/* The numerics a model gives and a Session holds, as data: the methods by number, the value types and the
   NumericsSettings struct. Pure data below the solvers (solver.h) and the readers of a model, which set it (W736). */
namespace xpp {

/* the methods by number (numerics.method) */
namespace method {
enum Id : int {
  DISCRETE, EULER, MOD_EULER, RK4, ADAMS, GEAR, VOLTERRA, BACKEUL,
  RKQS, STIFF, CVODE, DP5, DP83, RB23, SYMPLECT,
  COUNT
};
}

/* the numerics' value types: the words of each are model_options.cpp's
   (poincare_map_names, section_sign_names), the number of a word its
   position there */
/* the Poincare map's kind: a section, the extrema or the period of a variable */
enum class PoincareMap { none, section, max, period };
/* which crossings of the section count: those going down, either way, or up
   (the sign of the crossing's direction) */
enum class SectionSign { negative = -1, both = 0, positive = 1 };
/* What the independent subsystems are split by (W433, Model::subsystems): the
   equations alone, or also the run, where a Poincare section or stop makes one
   variable decide every other variable's run */
enum class SubsystemSplit { equations, run };
inline constexpr std::array<std::string_view, 2> subsystem_split_names{"equations", "run"};
/* What the data store keeps each value as (W613, storage.h): the integrator's value rounded to a float, XPPAUT's own, or
   the double itself; the words are the .odex's `storage_precision` */
enum class StoragePrecision { single, double_ };
inline constexpr std::array<std::string_view, 2> storage_precision_names{"single", "double"};
/* the sign a number's own sign says (XPPAUT's +1, -1 and 0) */
constexpr SectionSign section_sign(double x) { return x < 0 ? SectionSign::negative : x == 0 ? SectionSign::both : SectionSign::positive; }
/* the crossings' words, by the sign's number plus 1 (negative, both, positive); the .odex's and the commands' */
inline constexpr std::array<std::string_view, 3> section_sign_names{"negative", "both", "positive"};

/* The equilibria's settings (the nUmerics menu's Sing pt ctrl, the options'
   `equilibria` group): Newton's tolerance and iterations, the Jacobian's
   step and the limit past which a search gives up, and what a search shows
   beside the equilibrium it finds (off: the search asks nothing) */
struct EquilibriaSettings {
  double newton_tolerance = 0, jacobian_step = 0, search_limit = 0;
  int max_iterations = 0;
  bool print_eigenvalues = false, draw_invariant_sets = false, draw_strong_sets = false;
};

/* The numerics settings in use (the nUmerics menu, the model's @ options,
   the command line), a Session's (session.h) */
struct NumericsSettings {
  /* the time span: from start_time for total_time, step delta_t, every store_every-th step
     stored once |t| is past store_from */
  double start_time = 0, total_time = 0, delta_t = 0, store_from = 0;
  int store_every = 0;
  /* what the data store keeps each value as (W613); the store itself converts when it changes (storage_precision_changed) */
  StoragePrecision storage_precision = StoragePrecision::single;
  /* the method and its step bounds, tolerances, the limit on the variables
     (and whether passing it ends the run) and the delays' maximum */
  method::Id method = method::DISCRETE;
  double min_step = 0, max_step = 0, relative_tolerance = 0, absolute_tolerance = 0, state_limit = 0, delay = 0;
  bool stop_at_limit = true;
  /* the equations compiled at load (MIR, W258, W714); false runs the interpreter (read when a model loads) */
  bool compile = true;
  /* the Volterra equations' memory, in points */
  int max_points = 0;
  EquilibriaSettings equilibria;
  /* the boundary value solver */
  double bvp_epsilon = 0, bvp_tolerance = 0;
  int bvp_max_iterates = 0;
  /* the backward Euler's Newton solve */
  double eul_tol = 0;
  int max_eul_iter = 0;
  /* the nullclines' mesh */
  int nullcline_mesh = 0;
  /* the Poincare map: its kind, the section's variable, sign and plane,
     extrema, stop on section */
  PoincareMap poincare_map = PoincareMap::none;
  int poincare_variable = 0, poiext = 0;
  SectionSign poincare_sign = SectionSign::both;
  bool poincare_stop = false;
  /* the independent subsystems the reports show (Model::subsystems is the equations' own) */
  SubsystemSplit subsystem_split = SubsystemSplit::equations;
  double poincare_plane = 0;
  /* the torus and its period (each variable's flag is Session::itor) */
  bool torus = false;
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
  bool cv_bandflag = false;
  int cv_bandupper = 1, cv_bandlower = 1;
  /* how every Jacobian of the model is formed: JACOBIAN_DIFFERENCE or
     JACOBIAN_EXACT (jacobian.h, W260) */
  int jacobian = 0;
  /* the global flags' crossing tolerance */
  double stol = 1.e-10;
  /* recompute the tables and kernels when a parameter changes
     (Numerics' AutoEval, @ autoeval=) */
  bool auto_evaluate = false;
  /* a run's state: data were stored (inflag), storing is on (storflag),
     a range stops (endsing), pauses
     between runs (pauser), shoots (shoot) or follows its parameter
     (par_fol); the browser shows a Fourier transform (fft) or a
     histogram (hist); nullclines were computed (null_here) */
  bool inflag = false, storflag = false, endsing = false, pauser = false;
  bool shoot = false, par_fol = false, fft = false, hist = false, null_here = false;
};

} // namespace xpp
