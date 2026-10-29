# An exact Jacobian: measure first (W252, #308)

Question (maintainer, 2026-10-10): would an analytic Jacobian of the compiled
model add enough to keep, and does it work for every model?

## Method

- A test build, never merged: the pinned xppautX (88e09741) patched by
  `../rhs-ceiling/apply_patch.py`, `../auto-parallel/apply_parallel.py` and
  `apply_jacobian.py`. `jac_ad.inc` computes PY_S1Bf's right-hand side and
  its exact Jacobian (8 states and Iapp) in one pass by forward-mode
  automatic differentiation: the derivatives of the same formulas a
  symbolic differentiation of the model would give, at about its cost. Its
  values use hand_rhs.inc's operations, so `f` is unchanged.
- `XPP_JAC=1` (with the pure compiled model, `XPP_HAND_RHS=4`):
  - AUTO runs with `JAC=1`: `funi` takes `dfdu` and `dfdp` from `func`
    instead of 1 + 2 ndim + nfpr right-hand-side calls per point;
  - the Stiff integrator's `jacobn` takes it instead of n + 2 calls
    (the model is autonomous: df/dt = 0).
- Checked: `XPP_JAC_CHECK=FILE` compares it with central differences on
  AUTO's first 20 calls: largest relative difference 3.6e-11 to 4.2e-10,
  the differencing's own error.
- Timing: `../auto-parallel/parallel.py --variants
  interpreter,pure-fa,pure-fa-4t,jac,jac-2t,jac-4t` (W246's AUTO run, warm-up
  plus five, one physical core per thread) and `integrate_jac.py` (the
  stiff integration of W249, warm-up plus five, one pinned CPU).
  `compare.py` compares the branches point by point.
- Results: `../auto-parallel/results/wsl-jac/`, `results/wsl/`.

## Results (2026-10-10)

AUTO periodic run, wall s (W251's make_fa fix in all):

| Threads | Finite differences | Exact Jacobian | Gain |
|---:|---:|---:|---:|
| 1 | 4.87 +/- 0.02 | 3.28 +/- 0.02 | 1.48x |
| 2 | (3.44, W251) | 2.45 +/- 0.01 | 1.40x |
| 4 | 2.87 +/- 0.01 | 2.02 +/- 0.02 | 1.42x |

From today's interpreter (23.94 s, W251) to the compiled model with the
fix, the exact Jacobian and 4 threads: 11.9x. AUTO-07p (W246, its default
finite differences): 2.63 s on one thread.

Stiff integration, CPU s: finite differences 6.36 +/- 0.05, exact 4.16 +/-
0.02 (1.53x).

What changes:

- AUTO: the same 403 steady and 336 periodic points, the same labels (HB
  16409.42, BP 16394.43, EP 192.813, to the printed digits); the largest
  difference between points of the same number is 1e-4 in Iapp (steady)
  and 0.0116 in the plotted norm (periodic).
- Integration: the same 265 spikes over 2000 ms; the largest difference
  is 0.12 mV in V (a slight shift in spike timing; tolerance 1e-8).
- The results no longer equal XPPAUT's bit for bit (they are, if anything,
  more accurate: no differencing error). Exact-Jacobian results are
  identical for 1, 2 and 4 threads.

## Which models it covers

- Exact: everything built from arithmetic, `exp`, `log`, powers, trig and
  hyperbolic functions, user functions (inlined), `sum` and the network
  operators (linear in the states they combine).
- Piecewise (`if`, `heav`, `sign`, `abs`, `max`, `min`, `mod`, interpolated
  tables): the derivative of the branch taken, exact except at the switch
  itself, where a finite difference is no better.
- No Jacobian: random numbers, Markov variables, delays (and the Jacobian
  of a delayed term), Volterra integrals. Such a model keeps finite
  differences.

## What this says

The exact Jacobian is worth about 1.4-1.5x on top of everything else, in
AUTO and in the stiff integrator alike, but it changes the numbers slightly.
Kept, it is an opt-in, off by default, so a model gives XPPAUT's results
unless asked otherwise.
