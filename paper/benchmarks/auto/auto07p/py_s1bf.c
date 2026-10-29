/* PY_S1Bf (../models/PY_S1Bf.ode) for AUTO-07p, W246 (#302): the same right-hand
 * side, PAR(1) = Iapp (pA). STPNT is the depolarised equilibrium at Iapp = 20000 pA,
 * Newton-converged by xppautX's Sing pts (hopf_xppautx.py's start). */
#include "auto_f2c.h"
#include <math.h>

static double erel(double x) /* Brian2 exprel(x) = (exp(x)-1)/x, as the .ode writes it */
{
  return fabs(x) < 1e-5 ? 1 + x / 2 + x * x / 6 + x * x * x / 24 : (exp(x) - 1) / x;
}

/* W269: erel's derivative, (x e^x - (e^x - 1)) / x^2, and its series's below 1e-5 */
static double erel_d(double x)
{
  return fabs(x) < 1e-5 ? 0.5 + x / 3 + x * x / 8 : (x * exp(x) - (exp(x) - 1)) / (x * x);
}

#define NPAR_USED 1 /* W269: only PAR(1), Iapp, is differentiated (AUTO adds the period's column) */

int func(integer ndim, const doublereal *u, const integer *icp, const doublereal *par, integer ijac,
         doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  const double Iapp = par[0];
  const double C = 200, gNa = 11.2, gKdr = 1.2, gL = 0.00549494, gM = 0.015, gA = 0, gNaP = 0;
  const double EL = -67.73070881, ENa = 50, EK = -100, VT = -56.16;
  const double phiF = 0.6020561713201731, phiM = 0.8465535452071854, tauM = 608;
  const double v = u[0], m = u[1], h = u[2], n = u[3], pM = u[4], aA = u[5], bA = u[6], pNa = u[7];
  const double vv = v - VT;
  const double am = 1.28 / erel((13 - vv) / 4), bm = 1.4 / erel((vv - 40) / 5);
  const double ah = 0.128 * exp((17 - vv) / 18), bh = 4 / (1 + exp((40 - vv) / 5));
  const double an = 0.16 / erel((15 - vv) / 5), bn = 0.5 * exp((10 - vv) / 40);
  const double minf = am / (am + bm), hinf = ah / (ah + bh), ninf = an / (an + bn);
  const double pinf = 1 / (1 + exp(-(v + 35) / 10));
  const double taup = tauM / (3.3 * exp((v + 35) / 20) + exp(-(v + 35) / 20)) / phiM;
  const double ainf = 1 / (1 + exp(-(v + 50) / 20)), binf = 1 / (1 + exp((v + 80) / 6));
  const double pninf = 1 / (1 + exp(-(v + 55) / 6.2));
  const double INa = 1000 * gNa * pow(m, 3) * h * (v - ENa);
  const double IKdr = 1000 * gKdr * pow(n, 4) * (v - EK);
  const double IL = 1000 * gL * (v - EL);
  const double IM = 1000 * gM * pM * (v - EK);
  const double IA = 1000 * gA * pow(aA, 3) * bA * (v - EK);
  const double INaP = 1000 * gNaP * pNa * (v - ENa);
  f[0] = (Iapp - INa - IKdr - IL - IM - IA - INaP) / C;
  f[1] = phiF * (am + bm) * (minf - m);
  f[2] = phiF * (ah + bh) * (hinf - h);
  f[3] = phiF * (an + bn) * (ninf - n);
  f[4] = (pinf - pM) / taup;
  f[5] = (ainf - aA) / 2;
  f[6] = (binf - bA) / 20;
  f[7] = (pninf - pNa) / 5;
  if (ijac == 0) return 0; /* JAC = 0 (W246): AUTO differences f itself */
  /* W269 (#326): JAC = 1, the exact Jacobian written by hand (derivatives of the
   * lines above, checked against central differences by jac_check.c);
   * ARRAY2D(dfdu, i, j) = dfdu[i + ndim * j] = d f_i / d u_j. */
  for (integer k = 0; k < ndim * ndim; ++k) dfdu[k] = 0;
#define J(i, j) dfdu[(i) + ndim * (j)]
  const double ex = (13 - vv) / 4, ey = (vv - 40) / 5, ez = (15 - vv) / 5;
  const double dam = -1.28 * erel_d(ex) / (erel(ex) * erel(ex)) * (-0.25);
  const double dbm = -1.4 * erel_d(ey) / (erel(ey) * erel(ey)) * 0.2;
  const double dah = -ah / 18;
  const double Eh = exp((40 - vv) / 5), dbh = 4 * Eh / (5 * (1 + Eh) * (1 + Eh));
  const double dan = -0.16 * erel_d(ez) / (erel(ez) * erel(ez)) * (-0.2);
  const double dbn = -bn / 40;
  const double P = exp(-(v + 35) / 10), dpinf = P / (10 * (1 + P) * (1 + P));
  const double D = 3.3 * exp((v + 35) / 20) + exp(-(v + 35) / 20);
  const double dD = (3.3 * exp((v + 35) / 20) - exp(-(v + 35) / 20)) / 20;
  const double dtaup = -tauM * dD / (D * D * phiM);
  const double A = exp(-(v + 50) / 20), dainf = A / (20 * (1 + A) * (1 + A));
  const double B = exp((v + 80) / 6), dbinf = -B / (6 * (1 + B) * (1 + B));
  const double Q = exp(-(v + 55) / 6.2), dpninf = Q / (6.2 * (1 + Q) * (1 + Q));
  /* the currents' derivatives: 1000 * g * (gates) by v, and by each gate */
  J(0, 0) = -1000 * (gNa * m * m * m * h + gKdr * n * n * n * n + gL + gM * pM + gA * aA * aA * aA * bA + gNaP * pNa) / C;
  J(0, 1) = -3000 * gNa * m * m * h * (v - ENa) / C;
  J(0, 2) = -1000 * gNa * m * m * m * (v - ENa) / C;
  J(0, 3) = -4000 * gKdr * n * n * n * (v - EK) / C;
  J(0, 4) = -1000 * gM * (v - EK) / C;
  J(0, 5) = -3000 * gA * aA * aA * bA * (v - EK) / C;
  J(0, 6) = -1000 * gA * aA * aA * aA * (v - EK) / C;
  J(0, 7) = -1000 * gNaP * (v - ENa) / C;
  /* phiF * (a + b) * (a / (a + b) - x) = phiF * (a - (a + b) * x) */
  J(1, 0) = phiF * (dam - (dam + dbm) * m);
  J(1, 1) = -phiF * (am + bm);
  J(2, 0) = phiF * (dah - (dah + dbh) * h);
  J(2, 2) = -phiF * (ah + bh);
  J(3, 0) = phiF * (dan - (dan + dbn) * n);
  J(3, 3) = -phiF * (an + bn);
  J(4, 0) = dpinf / taup - (pinf - pM) * dtaup / (taup * taup);
  J(4, 4) = -1 / taup;
  J(5, 0) = dainf / 2;
  J(5, 5) = -0.5;
  J(6, 0) = dbinf / 20;
  J(6, 6) = -1.0 / 20;
  J(7, 0) = dpninf / 5;
  J(7, 7) = -0.2;
#undef J
  if (ijac == 1) return 0;
  for (integer k = 0; k < ndim * NPAR_USED; ++k) dfdp[k] = 0;
  dfdp[0] = 1 / C; /* d f_0 / d Iapp = PAR(1) */
  return 0;
}

int stpnt(integer ndim, doublereal t, doublereal *u, doublereal *par)
{
  par[0] = 20000;
  u[0] = -18.68007298508246;
  u[1] = 0.814980486244479;
  u[2] = 0.02651387739045536;
  u[3] = 0.7430659014712411;
  u[4] = 0.8364424360688298;
  u[5] = 0.8272121071765125;
  u[6] = 3.643341281469538e-05;
  u[7] = 0.9971513368719562;
  return 0;
}

int bcnd(integer ndim, const doublereal *par, const integer *icp, integer nbc, const doublereal *u0,
         const doublereal *u1, integer ijac, doublereal *fb, doublereal *dbc) { return 0; }
int icnd(integer ndim, const doublereal *par, const integer *icp, integer nint, const doublereal *u,
         const doublereal *uold, const doublereal *udot, const doublereal *upold, integer ijac,
         doublereal *fi, doublereal *dint) { return 0; }
int fopt(integer ndim, const doublereal *u, const integer *icp, const doublereal *par, integer ijac,
         doublereal *fs, doublereal *dfdu, doublereal *dfdp) { return 0; }
int pvls(integer ndim, const doublereal *u, doublereal *par) { return 0; }
