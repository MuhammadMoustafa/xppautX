#pragma once


namespace xpp {

struct Session; /* session.h */

int gear(Session &s, int n, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork);
int ggear(Session &s, int n, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork);

} // namespace xpp
