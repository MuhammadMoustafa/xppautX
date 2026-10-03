#ifndef _cv_backend_h_
#define _cv_backend_h_

#include <memory>

/* The CVODE xppautX's driver (cv2.cpp) steps with: the vendored one
   (cv_vendored.cpp) or SUNDIALS' (cv_sundials.cpp, `make SUNDIALS=DIR`);
   a build links one of them. */
namespace xpp {

struct Session; /* session.h */
struct CvodeRun; /* cv2.h */

/* One integration's memory: CVODE's own and the state vector it steps */
struct CvodeMemory {
  CvodeMemory() = default;
  virtual ~CvodeMemory() = default;
  CvodeMemory(const CvodeMemory &) = delete;
  CvodeMemory &operator=(const CvodeMemory &) = delete;

  /* advance y (the state at *t) towards tout, CVODE's NORMAL mode: 0, or
     the failure flag cvode_error_text knows, with run's error, error_var
     and tolerance_factor set */
  virtual int step(CvodeRun &run, double tout, double *t, double *y) = 0;
};

/* a fresh integration from y at t (n equations), the tolerances CVODE's:
   rtol relative, atol absolute (CVODE keeps the pointers, which stay
   valid: they point into the Session's numerics) */
std::unique_ptr<CvodeMemory> start_cvode_memory(Session &s, const double *y, double t, int n, double *atol, double *rtol);

} // namespace xpp
#endif
