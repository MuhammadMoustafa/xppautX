"""xppautX's and AUTO-07p's error along Mu26's to_snic, against the model's own cycles (W584, #649).

Both programs at the same constants (NTST 50, NCOL 4, EPSL = EPSU 1e-7, EPSS 1e-5, difference Jacobians, AUTO-07p's JAC=0),
from the W557 timed series' pilots (~/xppautx-comparison/final/continuation/attempt-001/ode_mu26_{blocked,control}/adaptive:
pilot-xppautx/diagram.csv, pilot-auto07p/files/series/b.r2 and d.r2). Every third point of each program's to_snic with a period
over 300 is checked at that program's own parameter by docs/studies/floquet_check.py --norm (the model's equations, exact
derivatives, no collocation). First print the parameters, then give floquet_check.py's output for each program:

    uv run --locked python docs/studies/w584_snic_accuracy.py DATA --samples
    uv run --locked python docs/studies/floquet_check.py --description ode_mu26_blocked --section SmV=-20 --norm \
        --parameter SmIapp <ours' parameters> > ours.txt   (and <AUTO-07p's> > auto.txt)
    uv run --locked python docs/studies/w584_snic_accuracy.py DATA --ours ours.txt --auto auto.txt

Prints each sample's norm, period and neutral multiplier error (the last against its exact value, 1; AUTO-07p's multipliers have six digits in d.r2).
"""
import argparse
import csv
import json
import re
from pathlib import Path

LONG_PERIOD = 300  # the cycles whose collocation system becomes nearly singular towards the SNIC
EVERY = 3  # of those points, every third is checked
SAMPLES = 10  # at most, from the run's start: AUTO-07p's run goes on past xppautX's last point
RUN = 'to_snic'
AUTO07P_RUN = 'r2'  # AUTO-07p's files of to_snic, the third run of the description


def ours(data):
    """xppautX's to_snic points: parameter -> (norm, period, the multiplier nearest 1)"""
    first, last = json.loads((data / 'pilot-xppautx/run-ranges.json').read_text())[RUN]
    with open(data / 'pilot-xppautx/diagram.csv', newline='') as f:
        rows = list(csv.DictReader(line for line in f if not line.startswith('branch,')))[first:last]
    points = {}
    for r in rows:
        multipliers = [complex(float(r[k]), float(r[k.replace('evr', 'evi')])) for k in r if k.startswith('evr')]
        points[float(re.split(r'[;\s]', r['values'])[0])] = (float(r['norm']), float(r['per']), neutral(multipliers))
    return points


def auto07p(data):
    """AUTO-07p's to_snic points, the same, its multipliers the last ones d.r2 prints for each point"""
    series = data / 'pilot-auto07p/files/series'
    multipliers = {}
    for line in (series / ('d.' + AUTO07P_RUN)).read_text().splitlines():
        m = re.match(r'\s+\d+\s+(\d+)\s+Multiplier\s+(\d+)\s+(\S+)\s+(\S+)', line)
        if m:
            if m[2] == '1':
                multipliers[int(m[1])] = []
            multipliers[int(m[1])].append(complex(float(m[3]), float(m[4])))
    points = {}
    for line in (series / ('b.' + AUTO07P_RUN)).read_text().splitlines():
        w = line.split()
        if len(w) > 8 and w[0] != '0':
            point = abs(int(w[1]))
            points[float(w[4])] = (float(w[5]), float(w[-1]), neutral(multipliers[point]) if point in multipliers else float('nan'))
    return points


def neutral(multipliers):
    return min(multipliers, key=lambda z: abs(z - 1)).real


def samples(points):
    return [p for p, (_, period, _) in sorted(points.items(), reverse=True) if period > LONG_PERIOD][::EVERY][:SAMPLES]


def truth(path):
    """floquet_check.py's output: parameter -> (period, norm)"""
    found = {}
    for line in Path(path).read_text().splitlines():
        m = re.match(r'\S+ (\S+): period (\S+), L2 norm (\S+)', line)
        if m:
            found[float(m[1])] = (float(m[2]), float(m[3]))
    return found


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('data', type=Path, help="a workload's adaptive folder of the timed series")
    p.add_argument('--samples', action='store_true', help='print the parameters to check')
    p.add_argument('--ours', help="floquet_check.py's output at xppautX's parameters")
    p.add_argument('--auto', help="floquet_check.py's output at AUTO-07p's parameters")
    a = p.parse_args()
    programs = {'xppautX': ours(a.data), 'AUTO-07p': auto07p(a.data)}
    if a.samples:
        for name, points in programs.items():
            print(name, ' '.join('%.12g' % v for v in samples(points)))
        return
    print('%-9s %16s %11s %11s %11s' % ('program', 'parameter', 'norm', 'period', 'multiplier'))
    for name, path in (('xppautX', a.ours), ('AUTO-07p', a.auto)):
        points = programs[name]
        for parameter, (period, norm) in sorted(truth(path).items(), reverse=True):
            n, per, mu = points[min(points, key=lambda q: abs(q - parameter))]
            print('%-9s %16.11g %11.2e %11.2e %11.2e' % (name, parameter, n - norm, per - period, mu - 1))  # the neutral multiplier is exactly 1


if __name__ == '__main__':
    main()
