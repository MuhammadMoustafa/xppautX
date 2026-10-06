#!/usr/bin/env python3
"""Create and validate portable PY_S1Bf live-value demo files using core writers."""
import argparse
import json
import shutil
import tempfile
from pathlib import Path
from xppclient import Server, is_idle, read_recx, replay_recording

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
        final = state(command(server, cmd='continue', extra=200))
        command(server, cmd='record', op='stop', name='PY_S1Bf-live.recx')
        command(server, cmd='session', op='save', name='PY_S1Bf-live.snapx', data=True)
    finally:
        server.close()
    rec = Path(folder) / 'PY_S1Bf-live.recx'
    snap = Path(folder) / 'PY_S1Bf-live.snapx'
    header, files, steps, fingerprint, computed = read_recx(rec.read_text(encoding='utf-8'))
    assert fingerprint == computed and len(steps) == 3
    with tempfile.TemporaryDirectory(prefix='xpp-sandbox-replay-') as replay:
        quiet = replay_recording(binary, model, str(rec), replay)
        events = [json.loads(line) for line in quiet.stdout.splitlines()]
        assert quiet.returncode == 0, quiet.stderr
        played = state(events)
        for key in ('pars', 'ics', 'now', 'time', 'rows'):
            assert played[key] == final[key], (key, played[key], final[key])
    restored = Server(binary, str(snap))
    try:
        loaded, idle = restored.collect(is_idle)
        assert idle is not None and not any(e.get('error') for e in loaded), loaded[-5:]
        saved = state(loaded)
        for key in ('pars', 'ics', 'now', 'time', 'rows'):
            assert saved[key] == final[key], (key, saved.get(key), final[key])
    finally:
        restored.close()
    for path in (rec, snap):
        shutil.copy2(path, output / path.name)
    print('PASS: 8 live states; intact 3-step recording replays identically; snapshot restores all states, parameters, time and %d trajectory rows' % final['rows'])
