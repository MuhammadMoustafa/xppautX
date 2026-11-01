"""W585 (#650): the strong resonances xppautX now labels beside a torus curve's branch point (BP), each checked from the model's own
equations (scipy, numpy), with none of xppautX's continuation. At its 1:2 (1:1) point a torus curve's own equations have a BP, and the
two tests are zero at one point; xppautX used to keep the BP's label alone (AUTO-07p's coincidence rule).

  hill_cycles_torus  torus_down: the Hill equation's Floquet pair (docs/studies/w578_converged_check.py) at the BP and the R2: trace + 2
      and det - 1 are 0 at the R2 (a double -1); d - d* is the distance from the true 1:2 point of c = -mu at the point's own mu.
  forced_pp_ns  ns_up: the Poincare map of the forced predator-prey model (docs/studies/w557_special_point_check.py) at the BP and the R2,
      and the true R2 (trace -2, det 1) at the run's m.
  res_cycles  torus_up: the (w, v) equilibrium w = sqrt(b) of tools/models/res_cycles.odex, whose eigenvalues lambda give the cycle's pair
      exp(2 pi lambda) (period 2 pi); the R1 (a double +1) is at b = 0, c = -mu (closed form in the model's header).

The points are those of tests/auto/<model>.diagram.csv, at the descriptions' settings.

    uv run --locked python docs/studies/w585_torus_r2_check.py
"""
from pathlib import Path
import sys

import numpy as np
from scipy.optimize import brentq, fsolve

sys.path.insert(0, str(Path(__file__).resolve().parent))
from w578_converged_check import hill_monodromy  # noqa: E402
import w557_special_point_check as w557  # noqa: E402

HILL_MU = 0.29999999975303226  # the cycles' TR the torus curves start from (hill_cycles_torus.diagram.csv)
HILL = {'BP': (-0.2999999997517784, 0.2770059707503156), 'R2': (-0.29999999975473624, 0.2770059707510082)}  # (c, d)
NS_M = 0.3175425693891668  # the cycles' TR (forced_pp_ns.diagram.csv)
NS_START = (0.5, 0.338)  # (eps, b0) where the curve's cycle is first found (w578_converged_check.forced_pp_ns)
NS = {'BP': (0.5966569359300297, 0.3401301118010629), 'R2': (0.5966569357319971, 0.3401301118437829)}  # (eps, b0)
RES_MU, RES_G = 0.5000000059882198, 1.0  # the cycles' TR (res_cycles.diagram.csv), the model's g
RES = {'BP': (-0.5000000059886828, -7.040083787120241e-20), 'R1': (-0.50000000598855, -2.1644556564622926e-20)}  # (c, b)


def hill_cycles_torus():
    dstar = brentq(lambda x: np.trace(hill_monodromy(-HILL_MU, x, HILL_MU)) + 2, 0.26, 0.29, xtol=1e-15)
    print('hill_cycles_torus torus_down: the true R2 at c = -mu, d* = %.13f' % dstar)
    for name, (c, d) in HILL.items():
        m = hill_monodromy(c, d, HILL_MU)
        print('   %s: d - d* = %+.2e, c + mu = %+.2e, trace + 2 = %+.2e, det - 1 = %+.2e, multipliers %s'
              % (name, d - dstar, c + HILL_MU, np.trace(m) + 2, np.linalg.det(m) - 1, np.linalg.eigvals(m)))


def forced_pp_ns():
    cycle = w557.Cycle([0.33, 0.17], NS_M, 0.0)
    cycle.move_to(*NS_START)
    cycle.move_to(*NS['BP'])
    true = fsolve(lambda p: [np.trace(cycle.multipliers(*p)) + 2, np.linalg.det(cycle.multipliers(*p)) - 1], NS['BP'], xtol=1e-12)
    print('forced_pp_ns ns_up: the true R2 at m = %.13f: (eps, b0) = (%.10f, %.10f)' % (NS_M, *true))
    for name, (eps, b0) in NS.items():
        m = cycle.multipliers(eps, b0)
        print('   %s: eps, b0 - true = %+.2e, %+.2e, trace + 2 = %+.2e, det - 1 = %+.2e, multipliers %s'
              % (name, eps - true[0], b0 - true[1], np.trace(m) + 2, np.linalg.det(m) - 1, np.linalg.eigvals(m)))


def res_cycles():
    print('res_cycles torus_up: the true R1 at b = 0, c = -mu = %.16f' % -RES_MU)
    for name, (c, b) in RES.items():
        w = np.sqrt(complex(b))
        lam = np.linalg.eigvals(np.array([[0, 1], [-2 * w, -(c + RES_MU + RES_G * w)]]))
        print('   %s: b = %+.2e, c + mu = %+.2e, multipliers - 1 = %s' % (name, b, c + RES_MU, np.exp(2 * np.pi * lam) - 1))


if __name__ == '__main__':
    hill_cycles_torus()
    forced_pp_ns()
    res_cycles()
