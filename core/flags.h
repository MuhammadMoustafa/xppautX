#pragma once
/* the global flags (events) of the Session s's Model (flags.cpp), C++ only */
#include <functional>
#include <span>
#include <string>
#include <vector>
#include "model_declarations.h"


namespace xpp {
struct RunParts; /* run_parts.h */
struct CvodeRun; /* cv2.h */
}

void apply_dense_flags(xpp::RunParts &s, double *y, double time, int neq, std::span<const int> hits);
int one_flag_step(xpp::RunParts &s, double *yold, double *ynew, int *istart, double told, double *tnew, int neq, double *frac);
/* a fixed-step method's step of length dt, repeated shorter back to each
   event the model's flags fire in it; step(dtt) takes one step of dtt */
void one_flag_steps(xpp::RunParts &s, double *y, int neq, double *tim, int *istart, double dt, const std::function<void(double)> &step);
int one_flag_step_gear(xpp::RunParts &s, int neq, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork);
int one_flag_step_rosen(xpp::RunParts &s, double *y, double *tstart, double tfinal, int *istart, int n, double *work, int *ierr);
int one_flag_step_cvode(xpp::RunParts &s, xpp::CvodeRun &run, int *command,double *y,double *t,int n,double tout,int *kflag,double *atol,double *rtol);
int one_flag_step_adap(xpp::RunParts &s, double *y, int neq, double *t, double tout, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart);
int one_flag_step_backeul(xpp::RunParts &s, double *y, double *t, double dt, int neq, double *yg, double *yp, double *yp2, double *errvec, double *jac, int *istart);


