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
struct DaeState {
  std::vector<double> work;
  std::vector<int> iwork;
  int status = 0;
  std::array<double, MAXDAE> svar_last{};
  /* since the integration started: the sign of the algebraic equations'
     Jacobian at the last solution (0 before the first Newton step), which
     a solution on the same branch keeps (W127: a sign change is a fold), and
     the time of the last solution, where a run that can go no further ends */
  int jac_sign = 0;
  std::optional<double> last_t;
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
