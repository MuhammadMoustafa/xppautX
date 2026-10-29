"""W378: deterministic stiff networks; n always counts state variables."""
import argparse
import json
import math
from pathlib import Path


def generate(family, n, folder):
    if n < 6 or (family == 'ring' and n % 2):
        raise ValueError('n must be >= 6, and even for the two-state ring')
    folder.mkdir(parents=True, exist_ok=True)
    lines = ['version 4', f'# W378 {family}, {n} states; generated, do not edit']
    initial = []
    if family == 'cable':
        # Unit cable, Dirichlet ends: refining n increases diffusion stiffness.
        rate = (n + 1)**2
        for i in range(n):
            left = f'x{i-1}' if i else '0'
            right = f'x{i+1}' if i + 1 < n else '0'
            lines.append(f"x{i}'={rate}*({left}-2*x{i}+{right})-x{i}")
            # Multiple modes and a local perturbation avoid a one-mode LU test.
            z = (i + 1) / (n + 1)
            initial.append(math.sin(math.pi*z) + .2*math.sin(7*math.pi*z)
                           + .1*math.exp(-200*(z-.3)**2))
        names = [f'x{i}' for i in range(n)]
        total = .2
    else:
        cells = n // 2
        # Fold a cycle into a path: 0,1,m-1,2,m-2,... . Adjacent cycle
        # cells differ by at most two slots; interleaved v,w gives band 4.
        order = [0]
        for i in range(1, cells//2 + 1):
            order.append(i)
            if cells-i != i:
                order.append(cells-i)
        names = [f'{v}{i}' for i in order for v in ('v', 'w')]
        lines += ['fun minf(v)=.5*(1+tanh((v+.01)/.15))',
                  'fun winf(v)=.5*(1+tanh((v-.1)/.145))',
                  'fun lam(v)=.333*cosh((v-.1)/.29)']
        for i in order:
            # lecdiff's conductances, with capacitance .001 and applied .1.
            lines += [f"v{i}'=(.1+.5*(-.5-v{i})+2*w{i}*(-.7-v{i})-minf(v{i})*(v{i}-1)+5*(v{(i-1)%cells}-2*v{i}+v{(i+1)%cells}))/.001",
                      f"w{i}'=lam(v{i})*(winf(v{i})-w{i})"]
            initial += [-.49+.05*math.cos(2*math.pi*i/cells), .1]
        total = 2.
    lines += [f'init {name}={value:.17g}' for name, value in zip(names, initial)]
    lines += ['options { integration: { method: cvode, total_time: '+str(total)+', dt: '+str(total/100)+' } }']
    path = folder / f'{family}-{n}.odex'
    path.write_text('\n'.join(lines)+'\n')
    meta = dict(family=family, n=n, total=total, rows=101, initial=initial,
                order=names, expected_band=1 if family == 'cable' else 4)
    path.with_suffix('.json').write_text(json.dumps(meta, indent=2)+'\n')
    return path


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sizes', nargs='+', type=int, default=[1000, 2000, 3000, 5000])
    ap.add_argument('--out', type=Path, required=True)
    a = ap.parse_args()
    for family in ('cable', 'ring'):
        for n in a.sizes:
            print(generate(family, n, a.out))
