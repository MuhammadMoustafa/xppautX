#!/usr/bin/env python3
"""Core unchanged-digits and direct continuation regressions, using the shared server client."""
import argparse
import json
import math
from pathlib import Path
import tempfile
from xppclient import Server, is_idle, whole_series, make_recording, replay_recording

parser = argparse.ArgumentParser()
parser.add_argument('--server', default='./xppautX')
args = parser.parse_args()
checks = failures = 0

def check(name, condition, detail=''):
    global checks, failures
    checks += 1
    failures += not condition
    print(('PASS ' if condition else 'FAIL ') + name + (': ' + str(detail) if not condition else ''))

def command(server, **fields):
    server.send(**fields)
    events, idle = server.collect(is_idle, timeout=60)
    check('command completes: ' + fields['cmd'], idle is not None, events[-3:])
    state = next((e for e in reversed(events) if e.get('ev') == 'state'), None)
    return events, state

def scenario(folder, name, model, action):
    ode = folder / (name + '.ode')
    ode.write_text(model + '\ndone\n', encoding='utf-8')
    server = Server(args.server, str(ode))
    try:
        events, idle = server.collect(is_idle)
        check(name + ': model loads', idle is not None and not any(e.get('error') for e in events), events[-3:])
        server.send(cmd='data', events=['series', 'numerics'])
        server.collect(is_idle)
        action(server)
    finally:
        server.close()

with tempfile.TemporaryDirectory(prefix='xpp-steady-') as temp:
    folder = Path(temp)

    def decays(server):
        events, state = command(server, cmd='steady', decimals=9, hold=1, maximum=50)
        check('decay: settles before the limit, after the nine-decimal transient',
              state and state['steady']['status'] == 'settled' and 20 < state['time'] < 50, state)
        check('decay: both states approach their analytic equilibria',
              state and abs(state['now'][0]) < 1e-8 and abs(state['now'][1] - 2) < 1e-8, state)
        server.send(cmd='data', events=['series', 'numerics'])
        settings, _ = server.collect(is_idle)
        check('decay: original duration and stride restored',
              any(e.get('ev') == 'numerics' and {f['key']: f['value'] for f in e['fields']}.get('total') == 30
                  and {f['key']: f['value'] for f in e['fields']}.get('nout') == 7 for e in settings), settings[-4:])
        for invalid in [dict(decimals=9.5, hold=1, maximum=10), dict(decimals=True, hold=1, maximum=10),
                        dict(decimals=9, hold=0, maximum=10), dict(decimals=9, hold=2, maximum=1),
                        dict(decimals=9, hold=1, maximum=1e100), dict(decimals='9', hold=1, maximum=10)]:
            got, after = command(server, cmd='steady', **invalid)
            check('invalid steady input is rejected without changing the trajectory',
                  any(e.get('error') for e in got) and after and after['now'] == state['now'] and after['time'] == state['time'], invalid)
        old_time, old_rows = state['time'], state['rows']
        # Deliberately malformed JSON: the legacy numeric parser accepts
        # signed NaN tokens, which must never reach an integer precision cast.
        server.proc.stdin.write('{"cmd":"steady","decimals":-NaN,"hold":1,"maximum":10}\n')
        server.proc.stdin.flush()
        got, idle = server.collect(is_idle)
        after = next((e for e in reversed(got) if e.get('ev') == 'state'), None)
        check('invalid signed NaN precision is rejected before integer conversion',
              idle is not None and any(e.get('error') for e in got) and after and after['now'] == state['now'] and after['time'] == old_time, got[-3:])
        got, after = command(server, cmd='continue', extra=1)
        check('extra duration continues from the full core time and appends rows',
              after and abs(after['time'] - (old_time + 1)) < 1e-8 and after['rows'] > old_rows and 'steady' not in after, after)
        target = after['time'] + 1
        got, until = command(server, cmd='continue', until=target)
        check('until time continues to the chosen end time without asking a question',
              until and abs(until['time'] - target) < 1e-8 and not any(e.get('ev') == 'ask' for e in got), until)
        for invalid in [dict(extra=0), dict(until=target - 1), dict(extra=1, until=target + 1), dict(extra='1'),
                         dict(until=until['time'] + .075), dict(until=until['time'] + 1.02)]:
            got, after = command(server, cmd='continue', **invalid)
            check('invalid continuation does not mutate current values or time',
                  any(e.get('error') for e in got) and after and after['now'] == until['now'] and after['time'] == until['time'], invalid)
        got, one_step = command(server, cmd='continue', until=until['time'] + .05)
        check('continuation: one Dt is accepted despite subtraction rounding',
              one_step and abs(one_step['time'] - (until['time'] + .05)) < 1e-8 and not any(e.get('error') for e in got), one_step)
        # An ordinary legacy run remains unchanged and clears the transient result.
        server.send(cmd='key', menu='main', item='initialconds')
        got, ask = server.collect(lambda e: e.get('ev') == 'ask')
        check('ordinary Go opens the legacy integration menu', ask is not None, got[-3:])
        got, normal = command(server, cmd='answer', key='g')
        check('ordinary Go still uses the original duration and output stride',
              normal and abs(normal['time'] - 30.1) < 1e-8 and normal['rows'] == 87 and 'steady' not in normal, normal)

    scenario(folder, 'decay', "x'=-x\ny'=-(y-2)\ninit x=1,y=0\n@ dt=.05,total=30,nout=7,bound=100", decays)

    def periodic(server):
        got, state = command(server, cmd='steady', decimals=9, hold=1, maximum=5)
        check('periodic: stops at the duration limit, never reports settled',
              state and state['steady']['status'] == 'limit' and abs(state['time'] - 5) < 1e-8, state)
        check('periodic: trajectory agrees with the analytic oscillator',
              state and abs(state['now'][0] - math.cos(5)) < 1e-6 and abs(state['now'][1] + math.sin(5)) < 1e-6, state)
    scenario(folder, 'periodic', "x'=y\ny'=-x\ninit x=1,y=0\n@ dt=.01,total=10,bound=10", periodic)

    def tiny_drift(server):
        got, state = command(server, cmd='steady', decimals=9, hold=1, maximum=5)
        check('tiny drift: ninth-place movement prevents float32 false convergence',
              state and state['steady']['status'] == 'limit' and state['now'][0] > 1 + 4e-9, state)
        series = whole_series(got)
        check('tiny drift: plotted float32 values actually hide this motion',
              bool(series) and all(value == 1 for value in series[-1]['columns'][1]['data']), series[-1:] if not series else '')
    scenario(folder, 'tiny-drift', "x'=1e-9\ninit x=1\n@ dt=1,total=5,bound=10", tiny_drift)

    def diverges(server):
        got, state = command(server, cmd='steady', decimals=9, hold=1, maximum=10)
        check('divergence: existing bounds stop the run with a placed error',
              state and state['steady']['status'] == 'failed' and state['time'] < 1 and
              any('out of bounds' in e.get('error', '') and e.get('file') and e.get('line') for e in got), got[-4:])
    scenario(folder, 'diverges', "x'=x\ninit x=1\n@ dt=.05,total=10,bound=2", diverges)

    def nonfinite(server):
        got, state = command(server, cmd='steady', decimals=9, hold=1, maximum=5)
        check('nonfinite: cannot be reported as settled',
              state and state['steady']['status'] == 'failed' and any(e.get('error') for e in got), got[-4:])
    scenario(folder, 'nonfinite', "x'=sqrt(-1)\ninit x=1\n@ dt=.1,total=5,bound=10", nonfinite)

    def storage(server):
        got, state = command(server, cmd='steady', decimals=9, hold=1, maximum=10)
        check('storage: stops with a reason instead of opening a storage prompt',
              state and state['steady']['status'] == 'storage-limit' and not any(e.get('ev') == 'ask' for e in got), state)
    scenario(folder, 'storage', "x'=1\ninit x=0\n@ dt=.1,total=10,maxstor=10,bound=100", storage)

    def discrete(server):
        got, state = command(server, cmd='steady', decimals=9, hold=2, maximum=50)
        check('discrete: compares each iteration and converges on the map fixed point',
              state and state['steady']['status'] == 'settled' and abs(state['now'][0]) < 1e-9, state)
    scenario(folder, 'discrete', "x(t+1)=.5*x\ninit x=1\n@ meth=Discrete,dt=1,total=50,bound=100", discrete)

    def adaptive(server):
        got, state = command(server, cmd='steady', decimals=9, hold=1, maximum=50)
        check('adaptive: compares requested Dt intervals and reaches unchanged digits',
              state and state['steady']['status'] == 'settled' and abs(state['now'][0]) < 1e-8, state)
    scenario(folder, 'adaptive', "x'=-x\ninit x=1\n@ meth=QualRK,dt=.1,total=50,tol=1e-12,bound=100", adaptive)

    recording_folder = folder / 'recording'
    recording_folder.mkdir()
    model = folder / 'decay.ode'
    steps = [dict(cmd='steady', decimals=9, hold=1, maximum=50), dict(cmd='continue', extra=1)]
    recorded = make_recording(args.server, str(model), steps, str(recording_folder))
    replay = replay_recording(args.server, str(model), recorded, str(recording_folder))
    check('recording: steady and direct continuation replay silently', replay.returncode == 0, replay.stderr)
    replay_states = [e for line in replay.stdout.splitlines() if (e := json.loads(line)).get('ev') == 'state' and e.get('now')]
    check('recording: replay appends after the settled trajectory',
          bool(replay_states) and 24 < replay_states[-1]['time'] < 50 and 'steady' not in replay_states[-1], replay_states[-1:] if replay_states else '')

    cancel_folder = folder / 'cancel'
    cancel_folder.mkdir()
    cancel_steps = [steps[0], dict(cmd='abort', at=dict(what='integrate', rows=100, t=0))]
    cancelled = make_recording(args.server, str(model), cancel_steps, str(cancel_folder))
    replay = replay_recording(args.server, str(model), cancelled, str(cancel_folder))
    cancel_states = [e for line in replay.stdout.splitlines() if (e := json.loads(line)).get('ev') == 'state' and e.get('steady')]
    check('cancellation: replayed Stop keeps the partial trajectory and reports stopped',
          replay.returncode == 0 and bool(cancel_states) and cancel_states[-1]['steady']['status'] == 'stopped' and cancel_states[-1]['rows'] == 100,
          (replay.stderr, cancel_states[-1:]))

print(f'steady checks: {checks} checks, {failures} failed')
raise SystemExit(bool(failures))
