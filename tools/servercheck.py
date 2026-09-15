#!/usr/bin/env python3
"""Drive xppcore-server through the JSON protocol and check what comes back.

usage: tools/servercheck.py [--server ./xppcore-server] [--ode examples/ode/lecar.ode] [-v]

Plays a fixed session (integrate, change a parameter, answer a menu, a
string prompt and a form, find an equilibrium, open a second plot window)
and prints PASS/FAIL per step. No display needed; runs in a few seconds.
"""
import argparse, json, os, shutil, subprocess, sys, tempfile, threading, queue

ap = argparse.ArgumentParser()
ap.add_argument('--server', default='./xppcore-server')
ap.add_argument('--ode', default='examples/ode/lecar.ode')
ap.add_argument('-v', action='store_true')
args = ap.parse_args()

run = tempfile.mkdtemp(prefix='xppserver')
shutil.copy(args.ode, run)
proc = subprocess.Popen([os.path.abspath(args.server), os.path.basename(args.ode)], cwd=run,
                        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                        text=True, bufsize=1)
events = queue.Queue()
def reader():
    for l in proc.stdout:
        try:
            events.put(json.loads(l))
        except ValueError:
            print('BAD LINE: %r' % l[:300])
            events.put({'ev': 'bad'})


threading.Thread(target=reader, daemon=True).start()
threading.Thread(target=lambda: [None for _ in proc.stderr], daemon=True).start()
failures = 0


def send(**cmd):
    if args.v:
        print('  >', json.dumps(cmd))
    proc.stdin.write(json.dumps(cmd) + '\n')
    proc.stdin.flush()


def collect(until, timeout=10):
    """events up to and including the first one for which until(ev) is true"""
    got = []
    while True:
        try:
            ev = events.get(timeout=timeout)
        except queue.Empty:
            return got, None
        if args.v:
            s = json.dumps(ev)
            print('  <', s[:160] + ('...' if len(s) > 160 else ''))
        got.append(ev)
        if until(ev):
            return got, ev


def check(name, ok, detail=''):
    global failures
    print(('PASS ' if ok else 'FAIL ') + name + ('' if ok else '  ' + detail))
    failures += 0 if ok else 1


def draw_ops(evs, win=1):
    return [o for e in evs if e.get('ev') == 'draw' and e.get('win') == win for o in e['ops']]


def is_state(e):
    return e.get('ev') == 'state'


def is_idle(e):
    return e.get('ev') == 'idle'


def last_state(evs):
    st = [e for e in evs if e.get('ev') == 'state']
    return st[-1] if st else None


evs, _ = collect(is_idle)
st = last_state(evs)
hello = next((e for e in evs if e.get('ev') == 'hello'), None)
check('hello', hello is not None and len(hello['menus']['main']) == 20)
check('palette', any(e.get('ev') == 'palette' and len(e['colors']) == 256 for e in evs))
check('state after hello', st is not None and any(p[0] == 'iapp' for p in st['pars']), str(st))
check('axes drawn', len(draw_ops(evs)) > 10)

send(cmd='size', win=1, w=800, h=600)
collect(is_idle)
send(cmd='key', key='i')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('Initialconds opens a menu', ask is not None and ask['kind'] == 'menu' and 'g' in ask['keys'], str(ask))
send(cmd='answer', id=ask['id'], key='g')
evs, _ = collect(is_idle, timeout=30)
st = last_state(evs)
lines = [o for o in draw_ops(evs) if o[0] == 'line']
check('integration draws the trajectory', len(lines) > 100, '%d lines' % len(lines))
check('storage has 601 rows', st is not None and st['rows'] == 601, str(st and st['rows']))

send(cmd='set', kind='par', name='iapp', value=0.1)
send(cmd='state')
evs, st = collect(is_state)
collect(is_idle)
check('set parameter', st is not None and dict(st['pars'])['iapp'] == 0.1, str(st and st['pars']))

send(cmd='key', key='f')
send(cmd='key', key='s')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('File/Save info asks for a file name', ask is not None and ask['kind'] in ('file', 'string'), str(ask))
if ask:
    if ask['kind'] == 'file':
        send(cmd='answer', id=ask['id'], ok=1, file='info.txt')
    else:
        send(cmd='answer', id=ask['id'], ok=1, value='info.txt')
    collect(is_idle)
    check('info file written', os.path.exists(os.path.join(run, 'info.txt')))

send(cmd='key', key='u')  # nUmerics menu
send(cmd='key', key='t')  # total
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('nUmerics/Total asks for a number', ask is not None and ask['kind'] == 'string', str(ask))
if ask:
    send(cmd='answer', id=ask['id'], ok=1, value='40')
    collect(is_idle)
send(cmd='key', key='Escape')
collect(is_idle)

send(cmd='key', key='v')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='2')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('Viewaxes/2D opens a form', ask is not None and ask['kind'] == 'form' and 'Xmax' in ask['names'][4], str(ask))
if ask:
    vals = list(ask['values'])
    vals[4] = '40'
    send(cmd='answer', id=ask['id'], ok=1, values=vals)
    collect(is_idle)

send(cmd='key', key='i')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='g')
evs, _ = collect(is_idle, timeout=30)
st = last_state(evs)
check('total 40 gives 801 rows', st is not None and st['rows'] == 801, str(st and st['rows']))

send(cmd='key', key='s')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='g')
eq = None
for _ in range(6):
    evs, e = collect(lambda e: e.get('ev') in ('ask', 'equilibrium'), timeout=10)
    if e is None:
        break
    if e['ev'] == 'equilibrium':
        eq = e
    else:
        send(cmd='answer', id=e['id'], key='n')
    if eq:
        break
check('Sing pts/Go reports an equilibrium', eq is not None and eq['type'] in ('STABLE', 'UNSTABLE', 'NEUTRAL'), str(eq))
collect(is_idle)

send(cmd='key', key='m')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='c')
evs, _ = collect(lambda e: e.get('ev') == 'window' and e['op'] == 'create', timeout=5)
check('Makewindow/Create opens window 2', any(e.get('ev') == 'window' and e.get('win') == 2 for e in evs))
collect(is_idle)

send(cmd='key', key='f')
send(cmd='key', key='a')
evs, _ = collect(lambda e: e.get('ev') == 'window' and e.get('win') == 101)
check('File/Auto opens the AUTO window', any(e.get('ev') == 'window' and e.get('win') == 101 for e in evs))
collect(is_idle)
send(cmd='size', win=101, w=500, h=300)
evs, _ = collect(is_idle)
win = [e for e in evs if e.get('ev') == 'window' and e.get('win') == 101]
check('size resizes the AUTO diagram', win and win[-1]['w'] == 500 and win[-1]['h'] == 300
      and len(draw_ops(evs, 101)) > 5, str(win))

send(cmd='key', key='f')
send(cmd='key', key='q')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
if ask:
    send(cmd='answer', id=ask['id'], key='y')
try:
    proc.wait(timeout=5)
    check('File/Quit exits', True)
except subprocess.TimeoutExpired:
    proc.kill()
    check('File/Quit exits', False)

shutil.rmtree(run, ignore_errors=True)
print('server checks: %s' % ('all passed' if failures == 0 else '%d failed' % failures))
sys.exit(1 if failures else 0)
