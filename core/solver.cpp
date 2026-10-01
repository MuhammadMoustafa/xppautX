/* The integration methods' registry and their Solvers (solver.h, W51):
   each Solver wraps its method's routine (odesol2.cpp, gear.cpp,
   stiff.cpp, dormpri.cpp, cv2.cpp, volterra2.cpp), owns the work memory
   it steps with and translates the routine's own flag into a
   Result<>. */
#include "solver.h"

#include <array>
#include <new>
#include <string>
#include <vector>

#include "cv2.h"
#include "dormpri.h"
#include "gear.h"
#include "odesol2.h"
#include "session.h"
#include "stiff.h"
#include "volterra2.h"
#include "xpp_mem.h" /* xpp::out_of_memory */

namespace xpp {
namespace {

/* the work memory a solver of n equations steps with */
int standard_work(int n) { return 30*n; }
int implicit_work(int n) { return 10*n+n*n+100; } /* backward Euler, Volterra */
int gear_work(int n) { return 30*n+n*n+100; }
int stiff_work(int n) { return 2*n*n+13*n+100; }
int rosenbrock_work(int n) { return 12*n+100+n*n; }

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
    gear(session_,s.neq,s.t,s.tout,s.y,num.hmin,num.hmax,num.toler,2,error.data(),&kflag,s.start,
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
    adaptive(session_,s.y,s.neq,s.t,s.tout,num.toler,s.hguess,num.hmin,work_.data(),&kflag,
             num.newt_err,info().id,s.start);
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
    cvode(session_,s.start,s.y,s.t,s.neq,s.tout,&kflag,&num.toler,&num.atoler);
    if(kflag<0)return failed(cvode_error_text(session_,kflag));
    return {};
  }
  void finish() override { end_cv(); }
};

/* dormpri.cpp's Dormand-Prince 5 and 8(3) */
class DormandPrince final : public Solver {
public:
  DormandPrince(const SolverInfo &info, Session &s, int n) : Solver(info,s), work_(make_work(standard_work(n))) {}
  Result<> advance(const SolverStep &s) override
  {
    NumericsSettings &num=session_.numerics;
    int kflag=0;
    dp(session_,s.start,s.y,s.t,s.neq,s.tout,&num.toler,&num.atoler,info().id==method::DP83,&kflag,
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
  Rosenbrock(const SolverInfo &info, Session &s, int n) : Solver(info,s), work_(make_work(rosenbrock_work(n))) {}
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

template <StepFn F, int (*Work)(int)>
std::unique_ptr<Solver> fixed_step(const SolverInfo &info, Session &s, int n)
{
  return std::make_unique<FixedStep>(info,s,F,Work(n));
}

std::unique_ptr<Solver> start_gear(const SolverInfo &info, Session &s, int n)
{
  return std::make_unique<Gear>(info,s,n);
}

template <int (*Work)(int)>
std::unique_ptr<Solver> start_adaptive(const SolverInfo &info, Session &s, int n)
{
  return std::make_unique<Adaptive>(info,s,Work(n));
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
  {method::VOLTERRA,"Volterra","Volterra",implicit_steps,fixed_step<volterra,implicit_work>},
  {method::BACKEUL,"BackEul","BackEul",implicit_steps,fixed_step<bak_euler,implicit_work>},
  {method::RKQS,"QualRK","Qual RK",step_tolerance,start_adaptive<standard_work>},
  {method::STIFF,"Stiff","Stiff",step_tolerance,start_adaptive<stiff_work>},
  {method::CVODE,"CVode","CVode",rel_abs_banded,start_cvode},
  {method::DP5,"DoPri5","DorPrin5",rel_abs,start_dormand_prince},
  {method::DP83,"DoPri8(3)","DorPri8(3)",rel_abs,start_dormand_prince},
  {method::RB23,"Rosenbrock","Rosenbrock",rel_abs_banded,start_rosenbrock},
  {method::SYMPLECT,"Symplectic","Symplectic",steps,fixed_step<symplect3,standard_work>},
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

const SolverInfo &solver_info(int m)
{
  if(m<0||m>=method::COUNT)return registry[method::RK4];
  return registry[m];
}

void start_solver(Session &s)
{
  const SolverInfo &info=solver_info(s.numerics.method);
  s.integrator.solver=info.start(info,s,s.solver_work.xpv.node+s.solver_work.xpv.nvec);
}

} // namespace xpp
