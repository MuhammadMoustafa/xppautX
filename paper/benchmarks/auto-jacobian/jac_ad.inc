/* W252 (#308), test build only, never merged: PY_S1Bf's exact Jacobian by forward-mode
   automatic differentiation, the stand-in for a symbolically differentiated, compiled
   model (the same derivatives of the same formulas, in one pass). Included into
   core/autpp.cpp by apply_jacobian.py after hand_rhs.inc. Every value is computed with
   the operations of hand_rhs.inc's hand_core (xpp::math exp/pow), so f is
   bit-identical; the derivatives are exact for each formula's branch (erel's series
   and quotient branches, ode_divisor's guard taken as the identity). */
#include <array>

namespace {

constexpr int JAC_N = 9; /* d/dy[0..7], d/dIapp */

struct Dual {
  double v;
  std::array<double, JAC_N> d;
};

Dual dconst(double c) { return {c, {}}; }
Dual operator+(const Dual &a, const Dual &b) { Dual r{a.v + b.v, {}}; for (int k = 0; k < JAC_N; k++) r.d[k] = a.d[k] + b.d[k]; return r; }
Dual operator-(const Dual &a, const Dual &b) { Dual r{a.v - b.v, {}}; for (int k = 0; k < JAC_N; k++) r.d[k] = a.d[k] - b.d[k]; return r; }
Dual operator-(const Dual &a) { Dual r{-a.v, {}}; for (int k = 0; k < JAC_N; k++) r.d[k] = -a.d[k]; return r; }
Dual operator*(const Dual &a, const Dual &b) { Dual r{a.v * b.v, {}}; for (int k = 0; k < JAC_N; k++) r.d[k] = a.d[k] * b.v + a.v * b.d[k]; return r; }
Dual operator/(const Dual &a, const Dual &b)
{
  Dual r{a.v / b.v, {}};
  for (int k = 0; k < JAC_N; k++) r.d[k] = (a.d[k] * b.v - a.v * b.d[k]) / (b.v * b.v);
  return r;
}
Dual operator+(double a, const Dual &b) { return dconst(a) + b; }
Dual operator+(const Dual &a, double b) { Dual r = a; r.v = a.v + b; return r; }
Dual operator-(double a, const Dual &b) { return dconst(a) - b; }
Dual operator-(const Dual &a, double b) { Dual r = a; r.v = a.v - b; return r; }
Dual operator*(double a, const Dual &b) { Dual r{a * b.v, {}}; for (int k = 0; k < JAC_N; k++) r.d[k] = a * b.d[k]; return r; }
Dual operator*(const Dual &a, double b) { Dual r{a.v * b, {}}; for (int k = 0; k < JAC_N; k++) r.d[k] = a.d[k] * b; return r; }
Dual operator/(const Dual &a, double b) { Dual r{a.v / b, {}}; for (int k = 0; k < JAC_N; k++) r.d[k] = a.d[k] / b; return r; }
Dual operator/(double a, const Dual &b) { return dconst(a) / b; }

Dual dexp(const Dual &a)
{
  const double e = xpp::math::exp(a.v);
  Dual r{e, {}};
  for (int k = 0; k < JAC_N; k++) r.d[k] = e * a.d[k];
  return r;
}
Dual dpow(const Dual &a, int n) /* a^n, n > 0: d = n a^(n-1) da */
{
  const double p = xpp::math::pow(a.v, n), q = n * xpp::math::pow(a.v, n - 1);
  Dual r{p, {}};
  for (int k = 0; k < JAC_N; k++) r.d[k] = q * a.d[k];
  return r;
}
Dual ddiv_guard(const Dual &a) { Dual r = a; if (r.v == 0.0) r.v = 2.23e-15; return r; }
Dual derel(const Dual &x)
{
  return std::fabs(x.v) < 1e-05 ? 1 + x / 2 + x * x / 6 + x * x * x / 24 : (dexp(x) - 1) / ddiv_guard(x);
}

/* f, dfdu (column-major, dfdu[i + j*8] = df_i/dy_j) and df/dIapp, from one pass */
void jac_ad(const double *yv, double iappv, double *f, double *dfdu, double *dfdiapp)
{
  const double C = 200, gNa = 11.2, gKdr = 1.2, gL = 0.00549494, gM = 0.015, gA = 0, gNaP = 0;
  const double EL = -67.73070881, ENa = 50, EK = -100, VT = -56.16;
  const double phiF = 0.6020561713201731, phiM = 0.8465535452071854, tauM = 608;
  Dual y[8];
  for (int i = 0; i < 8; i++) {
    y[i] = dconst(yv[i]);
    y[i].d[i] = 1;
  }
  Dual Iapp = dconst(iappv);
  Iapp.d[8] = 1;
  const Dual &v = y[0], &m = y[1], &h = y[2], &n = y[3], &pM = y[4], &aA = y[5], &bA = y[6], &pNa = y[7];
  const Dual vv = v - VT;
  const Dual am = 1.28 / ddiv_guard(derel((13 - vv) / 4));
  const Dual bm = 1.4 / ddiv_guard(derel((vv - 40) / 5));
  const Dual ah = 0.128 * dexp((17 - vv) / 18);
  const Dual bh = 4 / (1 + dexp((40 - vv) / 5));
  const Dual an = 0.16 / ddiv_guard(derel((15 - vv) / 5));
  const Dual bn = 0.5 * dexp((10 - vv) / 40);
  const Dual minf = am / ddiv_guard(am + bm);
  const Dual hinf = ah / ddiv_guard(ah + bh);
  const Dual ninf = an / ddiv_guard(an + bn);
  const Dual pinf = 1 / (1 + dexp(-(v + 35) / 10));
  const Dual taup = tauM / ddiv_guard(3.3 * dexp((v + 35) / 20) + dexp(-(v + 35) / 20)) / phiM;
  const Dual ainf = 1 / (1 + dexp(-(v + 50) / 20));
  const Dual binf = 1 / (1 + dexp((v + 80) / 6));
  const Dual pninf = 1 / (1 + dexp(-(v + 55) / 6.2));
  const Dual INa = 1000 * gNa * dpow(m, 3) * h * (v - ENa);
  const Dual IKdr = 1000 * gKdr * dpow(n, 4) * (v - EK);
  const Dual IL = 1000 * gL * (v - EL);
  const Dual IM = 1000 * gM * pM * (v - EK);
  const Dual IA = 1000 * gA * dpow(aA, 3) * bA * (v - EK);
  const Dual INaP = 1000 * gNaP * pNa * (v - ENa);
  const Dual out[8] = {
      (Iapp - INa - IKdr - IL - IM - IA - INaP) / C,
      phiF * (am + bm) * (minf - m),
      phiF * (ah + bh) * (hinf - h),
      phiF * (an + bn) * (ninf - n),
      (pinf - pM) / ddiv_guard(taup),
      (ainf - aA) / 2,
      (binf - bA) / 20,
      (pninf - pNa) / 5,
  };
  for (int i = 0; i < 8; i++) {
    f[i] = out[i].v;
    for (int j = 0; j < 8; j++) dfdu[i + j * 8] = out[i].d[j];
    dfdiapp[i] = out[i].d[8];
  }
}

} // namespace
