#!/usr/bin/env python3
"""Create and validate portable PY_S1Bf live-value demo files using core writers."""
import argparse
import json
import io
import csv
import shutil
import tempfile
import zipfile
from pathlib import Path
from xppclient import Server, is_idle, read_recx, replay_recording, file_bytes

parser = argparse.ArgumentParser()
parser.add_argument('--server', required=True)
parser.add_argument('--model', required=True)
parser.add_argument('--output', required=True)
args = parser.parse_args()
binary = str(Path(args.server).resolve())
model = str(Path(args.model).resolve())
output = Path(args.output).resolve()
output.mkdir(parents=True, exist_ok=True)

def command(server, **fields):
    server.send(**fields)
    events, idle = server.collect(is_idle, timeout=120)
    if idle is None or any(e.get('error') or e.get('ev') == 'ask' for e in events):
        raise AssertionError((fields, events[-6:]))
    return events

def state(events):
    return next(e for e in reversed(events) if e.get('ev') == 'state')

with tempfile.TemporaryDirectory(prefix='xpp-sandbox-fixture-') as folder:
    server = Server(binary, model, run=folder)
    try:
        initial, idle = server.collect(is_idle)
        assert idle is not None and len(state(initial)['ics']) == 8
        command(server, cmd='record', op='start')
        command(server, cmd='record', op='note', text='Short 200 ms run; original model equations and initial conditions unchanged.')
        command(server, cmd='set', kind='num', name='total', value=200)
        command(server, cmd='record', op='note', text='Resting cell: all eight states update, although only voltage is plotted.')
        server.send(cmd='key', key='i')
        _, ask = server.collect(lambda e: e.get('ev') == 'ask')
        assert ask is not None
        server.send(cmd='answer', id=ask['id'], key='g')
        events, idle = server.collect(is_idle, timeout=120)
        assert idle is not None and not any(e.get('error') for e in events)
        live = [e for e in events if e.get('ev') == 'liveState']
        assert live and all(len(e['now']) == 8 for e in live)
        command(server, cmd='record', op='note', text='Continue for another 200 ms: time advances to 400 ms and states keep updating.')
        server.send(cmd='key', key='c')
        _, ask = server.collect(lambda e: e.get('ev') == 'ask')
        assert ask is not None and ask.get('kind') == 'string'
        server.send(cmd='answer', id=ask['id'], ok=1, value='400')
        events, idle = server.collect(is_idle, timeout=120)
        assert idle is not None and not any(e.get('error') for e in events), events[-5:]
        final = state(events)
        assert final['time'] == 400

        command(server, cmd='record', op='note', text='Open AUTO to continue the resting equilibrium in applied current Iapp.')
        command(server, cmd='key', key='f')
        command(server, cmd='key', key='a')
        command(server, cmd='record', op='note', text='Use bounded equilibrium continuation in Iapp; allow the resting state norm.')
        command(server, cmd='auto', op='set', numerics={'ntst': 15, 'nmx': 1500, 'npr': 20,
            'ds': 1, 'dsmax': 50, 'rl0': -500, 'rl1': 17500, 'a0': -200, 'a1': 200, 'mxbf': 5})
        command(server, cmd='record', op='note', text='Show voltage V versus applied current Iapp, with fixed readable axes.')
        command(server, cmd='auto', op='set', axes={'plot': 0, 'var': 'V', 'par1': 'Iapp',
            'xmin': -20, 'xmax': 200, 'ymin': -90, 'ymax': -35})
        command(server, cmd='record', op='note', text='Run steady-state continuation; AUTO marks stability changes and labelled special points.')
        server.send(cmd='key', win='auto', key='r')
        _, ask = server.collect(lambda e: e.get('ev') == 'ask')
        assert ask is not None
        server.send(cmd='answer', id=ask['id'], key='s')
        events, idle = server.collect(is_idle, timeout=120)
        assert idle is not None and not any(e.get('error') for e in events), events[-5:]
        final = state(events)
        command(server, cmd='record', op='stop', name='PY_S1Bf-live.recx')
        command(server, cmd='session', op='save', name='PY_S1Bf-live.snapx', data=True)
    finally:
        server.close()
    rec = Path(folder) / 'PY_S1Bf-live.recx'
    snap = Path(folder) / 'PY_S1Bf-live.snapx'
    header, files, steps, fingerprint, computed = read_recx(rec.read_text(encoding='utf-8'))
    assert fingerprint == computed and len(steps) >= 7
    assert steps[2][0]['keys'] == ['c'] and steps[2][0]['answers'] == ['400']
    with tempfile.TemporaryDirectory(prefix='xpp-sandbox-replay-') as replay:
        quiet = replay_recording(binary, model, str(rec), replay)
        events = [json.loads(line) for line in quiet.stdout.splitlines()]
        assert quiet.returncode == 0, quiet.stderr
        played = state(events)
        for key in ('pars', 'ics', 'now', 'time', 'rows'):
            assert played[key] == final[key], (key, played[key], final[key])
    with zipfile.ZipFile(snap) as archive:
        auto_saved = {name: archive.read(name) for name in
            ('auto/diagram.csv', 'auto/solutions.s', 'auto/settings.txt', 'auto/views.txt')}
    point_rows = [line for line in auto_saved['auto/diagram.csv'].decode().splitlines() if line and not line.startswith('#')]
    points = list(csv.DictReader(io.StringIO(auto_saved['auto/diagram.csv'].decode())))
    assert len(points) > 20, points
    assert any(int(point['itp']) == 3 for point in points), 'Hopf point must be retained'
    assert any(int(point['itp']) == 2 for point in points), 'Fold point must be retained'
    assert auto_saved['auto/solutions.s'], 'AUTO must retain labelled solutions'
    # Normal paced, automatic first playback exercises the user's path, in
    # addition to silent and fast-forward playback above.
    paced = Server(binary, str(rec))
    try:
        events, ended = paced.collect(lambda e: e.get('error') or (e.get('ev') == 'state' and
            (e.get('player') or {}).get('step') == len(steps) and e['player']['running'] == -1), timeout=120)
        assert ended is not None and not any(e.get('error') for e in events), events[-5:]
        assert any(e.get('ev') == 'state' and (e.get('player') or {}).get('playing') for e in events)
        played = state(events)
        for key in ('pars', 'ics', 'now', 'time', 'rows'):
            assert played[key] == final[key], key
        paced.collect(is_idle)
        command(paced, cmd='session', op='save', name='paced.snapx', data=True)
        with zipfile.ZipFile(io.BytesIO(file_bytes(paced, 'paced.snapx'))) as archive:
            for name, body in auto_saved.items():
                assert archive.read(name) == body, 'Paced replay: ' + name
    finally:
        paced.close()
    restored = Server(binary, str(snap))
    try:
        loaded, idle = restored.collect(is_idle)
        assert idle is not None and not any(e.get('error') for e in loaded), loaded[-5:]
        saved = state(loaded)
        for key in ('pars', 'ics', 'now', 'time', 'rows'):
            assert saved[key] == final[key], (key, saved.get(key), final[key])
        command(restored, cmd='session', op='save', name='restored.snapx', data=True)
        with zipfile.ZipFile(io.BytesIO(file_bytes(restored, 'restored.snapx'))) as archive:
            for name, body in auto_saved.items():
                assert archive.read(name) == body, 'Snapshot restore: ' + name
    finally:
        restored.close()
    for path in (rec, snap):
        shutil.copy2(path, output / path.name)
    print('PASS: 8 live states; intact recording replays silently, fast and normally; snapshot restores %d trajectory rows and %d AUTO diagram lines with identical settings and solutions' % (final['rows'], len(point_rows)))
