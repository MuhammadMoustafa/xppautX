#ifndef _dae_fun_h_
#define _dae_fun_h_

#include <array>
#include <optional>
#include <vector>

#include "xpplim.h"

namespace xpp {

struct Session; /* session.h */

/* the algebraic variables' solver (dae_fun.cpp), a Session's
   (session.h): its work memory, how the last solve went, and each
   variable's last solution (xpp::Model has the definitions), the next
   solve's first guess */
/* the branch of solutions one run of the integrator follows (W127): the
   sign of the algebraic equations' Jacobian at the run's last solution (0
   before its first Newton step), which a solution on the same branch
   keeps (a change is a fold), and the time of that solution, where a run
   that can go no further ends */
struct DaeRunBranch {
  int jac_sign = 0;
  std::optional<double> last_t;
};

struct DaeState {
  std::vector<double> work;
  std::vector<int> iwork;
  int status = 0;
  std::array<double, MAXDAE> svar_last{};
  /* the branch the run in progress follows; none outside a run (DaeRun) */
  std::optional<DaeRunBranch> run;
};

/* One run of the integrator (integrate()): while it lives, solve_dae
   follows the branch of the run's first solution, from step to step.
   Every other solve (Sing pts, nullclines, a direction field, AUTO, the
   next run) starts without a branch, so no operation inherits another's.
   A run inside a run (none today) keeps the outer one's branch. */
class DaeRun {
public:
  explicit DaeRun(Session &s);
  ~DaeRun();
  DaeRun(const DaeRun &) = delete;
  DaeRun &operator=(const DaeRun &) = delete;
private:
  Session &s_;
  bool outer_;
};

/* solve_dae's result: solved, or why not (the run stops there) */
constexpr int DAE_SOLVED = 1;
constexpr int DAE_SINGULAR = -1;
constexpr int DAE_NO_CONVERGENCE = -2;
constexpr int DAE_OUT_OF_BOUNDS = -3;
constexpr int DAE_FOLD = -4;

int add_svar(Session &s, const char *name, const char *rhs);
int add_svar_names(Session &s);
int add_aeqn(Session &s, const char *rhs);
int compile_svars(Session &s);
void reset_dae(Session &s);
void set_init_guess(Session &s);
void init_dae_work(Session &s);
void get_dae_fun(Session &s, double *y, double *f);
void do_daes(Session &s);
int solve_dae(Session &s);
void get_new_guesses(Session &s);

} // namespace xpp
#endif
