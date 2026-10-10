/* The integration methods' registry and their Solvers (solver.h, W51):
   each Solver wraps its method's routine (odesol2.cpp, gear.cpp,
   stiff.cpp, dormpri.cpp, cv2.cpp, volterra2.cpp), owns the work memory
   it steps with and translates the routine's own flag into a
   Result<>. */
#include "solver.h"

#include <algorithm>
#include <array>
#include <cassert>
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
  Result<> advance(const SolverStep &s) override
  {
    int kflag=step_(session_,s.y,s.t,s.dt,s.steps,s.neq,s.start,work_.data());
    switch(kflag){
    case -1: return failed("Singular Jacobian");
    case -2: return failed("Too many iterates");
    default: break;
    }
    if(kflag<0)return failed("");
    return {};
  }
private:
  StepFn step_;
  std::vector<double> work_;
};

class Gear final : public Solver {
public:
  Gear(const SolverInfo &info, Session &s, int n) : Solver(info,s), work_(make_work(gear_work(n))) {}
  /* Gear starts afresh from 0 and continues from its order */
  void begin(int *start) override { if(*start==1)*start=0; }
  Result<> advance(const SolverStep &s) override
  {
    NumericsSettings &num=session_.numerics;
    std::array<double,MAXODE> error{};
    int kflag=0;
    gear(session_,s.neq,s.t,s.tout,s.y,num.min_step,num.max_step,num.tolerance,2,error.data(),&kflag,s.start,
         work_.data(),iwork_.data());
    switch(kflag){
    case -1: return failed("kflag=-1: minimum step too big");
    case -2: return failed("kflag=-2: required order too big");
    case -3: return failed("kflag=-3: minimum step too big");
    case -4: return failed("kflag=-4: tolerance too small");
    default: break;
    }
    if(kflag<0)return failed("");
    return {};
  }
private:
  std::vector<double> work_;
  std::array<int,8> iwork_{}; /* its step's order and counters (ggear) */
};

/* stiff.cpp's adaptive Runge-Kutta (QualRK) and its stiff method */
class Adaptive final : public Solver {
public:
  Adaptive(const SolverInfo &info, Session &s, int work) : Solver(info,s), work_(make_work(work)) {}
  Result<> advance(const SolverStep &s) override
  {
    NumericsSettings &num=session_.numerics;
    int kflag=0;
    adaptive(session_,s.y,s.neq,s.t,s.tout,num.tolerance,s.hguess,num.min_step,work_.data(),&kflag,
             num.singpt_jacobian_epsilon,info().id,s.start);
    switch(kflag){
    case 0: return {};
    case 1: return failed("stepsize is close to 0");
    case 2: return failed("Step size too small");
    case 3: return failed("Too many steps");
    case 4: return failed("exceeded MAXTRY in stiff");
    case -1: return failed("singular jacobian encountered");
    default: return failed("");
    }
  }
private:
  std::vector<double> work_;
};

class Cvode final : public Solver {
public:
  Cvode(const SolverInfo &info, Session &s) : Solver(info,s) {}
  Result<> advance(const SolverStep &s) override
  {
    NumericsSettings &num=session_.numerics;
    int kflag=0;
    cvode(session_,run_,s.start,s.y,s.t,s.neq,s.tout,&kflag,&num.tolerance,&num.abs_tolerance);
    if(kflag<0){
      /* the short reason, then CVODE's own words; at the equation of the
         variable CVODE's error test or corrector failed at, if it names one */
      std::string what=cvode_error_text(session_,run_,kflag);
      if(!run_.error.empty())what+="\n"+run_.error;
      const Model &m=session_.model();
      if(run_.error_var>=0&&run_.error_var<m.neq)
        return failed(std::move(what),model_place(m,m.uvar_names[static_cast<size_t>(run_.error_var)]));
      return failed(std::move(what));
    }
    return {};
  }
  void finish() override { end_cv(run_); }
private:
  /* CVODE's memory: an integration that stops without finish() (a step
     error, the Poincare map's) leaves it here, for the next start or
     this solver's end to free */
  CvodeRun run_;
};

/* dormpri.cpp's Dormand-Prince 5 and 8(3) */
class DormandPrince final : public Solver {
public:
  DormandPrince(const SolverInfo &info, Session &s, int n) : Solver(info,s), work_(make_work(standard_work(n,s))) {}
  Result<> advance(const SolverStep &s) override
  {
    NumericsSettings &num=session_.numerics;
    int kflag=0;
    dp(session_,s.start,s.y,s.t,s.neq,s.tout,&num.tolerance,&num.abs_tolerance,info().traits.eighth_order,&kflag,
       work_.data());
    switch(kflag){
    case -1: return failed("Input is not consistent");
    case -2: return failed("Larger nmax needed");
    case -3: return failed("Step size too small");
    case -4: return failed("Problem became stiff");
    default: break;
    }
    if(kflag<0)return failed(""); /* the events' own warning said it */
    return {};
  }
private:
  std::vector<double> work_;
};

class Rosenbrock final : public Solver {
public:
  Rosenbrock(const SolverInfo &info, Session &s, int n) : Solver(info,s), work_(make_work(rosenbrock_work(n,s))) {}
  Result<> advance(const SolverStep &s) override
  {
    int kflag=0;
    rb23(session_,s.y,s.t,s.tout,s.start,s.neq,work_.data(),&kflag);
    if(kflag<0)return failed("Step size too small");
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
  return std::make_unique<DormandPrince>(info,s,n);
}

std::unique_ptr<Solver> start_rosenbrock(const SolverInfo &info, Session &s, int n)
{
  return std::make_unique<Rosenbrock>(info,s,n);
}

/* traits */
constexpr SolverTraits map{.discrete=true,.fixed_step=true};
constexpr SolverTraits steps{.fixed_step=true};
constexpr SolverTraits implicit_steps{.fixed_step=true,.newton=true};
constexpr SolverTraits integral_steps{.fixed_step=true,.newton=true,.integral_history=true};
constexpr SolverTraits paired_steps{.fixed_step=true,.paired_dimension=true};
constexpr SolverTraits stiff_tolerance{.step_tolerance=true,.stiff_step=true};
constexpr SolverTraits rel_abs_eighth{.rel_abs_tolerance=true,.eighth_order=true};
constexpr SolverTraits step_tolerance{.step_tolerance=true};
constexpr SolverTraits rel_abs{.rel_abs_tolerance=true};
constexpr SolverTraits rel_abs_banded{.rel_abs_tolerance=true,.banded=true};

/* The registry: every method, by number. An explicit table rather than
   solvers that register themselves from static initializers, which a
   --gc-sections link would drop without a word. */
constexpr std::array<SolverInfo,method::COUNT> registry{{
  {method::DISCRETE,"Discrete","Discrete",map,fixed_step<discrete,standard_work>},
  {method::EULER,"Euler","Euler",steps,fixed_step<euler,standard_work>},
  {method::MOD_EULER,"Mod. Euler","Mod. Euler",steps,fixed_step<mod_euler,standard_work>},
  {method::RK4,"Runge-Kutta","Runge-Kutta",steps,fixed_step<rung_kut,standard_work>},
  {method::ADAMS,"Adams","Adams",steps,fixed_step<adams,standard_work>},
  {method::GEAR,"Gear","Gear",step_tolerance,start_gear},
  {method::VOLTERRA,"Volterra","Volterra",integral_steps,fixed_step<volterra,implicit_work>},
  {method::BACKEUL,"BackEul","BackEul",implicit_steps,fixed_step<bak_euler,implicit_work>},
  {method::RKQS,"QualRK","Qual RK",step_tolerance,start_adaptive<standard_work>},
  {method::STIFF,"Stiff","Stiff",stiff_tolerance,start_adaptive<stiff_work>},
  {method::CVODE,"CVode","CVode",rel_abs_banded,start_cvode},
  {method::DP5,"DoPri5","DorPrin5",rel_abs,start_dormand_prince},
  {method::DP83,"DoPri8(3)","DorPri8(3)",rel_abs_eighth,start_dormand_prince},
  {method::RB23,"Rosenbrock","Rosenbrock",rel_abs_banded,start_rosenbrock},
  {method::SYMPLECT,"Symplectic","Symplectic",paired_steps,fixed_step<symplect3,standard_work>},
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

Result<> Solver::failed(std::string what) const
{
  return fail(info_.name,std::move(what),command_place());
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
  return static_cast<method::Id>(id);
}

Result<method::Id> pick_method(const Model &model, std::string_view text, Place place)
{
  /* XPPAUT's @ meth keys in registry order (load_eqn.c:1116). */
  constexpr std::string_view keys = "demragvbqsc582y";
  static_assert(keys.size() == registry.size());
  const SolverInfo *chosen = nullptr;
  const std::string name = lower_case(std::string(trim_blanks(text)));
  for (const SolverInfo &info : registry)
    if (equal_ignoring_case(name, info.name) || equal_ignoring_case(name, info.set_label)) chosen = &info;
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
  if (name == "backward euler") chosen = &registry[method::BACKEUL];
  if (!chosen) return fail("method", xpp::format("Unknown method `{}`", text), std::move(place));
  return check_method(model, chosen->id, std::move(place));
}

void start_solver(Session &s)
{
  const SolverInfo &info=solver_info(s.numerics.method);
  s.integrator.solver=info.start(info,s,s.solver_work.xpv.node+s.solver_work.xpv.nvec);
}

} // namespace xpp
