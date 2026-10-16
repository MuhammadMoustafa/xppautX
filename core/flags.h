#ifndef _flags_h_
#define _flags_h_
/* the global flags (events) of the Session s's Model (flags.cpp), C++ only */
#include <functional>
#include <span>
#include <string>
#include <vector>


namespace xpp {
struct Session; /* session.h */
struct CvodeRun; /* cv2.h */
}

void apply_dense_flags(xpp::Session &s, double *y, double time, int neq, std::span<const int> hits);
int compile_flags(xpp::Session &s);
int one_flag_step(xpp::Session &s, double *yold, double *ynew, int *istart, double told, double *tnew, int neq, double *frac);
/* a fixed-step method's step of length dt, repeated shorter back to each
   event the model's flags fire in it; step(dtt) takes one step of dtt */
void one_flag_steps(xpp::Session &s, double *y, int neq, double *tim, int *istart, double dt, const std::function<void(double)> &step);
int one_flag_step_gear(xpp::Session &s, int neq, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork);
int one_flag_step_rosen(xpp::Session &s, double *y, double *tstart, double tfinal, int *istart, int n, double *work, int *ierr);
int one_flag_step_cvode(xpp::Session &s, xpp::CvodeRun &run, int *command,double *y,double *t,int n,double tout,int *kflag,double *atol,double *rtol);
int one_flag_step_adap(xpp::Session &s, double *y, int neq, double *t, double tout, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart);
int one_flag_step_backeul(xpp::Session &s, double *y, double *t, double dt, int neq, double *yg, double *yp, double *yp2, double *errvec, double *jac, int *istart);

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
int add_global(xpp::Session &s, const char *cond, int sign, const std::vector<FlagEvent> &events);
#endif

