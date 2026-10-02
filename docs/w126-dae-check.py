#!/usr/bin/env python3
"""W126: independent check of xppautX's DAE results (docs/w126-dae-check.md).

Usage: python3 docs/w126-dae-check.py OLD_EXE NEW_EXE [--work DIR]
Runs each exe as `<exe> model.ode --silent` in a scratch copy of the model's
folder, reads output.dat, and compares it with two references per model:
(a) scipy solve_ivp (Radau, rtol 1e-10, atol 1e-12) with the algebraic
variable solved to full tolerance (brentq) inside the right-hand side;
(b) a fixed-step RK4 (dt/20) with Newton on the constraint at every stage.
dae_ex3 also has a closed form (the DAE is a fold). Needs numpy, scipy.
"""
import os, shutil, subprocess, sys, tempfile
import numpy as np
from scipy.integrate import solve_ivp
from scipy.optimize import brentq

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODELS = {  # name -> path of the .ode below examples/
    "dae": "ode/dae.ode", "dae_ex3": "ode/dae_ex3.ode",
    "exdaebvp": "canonical/exdaebvp.ode"}

def newton(f, df, z, tol=1e-15):
    for _ in range(50):
        d = f(z) / df(z); z -= d
        if abs(d) < tol: break
    return z

# ---- dae.ode: x'=y, 0=x+y+y^3+y^5 (y monotone in x), columns t x yy zz
def dae_alg(x): return brentq(lambda y: x + y + y**3 + y**5, -50, 50, xtol=1e-15, rtol=1e-15)
def dae_cols(t, X): return np.array([[x, dae_alg(x), 0.0] for x in X])
def dae_ref_a(t):
    s = solve_ivp(lambda _, x: [dae_alg(x[0])], (0, t[-1]), [3.0], method="Radau",
                  rtol=1e-10, atol=1e-12, t_eval=t)
    return dae_cols(t, s.y[0])
def rk4(f, y0, t, sub=20):
    y = np.array(y0, float); out = [y.copy()]
    for a, b in zip(t[:-1], t[1:]):
        h = (b - a) / sub
        for i in range(sub):
            k1 = f(y); k2 = f(y + h/2*k1); k3 = f(y + h/2*k2); k4 = f(y + h*k3)
            y = y + h/6*(k1 + 2*k2 + 2*k3 + k4)
        out.append(y.copy())
    return np.array(out)
def dae_ref_b(t):
    g = lambda x: newton(lambda y: x + y + y**3 + y**5, lambda y: 1 + 3*y*y + 5*y**4, -np.tanh(x))
    X = rk4(lambda y: np.array([g(y[0])]), [3.0], t)[:, 0]
    return np.array([[x, g(x), 0.0] for x in X])

# ---- dae_ex3.ode: w'=v, 0=v(1-v^2)-w, solv v=1; columns t w v_
# v'(1-3v^2)=v: t(v) = ln v - 1.5(v^2-1); a fold at v=1/sqrt3, t*=0.45069
TSTAR = np.log(1/np.sqrt(3)) - 1.5*(1/3 - 1)
def ex3_closed(t):
    rows = []
    for tt in t:
        if tt >= TSTAR: rows.append([np.nan, np.nan]); continue
        v = brentq(lambda v: np.log(v) - 1.5*(v*v - 1) - tt, 1/np.sqrt(3), 1.0, xtol=1e-15)
        rows.append([v - v**3, v])
    return np.array(rows)
def ex3_ref_a(t):
    last = [1.0]
    def f(_, w):
        v = brentq(lambda v: v*(1 - v*v) - w[0], 1/np.sqrt(3), 1.5 if w[0] <= 0 else 1.0, xtol=1e-15)
        return [v]
    tt = t[t < 0.44]
    s = solve_ivp(f, (0, tt[-1]), [0.0], method="Radau", rtol=1e-10, atol=1e-12, t_eval=tt)
    return np.array([[w, brentq(lambda v: v*(1-v*v) - w, 1/np.sqrt(3), 1.0 if w > 0 else 1.5, xtol=1e-15)] for w in s.y[0]])
def ex3_ref_b(t):
    tt = t[t < 0.44]
    def g(w):
        return newton(lambda v: v*(1-v*v) - w, lambda v: 1 - 3*v*v, 0.9 if w > 0.3 else 1.0)
    W = rk4(lambda y: np.array([g(y[0])]), [0.0], tt, sub=100)[:, 0]
    return np.array([[w, g(w)] for w in W])

# ---- exdaebvp.ode: u'=up, up'=v up+lamu(u-w), 0=-k w/(1+w)+lamv(u-w), solve w;
# columns t u up ww z (z = the constraint residual, zero for the true solution).
K, LAMU, LAMV, V = 1.0, .25, .25, .2
def bvp_alg(u):
    return brentq(lambda w: -K*w/(1 + w) + LAMV*(u - w), -0.9, max(u, .5) + 1, xtol=1e-15, rtol=1e-15)
def bvp_rhs(y): w = bvp_alg(y[0]); return np.array([y[1], V*y[1] + LAMU*(y[0] - w)])
def bvp_cols(Y): return np.array([[u, up, bvp_alg(u), 0.0] for u, up in Y])
def bvp_ref_a(t):
    s = solve_ivp(lambda _, y: bvp_rhs(y), (0, t[-1]), [1.0, -.34], method="Radau",
                  rtol=1e-10, atol=1e-12, t_eval=t)
    return bvp_cols(s.y.T)
def bvp_ref_b(t):
    def rhs(y):
        w = newton(lambda w: -K*w/(1 + w) + LAMV*(y[0] - w), lambda w: -K/(1 + w)**2 - LAMV, 0.2)
        return np.array([y[1], V*y[1] + LAMU*(y[0] - w)])
    Y = rk4(rhs, [1.0, -.34], t, sub=10)
    return bvp_cols(Y)

def run(tag, exe, name, work):
    d = os.path.join(work, "run_%s_%s" % (tag, name))
    os.makedirs(d, exist_ok=True)
    src = os.path.join(ROOT, "examples", MODELS[name])
    shutil.copy(src, d)
    p = subprocess.run([os.path.abspath(exe), os.path.basename(src), "--silent"], cwd=d,
                       capture_output=True, text=True, timeout=300)
    a = np.loadtxt(os.path.join(d, "output.dat"))
    return a, (p.stdout + p.stderr)

def err(a, ref, cols):
    """max abs error, and max abs error over the column's range, per column."""
    n = min(len(a), len(ref)); res = []
    ok = ~np.isnan(ref[:n, 0]); a, ref = a[:n][ok], ref[:n][ok]; n = len(a)
    for j in range(cols):
        e = np.abs(a[1:n, 1 + j] - ref[1:n, j]); sc = max(np.abs(ref[1:n, j]).max(), 1e-300)
        res.append((e.max(), e.max() / sc))
    return n, res

def main():
    old, new = sys.argv[1], sys.argv[2]
    work = sys.argv[sys.argv.index("--work") + 1] if "--work" in sys.argv else tempfile.mkdtemp()
    for name in MODELS:
        print("==", name)
        outs = {lbl: run(lbl, exe, name, work) for lbl, exe in (("old", old), ("new", new))}
        for lbl, (a, log) in outs.items():
            print(f"  {lbl}: {len(a)} rows, t_end={a[-1,0]:.6g}")
        for lbl, (a, log) in outs.items():
            t = a[:, 0].astype(float)
            if name == "dae": refs = {"a": dae_ref_a(t), "b": dae_ref_b(t)}; nc = 2
            elif name == "exdaebvp": refs = {"a": bvp_ref_a(t), "b": bvp_ref_b(t)}; nc = 3
            else:
                refs = {"closed": ex3_closed(t), "a": ex3_ref_a(t), "b": ex3_ref_b(t)}; nc = 2
            if name != "dae_ex3":
                # the residual column (last) against the true value 0
                nres = a.shape[1] - 1
                print(f"  {lbl}: max |constraint residual| = {np.abs(a[1:, -1]).max():.3g}")
            if name == "dae_ex3":
                # past the fold t*=%g the DAE has no solution; count rows after it
                print(f"  {lbl}: rows at t >= t* ({TSTAR:.5f}): {(t >= TSTAR).sum()}")
                late = a[t >= TSTAR]
                if len(late): print(f"  {lbl}: after t*: max |v(1-v^2)-w| = {np.abs(late[:,2]*(1-late[:,2]**2)-late[:,1]).max():.3g}, v ranges {late[:,2].min():.3g}..{late[:,2].max():.3g}, w ranges {late[:,1].min():.3g}..{late[:,1].max():.3g}")
            for r, ref in refs.items():
                n, e = err(a, ref, nc)
                print(f"  {lbl} vs ref {r} (first {n} rows): " + ", ".join(f"col{j+1} abs {x[0]:.3g} rel {x[1]:.3g}" for j, x in enumerate(e)))
        # the references against each other
        t = outs["new"][0][:, 0].astype(float)
        if name == "dae": ra, rb = dae_ref_a(t), dae_ref_b(t)
        elif name == "exdaebvp": ra, rb = bvp_ref_a(t), bvp_ref_b(t)
        else: ra, rb = ex3_closed(t)[:len(ex3_ref_a(t))], ex3_ref_a(t)
        n = min(len(ra), len(rb)); print("  refs agree: max abs diff", np.abs(ra[:n, :2] - rb[:n, :2]).max())

main()
