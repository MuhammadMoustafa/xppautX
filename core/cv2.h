#ifndef _cv2_h_
#define _cv2_h_
#ifdef __cplusplus
extern "C" {
#endif


void end_cv(void);


#ifdef __cplusplus
}

#include <string>


namespace xpp {
struct Session; /* session.h */
}

void start_cv(xpp::Session &s, double *y, double t, int n, double tout, double *atol, double *rtol);
int cvode(xpp::Session &s, int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol);
int ccvode(xpp::Session &s, int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol);

/* what a CVODE failure flag means, for the user (empty: nothing to say) */
std::string cvode_error_text(const xpp::Session &s, int kflag);
#endif
#endif

