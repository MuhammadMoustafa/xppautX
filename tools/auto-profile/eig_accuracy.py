#!/usr/bin/env python3
"""W358 (#420): how accurate EISPACK's and LAPACK's eigenvalues are, on the matrices our AUTO gives them.

    uv run python tools/auto-profile/eig_accuracy.py DIR [--per-file N] [--dps D]

DIR holds, per workload, NAME.matrices (tools/auto-profile/dump_matrices.py, from the program under gdb)
and NAME.results (build/<lapack build>/eigcompare NAME.matrices: both libraries' answers on each
matrix). The reference is computed here with mpmath at D digits (default 50) from the exact doubles
that were dumped: eigenvalues of A for an `rg` matrix, of B^-1 A for a `qz` pencil (flowkm's deflated
Floquet pencil). Needs mpmath: run it with `uv run python` (pyproject.toml, uv.lock).

Per workload and kind it prints, for each library over the matrices sampled (at most N, evenly spaced):
the largest and the median error of the eigenvalues, each eigenvalue matched with its nearest
unmatched reference, as
  - absolute error / ||A||_F (||A||_F + ||B||_F for a pencil): the normwise backward-stable size;
  - relative error |mu-lambda| / |lambda| over the eigenvalues with |lambda| > 1e-6 ||A||_F.
A pencil with an infinite eigenvalue (beta = 0 in either library or det B = 0 in the reference) is
counted apart and left out.
"""
import argparse, statistics
from pathlib import Path

import mpmath as mp


def read_matrices(path):
    out = []
    for line in path.read_text().splitlines():
        t = line.split()
        kind, nm, n = t[0], int(t[1]), int(t[2])
        if kind == 'rg':
            a = [float.fromhex(x) for x in t[4:]]
            out.append((kind, n, [a]))
        else:
            v = [float.fromhex(x) for x in t[3:]]
            out.append((kind, n, [v[:n * n], v[n * n:]]))
    return out


def read_results(path):
    """per matrix {library: (ierr, [numbers])}"""
    out, current = [], None
    for line in path.read_text().splitlines():
        t = line.split()
        if t[0] in ('rg', 'qz'):
            current = {}
            out.append(current)
        else:
            current[t[0]] = (int(t[1]), [float.fromhex(x) for x in t[2:]])
    return out


def column_major(values, n):
    return mp.matrix([[values[i + j * n] for j in range(n)] for i in range(n)])


def reference(kind, n, mats):
    """the exact eigenvalues (mp.mpc) of the dumped matrix, or None for a pencil with an infinite one"""
    a = column_major(mats[0], n)
    if kind == 'qz':
        b = column_major(mats[1], n)
        sv = mp.svd_r(b, compute_uv=False)  # B counts as singular below 1e-30 of its largest singular value
        if min(sv) <= mp.mpf(10) ** -30 * max(sv):
            return None
        a = mp.inverse(b) * a
    return [mp.mpc(z) for z in mp.eig(a, left=False, right=False)]


def computed(kind, result):
    ierr, v = result
    if kind == 'rg':
        return ierr, [complex(v[i], v[i + 1]) for i in range(0, len(v), 2)]
    triples = [v[i:i + 3] for i in range(0, len(v), 3)]
    if any(beta == 0 for _, _, beta in triples):
        return ierr, None
    return ierr, [complex(ar, ai) / beta for ar, ai, beta in triples]


def errors(ref, got, scale):
    """(absolute error / scale, relative error) of the worst matched eigenvalue"""
    free = list(range(len(got)))
    worst_abs = worst_rel = 0.0
    for lam in sorted(ref, key=lambda z: -abs(z)):
        j = min(free, key=lambda k: abs(mp.mpc(got[k]) - lam))
        free.remove(j)
        e = abs(mp.mpc(got[j]) - lam)
        worst_abs = max(worst_abs, float(e / scale))
        if abs(lam) > 1e-6 * scale:
            worst_rel = max(worst_rel, float(e / abs(lam)))
    return worst_abs, worst_rel


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('dir')
    ap.add_argument('--per-file', type=int, default=6)
    ap.add_argument('--dps', type=int, default=50)
    args = ap.parse_args()
    mp.mp.dps = args.dps
    print('| workload | kind | n | matrices | infinite | EISPACK abs/norm max | median | rel max | LAPACK abs/norm max | median | rel max | failures E/L |')
    print('|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
    for mpath in sorted(Path(args.dir).glob('*.matrices')):
        matrices = read_matrices(mpath)
        results = read_results(mpath.with_suffix('.results'))
        for kind in ('rg', 'qz'):
            idx = [i for i, m in enumerate(matrices) if m[0] == kind]
            if not idx:
                continue
            step = max(1, len(idx) // args.per_file)
            sample = idx[::step][:args.per_file]
            n = matrices[idx[0]][1]
            stats = {'eispack': ([], []), 'lapack': ([], [])}
            fails = {'eispack': 0, 'lapack': 0}
            infinite = 0
            for i in sample:
                _, n, mats = matrices[i]
                ref = reference(kind, n, mats)
                scale = mp.sqrt(sum(mp.mpf(x) ** 2 for m in mats for x in m))
                if ref is None:
                    infinite += 1
                    continue
                for lib in stats:
                    ierr, got = computed(kind, results[i][lib])
                    fails[lib] += ierr != 0
                    if got is None:
                        infinite += 1
                        continue
                    a, r = errors(ref, got, scale)
                    stats[lib][0].append(a)
                    stats[lib][1].append(r)
            cells = []
            for lib in ('eispack', 'lapack'):
                a, r = stats[lib]
                cells += [f'{max(a):.2e}', f'{statistics.median(a):.2e}', f'{max(r):.2e}'] if a else ['-'] * 3
            print(f'| {mpath.stem} | {kind} | {n} | {len(sample)} | {infinite} | ' + ' | '.join(cells) +
                  f' | {fails["eispack"]}/{fails["lapack"]} |', flush=True)


if __name__ == '__main__':
    main()
