#!/usr/bin/env python3
"""Drive xppautX --server through the JSON protocol and check what comes back.

usage: tools/servercheck.py [--server ./xppautX] [--ode examples/ode/lecar.ode] [-v]

Plays a fixed session (integrate, change a parameter, answer a menu, a
string prompt and a form, find an equilibrium, open a second plot window)
and prints PASS/FAIL per step. No display needed; runs in a few seconds.
"""
import argparse, base64, json, os, shutil, struct, subprocess, sys, tempfile, threading, queue

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


def launch_server(extra_env=None, ode=None):
    """Start one xppautX --server instance in its own scratch directory and
    return (proc, run_dir, send, collect, events) -- send/collect work just
    like the module-level ones below but are bound to this instance, so a
    second, differently-configured server (e.g. a bad HOME, another model)
    can be driven the same way without disturbing the main session."""
    ode = ode or args.ode
    run_dir = tempfile.mkdtemp(prefix='xppserver')
    shutil.copy(ode, run_dir)
    env = None
    if extra_env is not None:
        env = dict(os.environ)
        env.update(extra_env)
    proc = subprocess.Popen([os.path.abspath(args.server), '--server', os.path.basename(ode)], cwd=run_dir,
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
check('hello lists the series feature', 'series' in hello.get('features', []), str(hello.get('features')))
send(cmd='data', events=['series'])
evs, _ = collect(is_idle)
ser = [e for e in evs if e.get('ev') == 'series']
check('asking for the series data sends the plot at once', len(ser) == 1 and ser[0]['win'] == 1 and ser[0]['rows'] == 0
      and len(ser[0]['curves']) == 1, str(ser)[:300])
send(cmd='key', key='i')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('Initialconds opens a menu', ask is not None and ask['kind'] == 'menu' and 'g' in ask['keys'], str(ask))
send(cmd='answer', id=ask['id'], key='g')
evs, _ = collect(is_idle, timeout=30)
st = last_state(evs)
lines = [o for o in draw_ops(evs) if o[0] == 'line']
check('integration draws the trajectory', len(lines) > 100, '%d lines' % len(lines))
check('storage has 601 rows', st is not None and st['rows'] == 601, str(st and st['rows']))


f32 = lambda v: struct.unpack('<f', struct.pack('<f', v))[0]


def values(ev_col, enc):
    """a series column's data as floats (NaN for null), whatever the encoding"""
    d = ev_col['data']
    if enc == 'f32':
        raw = base64.b64decode(d)
        return list(struct.unpack('<%df' % (len(raw) // 4), raw))
    return [float('nan') if v is None else f32(v) for v in d]


def same_floats(a, b):
    return len(a) == len(b) and all(x == y or (x != x and y != y) for x, y in zip(a, b))


def series_matches_output_dat(ser, ode=None):
    """the series' columns hold the numbers output.dat has for the same run
    (xppautX -silent): both are the stored floats, output.dat prints %.8g"""
    ode = ode or args.ode
    silent = tempfile.mkdtemp(prefix='xppsilent')
    shutil.copy(ode, silent)
    subprocess.run([os.path.abspath(args.server), os.path.basename(ode), '-silent'], cwd=silent,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60)
    with open(os.path.join(silent, 'output.dat')) as f:
        rows = [l.split() for l in f if l.strip()]
    shutil.rmtree(silent, ignore_errors=True)
    for c in ser['columns']:
        want = [r[c['col']] for r in rows]
        got = ['%.8g' % v for v in values(c, ser.get('enc'))]
        if got != want:
            bad = next(i for i, (a, b) in enumerate(zip(got, want)) if a != b) if len(got) == len(want) else -1
            return 'column %s differs at row %d (%d rows, output.dat %d)' % (c['name'], bad, len(got), len(want))
    return None


ser = [e for e in evs if e.get('ev') == 'series' and 'op' not in e]  # the full series, after any appends
check('integration sends the series: the curve V against W, 601 rows',
      len(ser) == 1 and ser[0]['rows'] == 601 and ser[0]['curves'][0]['x'] == 1 and ser[0]['curves'][0]['y'] == 2
      and [c['name'] for c in ser[0]['columns']] == ['T', 'V', 'W'], str(ser)[:300])
if ser:
    problem = series_matches_output_dat(ser[0])
    check('the series carries the numbers of output.dat', problem is None, problem or '')
send(cmd='redraw')
evs, _ = collect(is_idle)
check('a redraw of unchanged data sends no series', not any(e.get('ev') == 'series' for e in evs))
send(cmd='key', key='x')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
if ask:
    send(cmd='answer', id=ask['id'], ok=1, value='V')
evs, _ = collect(is_idle)
ser = [e for e in evs if e.get('ev') == 'series']
check('Xi vs t sends the series of V against T', len(ser) == 1 and ser[0]['curves'][0]['x'] == 0
      and ser[0]['curves'][0]['y'] == 1 and [c['name'] for c in ser[0]['columns']] == ['T', 'V'], str(ser)[:300])
send(cmd='data', events=[])
collect(is_idle)
send(cmd='key', key='i')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='g')
evs, _ = collect(is_idle, timeout=30)
check('an empty data list stops the series', not any(e.get('ev') == 'series' for e in evs))

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
check('a run that is not cancelled sends no stopped', not any(e.get('ev') == 'stopped' for e in evs))

# an Abort right behind the answer that starts the run: the run stops, and
# says where (docs/protocol.md "stopped") before its state and idle
send(cmd='key', key='i')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
proc.stdin.write(json.dumps({'cmd': 'answer', 'id': ask['id'], 'key': 'g'}) + '\n' +
                 json.dumps({'cmd': 'abort'}) + '\n')
proc.stdin.flush()
evs, _ = collect(is_idle, timeout=30)
kinds = [e.get('ev') for e in evs]
stopped = next((e for e in evs if e.get('ev') == 'stopped'), None)
st = last_state(evs)
check('a cancelled integration sends stopped, then state and idle',
      stopped is not None and kinds.index('stopped') < len(kinds) - 1 - kinds[::-1].index('state'), str(kinds[-5:]))
at = stopped['at'] if stopped else {}
check('stopped says how many rows the integration stored, and the last time',
      at.get('what') == 'integrate' and st is not None and at.get('rows') == st['rows'] and 0 < st['rows'] < 801
      and isinstance(at.get('t'), (int, float)), '%s, state rows %s' % (at, st and st['rows']))
if stopped and st and st['rows'] > 0:
    send(cmd='browser', **{'from': st['rows'] - 1, 'count': 1, 'col': 1, 'ncol': 1})
    evs, br = collect(lambda e: e.get('ev') == 'browser')
    collect(is_idle)
    check('stopped\'s t is the time of the last stored row', br is not None and br['data'] and
          '%.8g' % br['data'][0][0] == '%.8g' % at['t'], '%s vs %s' % (at.get('t'), br and br['data']))
    send(cmd='browser', **{'from': 0, 'count': 0})
    collect(is_idle)
send(cmd='key', key='i')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
send(cmd='answer', id=ask['id'], key='g')
evs, _ = collect(is_idle, timeout=30)
st = last_state(evs)
check('the next run is whole again', st is not None and st['rows'] == 801 and
      not any(e.get('ev') == 'stopped' for e in evs), str(st and st['rows']))

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


# Live plotting (docs/protocol.md "The plot as data"): while an integration
# runs, a subscribed client gets the rows as they are stored, in "append"
# series events, then the full series. tools/models/live.ode stores 20 001
# rows in about a second, long enough for several appends.
LIVE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'models', 'live.ode')


def live_run(send, collect, key='i', answer=None):
    """integrate (Initialconds/Go, or Continue) and return the series events of the command"""
    send(cmd='key', key=key)
    evs, ask = collect(lambda e: e.get('ev') == 'ask', timeout=20)
    if ask:
        send(cmd='answer', id=ask['id'], **(answer or {'key': 'g'}))
    evs, _ = collect(is_idle, timeout=120)
    return [e for e in evs if e.get('ev') == 'series']


def check_appends(what, ser, first_from, enc=None):
    """appends then one full series; returns the full series' columns as floats"""
    apps = [e for e in ser if e.get('op') == 'append']
    full = [e for e in ser if 'op' not in e]
    check('%s: several appends before the full series' % what, len(apps) >= 3, '%d appends' % len(apps))
    check('%s: one full series, and nothing after it' % what,
          len(full) == 1 and ser and ser[-1] is full[0], str([(e.get('op'), e.get('rows')) for e in ser])[:300])
    if not apps or not full:
        return None
    froms = [e['from'] for e in apps]
    contiguous = froms[0] == first_from and all(apps[i]['from'] == apps[i - 1]['rows'] for i in range(1, len(apps)))
    sizes = all(len(values(c, enc)) == e['rows'] - e['from'] for e in apps for c in e['columns'])
    check('%s: the appends go from row %d on, contiguous' % (what, first_from), contiguous and sizes,
          str([(e['from'], e['rows']) for e in apps])[:300])
    check('%s: in the encoding asked for' % what, all(e.get('enc') == enc for e in ser),
          str([e.get('enc') for e in ser]))
    final = {c['col']: values(c, enc) for c in full[0]['columns']}
    cols_ok = all([c['col'] for c in e['columns']] == list(final) for e in apps)
    held = {c: final[c][:first_from] for c in final}
    for e in apps:
        for c in e['columns']:
            held[c['col']] = held[c['col']][:e['from']] + values(c, enc)
    n = apps[-1]['rows']
    check('%s: the appended rows are the final series\' rows (%d of %d)' % (what, n, full[0]['rows']),
          cols_ok and all(same_floats(held[c], final[c][:n]) for c in final) and n <= full[0]['rows'],
          str([c['col'] for c in apps[0]['columns']]) + ' vs ' + str(list(final)))
    return final


def check_live_series():
    proc3, run3, send3, collect3, _ = launch_server(ode=LIVE)
    try:
        collect3(is_idle)
        send3(cmd='data', events=['series'])
        collect3(is_idle)
        ser = live_run(send3, collect3)
        final = check_appends('live run', ser, 0)
        full = [e for e in ser if 'op' not in e]
        if full:
            check('live run: the full series has the 20 001 rows of output.dat',
                  full[0]['rows'] == 20001 and series_matches_output_dat(full[0], LIVE) is None,
                  str(full[0]['rows']) + ' ' + str(series_matches_output_dat(full[0], LIVE)))
        ser = live_run(send3, collect3)
        again = check_appends('run again', ser, 0)
        check('run again: the same numbers', final is not None and again is not None
              and all(same_floats(final[c], again[c]) for c in final))
        ser = live_run(send3, collect3, 'c', {'value': '2000'})
        more = check_appends('Continue', ser, 20001)
        check('Continue: the rows before it are kept', more is not None and final is not None
              and all(same_floats(more[c][:20001], final[c]) for c in final))

        send3(cmd='data', events=['series'], enc='f32')
        evs, _ = collect3(is_idle)
        now = [e for e in evs if e.get('ev') == 'series']
        check('enc f32: the series comes at once, base64 in each column',
              len(now) == 1 and now[0].get('enc') == 'f32' and all(isinstance(c['data'], str) for c in now[0]['columns']),
              str(now)[:200])
        if now and more is not None:
            check('enc f32: the decoded columns equal the JSON ones',
                  all(same_floats(values(c, 'f32'), more[c['col']]) for c in now[0]['columns']))
        ser = live_run(send3, collect3)
        dec = check_appends('enc f32 run', ser, 0, 'f32')
        check('enc f32 run: the decoded numbers equal the JSON run\'s', dec is not None and final is not None
              and all(same_floats(dec[c], final[c]) for c in final))

        send3(cmd='data', events=[])
        collect3(is_idle)
        ser = live_run(send3, collect3)
        check('a client not subscribed gets no series, appended or full', ser == [], str(ser)[:200])
    finally:
        send3(cmd='quit')
        try:
            proc3.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc3.kill()
        shutil.rmtree(run3, ignore_errors=True)


check_live_series()


# Answers in data coordinates (docs/protocol.md "Asks"): a mouse, rubber or
# drag ask also takes xd,yd (xd2,yd2), which the core turns into the pixels
# of its window that map to them. Two fresh servers zoom by the same box,
# one answered in pixels, the other in the data coordinates of those pixels
# (state.view's mapping): the views must be the same.
def zoomed_view(answer_of):
    """a fresh lecar server's view after Window/Zoom answered with answer_of(view), and the server's session"""
    p, r, snd, col, _ = launch_server()
    col(is_idle)
    snd(cmd='state')
    evs, st = col(is_state)
    col(is_idle)
    view = st['view']
    snd(cmd='key', key='w')
    evs, ask = col(lambda e: e.get('ev') == 'ask')
    snd(cmd='answer', id=ask['id'], key='z')
    evs, ask = col(lambda e: e.get('ev') == 'ask')
    ok = ask is not None and ask['kind'] == 'rubber'
    if ok:
        snd(cmd='answer', id=ask['id'], **answer_of(view))
    evs, _ = col(is_idle)
    st = last_state(evs)
    return ok, view, st and st['view'], (p, r, snd, col)


def pixel_to_data(v, i, j):
    return (v['xlo'] + (v['xhi'] - v['xlo']) * (i - v['left']) / (v['right'] - v['left']),
            v['ylo'] + (v['yhi'] - v['ylo']) * (j - v['bottom']) / (v['top'] - v['bottom']))


def stop_server(p, r, snd):
    snd(cmd='quit')
    try:
        p.wait(timeout=5)
    except subprocess.TimeoutExpired:
        p.kill()
    shutil.rmtree(r, ignore_errors=True)


def box_of(v):
    """a box from 20% to 60% of the window across and 30% to 80% down, in whole pixels"""
    w, h = v['right'] - v['left'], v['bottom'] - v['top']
    return (v['left'] + int(0.2 * w), v['top'] + int(0.3 * h), v['left'] + int(0.6 * w), v['top'] + int(0.8 * h))


def check_data_coordinates():
    ok1, v1, by_pixels, s1 = zoomed_view(lambda v: dict(zip(('x', 'y', 'x2', 'y2'), box_of(v))))
    stop_server(*s1[:3])

    def data_box(v):
        i1, j1, i2, j2 = box_of(v)
        (xd, yd), (xd2, yd2) = pixel_to_data(v, i1, j1), pixel_to_data(v, i2, j2)
        return {'xd': xd, 'yd': yd, 'xd2': xd2, 'yd2': yd2}
    ok2, v2, by_data, s2 = zoomed_view(data_box)
    p, r, snd, col = s2
    try:
        check('Window/Zoom asks for a box (rubber)', ok1 and ok2)
        keys = ('xlo', 'xhi', 'ylo', 'yhi')
        check('a box answered in data coordinates zooms as the same box in pixels',
              by_pixels is not None and by_data is not None and v1 == v2
              and all(by_pixels[k] == by_data[k] for k in keys) and by_data['xhi'] - by_data['xlo'] < v2['xhi'] - v2['xlo'],
              '%s vs %s' % (by_pixels, by_data))
        want = data_box(v2)
        px = (v2['xhi'] - v2['xlo']) / (v2['right'] - v2['left'])
        py = (v2['yhi'] - v2['ylo']) / (v2['bottom'] - v2['top'])
        got = by_data or {}
        check('the zoomed view is the box asked for, within a pixel',
              by_data is not None and abs(min(got['xlo'], got['xhi']) - min(want['xd'], want['xd2'])) <= px
              and abs(max(got['xlo'], got['xhi']) - max(want['xd'], want['xd2'])) <= px
              and abs(min(got['ylo'], got['yhi']) - min(want['yd'], want['yd2'])) <= py
              and abs(max(got['ylo'], got['yhi']) - max(want['yd'], want['yd2'])) <= py, '%s vs %s' % (got, want))
        # Initialconds/Mouse picked in data coordinates: the ICs are that point
        if by_data:
            v = by_data
            xd, yd = v['xlo'] + 0.3 * (v['xhi'] - v['xlo']), v['ylo'] + 0.6 * (v['yhi'] - v['ylo'])
            snd(cmd='key', key='i')
            evs, ask = col(lambda e: e.get('ev') == 'ask')
            snd(cmd='answer', id=ask['id'], key='m')
            evs, ask = col(lambda e: e.get('ev') == 'ask')
            if ask is not None and ask['kind'] == 'mouse':
                snd(cmd='answer', id=ask['id'], xd=xd, yd=yd)
            evs, _ = col(is_idle, timeout=30)
            st = last_state(evs)
            ics = dict(st['ics']) if st else {}
            pxm = abs(v['xhi'] - v['xlo']) / (v['right'] - v['left'])
            pym = abs(v['yhi'] - v['ylo']) / (v['bottom'] - v['top'])
            check('Initialconds/Mouse answered in data coordinates starts from that point, within a pixel',
                  ask is not None and ask['kind'] == 'mouse' and abs(ics.get('V', 1e9) - xd) <= pxm
                  and abs(ics.get('W', 1e9) - yd) <= pym, '%s vs %s' % (ics, (xd, yd)))
    finally:
        stop_server(p, r, snd)


check_data_coordinates()


# Plot windows as data (docs/protocol.md "The plot as data", docs/ui-v2.md
# T6): "plots" lists every window and the active one; "series" comes per
# window, each with its win, when that window's data or curves changed;
# appends only for the active window. live.ode runs long enough for appends.
def check_plot_windows():
    proc4, run4, send4, collect4, _ = launch_server(ode=LIVE)

    def command(key, *answers):
        """a key and the answers to the asks it opens; the events up to its idle"""
        send4(cmd='key', key=key)
        got, pending = [], list(answers)
        while True:
            evs, e = collect4(lambda e: e.get('ev') in ('ask', 'idle'), timeout=120)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            send4(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))

    def after(cmd):
        send4(**cmd)
        return collect4(is_idle, timeout=30)[0]

    plots = lambda evs: [e for e in evs if e.get('ev') == 'plots']
    full = lambda evs: [e for e in evs if e.get('ev') == 'series' and 'op' not in e]
    wins = lambda p: [w['win'] for w in p['windows']]
    try:
        evs, _ = collect4(is_idle)
        hello4 = next((e for e in evs if e.get('ev') == 'hello'), {})
        check('hello lists the plots feature', 'plots' in hello4.get('features', []), str(hello4.get('features')))
        evs = after({'cmd': 'data', 'events': ['series', 'plots']})
        pl = plots(evs)
        w1 = pl[0]['windows'][0] if pl and pl[0]['windows'] else {}
        check('plots at once: window 1, active, its title, axes and curves',
              len(pl) == 1 and pl[0]['active'] == 1 and wins(pl[0]) == [1] and w1.get('title') == 'W vs V'
              and w1.get('three') == 0 and w1.get('xlo') == -0.6 and w1.get('yhi') == 1.2
              and w1.get('curves') and w1['curves'][0]['x'] == 1 and 'theta' in w1 and 'zmax' in w1.get('box', {}),
              str(pl)[:400])
        check('plots comes before the series', evs.index(pl[0]) < evs.index(full(evs)[0]) if pl and full(evs) else False)
        command('i', {'key': 'g'})
        evs = command('m', {'key': 'c'})
        pl, ser = plots(evs), full(evs)
        check('Makewindow/Create: plots has windows 1 and 2, 2 active',
              len(pl) == 1 and wins(pl[0]) == [1, 2] and pl[0]['active'] == 2, str(pl)[:300])
        check('the new window gets its own series, with the data', [e['win'] for e in ser] == [2]
              and ser[0]['rows'] == 20001, str([(e['win'], e['rows']) for e in ser]))
        evs = command('x', {'value': 'V'})
        pl, ser = plots(evs), full(evs)
        cur = {w['win']: (w['curves'][0]['x'], w['curves'][0]['y']) for w in pl[0]['windows']} if pl else {}
        check('Xi vs t in window 2: its curves change, window 1 keeps its own',
              cur == {1: (1, 2), 2: (0, 1)} and [w['title'] for w in pl[0]['windows']] == ['W vs V', 'V vs T'],
              str(pl)[:400])
        check('... and only window 2 gets a new series', [e['win'] for e in ser] == [2]
              and [c['name'] for c in ser[0]['columns']] == ['T', 'V'], str([(e['win'], e['rows']) for e in ser]))
        evs = after({'cmd': 'click', 'win': 1})
        pl = plots(evs)
        check('selecting window 1 (click) makes it active in plots, and sends no series',
              len(pl) == 1 and pl[0]['active'] == 1 and not full(evs), str(pl)[:200])
        evs = command('i', {'key': 'g'})
        ser = [e for e in evs if e.get('ev') == 'series']
        apps = [e for e in ser if e.get('op') == 'append']
        check('a run appends to the active window only', len(apps) >= 3 and all(e['win'] == 1 for e in apps),
              str([(e['win'], e.get('op')) for e in ser])[:300])
        check('... and ends with a full series for each window, the active one first',
              [e['win'] for e in full(evs)] == [1, 2] and all(e['rows'] == 20001 for e in full(evs)),
              str([(e['win'], e['rows']) for e in full(evs)]))
        after({'cmd': 'click', 'win': 2})
        evs = command('m', {'key': 'd'})
        pl = plots(evs)
        check('Makewindow/Destroy of window 2: plots has window 1 only, active',
              len(pl) == 1 and wins(pl[0]) == [1] and pl[0]['active'] == 1 and not full(evs), str(pl)[:300])
        command('m', {'key': 'c'})
        evs = command('m', {'key': 'c'})
        check('two more windows', plots(evs) and wins(plots(evs)[0]) == [1, 2, 3], str(plots(evs))[:200])
        evs = command('m', {'key': 'k'}, {'key': 'y'})
        pl = plots(evs)
        check('Makewindow/Kill all leaves window 1', len(pl) == 1 and wins(pl[0]) == [1] and pl[0]['active'] == 1,
              str(pl)[:300])
        evs = command('m', {'key': 'c'})
        check('a window made again gets its series again', [e['win'] for e in full(evs)] == [2],
              str([(e['win'], e['rows']) for e in full(evs)]))
        evs = after({'cmd': 'redraw'})
        check('a redraw sends neither plots nor series', not plots(evs) and not full(evs))
        evs = after({'cmd': 'data', 'events': ['series']})
        check('without plots in the list: series only', not plots(evs) and [e['win'] for e in full(evs)] == [2, 1],
              str([(e['ev'], e.get('win')) for e in evs if e.get('ev') in ('plots', 'series')]))
    finally:
        stop_server(proc4, run4, send4)


check_plot_windows()

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
