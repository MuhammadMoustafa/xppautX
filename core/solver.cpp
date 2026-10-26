/* The integration methods' registry and their Solvers (solver.h, W51):
   each Solver wraps its method's routine (odesol2.cpp, gear.cpp,
   stiff.cpp, dormpri.cpp, cv2.cpp, volterra2.cpp), owns the work memory
   it steps with and translates the routine's own flag into a
   Result<>. */
#include "solver.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include "xpp_io.h"
#include <new>
#include <string>
#include <vector>

#include "cv2.h"
#include "dormpri.h"
#include "gear.h"
#include "jacobian.h"
#include "odesol2.h"
#include "session.h"
#include "stiff.h"
#include "volterra2.h"
#include "xpp_mem.h" /* xpp::out_of_memory */
#include "xpp_ui.h"
#include "model.h"
#include "model_files.h"
#include "numerics_settings.h"
#include "xpp_math.h"

namespace xpp {
namespace {

/* the work memory a solver of n equations steps with */
int standard_work(int n, const Session &) { return 30*n; }
/* the model's Jacobian, banded or full, never less than n*n */
int jacobian_work(int n, const Session &s) { return std::max(n*n,jacobian_entries(n,model_jacobian_form(s,n))); }
int implicit_work(int n, const Session &s) { return 10*n+jacobian_work(n,s)+100; } /* backward Euler, Volterra */
int gear_work(int n) { return 30*n+n*n+100; }
int stiff_work(int n, const Session &) { return 2*n*n+13*n+100; }
int rosenbrock_work(int n, const Session &s) { return 12*n+100+jacobian_work(n,s); }

std::vector<double> make_work(int size)
{
  try {
    return std::vector<double>(size,0.0);
  } catch (const std::bad_alloc &) {
    xpp::out_of_memory("the solver's work space");
  }
  return {};
}

/* a method that advances a number of steps of dt (odesol2.cpp's,
   volterra2.cpp's): a negative flag is a failure */
using StepFn = int (*)(Session &s, double *y, double *t, double dt, int nt, int neq, int *start, double *work);

class FixedStep final : public Solver {
public:
  FixedStep(const SolverInfo &info, Session &s, StepFn step, int work)
    : Solver(info,s), step_(step), work_(make_work(work)) {}
  Result<> step_by(const SolverStep &s) override
  {
    const int kflag=step_(session_,s.y,s.t,s.dt,s.steps,s.neq,s.start,work_.data());
    /* an implicit step's Newton iteration (backward Euler's, Volterra's) */
    const SolverWork::Stop &stop=session_.solver_work.stop;
    switch(kflag){
    case NEWTON_SINGULAR: return stopped("the Newton iteration's matrix is singular",*s.t,stop.variable,{});
    case NEWTON_NO_CONVERGENCE:
      return stopped(xpp::format("the Newton iteration did not converge in {} iterations",stop.iterations),*s.t,stop.variable,
                     {"eul_iter","eul_tol"});
    default: break;
    }
    return {};
  }
private:
  StepFn step_;
  std::vector<double> work_;
};

class Gear final : public Solver {
public:
  Gear(const SolverInfo &info, Session &s, int n) : Solver(info,s), work_(make_work(gear_work(n))), error_(static_cast<std::size_t>(n)) {}
  /* Gear starts afresh from 0 and continues from its order */
  void begin(int *start) override { if(*start==1)*start=0; }
  Result<> step_by(const SolverStep &s) override
  {
    NumericsSettings &num=session_.numerics;
    int kflag=0;
    gear(session_,s.neq,s.t,s.tout,s.y,num.min_step,num.max_step,num.relative_tolerance,2,error_.data(),&kflag,s.start,
         work_.data(),iwork_.data());
    /* on a failure, the variable whose error estimate is the largest */
    const int worst=kflag<0?largest_magnitude(error_.data(),s.neq):-1;
    switch(kflag){
    case -1: return stopped("the error test failed with the step at min_step",*s.t,worst,{"min_step","relative_tolerance"});
    case -2: return stopped("the method's order would exceed its highest, 6",*s.t,-1,{});
    case -3: return stopped("the corrector did not converge with the step at min_step",*s.t,worst,{"min_step","relative_tolerance"});
    case -4: return stopped("the error test failed repeatedly at order 1: the tolerance is too small for the rounding",*s.t,worst,
                            {"relative_tolerance"});
    default: break;
    }
    return {};
  }
private:
  std::vector<double> work_;
  std::vector<double> error_; /* gear's error estimates, one per variable */
  std::array<int,8> iwork_{}; /* its step's order and counters (ggear) */
};

/* stiff.cpp's adaptive Runge-Kutta (QualRK) and its stiff method */
class Adaptive final : public Solver {
public:
  Adaptive(const SolverInfo &info, Session &s, int work) : Solver(info,s), work_(make_work(work)) {}
  Result<> step_by(const SolverStep &s) override
  {
    NumericsSettings &num=session_.numerics;
    int kflag=0;
    adaptive(session_,s.y,s.neq,s.t,s.tout,num.relative_tolerance,s.hguess,num.min_step,work_.data(),&kflag,
             num.equilibria.jacobian_step,info().id,s.start);
    const SolverWork::Stop &stop=session_.solver_work.stop;
    switch(kflag){
    case ADAPTIVE_SINGULAR:
      return stopped("the step matrix is singular",stop.t,stop.variable,{"dt","jacobian"});
    case ADAPTIVE_STEP_UNDERFLOW: return stopped("the step size became too small to change t",stop.t,stop.variable,{"relative_tolerance"});
    case ADAPTIVE_BELOW_MIN_STEP:
      return stopped(xpp::format("the step size {} fell below min_step",stop.h),stop.t,stop.variable,{"min_step","relative_tolerance"});
    case ADAPTIVE_TOO_MANY_STEPS:
      return stopped(xpp::format("more than {} steps between two output times",ADAPTIVE_MAX_STEPS),stop.t,-1,{"dt","relative_tolerance"});
    case ADAPTIVE_TOO_MANY_TRIES:
      return stopped(xpp::format("the error test failed {} times with ever smaller steps",ADAPTIVE_MAX_TRIES),stop.t,stop.variable,
                     {"relative_tolerance"});
    default: return {};
    }
  }
private:
  std::vector<double> work_;
};

class Cvode final : public Solver {
public:
  Cvode(const SolverInfo &info, Session &s) : Solver(info,s) {}
  Result<> step_by(const SolverStep &s) override
  {
    NumericsSettings &num=session_.numerics;
    int kflag=0;
    cvode(session_,run_,s.start,s.y,s.t,s.neq,s.tout,&kflag,&num.relative_tolerance,&num.absolute_tolerance);
    if(kflag<0)
      /* the reason, then CVODE's own words; at the equation of the variable
         CVODE's error test or corrector failed at, if it names one */
      return stopped(cvode_error_text(session_,run_,kflag),*s.t,static_cast<int>(run_.error_var),
                     {"relative_tolerance","absolute_tolerance"},run_.error);
    return {};
  }
  void finish() override { end_cv(run_); }
private:
  /* CVODE's memory: an integration that stops without finish() (a step
     error, the Poincare map's) leaves it here, for the next start or
     this solver's end to free */
  CvodeRun run_;
};

class Rosenbrock final : public Solver {
public:
  Rosenbrock(const SolverInfo &info, Session &s, int n) : Solver(info,s), work_(make_work(rosenbrock_work(n,s))) {}
  Result<> step_by(const SolverStep &s) override
  {
    int kflag=0;
    rb23(session_,s.y,s.t,s.tout,s.start,s.neq,work_.data(),&kflag);
    const SolverWork::Stop &stop=session_.solver_work.stop;
    if(kflag<0)
      return stopped(xpp::format("the step size fell to the rounding limit {} with the error still above the tolerance",stop.h),
                     stop.t,stop.variable,{"relative_tolerance","absolute_tolerance"});
    return {};
  }
private:
  std::vector<double> work_;
};

template <StepFn F, int (*Work)(int, const Session &)>
std::unique_ptr<Solver> fixed_step(const SolverInfo &info, Session &s, int n)
{
  return std::make_unique<FixedStep>(info,s,F,Work(n,s));
}

std::unique_ptr<Solver> start_gear(const SolverInfo &info, Session &s, int n)
{
  return std::make_unique<Gear>(info,s,n);
}

template <int (*Work)(int, const Session &)>
std::unique_ptr<Solver> start_adaptive(const SolverInfo &info, Session &s, int n)
{
  return std::make_unique<Adaptive>(info,s,Work(n,s));
}

std::unique_ptr<Solver> start_cvode(const SolverInfo &info, Session &s, int)
{
  return std::make_unique<Cvode>(info,s);
}

std::unique_ptr<Solver> start_dormand_prince(const SolverInfo &info, Session &s, int n)
{
  return make_dormand_prince(info,s,n);
}

std::unique_ptr<Solver> start_rosenbrock(const SolverInfo &info, Session &s, int n)
{
  return std::make_unique<Rosenbrock>(info,s,n);
}

/* traits */
constexpr SolverTraits map{.discrete=true,.fixed_step=true,.wiener=true};
constexpr SolverTraits steps{.fixed_step=true,.wiener=true};
constexpr SolverTraits implicit_steps{.fixed_step=true,.newton=true,.wiener=true};
constexpr SolverTraits integral_steps{.fixed_step=true,.newton=true,.integral_history=true,.wiener=true};
constexpr SolverTraits paired_steps{.fixed_step=true,.paired_dimension=true,.wiener=true};
/* Gear: its step between min_step and max_step; QualRK and Stiff stop at
   min_step and have no bound above; CVODE and Rosenbrock read neither; the
   Dormand-Prince pair bounds its steps by max_step */
constexpr SolverTraits gear_steps{.relative_tolerance=true,.min_step=true,.max_step=true};
constexpr SolverTraits adaptive_steps{.relative_tolerance=true,.min_step=true};
constexpr SolverTraits stiff_steps{.relative_tolerance=true,.min_step=true,.stiff_step=true};
constexpr SolverTraits rel_abs_eighth{.relative_tolerance=true,.absolute_tolerance=true,.max_step=true,.eighth_order=true,.dense_output=true};
constexpr SolverTraits rel_abs{.relative_tolerance=true,.absolute_tolerance=true,.max_step=true,.dense_output=true};
constexpr SolverTraits rel_abs_banded{.relative_tolerance=true,.absolute_tolerance=true,.banded=true};

/* The registry: every method, by number. An explicit table rather than
   solvers that register themselves from static initializers, which a
   --gc-sections link would drop without a word. */
constexpr std::array<SolverInfo,method::COUNT> registry{{
  {method::DISCRETE,"Discrete","Discrete","discrete",map,fixed_step<discrete,standard_work>},
  {method::EULER,"Euler","Euler","euler",steps,fixed_step<euler,standard_work>},
  {method::MOD_EULER,"Mod. Euler","Mod. Euler","modified_euler",steps,fixed_step<mod_euler,standard_work>},
  {method::RK4,"Runge-Kutta","Runge-Kutta","runge_kutta",steps,fixed_step<rung_kut,standard_work>},
  {method::ADAMS,"Adams","Adams","adams",steps,fixed_step<adams,standard_work>},
  {method::GEAR,"Gear","Gear","gear",gear_steps,start_gear},
  {method::VOLTERRA,"Volterra","Volterra","volterra",integral_steps,fixed_step<volterra,implicit_work>},
  {method::BACKEUL,"Backward Euler","BackEul","backward_euler",implicit_steps,fixed_step<bak_euler,implicit_work>},
  {method::RKQS,"QualRK","Qual RK","qualrk",adaptive_steps,start_adaptive<standard_work>},
  {method::STIFF,"Stiff","Stiff","stiff",stiff_steps,start_adaptive<stiff_work>},
  {method::CVODE,"CVode","CVode","cvode",rel_abs_banded,start_cvode},
  {method::DP5,"DoPri5","DorPrin5","dopri5",rel_abs,start_dormand_prince},
  {method::DP83,"DoPri8(3)","DorPri8(3)","dopri8",rel_abs_eighth,start_dormand_prince},
  {method::RB23,"Rosenbrock","Rosenbrock","rosenbrock",rel_abs_banded,start_rosenbrock},
  {method::SYMPLECT,"Symplectic","Symplectic","symplectic",paired_steps,fixed_step<symplect3,standard_work>},
}};

constexpr bool in_method_order()
{
  for(int i=0;i<method::COUNT;i++)
    if(registry[i].id!=i)return false;
  return true;
}
static_assert(in_method_order(),"the registry's rows are in method order");

} // namespace

std::span<const SolverInfo> solvers()
{
  return registry;
}

Result<> Solver::advance(const SolverStep &step)
{
  try {
    return step_by(step);
  } catch (const JacobianFailed &failed) {
    return std::unexpected(failed.error);
  }
}

Result<> Solver::failed(std::string what) const
{
  return fail(info_.name,std::move(what),command_place());
}

Result<> Solver::stopped(std::string_view what, double t, int variable, std::initializer_list<std::string_view> settings,
                         std::string_view detail) const
{
  const Model &m=session_.model();
  std::string text=xpp::format("{}: {} at t = {}",info_.name,what,t);
  Place place;
  if(variable>=0&&variable<m.neq){
    const std::string &name=m.uvar_names[static_cast<std::size_t>(variable)];
    text+=xpp::format(", in {}",name);
    place=model_place(m,name);
  }
  if(settings.size()>0)text+=xpp::format(" ({})",named_settings(session_,settings));
  if(!detail.empty())text+=xpp::format("\n{}",detail);
  if(place.line==0)place=settings_place(m,settings);
  return fail(info_.name,std::move(text),std::move(place));
}

Result<> check_step_limits(const Session &s, const SolverInfo &info, double dt, double length)
{
  const double min_step=s.numerics.min_step;
  if(!info.traits.min_step)return {};
  auto refuse=[&](std::string what,std::initializer_list<std::string_view> settings)->Result<>{
    return fail(info.name,xpp::format("{}: {} ({})",info.name,what,named_settings(s,settings)),settings_place(s.model(),settings));
  };
  if(info.traits.max_step&&min_step>s.numerics.max_step)
    return refuse("min_step is larger than max_step, so no step size satisfies both",{"min_step","max_step"});
  if(std::fabs(dt)<min_step)
    return refuse(xpp::format("Dt = {} is below min_step, so no step fits between two output times",dt),{"min_step"});
  if(std::fabs(length)<min_step)
    return refuse(xpp::format("the run's length {} is shorter than min_step",length),{"min_step"});
  return {};
}

Result<> check_store_from(const Session &s, double start, double dt, double length)
{
  if(!s.numerics.storflag)return {};
  const double end=start+(dt<0?-length:length);
  if(stored_from(s.numerics.store_from,end,dt)||std::isnan(s.numerics.store_from))return {};
  /* a model whose events write rows (out_put) stores something past store_from or not */
  for(const Model::GlobalFlag &flag:s.model().flags)
    for(int e=0;e<flag.nevents;++e)
      if(flag.type[e]==2)return {};
  return fail("store_from",xpp::format("store_from is beyond the end of the run: it starts at t = {} and ends at t = {}, so no row would be stored ({})",
                                      start,end,named_settings(s,{"store_from"})),settings_place(s.model(),{"store_from"}));
}

const SolverInfo &solver_info(int m)
{
  assert(m>=0 && m<method::COUNT);
  return registry[m];
}

Result<method::Id> check_method(const Model &model, int id, Place place)
{
  if (id < 0 || id >= method::COUNT)
    return fail("method", xpp::format("{} is not a method's number (0 to {})", id, method::COUNT - 1), std::move(place));
  const SolverTraits &traits = registry[id].traits;
  if (traits.integral_history && model.nkernel == 0)
    return fail("method", "Volterra only for integral eqns", std::move(place));
  if (traits.paired_dimension && model.node % 2 != 0)
    return fail("method", "Symplectic is only for even dimensions", std::move(place));
  if (model.nkernel > 0 && !traits.integral_history)
    return fail("method", "a model with integral equations is integrated by Volterra", std::move(place));
  if (model.nwiener > 0 && !traits.wiener)
    return fail("method", "a model with Wiener noise requires a method that draws it each step", std::move(place));
  return static_cast<method::Id>(id);
}

Result<method::Id> method_by_key(const Model &model, std::string_view key, Place place)
{
  for (const SolverInfo &info : registry)
    if (info.key == key) return check_method(model, info.id, std::move(place));
  std::string keys;
  for (const SolverInfo &info : registry) keys += (keys.empty() ? "" : ", ") + std::string(info.key);
  return fail("method", xpp::format("`{}` is not a method ({})", key, keys), std::move(place));
}

Result<method::Id> pick_method(const Model &model, std::string_view text, Place place)
{
  /* XPPAUT's @ meth keys in registry order (load_eqn.c:1116). */
  constexpr std::string_view keys = "demragvbqsc582y";
  static_assert(keys.size() == registry.size());
  const SolverInfo *chosen = nullptr;
  const std::string name = lower_case(std::string(trim_blanks(text)));
  for (const SolverInfo &info : registry)
    if (equal_ignoring_case(name, info.name) || equal_ignoring_case(name, info.set_label) || name == info.key) chosen = &info;
  if (!chosen && name.size() == 1) {
    const auto i = keys.find(name[0]);
    if (i != std::string_view::npos) chosen = &registry[i];
  }
  /* Spellings used by shipped models and the long descriptive names. */
  if (name == "rk4") chosen = &registry[method::RK4];
  if (name == "disc") chosen = &registry[method::DISCRETE];
  if (name == "qualrk4") chosen = &registry[method::RKQS];
  if (name == "modified euler") chosen = &registry[method::MOD_EULER];
  if (name == "modeuler") chosen = &registry[method::MOD_EULER];
  if (!chosen) return fail("method", xpp::format("Unknown method `{}`", text), std::move(place));
  return check_method(model, chosen->id, std::move(place));
}

void start_solver(Session &s)
{
  const SolverInfo &info=solver_info(s.numerics.method);
  s.integrator.solver=info.start(info,s,s.solver_work.xpv.node+s.solver_work.xpv.nvec);
}

} // namespace xpp
