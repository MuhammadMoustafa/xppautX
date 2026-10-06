#!/usr/bin/env python3
"""All-state live values: doubles, every output step, independent of stored plots."""
import argparse
import math
import tempfile
from pathlib import Path
from xppclient import Server, is_idle

parser = argparse.ArgumentParser()
parser.add_argument('--server', default='./xppautX')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='xpp-live-') as folder:
    model = Path(folder) / 'live.ode'
    model.write_text("x'=1\ny'=2\nz'=3\ninit x=1.123456789012345,y=2,z=3\n@ total=1,dt=0.125,meth=euler,trans=.5\ndone\n", encoding='utf-8')
    server = Server(args.server, str(model), env={'XPP_NO_THROTTLE': '1'})
    try:
        server.collect(is_idle)
        server.send(cmd='key', key='i')
        _, ask = server.collect(lambda e: e.get('ev') == 'ask')
        assert ask is not None
        server.send(cmd='answer', id=ask['id'], key='g')
        events, idle = server.collect(is_idle)
        assert idle is not None and not any(e.get('error') for e in events), events
        live = [e for e in events if e.get('ev') == 'liveState']
        assert len(live) == 8, live
        for index, event in enumerate(live, 1):
            assert event['time'] == index / 8, event
            expected = [1.123456789012345 + event['time'], 2 + 2 * event['time'], 3 + 3 * event['time']]
            assert len(event['now']) == 3 and all(math.isclose(a, b, rel_tol=0, abs_tol=2e-15) for a, b in zip(event['now'], expected)), event
        final = [e for e in events if e.get('ev') == 'state'][-1]
        assert all(math.isclose(a, b, rel_tol=0, abs_tol=2e-15) for a, b in zip(final['now'], live[-1]['now']))
        assert final['ics'] == [['X', 1.123456789012345], ['Y', 2], ['Z', 3]], final['ics']
        print('PASS: all 3 solver doubles at all 8 output steps, including transient; final state and initial conditions verified')
    finally:
        server.close()
