#ifndef _flags_h_
#define _flags_h_
#ifdef __cplusplus
extern "C" {
#endif


/* flags.c */
int compile_flags(void);
int one_flag_step(double *yold, double *ynew, int *istart, double told, double *tnew, int neq, double *s);
int one_flag_step_symp(double *y, double dt, double *work, int neq, double *tim, int *istart);
int one_flag_step_euler(double *y, double dt, double *work, int neq, double *tim, int *istart);
int one_flag_step_discrete(double *y, double dt, double *work, int neq, double *tim, int *istart);
int one_flag_step_heun(double *y, double dt, double *yval[2], int neq, double *tim, int *istart);
int one_flag_step_rk4(double *y, double dt, double *yval[3], int neq, double *tim, int *istart);
int one_flag_step_gear(int neq, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork);
int one_flag_step_rosen(double *y, double *tstart, double tfinal, int *istart, int n, double *work, int *ierr);
int one_flag_step_dp(int *istart, double *y, double *t, int n, double tout, double *tol, double *atol, int flag, int *kflag, double *work);
int one_flag_step_adap(double *y, int neq, double *t, double tout, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart);
int one_flag_step_backeul(double *y, double *t, double dt, int neq, double *yg, double *yp, double *yp2, double *ytemp, double *errvec, double *jac, int *istart);
int one_flag_step_cvode(int *command,double *y,double *t,int n,double tout,int *kflag,double *atol,double *rtol);

#ifdef __cplusplus
}

#include <string>
#include <vector>

/* one event of a flag: the name it sets, and its value's formula */
struct FlagEvent {
  std::string name,formula;
};
/* an .ode global's events, rest "{name=formula;name=formula;...}" (blanks
   and braces dropped, each name the text before its event's last =): 0,
   or 1 (said why, cond the flag's condition) when one names nothing,
   there are none or too many */
int split_events(const char *cond, const char *rest, std::vector<FlagEvent> &events);
/* a flag: when cond crosses 0 in the direction sign, each event sets its
   name to its formula; 0, or 1 (said why) */
int add_global(const char *cond, int sign, const std::vector<FlagEvent> &events);
#endif
#endif

