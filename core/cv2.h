#ifndef _cv2_h_
#define _cv2_h_

#include <memory>
#include <string>

/* xpp's driver of CVODE (cv_backend.h: the vendored one or SUNDIALS') */
namespace xpp {

struct Session; /* session.h */
struct CvodeMemory; /* cv_backend.h */

/* One CVODE integration's memory (cv_backend.h), made when an
   integration starts and freed when it ends (end_cv), when the next one
   starts, or with the object, whichever comes first; so a run that stops
   on any path (a failed step, a step error, the Poincare map's) leaves
   nothing behind. The Cvode solver owns one (solver.cpp). */
struct CvodeRun {
  CvodeRun();
  ~CvodeRun();
  CvodeRun(const CvodeRun &) = delete;
  CvodeRun &operator=(const CvodeRun &) = delete;

  std::unique_ptr<CvodeMemory> memory; /* none between integrations */
  /* why its last integration failed, as CVODE words it, the variable (its
     index) CVODE's error test or corrector failed at, -1 for none, and
     the factor CVODE suggests the tolerances be scaled by when they ask
     for more accuracy than the machine has */
  std::string error;
  long error_var = -1;
  double tolerance_factor = 1;
};

/* the integration of run has ended: its memory freed (nothing when it has
   none) */
void end_cv(CvodeRun &run);
int cvode(Session &s, CvodeRun &run, int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol);
int ccvode(Session &s, CvodeRun &run, int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol);

/* what a CVODE failure flag of run means, for the user (empty: nothing to
   say) */
std::string cvode_error_text(const Session &s, const CvodeRun &run, int kflag);

} // namespace xpp
#endif
