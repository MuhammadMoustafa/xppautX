# W126: which DAE result is right (issue #178)

Question: before W119 XPPAUT ignored a model's `@ newt_iter`, `newt_tol`,
`jac_eps` and `poistop` (docs/xppaut-findings.md #1); W119 applies them.
`dae.ode`, `dae_ex3.ode` and `canonical/exdaebvp.ode` change. Which result
is right?

## Method

`docs/w126-dae-check.py OLD_EXE NEW_EXE` runs each build as
`xppautX.exe model.ode -silent` in a scratch copy of the model's folder and
reads `output.dat` (float32 text: about 1e-7 relative floor). Old = master's
Windows build (eeb6a8f, no W119), new = the W119 branch's (04fce1e). The
references (they agree to 5e-11 or better, per model):

- (a) scipy `solve_ivp`, Radau, rtol 1e-10, atol 1e-12, the algebraic
  variable solved by `brentq` to full tolerance inside the right-hand side;
- (b) a fixed-step RK4 (the output step divided by 20, or 100 for dae_ex3)
  with Newton on the constraint at every stage;
- for dae_ex3 also a closed form (below). No DAE library (IDA, assimulo) was
  installed; all three models are index 1, so (a) and (b) are sound.

Row t=0 is skipped (exdaebvp's `w=.235` is a guess, not the constraint's
root, in both builds). Errors: max abs over the rows, and in brackets over the
column's largest value.

## The models

- `dae.ode`: x'=y, 0=x+y+y^3+y^5, x(0)=3, to t=20 (401 rows; columns x, yy,
  zz = the residual, 0 for the true solution). Options: `newt_tol=1e-8`.
- `dae_ex3.ode`: w'=v, 0=v(1-v^2)-w, v(0)=1; `NEWT_ITER=1000, NEWT_TOL=1e-3,
  JAC_EPS=1e-5, METH=qualrk`. The constraint is a fold: with v on its branch
  w = v-v^3, w'=v gives (1-3v^2)v' = v, so t(v) = ln v - 1.5(v^2-1), and at the
  fold v=1/sqrt(3) (dw/dv = 0) t* = 0.450694. The DAE has an impasse point
  there: no solution continues past t*, which is also what XPP's manual says
  ("cannot integrate past singularities").
- `canonical/exdaebvp.ode`: u'=up, up'=v*up+lamu(u-w), 0=-k w/(1+w)+lamv(u-w),
  to t=5 (1001 rows; columns u, up, ww, z = the residual). `-silent` runs only
  the initial value problem (its `bdry` lines are for AUTO's BVP mode), so
  the references are IVPs. Options: `jac_eps=1e-5, newt_tol=1e-5`.

## Results (max abs error against references a and b, which agree)

| model, column | old (XPPAUT) | W119 |
|---|---|---|
| dae: x | 8.7e-4 (2.9e-4) | 1.5e-7 (4.9e-8) |
| dae: yy | 9.7e-4 (9.8e-4) | 1.9e-6 (1.9e-6) |
| dae: residual zz (true 0) | 1.0e-3 | 2.4e-6 |
| exdaebvp: u | 3.1e-3 (3.1e-3) | 3.9e-7 (3.9e-7) |
| exdaebvp: up | 1.9e-3 (5.6e-3) | 2.7e-7 (8.1e-7) |
| exdaebvp: ww | 1.5e-3 (6.5e-3) | 9.0e-6 (3.8e-5) |
| exdaebvp: residual z (true 0) | 1.0e-3 | 9.9e-6 |
| dae_ex3, t<0.44: w | 3.6e-5 (1.0e-4) | 3.5e-5 (9.9e-5) |
| dae_ex3, t<0.44: v_ | 8.5e-3 (8.7e-3) | 8.5e-3 (8.8e-3) |
| dae_ex3, rows with t >= t* | none (stops at t=0.45) | 64 rows to t=3.65 |

## Verdict

- `dae.ode` and `exdaebvp.ode`: W119 is right. The old results carry the
  default Newton tolerance (1e-3 residual left in the constraint, so errors
  of 1e-3); W119 applies the model's own `newt_tol` (1e-8, 1e-5) and is
  within 1e-6 to 1e-5 of the references, i.e. at the model's tolerance and the
  output's float32 floor. Old errors are 400 to 4000 times larger.
- `dae_ex3.ode`: on t < 0.44 the two builds are equally accurate (the same
  0.85% error in v_ and 1e-4 in w: the model's own loose `newt_tol=1e-3` and
  the integrator's step, not the Newton setting; neither build is more
  right). Past the fold the old behaviour is right and W119's is wrong: the
  old build stops at t=0.45 ("Maximum iterates exceeded") a hair before
  t*=0.4507, where the true solution ends. W119's tolerance 1e-3 lets Newton
  step across the fold and "continue" to t=3.65, but those 64 rows are not a
  solution: the constraint residual is up to 0.031, v swings between -1.14
  and 1.15 (jumping between the branches v=+-0.6..1.1) and w oscillates in
  -0.38..0.38, none of which any reference reproduces (the true w stays
  below 0.3849 and the solution does not exist past t*). dae_ex3 finishing
  to 3.65 is a loose tolerance accepting garbage; the old failure at 0.45 was
  the correct diagnosis, only its message ("Maximum iterates exceeded")
  misleads about the cause (an impasse point).

So the options taking effect (W119) is right in principle and in two of
three models measurably so; the one "improvement" in dae_ex3 is spurious.
Not changed here (analysis only): a DAE solve that steps over a fold should
be detected (a singular or tiny Jacobian in the Newton, or a residual above
tolerance after the step) and stop like the old run did, rather than accept
a step because `newt_tol` is loose.

## Rerun

    python3 docs/w126-dae-check.py C:/gitRepos/xppautX/xppautX.exe C:/gitRepos/xppautX/.claude/worktrees/W119/xppautX.exe --work SCRATCH

(any two builds; needs numpy and scipy; prints the rows, the errors against
each reference and the references' agreement).
