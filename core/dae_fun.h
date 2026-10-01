#ifndef _dae_fun_h_
#define _dae_fun_h_

#include <array>
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
};

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
