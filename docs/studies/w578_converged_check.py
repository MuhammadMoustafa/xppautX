"""Independent checks of the points where xppautX's exact-Jacobian rows and the comparison's converged exact run disagree
(W578, #643). Each comes from the model's own equations, with none of xppautX's, XPPAUT's or AUTO-07p's continuation:

  hill_cycles_torus  tools/models/hill_cycles.odex: the cycle is exactly (x, y) = sqrt(mu) (cos t, sin t), period 2 pi, and its
      non-trivial Floquet pair is that of the Hill equation w'' + (c + mu) w' + (d + q x(t)) w = 0 over a period (scipy). The
      torus curve is c + mu = 0 (det 1); at its 1:2 point (trace -2) the pair is a defective double -1, and the flip curve
      1 + trace + det = 0 passes through it: the torus system xppautX continues (bctr, theta and v2) has the flip system's
      solutions (theta = pi, v2 = 0) as a second branch through the point, a branch point.
  henon_rot_th  tools/models/henon_rot.odex: the torus system of a map (AUTO-07p's FFNSX, xppautX's ffhb: the fixed point,
      (A^2 - 2 cos(theta) A + I) w = 0, |w| = 1 and the phase w . (A^T - A) w_old = 0) on the curve beta = 1.8, theta = th.
      Its Jacobian loses rank at th = pi and its determinant with the curve's tangent, AUTO's branch-point test, changes sign.
  forced_pp_pd  The forced predator-prey model's Poincare map (docs/studies/w557_special_point_check.py): the true R2 of the
      period-doubling curve (trace -2, det 1), each program's distance from it, and how far a small perturbation of the
      monodromy matrix splits the defective double -1 there.
  forced_pp_ns  The same map: the true torus point of the cycles (det 1) at eps 0.5, b0 0.338, against the m each run locates.

    uv run --locked python docs/studies/w578_converged_check.py

The Mu26 cycles' norms and multipliers are docs/studies/floquet_check.py's (--norm). The values compared are the programs'
own, from the W557 timed series (~/xppautx-comparison/final/continuation/attempt-001).
"""
from pathlib import Path
import sys

import numpy as np
from scipy.integrate import solve_ivp
from scipy.optimize import brentq, fsolve

sys.path.insert(0, str(Path(__file__).resolve().parent))
import w557_special_point_check as w557  # noqa: E402

RTOL, ATOL = 1e-13, 1e-15  # of the Hill equation's integration
HILL_MU, HILL_Q = 0.3, 0.1  # the TR's mu, the model's q
HENON_ALPHA, HENON_E, HENON_BETA = 1.5, 0.5, 1.8  # the model's alpha and e, the curve's beta
JACOBIAN_STEP = 1e-7  # relative, of the map system's central differences (the system is a polynomial in its unknowns)
OFFSETS = (-1e-4, -1e-5, 1e-5, 1e-4)  # from the singular point, where each test is evaluated


def hill_monodromy(c, d):
    def f(t, z):
        a = np.array([[0, 1], [-(d + HILL_Q * np.sqrt(HILL_MU) * np.cos(t)), -(c + HILL_MU)]])
        return (a @ z.reshape(2, 2)).ravel()
    return solve_ivp(f, (0, 2 * np.pi), np.eye(2).ravel(), rtol=RTOL, atol=ATOL, method='DOP853').y[:, -1].reshape(2, 2)


def hill_cycles_torus():
    trace = lambda c, d: np.trace(hill_monodromy(c, d))
    dstar = brentq(lambda d: trace(-HILL_MU, d) + 2, 0.26, 0.29, xtol=1e-15)
    m = hill_monodromy(-HILL_MU, dstar)
    print('hill_cycles_torus: 1:2 point of the torus curve c = -0.3 at d* = %.13f, multipliers %s, M + I = %s'
          % (dstar, np.linalg.eigvals(m), np.round(m + np.eye(2), 9).tolist()))
    for dc in OFFSETS:
        flip = brentq(lambda d: 1 + trace(-HILL_MU + dc, d) + np.linalg.det(hill_monodromy(-HILL_MU + dc, d)), dstar - 0.02, dstar + 1e-9, xtol=1e-15)
        print('   the flip curve at c + mu = %+.0e: d - d* = %.3e' % (dc, flip - dstar))
    for name, d in (('xppautX finite-difference pilot BP', 0.2770059707503156), ('xppautX exact rows BP', 0.2770059707503154)):
        print('   %s: d - d* = %.3e' % (name, d - dstar))


def henon_system(z, w_old):
    x, w, theta, beta, th = z[0:4], z[4:8], z[8], z[9], z[10]
    s = 1 + HENON_E * (beta - 2 * HENON_ALPHA + 1.2)
    c, sn = np.cos(th), np.sin(th)
    fixed = np.array([x[1], HENON_ALPHA - beta * x[0] - x[1] ** 2, s * (c * x[2] - sn * x[3]), s * (sn * x[2] + c * x[3])]) - x
    a = np.array([[0, 1, 0, 0], [-beta, -2 * x[1], 0, 0], [0, 0, s * c, -s * sn], [0, 0, s * sn, s * c]])
    return np.concatenate([fixed, (a @ a - 2 * np.cos(theta) * a + np.eye(4)) @ w, [w @ w - 1, w @ ((a.T - a) @ w_old)]])


def henon_rot_th():
    y = (-(1 + HENON_BETA) + np.sqrt((1 + HENON_BETA) ** 2 + 4 * HENON_ALPHA)) / 2  # the fixed point x = y
    w = np.array([0, 0, 1.0, 0])  # in the rotation's plane, where the pair exp(+-i th) lives
    tangent = np.zeros(11)
    tangent[8] = tangent[10] = np.sqrt(0.5)  # theta = th, all else fixed along the curve
    print('henon_rot_th: the torus system on beta = 1.8 near th = pi')
    for offset in (*OFFSETS[:2], 0.0, *OFFSETS[2:]):
        z = np.concatenate([[y, y, 0, 0], w, [np.pi + offset, HENON_BETA, np.pi + offset]])
        jacobian = np.zeros((10, 11))
        for k in range(11):
            dz = np.zeros(11)
            dz[k] = JACOBIAN_STEP * (1 + abs(z[k]))
            jacobian[:, k] = (henon_system(z + dz, w) - henon_system(z - dz, w)) / (2 * dz[k])
        print('   th - pi = %+.0e: determinant with the tangent %+.4e, smallest singular value %.2e'
              % (offset, np.linalg.det(np.vstack([jacobian, tangent])), np.linalg.svd(jacobian, compute_uv=False)[-1]))
    print('   xppautX\'s BP at th = 3.1415923639 (W580; 3.141629293 before): th - pi = %.2e' % (3.1415923638884355 - np.pi))


# each program's R2 of pd_up: m (the curve's fixed parameter), eps, b0, its pair near -1; the cycle's state and the forcing's
# phase at t = 0 from the exact rows' R2 row (u0.X, u0.Y, atan2(u0.V, u0.U))
PD_R2 = {'xppautX exact rows (EPS 1e-7)': (0.3723241839881814, 1.9489338553245652, 2.5702267114087602, complex(-0.9999998167725831, 6.92263450992909e-4)),
         'converged run (EPS 1e-10)': (0.37232418230156694, 1.94893385926055, 2.5702267181901246, complex(-0.9999999999195499, 1.4676149326991227e-05))}
PD_STATE, PD_PHASE = (0.623130778725645, 0.045107399932300105), float(np.arctan2(0.4235735926338855, 0.43921474885789386))
PERTURBATIONS = (1e-10, 1e-7)  # relative, of the true monodromy matrix


def forced_pp_pd():
    print('forced_pp_pd: pd_up R2')
    for name, (m, eps, b0, pair) in PD_R2.items():
        cycle = w557.Cycle(PD_STATE, m, PD_PHASE)
        near = cycle.multipliers(eps, b0)
        true = fsolve(lambda p: [np.trace(cycle.multipliers(*p)) + 2, np.linalg.det(cycle.multipliers(*p)) - 1], [eps, b0], xtol=1e-14)
        jordan = cycle.multipliers(*true)
        splits = []
        for e in PERTURBATIONS:
            perturbation = np.zeros((2, 2))
            perturbation[1, 0] = e * np.abs(jordan).max()
            splits.append('%.0e: %.2e' % (e, abs(np.linalg.eigvals(jordan + perturbation)[0] + 1)))
        print('   %s: R2 off the true one (%.10f, %.10f) by %.2e, %.2e; the true map\'s pair there %s; its own pair %.10f %+.3ei'
              % (name, *true, eps - true[0], b0 - true[1], np.linalg.eigvals(near), pair.real, pair.imag))
        print('      the true R2\'s monodromy + I = %s; a relative perturbation splits its pair by %s' % (np.round(jordan + np.eye(2), 4).tolist(), ', '.join(splits)))


NS_EPS, NS_B0 = 0.5, 0.338
NS_TR = {'xppautX (pilots, exact rows)': 0.31754256949722404, 'AUTO-07p': 0.31754256981, 'converged run': 0.3175445941520383}
NS_NORM = 0.6938  # the cycles' norm at the TR: dnorm = dm / (2 norm), the forcing circle's u^2 + v^2 = m in the norm


def forced_pp_ns():
    cycle = w557.Cycle([0.33, 0.17], NS_TR['converged run'], 0.0)

    def torus(m):
        cycle.m, cycle.at = m, None
        return np.linalg.det(cycle.multipliers(NS_EPS, NS_B0)) - 1
    true = brentq(torus, 0.315, 0.32, xtol=1e-15, rtol=1e-15)
    print('forced_pp_ns: the cycles\' true torus point at eps 0.5, b0 0.338: m = %.12f' % true)
    for name, m in NS_TR.items():
        print('   %s: m - true = %+.3e, the norm\'s share dm / (2 norm) = %.3e' % (name, m - true, (true - m) / (2 * NS_NORM)))


if __name__ == '__main__':
    hill_cycles_torus()
    henon_rot_th()
    forced_pp_pd()
    forced_pp_ns()
