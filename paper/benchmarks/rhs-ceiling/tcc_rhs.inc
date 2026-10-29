/* W250 (#306), test build only, never merged: PY_S1Bf's right-hand side as the C a
   model compiler would generate, compiled in-process by TinyCC (libtcc) the first
   time it is called. XPP_HAND_RHS=3 picks it (hand_rhs.inc's other modes unchanged).
   The C has the interpreter's operations in its order, like hand_rhs.inc, and
   calls xpp::math's exp/pow through symbols given to TinyCC, so the outputs must be
   bit-identical to the interpreter's. Compile time goes to stderr once. */
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include "libtcc.h"

namespace {

const char tcc_model_source[] = R"C(
extern double xexp(double);
extern double xpow(double, double);
static double ode_divisor(double value) { return value != 0.0 ? value : 2.23e-15; }
static double erel(double x)
{
  double ax = x < 0 ? -x : x;
  return ax < 1e-05 ? 1 + x / 2 + x * x / 6 + x * x * x / 24 : (xexp(x) - 1) / ode_divisor(x);
}
void model_rhs(double t, const double *y, double *ydot, double *fixed, double Iapp)
{
  const double C = 200, gNa = 11.2, gKdr = 1.2, gL = 0.00549494, gM = 0.015, gA = 0, gNaP = 0;
  const double EL = -67.73070881, ENa = 50, EK = -100, VT = -56.16;
  const double phiF = 0.6020561713201731, phiM = 0.8465535452071854, tauM = 608;
  double v = y[0], m = y[1], h = y[2], n = y[3], pM = y[4], aA = y[5], bA = y[6], pNa = y[7];
  double vv = v - VT;
  double am = 1.28 / ode_divisor(erel((13 - vv) / 4));
  double bm = 1.4 / ode_divisor(erel((vv - 40) / 5));
  double ah = 0.128 * xexp((17 - vv) / 18);
  double bh = 4 / (1 + xexp((40 - vv) / 5));
  double an = 0.16 / ode_divisor(erel((15 - vv) / 5));
  double bn = 0.5 * xexp((10 - vv) / 40);
  double minf = am / ode_divisor(am + bm);
  double hinf = ah / ode_divisor(ah + bh);
  double ninf = an / ode_divisor(an + bn);
  double pinf = 1 / (1 + xexp(-(v + 35) / 10));
  double taup = tauM / ode_divisor(3.3 * xexp((v + 35) / 20) + xexp(-(v + 35) / 20)) / ode_divisor(phiM);
  double ainf = 1 / (1 + xexp(-(v + 50) / 20));
  double binf = 1 / (1 + xexp((v + 80) / 6));
  double pninf = 1 / (1 + xexp(-(v + 55) / 6.2));
  double INa = 1000 * gNa * xpow(m, 3) * h * (v - ENa);
  double IKdr = 1000 * gKdr * xpow(n, 4) * (v - EK);
  double IL = 1000 * gL * (v - EL);
  double IM = 1000 * gM * pM * (v - EK);
  double IA = 1000 * gA * xpow(aA, 3) * bA * (v - EK);
  double INaP = 1000 * gNaP * pNa * (v - ENa);
  fixed[0] = vv; fixed[1] = am; fixed[2] = bm; fixed[3] = ah; fixed[4] = bh; fixed[5] = an; fixed[6] = bn;
  fixed[7] = minf; fixed[8] = hinf; fixed[9] = ninf; fixed[10] = pinf; fixed[11] = taup; fixed[12] = ainf;
  fixed[13] = binf; fixed[14] = pninf; fixed[15] = INa; fixed[16] = IKdr; fixed[17] = IL; fixed[18] = IM;
  fixed[19] = IA; fixed[20] = INaP;
  ydot[0] = (Iapp - INa - IKdr - IL - IM - IA - INaP) / ode_divisor(C);
  ydot[1] = phiF * (am + bm) * (minf - m);
  ydot[2] = phiF * (ah + bh) * (hinf - h);
  ydot[3] = phiF * (an + bn) * (ninf - n);
  ydot[4] = (pinf - pM) / ode_divisor(taup);
  ydot[5] = (ainf - aA) / 2;
  ydot[6] = (binf - bA) / 20;
  ydot[7] = (pninf - pNa) / 5;
}
)C";

using model_fn = void (*)(double, const double *, double *, double *, double);

double tcc_exp(double x) { return xpp::math::exp(x); }
double tcc_pow(double x, double y) { return xpp::math::pow(x, y); }

model_fn tcc_compile()
{
  const auto t0 = std::chrono::steady_clock::now();
  TCCState *st = tcc_new();
  tcc_set_options(st, "-nostdlib");
  tcc_set_output_type(st, TCC_OUTPUT_MEMORY);
  tcc_add_symbol(st, "xexp", reinterpret_cast<const void *>(&tcc_exp));
  tcc_add_symbol(st, "xpow", reinterpret_cast<const void *>(&tcc_pow));
  if (tcc_compile_string(st, tcc_model_source) != 0 || tcc_relocate(st) < 0) {
    std::fprintf(stderr, "W250: TinyCC failed\n");
    std::exit(3);
  }
  auto fn = reinterpret_cast<model_fn>(tcc_get_symbol(st, "model_rhs"));
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  std::fprintf(stderr, "W250: TinyCC compiled the model in %.3f ms\n", ms);
  return fn; /* st is kept: the code lives in it */
}

void tcc_rhs(xpp::Session &s, double t, const double *y, double *ydot)
{
  static const model_fn fn = tcc_compile();
  static const xpp::Session *cached = nullptr;
  static const double *iapp = nullptr;
  if (cached != &s) {
    iapp = xpp::value_slot(s, "Iapp");
    cached = &s;
  }
  double fixed[21];
  fn(t, y, ydot, fixed, *iapp);
  setvar(s, 0, t);
  for (int i = 0; i < 8; i++) setvar(s, i + 1, y[i]);
  for (int i = 0; i < 21; i++) setvar(s, 8 + i + 1, fixed[i]);
}

} // namespace
