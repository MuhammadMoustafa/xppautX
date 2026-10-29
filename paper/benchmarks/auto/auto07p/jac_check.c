/* W269 (#326): checks py_s1bf.c's hand-written Jacobian (JAC = 1) against
 * central differences of its own f, at the start point and at points along a
 * spike's voltages. Not part of an AUTO run. From WSL:
 *   gcc -std=gnu17 -O2 -I$AUTO_DIR/include jac_check.c -lm -o jac_check && ./jac_check
 * Prints the largest relative difference per point; exits 1 above 1e-6. */
#include "py_s1bf.c"
#include <stdio.h>

int main(void)
{
  enum { N = 8 };
  double u0[N], par[36] = {0}, f[N], fp[N], fm[N], dfdu[N * N], dfdp[N * 36];
  integer icp[2] = {1, 11};
  stpnt(N, 0, u0, par);
  const double volts[] = {-18.68, -80, -67.7, -56.16 + 13, -56.16 + 40, -56.16 + 15, -35, -55, 0, 30};
  double worst_all = 0;
  for (unsigned k = 0; k < sizeof volts / sizeof *volts; ++k) {
    double u[N];
    for (int i = 0; i < N; ++i) u[i] = u0[i];
    u[0] = volts[k] + 1e-3 * k; /* also near erel's removable point (x = 0) */
    func(N, u, icp, par, 2, f, dfdu, dfdp);
    double worst = 0;
    for (int j = 0; j <= N; ++j) {
      double *x = j < N ? &u[j] : &par[0];
      const double h = 1e-6 * (fabs(*x) + 1), keep = *x;
      *x = keep + h; func(N, u, icp, par, 0, fp, 0, 0);
      *x = keep - h; func(N, u, icp, par, 0, fm, 0, 0);
      *x = keep;
      for (int i = 0; i < N; ++i) {
        const double fd = (fp[i] - fm[i]) / (2 * h), exact = j < N ? dfdu[i + N * j] : dfdp[i];
        const double rel = fabs(fd - exact) / (fabs(exact) + 1e-3 * (fabs(fd) + 1));
        if (rel > worst) worst = rel;
      }
    }
    printf("v = %8.3f: largest relative difference %.2e\n", u[0], worst);
    if (worst > worst_all) worst_all = worst;
  }
  return worst_all > 1e-6;
}
