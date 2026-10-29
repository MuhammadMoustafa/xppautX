/* W249 (#305), test build only, never merged: PY_S1Bf's right-hand side hand-written
   in C++, the ceiling of any compiled model. Included into core/my_rhs.cpp by
   apply_patch.py; XPP_HAND_RHS picks it at run time:
     unset or 0  the interpreter (stock behaviour)
     1           hand-written, xpp::math's exp/pow: the interpreter's operations in its
                 order (left-associative, ode_divisor as the .odex writes it), so the
                 outputs must be bit-identical
     2           hand-written, the C library's exp/pow
   Valid only for models/PY_S1Bf*.odex (8 states, 21 fixed quantities in .odex order). */
#include <cmath>
#include <cstdlib>
#include "expr.h"
#include "getvar.h"
#include "xpp_math.h"

namespace {

int hand_mode()
{
  static const int mode = [] { const char *e = std::getenv("XPP_HAND_RHS"); return e ? std::atoi(e) : 0; }();
  return mode;
}

inline double ode_divisor(double value) { return value != 0.0 ? value : 2.23e-15; }

double c_exp(double x) { return std::exp(x); }
double c_pow(double x, double y) { return std::pow(x, y); }

template <double (*EXP)(double)>
double erel(double x)
{
  return std::fabs(x) < 1e-05 ? 1 + x / 2 + x * x / 6 + x * x * x / 24 : (EXP(x) - 1) / ode_divisor(x);
}

/* the right-hand side alone: writes ydot and the 21 fixed quantities, touches no
   Session (W251's threads call it from AUTO's func with Iapp from AUTO's par) */
template <double (*EXP)(double), double (*POW)(double, double)>
void hand_core(const double *y, double *ydot, double *fixed, double Iapp)
{
  const double C = 200, gNa = 11.2, gKdr = 1.2, gL = 0.00549494, gM = 0.015, gA = 0, gNaP = 0;
  const double EL = -67.73070881, ENa = 50, EK = -100, VT = -56.16;
  const double phiF = 0.6020561713201731, phiM = 0.8465535452071854, tauM = 608;
  const double v = y[0], m = y[1], h = y[2], n = y[3], pM = y[4], aA = y[5], bA = y[6], pNa = y[7];
  const double vv = v - VT;
  const double am = 1.28 / ode_divisor(erel<EXP>((13 - vv) / 4));
  const double bm = 1.4 / ode_divisor(erel<EXP>((vv - 40) / 5));
  const double ah = 0.128 * EXP((17 - vv) / 18);
  const double bh = 4 / (1 + EXP((40 - vv) / 5));
  const double an = 0.16 / ode_divisor(erel<EXP>((15 - vv) / 5));
  const double bn = 0.5 * EXP((10 - vv) / 40);
  const double minf = am / ode_divisor(am + bm);
  const double hinf = ah / ode_divisor(ah + bh);
  const double ninf = an / ode_divisor(an + bn);
  const double pinf = 1 / (1 + EXP(-(v + 35) / 10));
  const double taup = tauM / ode_divisor(3.3 * EXP((v + 35) / 20) + EXP(-(v + 35) / 20)) / ode_divisor(phiM);
  const double ainf = 1 / (1 + EXP(-(v + 50) / 20));
  const double binf = 1 / (1 + EXP((v + 80) / 6));
  const double pninf = 1 / (1 + EXP(-(v + 55) / 6.2));
  const double INa = 1000 * gNa * POW(m, 3) * h * (v - ENa);
  const double IKdr = 1000 * gKdr * POW(n, 4) * (v - EK);
  const double IL = 1000 * gL * (v - EL);
  const double IM = 1000 * gM * pM * (v - EK);
  const double IA = 1000 * gA * POW(aA, 3) * bA * (v - EK);
  const double INaP = 1000 * gNaP * pNa * (v - ENa);
  const double all[] = {vv, am, bm, ah, bh, an, bn, minf, hinf, ninf, pinf, taup, ainf, binf, pninf,
                        INa, IKdr, IL, IM, IA, INaP};
  for (int i = 0; i < 21; i++) fixed[i] = all[i];
  ydot[0] = (Iapp - INa - IKdr - IL - IM - IA - INaP) / ode_divisor(C);
  ydot[1] = phiF * (am + bm) * (minf - m);
  ydot[2] = phiF * (ah + bh) * (hinf - h);
  ydot[3] = phiF * (an + bn) * (ninf - n);
  ydot[4] = (pinf - pM) / ode_divisor(taup);
  ydot[5] = (ainf - aA) / 2;
  ydot[6] = (binf - bA) / 20;
  ydot[7] = (pninf - pNa) / 5;
}

template <double (*EXP)(double), double (*POW)(double, double)>
void hand_rhs(xpp::Session &s, double t, const double *y, double *ydot)
{
  static const xpp::Session *cached = nullptr;
  static const double *iapp = nullptr;
  if (cached != &s) {
    iapp = xpp::value_slot(s, "Iapp");
    cached = &s;
  }
  double fixed[21];
  hand_core<EXP, POW>(y, ydot, fixed, *iapp);
  /* what the interpreter stores: t, the states, then the fixed quantities (aux output reads them) */
  setvar(s, 0, t);
  for (int i = 0; i < 8; i++) setvar(s, i + 1, y[i]);
  for (int i = 0; i < 21; i++) setvar(s, 8 + i + 1, fixed[i]);
}

} // namespace
