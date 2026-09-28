/* W72: compare the C library's functions with CORE-MATH's on the same
   inputs. Per function: a hash of the system libm's results, a hash of
   CORE-MATH's, how many of N inputs differ between the two, and ns/call
   of each. Inputs come from splitmix64, so every platform sees the same
   doubles. Build: cc -O2 -ffp-contract=off libmcmp.c cm_*.o -lm */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

double cr_sin(double), cr_cos(double), cr_tan(double), cr_asin(double),
    cr_acos(double), cr_atan(double), cr_sinh(double), cr_cosh(double),
    cr_tanh(double), cr_exp(double), cr_log(double), cr_log10(double),
    cr_erf(double), cr_erfc(double), cr_lgamma(double), cr_atan2(double, double),
    cr_pow(double, double), cr_hypot(double, double);

static uint64_t s = 0x9e3779b97f4a7c15u;
static uint64_t next(void)
{
  uint64_t z = (s += 0x9e3779b97f4a7c15u);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
  return z ^ (z >> 31);
}
/* uniform in [lo,hi) */
static double u(double lo, double hi)
{
  return lo + (hi - lo) * ((double)(next() >> 11) * 0x1.0p-53);
}
static uint64_t bits(double x)
{
  uint64_t b;
  memcpy(&b, &x, 8);
  return b;
}
static double now(void)
{
  struct timespec t;
  timespec_get(&t, TIME_UTC);
  return t.tv_sec + 1e-9 * t.tv_nsec;
}

#define N 1000000
static double xa[N], ya[N], ra[N];

typedef double (*F1)(double);
typedef double (*F2)(double, double);
static void run1(const char *name, F1 sys, F1 cr, double lo, double hi)
{
  s = 0x9e3779b97f4a7c15u;
  for (int i = 0; i < N; i++) xa[i] = u(lo, hi);
  uint64_t hs = 1469598103934665603u, hc = hs;
  long diff = 0;
  volatile F1 vs = sys, vc = cr;
  double t0 = now();
  for (int i = 0; i < N; i++) ra[i] = vs(xa[i]);
  double t1 = now();
  for (int i = 0; i < N; i++) hs = (hs ^ bits(ra[i])) * 1099511628211u;
  double t2 = now();
  for (int i = 0; i < N; i++) ya[i] = vc(xa[i]);
  double t3 = now();
  for (int i = 0; i < N; i++) {
    hc = (hc ^ bits(ya[i])) * 1099511628211u;
    diff += bits(ya[i]) != bits(ra[i]);
  }
  printf("%-7s sys %016llx cr %016llx differ %6ld  ns/call sys %5.1f cr %5.1f\n", name,
         (unsigned long long)hs, (unsigned long long)hc, diff, (t1 - t0) * 1e9 / N,
         (t3 - t2) * 1e9 / N);
}
static void run2(const char *name, F2 sys, F2 cr, double lo, double hi, double lo2, double hi2)
{
  static double za[N];
  s = 0x9e3779b97f4a7c15u;
  for (int i = 0; i < N; i++) { xa[i] = u(lo, hi); za[i] = u(lo2, hi2); }
  uint64_t hs = 1469598103934665603u, hc = hs;
  long diff = 0;
  volatile F2 vs = sys, vc = cr;
  double t0 = now();
  for (int i = 0; i < N; i++) ra[i] = vs(xa[i], za[i]);
  double t1 = now();
  for (int i = 0; i < N; i++) ya[i] = vc(xa[i], za[i]);
  double t2 = now();
  for (int i = 0; i < N; i++) {
    hs = (hs ^ bits(ra[i])) * 1099511628211u;
    hc = (hc ^ bits(ya[i])) * 1099511628211u;
    diff += bits(ya[i]) != bits(ra[i]);
  }
  printf("%-7s sys %016llx cr %016llx differ %6ld  ns/call sys %5.1f cr %5.1f\n", name,
         (unsigned long long)hs, (unsigned long long)hc, diff, (t1 - t0) * 1e9 / N,
         (t2 - t1) * 1e9 / N);
}

int main(void)
{
  run1("sin", sin, cr_sin, -10, 10);
  run1("cos", cos, cr_cos, -10, 10);
  run1("tan", tan, cr_tan, -10, 10);
  run1("asin", asin, cr_asin, -1, 1);
  run1("acos", acos, cr_acos, -1, 1);
  run1("atan", atan, cr_atan, -20, 20);
  run1("sinh", sinh, cr_sinh, -20, 20);
  run1("cosh", cosh, cr_cosh, -20, 20);
  run1("tanh", tanh, cr_tanh, -5, 5);
  run1("exp", exp, cr_exp, -30, 30);
  run1("log", log, cr_log, 1e-6, 100);
  run1("log10", log10, cr_log10, 1e-6, 100);
  run1("erf", erf, cr_erf, -4, 4);
  run1("erfc", erfc, cr_erfc, -4, 8);
  run1("lgamma", lgamma, cr_lgamma, 0.01, 50);
  run2("atan2", atan2, cr_atan2, -10, 10, -10, 10);
  run2("pow", pow, cr_pow, 0.01, 10, -5, 5);
  run2("hypot", hypot, cr_hypot, -10, 10, -10, 10);
  return 0;
}
