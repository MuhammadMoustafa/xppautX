#!/usr/bin/env python3
"""All-state live values: doubles, every output step, independent of stored plots."""
import argparse
import math
import tempfile
from pathlib import Path
from xppclient import Checker, Server, is_idle

parser = argparse.ArgumentParser()
parser.add_argument('--server', default='./xppautX')
args = parser.parse_args()
check = Checker(detail_sep=': ')
ABS_TOL = 2e-15  # the solver's doubles are exact to rounding for these linear models
RATE_TOL = 2e-6  # the rate is a difference quotient of the stored float32 rows


def close(got, want, tol):
    return len(got) == len(want) and all(math.isclose(a, b, rel_tol=0, abs_tol=tol) for a, b in zip(got, want))


def integrate(server):
    """Key i, answer g (go): the events up to idle."""
    server.send(cmd='key', key='i')
    _, ask = server.collect(lambda e: e.get('ev') == 'ask')
    check('integrate menu asks', ask is not None)
    if ask is None:
        return [], None
    server.send(cmd='answer', id=ask['id'], key='g')
    return server.collect(is_idle)


with tempfile.TemporaryDirectory(prefix='xpp-live-') as folder:
    model = Path(folder) / 'live.ode'
    model.write_text("x'=1\ny'=2\nz'=3\ninit x=1.123456789012345,y=2,z=3\n@ total=1,dt=0.125,meth=euler,trans=.5\ndone\n", encoding='utf-8')
    server = Server(args.server, str(model), env={'XPP_NO_THROTTLE': '1'})
    try:
        server.collect(is_idle)
        events, idle = integrate(server)
        check('all-state run completes without error', idle is not None and not any(e.get('error') for e in events), events[-3:])
        live = [e for e in events if e.get('ev') == 'liveState']
        check('one liveState per output step', len(live) == 8, len(live))
        check('all 3 solver doubles at every output step, including the transient',
              all(e['time'] == i / 8 and close(e['now'], [1.123456789012345 + e['time'], 2 + 2 * e['time'], 3 + 3 * e['time']], ABS_TOL)
                  for i, e in enumerate(live, 1)), live[:2])
        states = [e for e in events if e.get('ev') == 'state']
        final = states[-1] if states else {}
        check('final state equals the last liveState', bool(live) and bool(final) and close(final['now'], live[-1]['now'], ABS_TOL), final.get('now'))
        check('final rates are the model derivatives', bool(final) and close(final['rates'], [1, 2, 3], RATE_TOL), final.get('rates'))
        check('liveState carries the rates', bool(live) and len(live[-1]['rates']) == 3)
        check('initial conditions are full doubles', bool(final) and final['ics'] == [['X', 1.123456789012345], ['Y', 2], ['Z', 3]], final.get('ics'))
    finally:
        server.close()

for name, text, expected in [
    ('returning-map', "x'=2-x\ny'=y\nz'=if(t<2)then(z+1)else(z)\ninit x=0,y=2,z=0\n@ total=16,dt=1,meth=discrete", [2, 0, 0]),
    ('backward', "x'=1\ny'=2\nz'=3\ninit x=0,y=0,z=0\n@ total=1,dt=-.125,meth=euler", [1, 2, 3]),
]:
    with tempfile.TemporaryDirectory(prefix='xpp-tail-') as folder:
        model = Path(folder) / (name + '.ode')
        model.write_text(text + '\ndone\n', encoding='utf-8')
        server = Server(args.server, str(model))
        try:
            startup, idle = server.collect(is_idle)
            initial = [e for e in startup if e.get('ev') == 'state'][-1]
            check(name + ': no rates before a run', initial['rates'] == [None, None, None], initial['rates'])
            events, idle = integrate(server)
            check(name + ': run completes without error', idle is not None and not any(e.get('error') for e in events), events[-5:])
            states = [e for e in events if e.get('ev') == 'state']
            check(name + ': rates over the recent window', bool(states) and states[-1]['rates'] == expected, states[-1]['rates'] if states else None)
        finally:
            server.close()

print(f'live state checks: {check.checks} checks, {check.failures} failed')
raise SystemExit(bool(check.failures))
