"""File-validation and session round-trip review probes.

Run from a built repository's root:
    python3 tools/review_session_probes.py [./xppautX]

Each subprocess and scratch folder belongs to this probe and is closed on
completion. JSON lines describe observed behavior; exit 1 means an invariant
failed. These failures are expected on the reviewed 6eef7f710 snapshot.
"""
import json
import os
import sys
import zipfile

sys.path.insert(0, os.path.join(os.getcwd(), 'tools'))
from xppclient import Server, is_idle, is_ask

binary = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else './xppautX')
model = os.path.abspath('examples/ode/lecar.ode')
failures = 0

def report(observation, passed):
    global failures
    if not passed:
        failures += 1
    print(json.dumps(dict(observation, status='PASS' if passed else 'FAIL')), flush=True)

def run(s, **cmd):
    s.send(**cmd)
    events, end = s.collect(lambda e: is_idle(e) or is_ask(e), timeout=5)
    while end and is_ask(end):
        s.send(cmd='answer', id=end['id'], key='d')
        more, end = s.collect(lambda e: is_idle(e) or is_ask(e), timeout=5)
        events.extend(more)
    return events

def rewrite(src, dst, change):
    with zipfile.ZipFile(src) as z:
        members = {n: z.read(n) for n in z.namelist()}
    change(members)
    with zipfile.ZipFile(dst, 'w', zipfile.ZIP_DEFLATED) as z:
        for name, data in members.items():
            z.writestr(name, data)

def integrate(s):
    s.send(cmd='key', key='i')
    _, ask = s.collect(is_ask)
    if ask:
        s.send(cmd='answer', id=ask['id'], key='g')
    events, end = s.collect(is_idle, timeout=5)
    return events, end

def excessive_columns(members):
    lines = members['windows.set'].splitlines()
    at = next(i for i, line in enumerate(lines) if b'added columns' in line)
    members['windows.set'] = b'\n'.join(lines[:at] + [b'5001 added columns'] +
                                        [line for i in range(5001) for line in [f'EXTRA{i}'.encode(), b'v*w']]) + b'\n'

s = Server(binary, model)
try:
    s.collect(is_idle)
    integrate(s)
    s.send(cmd='key', win='browser', key='a')
    _, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], ok=1, value='VW')
    _, ask = s.collect(is_ask)
    s.send(cmd='answer', id=ask['id'], ok=1, value='v*w')
    s.collect(is_idle)
    before = run(s, cmd='browser', **{'from': 0, 'count': 2})
    run(s, cmd='session', op='save', name='saved')
    saved = os.path.join(s.run, 'saved.snapx')
    events = run(s, cmd='open', file=saved)
    after = run(s, cmd='browser', **{'from': 0, 'count': 2})
    before_rows = [e for e in before if e.get('ev')=='browser']
    after_rows = [e for e in after if e.get('ev')=='browser']
    same_columns = bool(before_rows and after_rows and before_rows[-1]['cols'] == after_rows[-1]['cols'])
    report({'probe':'added_column_round_trip',
                      'before':before_rows,
                      'after':after_rows,
                      'errors':[e for e in events if e.get('error')],
                      'exit':s.proc.poll()}, same_columns)
    for label, mutate in [
        ('malformed_number_control', lambda m: m.__setitem__('model.set', b'\n'.join(b'bad DeltaT' if b'DeltaT' in line else line for line in m['model.set'].split(b'\n')))),
        ('zero_delta_t', lambda m: m.__setitem__('model.set', b'\n'.join(b'0 DeltaT' if b'DeltaT' in line else line for line in m['model.set'].split(b'\n')))),
        ('zero_output_stride', lambda m: m.__setitem__('model.set', b'\n'.join(b'0 nout' if b' nout' in line else line for line in m['model.set'].split(b'\n')))),
        ('excessive_added_columns', excessive_columns),
        ('set_trailing_garbage', lambda m: m.__setitem__('model.set', m['model.set'] + b'\nnot_a_valid_model_line\n')),
        ('rng_invalid_spare_flag', lambda m: m.__setitem__('random.txt', b' '.join(m['random.txt'].rsplit(b' ', 2)[:-2] + [b'9', m['random.txt'].rsplit(b' ', 2)[-1]]))),
        ('rng_trailing_garbage', lambda m: m.__setitem__('random.txt', m['random.txt'] + b'\nnot_generator_state\n')),
        ('bad_added_formula', lambda m: m.__setitem__('windows.set', m['windows.set'].replace(b'v*w', b'unknown_symbol+'))),
        ('invalid_mark_type', lambda m: m.__setitem__('marks.set', b'0\n1\n0\n999\n999\n1\n0\n0\n1\n1\n0\n0\n')),
        ('duplicate_manifest_name', lambda m: m.__setitem__('session.txt', m['session.txt'] + b'name lecar.ode\n')),
    ]:
        dst = os.path.join(s.run, label+'.snapx')
        rewrite(saved, dst, mutate)
        events = run(s, cmd='open', file=dst)
        errors = [e for e in events if e.get('error')]
        report({'probe':label, 'errors':errors,
                          'state':[{'session':e.get('session'), 'numerics':e.get('numerics')} for e in events if e.get('ev')=='state'],
                          'exit':s.proc.poll()}, bool(errors))
        if label == 'zero_output_stride':
            crash = Server(binary, model)
            try:
                crash.collect(is_idle)
                run(crash, cmd='open', file=dst)
                ev, end = integrate(crash)
                if end is None:
                    crash.proc.wait(timeout=5)
                report({'probe':'zero_output_stride_go', 'idle':end is not None, 'exit':crash.proc.poll(),
                                  'errors':[e for e in ev if e.get('error')]}, end is not None and crash.alive())
            finally:
                crash.close()
finally:
    s.close()
print(json.dumps({'summary': 'session review probes', 'failures': failures}), flush=True)
sys.exit(1 if failures else 0)
