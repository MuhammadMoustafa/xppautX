#pragma once

#include <array>
#include <optional>
#include <cstddef>
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

/* what a method is, and the words it goes by (W744: the model's option table and its checks read these, the solvers (solver.h) add
   how each starts): the registry's numbers are the method::Id values */
/* what a method is: how Integrate drives it, what the numerics menu asks */
struct SolverTraits {
  /* the time counts iterations of a map (dt is +-1) */
  bool discrete = false;
  /* advances a number of steps of dt; otherwise it chooses its own steps
     on to an output time (its store_every is 1) */
  bool fixed_step = false;
  /* the options its steps read, which the numerics menu asks and the
     Numerics form shows (W381 questions 5 and 8): relative_tolerance,
     absolute_tolerance, min_step, max_step */
  bool relative_tolerance = false;
  bool absolute_tolerance = false;
  bool min_step = false;
  bool max_step = false;
  /* asks the Newton tolerance and iterations of its implicit step */
  bool newton = false;
  /* asks whether the system is banded */
  bool banded = false;
  /* keeps integral-equation history and asks its maximum stored points */
  bool integral_history = false;
  /* pairs coordinates and momenta, requiring an even dimension */
  bool paired_dimension = false;
  /* uses the eighth-order Dormand-Prince step rather than the fifth */
  bool eighth_order = false;
  /* retains an accepted-step interpolant across requested output times */
  bool dense_output = false;
  /* uses the stiff adaptive step rather than adaptive Runge-Kutta */
  bool stiff_step = false;
  /* draws Wiener noise once per step */
  bool wiener = false;
};

/* a row of the methods' table */
struct MethodInfo {
  method::Id id;
  const char *name;      /* do_info's and the front end's */
  const char *set_label; /* the label .set files write beside it */
  const char *key;       /* the one word a .odex writes (docs/odex.md "Options") */
  SolverTraits traits;
};

/* traits */
inline constexpr SolverTraits map{.discrete=true,.fixed_step=true,.wiener=true};
inline constexpr SolverTraits steps{.fixed_step=true,.wiener=true};
inline constexpr SolverTraits implicit_steps{.fixed_step=true,.newton=true,.wiener=true};
inline constexpr SolverTraits integral_steps{.fixed_step=true,.newton=true,.integral_history=true,.wiener=true};
inline constexpr SolverTraits paired_steps{.fixed_step=true,.paired_dimension=true,.wiener=true};
/* Gear: its step between min_step and max_step; QualRK and Stiff stop at
   min_step and have no bound above; CVODE and Rosenbrock read neither; the
   Dormand-Prince pair bounds its steps by max_step */
inline constexpr SolverTraits gear_steps{.relative_tolerance=true,.min_step=true,.max_step=true};
inline constexpr SolverTraits adaptive_steps{.relative_tolerance=true,.min_step=true};
inline constexpr SolverTraits stiff_steps{.relative_tolerance=true,.min_step=true,.stiff_step=true};
inline constexpr SolverTraits rel_abs_eighth{.relative_tolerance=true,.absolute_tolerance=true,.max_step=true,.eighth_order=true,.dense_output=true};
inline constexpr SolverTraits rel_abs{.relative_tolerance=true,.absolute_tolerance=true,.max_step=true,.dense_output=true};
inline constexpr SolverTraits rel_abs_banded{.relative_tolerance=true,.absolute_tolerance=true,.banded=true};

/* The table: every method, by number. */
inline constexpr std::array<MethodInfo,method::COUNT> methods{{
  {method::DISCRETE,"Discrete","Discrete","discrete",map},
  {method::EULER,"Euler","Euler","euler",steps},
  {method::MOD_EULER,"Mod. Euler","Mod. Euler","modified_euler",steps},
  {method::RK4,"Runge-Kutta","Runge-Kutta","runge_kutta",steps},
  {method::ADAMS,"Adams","Adams","adams",steps},
  {method::GEAR,"Gear","Gear","gear",gear_steps},
  {method::VOLTERRA,"Volterra","Volterra","volterra",integral_steps},
  {method::BACKEUL,"Backward Euler","BackEul","backward_euler",implicit_steps},
  {method::RKQS,"QualRK","Qual RK","qualrk",adaptive_steps},
  {method::STIFF,"Stiff","Stiff","stiff",stiff_steps},
  {method::CVODE,"CVode","CVode","cvode",rel_abs_banded},
  {method::DP5,"DoPri5","DorPrin5","dopri5",rel_abs},
  {method::DP83,"DoPri8(3)","DorPri8(3)","dopri8",rel_abs_eighth},
  {method::RB23,"Rosenbrock","Rosenbrock","rosenbrock",rel_abs_banded},
  {method::SYMPLECT,"Symplectic","Symplectic","symplectic",paired_steps},
}};

constexpr bool methods_in_order()
{
  for(int i=0;i<method::COUNT;i++)
    if(methods[static_cast<std::size_t>(i)].id!=i)return false;
  return true;
}
static_assert(methods_in_order(),"the methods' rows are in method order");

/* a method's row: callers must supply a checked id */
inline const MethodInfo &method_info(int m) { return methods[static_cast<std::size_t>(m)]; }
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
/* How the model's Jacobian is formed, the numerics' `jacobian` (W260):
   by forward differences, XPPAUT's (the default: a model gives XPPAUT's
   numbers), or exactly, from the derivatives translate_model made of the
   model's right-hand sides (Model::jacobian). Their names, by value, are
   what the `jacobian` option and `set num` take. */
constexpr int JACOBIAN_DIFFERENCE = 0, JACOBIAN_EXACT = 1;
inline constexpr std::array<std::string_view, 2> jacobian_names{"difference", "exact"};
inline constexpr std::string_view jacobian_name(int jacobian) { return jacobian_names[static_cast<std::size_t>(jacobian)]; }
/* its key in the numerics (`set num`, the numerics event) */
inline constexpr std::string_view JACOBIAN_KEY = "jacobian";
/* the sign a number's own sign says (XPPAUT's +1, -1 and 0) */
constexpr SectionSign section_sign(double x) { return x < 0 ? SectionSign::negative : x == 0 ? SectionSign::both : SectionSign::positive; }
/* the crossings' words, by the sign's number plus 1 (negative, both, positive); the .odex's and the commands' */
inline constexpr std::array<std::string_view, 3> section_sign_names{"negative", "both", "positive"};

/* The equilibria's options (the nUmerics menu's Sing pt ctrl, the options'
   `equilibria` group): Newton's tolerance and iterations, the Jacobian's
   step and the limit past which a search gives up, and what a search shows
   beside the equilibrium it finds (off: the search asks nothing) */
struct EquilibriaSettings {
  double newton_tolerance = 0, jacobian_step = 0, search_limit = 0;
  int max_iterations = 0;
  bool print_eigenvalues = false, draw_invariant_sets = false, draw_strong_sets = false;
};

/* The numerics options in use (the nUmerics menu, the model's @ options,
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
  /* CVODE's banded Jacobian (the nUmerics menu's Stiff options) */
  bool cv_bandflag = false;
  int cv_bandupper = 1, cv_bandlower = 1;
  /* how every Jacobian of the model is formed: JACOBIAN_DIFFERENCE or
     JACOBIAN_EXACT (model_numerics.h, W260) */
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
