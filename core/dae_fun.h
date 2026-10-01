#ifndef _dae_fun_h_
#define _dae_fun_h_

namespace xpp {

struct Session; /* session.h */

int add_svar(Session &s, const char *name, const char *rhs);
int add_svar_names(Session &s);
int add_aeqn(Session &s, const char *rhs);
int compile_svars(Session &s);
void reset_dae();
void set_init_guess(Session &s);
void init_dae_work(Session &s);
void get_dae_fun(Session &s, double *y, double *f);
void do_daes(Session &s);
int solve_dae(Session &s);
void get_new_guesses(Session &s);

} // namespace xpp
#endif
