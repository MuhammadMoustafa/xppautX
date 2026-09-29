#ifndef _cv2_h_
#define _cv2_h_
#ifdef __cplusplus
extern "C" {
#endif


/* cv2.c */
void start_cv(double *y, double t, int n, double tout, double *atol, double *rtol);
void end_cv(void);
int cvode(int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol);
int ccvode(int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol);


#ifdef __cplusplus
}

#include <string>

/* what a CVODE failure flag means, for the user (empty: nothing to say) */
std::string cvode_error_text(int kflag);
#endif
#endif

