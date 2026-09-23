#!/usr/bin/env python3
"""Drive xppautX --server through the JSON protocol and check what comes back.

usage: tools/servercheck.py [--server ./xppautX] [--ode examples/ode/lecar.ode] [-v]

Plays a fixed session (integrate, change a parameter, answer a menu, a
string prompt and a form, find an equilibrium, open a second plot window)
and prints PASS/FAIL per step. No display needed; runs in a few seconds.
"""
import argparse, base64, json, os, shutil, subprocess, sys, tempfile, threading, queue

ap = argparse.ArgumentParser()
ap.add_argument('--server', default='./xppautX')
ap.add_argument('--ode', default='examples/ode/lecar.ode')
ap.add_argument('-v', action='store_true')
args = ap.parse_args()

def check_logging():
    """core/xpp_log.h: -silent is quiet by default, --verbose shows the
    banner/parser stats, and a real parse error still reaches stderr even
    at the default (quiet) level. Uses -silent so this needs no JSON
    conversation; see CLAUDE.md "Logging"."""
    global failures
    run_dir = tempfile.mkdtemp(prefix='xppquiet')
    try:
        shutil.copy(args.ode, run_dir)
        odename = os.path.basename(args.ode)

        p = subprocess.run([os.path.abspath(args.server), odename, '-silent'],
                            cwd=run_dir, capture_output=True, text=True, timeout=30)
        check('log: -silent is quiet by default', p.stderr.strip() == '', repr(p.stderr[:300]))

        p = subprocess.run([os.path.abspath(args.server), '--verbose', odename, '-silent'],
                            cwd=run_dir, capture_output=True, text=True, timeout=30)
        check('log: --verbose shows the banner and parser stats',
              'Copyright' in p.stderr and 'nvar=' in p.stderr, repr(p.stderr[:300]))

        bad_dir = tempfile.mkdtemp(prefix='xppbadode')
        try:
            bad_ode = os.path.join(bad_dir, 'bad.ode')
            with open(bad_ode, 'w') as f:
                f.write("x'=(1+2\ndone\n")
            p = subprocess.run([os.path.abspath(args.server), 'bad.ode', '-silent'],
                                cwd=bad_dir, capture_output=True, text=True, timeout=30)
            check('log: a syntax error still reaches stderr at the default level',
                  p.stderr.strip() != '', repr(p.stderr[:300]))
        finally:
            shutil.rmtree(bad_dir, ignore_errors=True)
    finally:
        shutil.rmtree(run_dir, ignore_errors=True)


def launch_server(extra_env=None):
    """Start one xppautX --server instance in its own scratch directory and
    return (proc, run_dir, send, collect, events) -- send/collect work just
    like the module-level ones below but are bound to this instance, so a
    second, differently-configured server (e.g. a bad HOME) can be driven
    the same way without disturbing the main session."""
    run_dir = tempfile.mkdtemp(prefix='xppserver')
    shutil.copy(args.ode, run_dir)
    env = None
    if extra_env is not None:
        env = dict(os.environ)
        env.update(extra_env)
    proc = subprocess.Popen([os.path.abspath(args.server), '--server', os.path.basename(args.ode)], cwd=run_dir,
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, bufsize=1, env=env)
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

    return proc, run_dir, send, collect, events


proc, run, send, collect, events = launch_server()
failures = 0


def check(name, ok, detail=''):
    global failures
    print(('PASS ' if ok else 'FAIL ') + name + ('' if ok else '  ' + detail))
    failures += 0 if ok else 1


check_logging()


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

collect(is_idle)  # the idle of the state command above
send(cmd='set', kind='par', name='iapp', text='%0.1*2')
evs, _ = collect(is_idle)
st = last_state(evs)
check('set parameter by %formula', st is not None and abs(dict(st['pars'])['iapp'] - 0.2) < 1e-12, str(st and st['pars']))
send(cmd='default', kind='par')
evs, _ = collect(is_idle)
st = last_state(evs)
check('default parameters', st is not None and dict(st['pars'])['iapp'] == 0.05, str(st and st['pars']))

send(cmd='browser', **{'from': 10, 'count': 3, 'col': 2, 'ncol': 1})
evs, br = collect(lambda e: e.get('ev') == 'browser')
collect(is_idle)
check('browser sends the rows asked for', br is not None and br['rows'] == 601 and br['from'] == 10
      and len(br['data']) == 3 and len(br['data'][0]) == 2 and br['cols'][:3] == ['T', 'V', 'W'], str(br)[:200])
send(cmd='browser', op='get', row=10)
evs, _ = collect(is_idle)
st = last_state(evs)
check('browser Get sets the initial conditions from a row',
      st is not None and abs(dict(st['ics'])['W'] - br['data'][0][1]) < 1e-6, str(st and st['ics']))
send(cmd='browser', **{'from': 0, 'count': 0})
collect(is_idle)

send(cmd='slide', name='iapp', value=0.07, rerun=1)
evs, _ = collect(is_idle, timeout=30)
st = last_state(evs)
check('slide sets the parameter and integrates again', st is not None and dict(st['pars'])['iapp'] == 0.07
      and len([o for o in draw_ops(evs) if o[0] == 'line']) > 100, str(st and st['pars']))
send(cmd='equations')
evs, eqs = collect(lambda e: e.get('ev') == 'equations')
collect(is_idle)
check('equations', eqs is not None and eqs['lines'][0].startswith('dV/dT='), str(eqs))
send(cmd='key', key='f')
send(cmd='key', key='p')
evs, src = collect(lambda e: e.get('ev') == 'source')
collect(is_idle)
acts = [i for i, c in enumerate(src['comments']) if c[1]] if src else []
check('source lists the comments with actions', len(acts) >= 5, str(src and src['comments'][:3]))
if acts:
    send(cmd='action', index=acts[1])  # {gk=0}
    evs, _ = collect(is_idle)
    st = last_state(evs)
    check('a comment action sets its parameters', st is not None and dict(st['pars'])['gk'] == 0, str(st and st['pars']))
    send(cmd='set', kind='par', name='gk', value=2)
    send(cmd='set', kind='par', name='iapp', value=0.1)
    collect(is_idle)
    collect(is_idle)

send(cmd='key', key='f')
send(cmd='key', key='s')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('File/Save info asks for a file name', ask is not None and ask['kind'] in ('file', 'string'), str(ask))
check('the file ask lists its folder', ask is not None and ask.get('dir') and 'files' in ask
      and 'lecar.ode' not in ask['files'], str(ask)[:200])
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


def answer_asks(until, replies, timeout=20):
    """collect up to until(ev), answering asks whose kind is in replies"""
    got = []
    while True:
        evs, e = collect(lambda e: e.get('ev') == 'ask' or until(e), timeout)
        got += evs
        if e is None or e.get('ev') != 'ask' or e['kind'] not in replies:
            return got, e
        send(cmd='answer', id=e['id'], **replies[e['kind']](e))


pixels = lambda e: {'w': 2, 'h': 1, 'rgb': base64.b64encode(bytes([255, 0, 0, 0, 0, 255])).decode()}
menu = lambda key: (lambda e: {'key': key})
for _ in range(2):
    send(cmd='key', key='k')
    evs, _ = answer_asks(lambda e: e.get('ev') == 'film', {'menu': menu('c')})
    collect(is_idle)
check('Kinescope/Capture sends film captures', evs and evs[-1].get('count') == 2, str(evs[-1:]))
send(cmd='key', key='k')
evs, e = answer_asks(is_idle, {'menu': menu('s'), 'string': lambda e: {'value': 'kin'}, 'pixels': pixels})
check('Kinescope/Save asks for the pixels and writes GIFs',
      all(os.path.exists(os.path.join(run, 'kin_%d.gif' % i)) for i in range(2)), str(os.listdir(run)))
send(cmd='key', key='v')
evs, _ = answer_asks(is_idle, {'menu': menu('t')})
send(cmd='size', win=104, w=300, h=200)
evs, _ = collect(is_idle)
check('the animation window opens and resizes', any(e.get('ev') == 'window' and e.get('win') == 104
      and e['w'] == 300 and e['h'] == 200 for e in evs), str([e for e in evs if e.get('ev') == 'window']))
send(cmd='ani', op='close')
evs, _ = collect(is_idle)
check('the animation window closes', any(e.get('ev') == 'window' and e.get('op') == 'destroy' for e in evs))

send(cmd='plotvars', how=2, names=['V', 'W'])
evs, _ = collect(is_idle)
ap = [e for e in evs if e.get('ev') == 'aplot']
check('the IC arry button opens an array plot', ap and ap[-1]['nx'] == 2 and len(ap[-1]['cells']) == 2 * ap[-1]['ny'],
      str(ap[-1:])[:200])
send(cmd='aplot', op='close')
collect(is_idle)
send(cmd='click', win=1)
evs, _ = collect(is_idle)
send(cmd='state')
evs, st = collect(is_state)
collect(is_idle)
view = st and st.get('view')
check('state has the view of the active window', view is not None and view['right'] > view['left'], str(view))
send(cmd='key', key='w')
evs, _ = answer_asks(lambda e: e.get('ev') == 'ask' and e['kind'] == 'drag', {'menu': menu('s')})
drag = evs[-1] if evs else None
for d in [{'what': 'down', 'x': 300, 'y': 200}, {'what': 'move', 'x': 360, 'y': 200}, {'what': 'up', 'x': 360, 'y': 200}]:
    send(cmd='answer', id=drag['id'], **d)
    evs, drag = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=drag['id'], ok=0)
evs, _ = collect(is_idle)
st = last_state(evs)
check('Window/Scroll drags the view', st is not None and view is not None and st['view']['xlo'] < view['xlo'],
      str(st and st['view']))

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
axes = [e for e in evs if e.get('ev') == 'diagram' and e['op'] in ('axes', 'reset')]
check('the AUTO diagram sends its axes as data', axes and axes[-1]['wid'] > 0 and axes[-1]['xlabel'], str(axes))

# Run, Grab a labelled point, Run again: the second run restarts from the
# label in fort.3 (findlb/readlb). On Windows the backward fseek that located
# the label line was undefined on a text stream and corrupted the heap.
send(cmd='auto', op='run')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('Auto/Run opens the start menu', ask is not None and ask['kind'] == 'menu' and 's' in ask['keys'], str(ask))
if ask:
    send(cmd='answer', id=ask['id'], key='s')
    evs, _ = collect(is_idle, timeout=30)
    adds = [e for e in evs if e.get('ev') == 'diagram' and e['op'] == 'add']
    check('Auto/Run sends the points it draws as diagram data',
          adds and adds[0]['from'] == 0 and sum(len(r['x']) for e in adds for r in e['runs']) > 0, str(adds)[:200])
    send(cmd='auto', op='grab')
    evs, ask = collect(lambda e: e.get('ev') == 'ask')
    check('Auto/Grab asks for a point', ask is not None and ask['kind'] == 'grab', str(ask))
    for k in ['Tab', 'Tab', 'Return']:
        if ask is None:
            break
        send(cmd='answer', id=ask['id'], key=k)
        evs, ask = collect(lambda e: e.get('ev') == 'ask' or is_idle(e))
        if k == 'Return' and ask is not None and ask.get('ev') == 'ask':
            send(cmd='answer', id=ask['id'], ok=0)  # a confirmation is not expected; cancel it
            collect(is_idle)
    send(cmd='auto', op='run')
    evs, e = collect(lambda e: e.get('ev') == 'ask' or is_idle(e), timeout=30)
    if e is not None and e.get('ev') == 'ask':
        send(cmd='answer', id=e['id'], ok=0)
        evs, e = collect(is_idle, timeout=30)
    check('Auto/Run after a Grab restarts from the label and the server survives',
          e is not None and proc.poll() is None, 'exit code %s' % proc.poll())

# A HOME the process cannot write to used to make AUTO exit(1) under the
# client when it opened fort.8 there; open_auto() now falls back to the
# model's directory. Drive a second server with such a HOME and check it survives.
bad_home = tempfile.mkdtemp(prefix='xppbadhome')
shutil.rmtree(bad_home)  # a path that is guaranteed not to exist
proc2, run2, send2, collect2, events2 = launch_server(extra_env={'HOME': bad_home})
collect2(is_idle)  # startup hello/state/idle
send2(cmd='key', key='f')
send2(cmd='key', key='a')
collect2(lambda e: e.get('ev') == 'window' and e.get('win') == 101)
collect2(is_idle)
send2(cmd='auto', op='run')
evs, ask = collect2(lambda e: e.get('ev') == 'ask')
check('bad-HOME server: Auto/Run opens the Start menu',
      ask is not None and ask['kind'] == 'menu' and 's' in ask['keys'], str(ask))
if ask:
    send2(cmd='answer', id=ask['id'], key='s')
evs, e = collect2(is_idle, timeout=20)
st2 = [x for x in evs if x.get('ev') == 'state']
alive = proc2.poll() is None
check('bad-HOME server survives Run/Steady state (state then idle, still alive)',
      ask is not None and st2 and e is not None and alive, str(evs)[-300:])

if alive:
    # writing to a dead server's stdin would raise and end the script
    send2(cmd='key', key='f')
    send2(cmd='key', key='q')
    evs, ask = collect2(lambda e: e.get('ev') == 'ask')
    if ask:
        send2(cmd='answer', id=ask['id'], key='y')
    try:
        proc2.wait(timeout=5)
        check('bad-HOME server: File/Quit exits', True)
    except subprocess.TimeoutExpired:
        proc2.kill()
        check('bad-HOME server: File/Quit exits', False)
else:
    proc2.kill()
    check('bad-HOME server: File/Quit exits', False, 'server had already exited')
shutil.rmtree(run2, ignore_errors=True)

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
