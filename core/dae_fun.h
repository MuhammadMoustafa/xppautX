#ifndef _dae_fun_h_
#define _dae_fun_h_
#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}

namespace xpp {
struct Session; /* session.h */
}

int add_svar(xpp::Session &s, const char *name, const char *rhs);
int add_svar_names(xpp::Session &s);
int add_aeqn(xpp::Session &s, const char *rhs);
int compile_svars(xpp::Session &s);
void reset_dae(xpp::Session &s);
void set_init_guess(xpp::Session &s);
void init_dae_work(xpp::Session &s);
void get_dae_fun(xpp::Session &s, double *y, double *f);
void do_daes(xpp::Session &s);
int solve_dae(xpp::Session &s);
void get_new_guesses(xpp::Session &s);
#endif
#endif
