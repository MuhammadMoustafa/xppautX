#!/usr/bin/env -S uv run --locked python
"""W246 (#302) exploration: find PY_S1Bf's Hopf points in Iapp with xppautX's AUTO
(--server, tools/xppclient.py) and print the labelled points. Not a timing run.

    uv run --locked python explore_xppautx.py XPPAUTX [PARMAX DSMAX NMAX PARMIN]
"""
import sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / 'tools'))
from xppclient import Server, is_done, open_auto
from autocompare import ensure_completed  # noqa: E402

def step(s, timeout=60, **c):
    if c:
        s.send(**c)
    events, end = s.collect(is_done, timeout=timeout)
    ensure_completed(events, end)
    return events, end


def numerics(s, **values):
    """AUTO's numerics without the form (docs/protocol.md, auto set); refused values arrive as an error message"""
    step(s, cmd='auto.configure_numerics', **values)
    return values


def labels(evs):
    out = []
    for e in evs:
        if e.get('ev') == 'auto.diagram' and e.get('op') == 'add':
            for r in e['runs']:
                for i, lab, sym in r.get('lab', []):
                    out.append((r['br'], r['pt'] + i, lab, sym, r['x'][i], r['y'][i]))
    return out


def main():
    binary = sys.argv[1]
    parmax, dsmax, nmax, parmin = (sys.argv[2:6] + ['20000', '100', '2000', '-500'][len(sys.argv[2:6]):])
    s = Server(binary, str(HERE / 'models' / 'PY_S1Bf.ode'))
    try:
        step(s)
        step(s, cmd='events.subscribe', events=[], diagram=True)
        step(s, cmd='run.initial', timeout=600)
        step(s, cmd='equilibrium.configure', print_eigenvalues=False, draw_strong_sets=False)
        step(s, cmd='equilibrium.find')
        step(s, cmd='equilibrium.import_as_ic')
        open_auto(s)
        print(numerics(s, parameter_max=float(parmax), parameter_min=float(parmin), norm_min=-200,
                       norm_max=200, max_step_size=float(dsmax), max_steps=int(nmax)))
        evs, _ = step(s, cmd='auto.run', kind='equilibrium', timeout=600)
        for lab in labels(evs):
            print(lab)
    finally:
        s.close()



if __name__ == '__main__':
    main()
