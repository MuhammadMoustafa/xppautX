#ifndef _cv2_h_
#define _cv2_h_

#include <string>

/* xpp's driver of the vendored CVODE (cvode.h, its own C API) */
namespace xpp {

struct Session; /* session.h */

void end_cv();
void start_cv(Session &s, double *y, double t, int n, double tout, double *atol, double *rtol);
int cvode(Session &s, int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol);
int ccvode(Session &s, int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol);

/* what a CVODE failure flag means, for the user (empty: nothing to say) */
std::string cvode_error_text(const Session &s, int kflag);

} // namespace xpp
#endif
