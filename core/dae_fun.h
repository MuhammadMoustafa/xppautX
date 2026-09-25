#ifndef _dae_fun_h_
#define _dae_fun_h_
#ifdef __cplusplus
extern "C" {
#endif

int add_svar(const char *name, const char *rhs);
int add_svar_names(void);
int add_aeqn(const char *rhs);
int compile_svars(void);
void reset_dae(void);
void set_init_guess(void);
void err_dae(void);
void init_dae_work(void);
void get_dae_fun(double *y, double *f);
void do_daes(void);
int solve_dae(void);
void get_new_guesses(void);

#ifdef __cplusplus
}
#endif
#endif
