#pragma once


namespace xpp {

struct RunParts; /* run_parts.h */

int gear(RunParts &s, int n, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork);
int gear_step(RunParts &s, int n, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork);

} // namespace xpp
