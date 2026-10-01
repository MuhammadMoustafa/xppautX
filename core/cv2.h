#ifndef _cv2_h_
#define _cv2_h_

#include <array>
#include <memory>
#include <string>

/* xpp's driver of the vendored CVODE (cvode.h, its own C API) */
namespace xpp {

struct Session; /* session.h */

/* cvode.h's OPT_SIZE, the length of its optional input and output arrays
   (cv2.cpp checks they agree) */
inline constexpr int cvode_opt_size = 40;

/* One CVODE integration's memory: CVODE's own and the state vector it
   steps (Memory, cv2.cpp), made when an integration starts and freed when
   it ends (end_cv), when the next one starts, or with the object,
   whichever comes first; so a run that stops on any path (a failed step,
   a step error, the Poincare map's) leaves nothing behind. The Cvode
   solver owns one (solver.cpp). */
struct CvodeRun {
  CvodeRun();
  ~CvodeRun();
  CvodeRun(const CvodeRun &) = delete;
  CvodeRun &operator=(const CvodeRun &) = delete;

  struct Memory;
  std::unique_ptr<Memory> memory; /* none between integrations */
  /* CVODE's optional inputs and outputs (cvode.h's iopt, ropt), which it
     points at while it runs: here, so that they outlive its memory */
  std::array<double, cvode_opt_size> ropt{};
  std::array<int, cvode_opt_size> iopt{};
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
