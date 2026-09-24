#!/usr/bin/env python3
"""Drive xppautX --server through the JSON protocol and check what comes back.

usage: tools/servercheck.py [--server ./xppautX] [--ode examples/ode/lecar.ode] [-v]

Plays a fixed session (integrate, change a parameter, answer a menu, a
string prompt and a form, find an equilibrium, open a second plot window)
and prints PASS/FAIL per step. No display needed; runs in a few seconds.
"""
import argparse, base64, cmath, glob, hashlib, json, os, re, shutil, struct, subprocess, sys, tempfile, threading, queue

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
check('hello says protocol 2, and no draw ops or palette follow (removed in 2)',
      hello is not None and hello.get('protocol') == 2 and not any(e.get('ev') in ('draw', 'palette') for e in evs),
      str(hello and hello.get('protocol')))
check('state after hello', st is not None and any(p[0] == 'iapp' for p in st['pars']), str(st))
check('window 1 is created', any(e.get('ev') == 'window' and e.get('op') == 'create' and e.get('win') == 1
                                 for e in evs))
send(cmd='size', win=1, w=800, h=600)
evs, _ = collect(is_idle)
check('size is no command any more (removed in protocol 2): nothing but state and idle',
      [e.get('ev') for e in evs] == ['state', 'idle'], str(evs)[:200])
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

send(cmd='data', events=['series'])
collect(is_idle)
send(cmd='slide', name='iapp', value=0.07, rerun=1)
evs, _ = collect(is_idle, timeout=30)
st = last_state(evs)
ser = [e for e in evs if e.get('ev') == 'series' and 'op' not in e]
check('slide sets the parameter and integrates again (a new series of 601 rows)',
      st is not None and dict(st['pars'])['iapp'] == 0.07 and ser and ser[-1]['rows'] == 601,
      str(st and st['pars']) + str(ser)[:200])
send(cmd='data', events=[])
collect(is_idle)
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
    check('a file ask for writing says so (mode write)', ask.get('mode') == 'write', str(ask)[:200])
send(cmd='key', key='f')
send(cmd='key', key='r')
evs, ask = collect(lambda e: e.get('ev') == 'ask')
check('File/Read set asks for a file to read (mode read)', ask is not None and ask['kind'] == 'file'
      and ask.get('mode') == 'read' and ask.get('wild') == '*.set', str(ask)[:200])
if ask:
    send(cmd='answer', id=ask['id'], ok=0)
collect(is_idle)


# The model's folder for a client that cannot reach it (docs/protocol.md
# "Files"): the `file` command puts, gets and lists files there, base names
# only, as the /files endpoints of browser mode do (tools/webcheck.py).
def file_cmd(**kw):
    send(cmd='file', **kw)
    evs, ev = collect(lambda e: e.get('ev') == 'file')
    collect(is_idle)
    return ev or {}


blob = bytes(range(256)) * 4 + b'\x00\r\n\x1a'
before = sorted(os.listdir(run))
outside = os.path.join(os.path.dirname(run), 'x')  # where ../x would land
stat_of = lambda p: (os.stat(p).st_mtime_ns, os.stat(p).st_size) if os.path.exists(p) else None
above = stat_of(outside)
ev = file_cmd(op='put', name='srv.bin', data=base64.b64encode(blob).decode())
with open(os.path.join(run, 'srv.bin'), 'rb') as f:
    on_disk = f.read()
check('file put writes the bytes into the model\'s folder', ev.get('ok') == 1 and ev.get('name') == 'srv.bin'
      and ev.get('size') == len(blob) and ev.get('sha256') == hashlib.sha256(blob).hexdigest() and on_disk == blob, str(ev))
ev = file_cmd(op='get', name='srv.bin')
check('file get gives them back', ev.get('ok') == 1 and base64.b64decode(ev.get('data', '')) == blob
      and ev.get('sha256') == hashlib.sha256(blob).hexdigest(), str(ev)[:200])
ev = file_cmd(op='list')
names = [f['name'] for f in ev.get('files', [])]
check('file list names the folder\'s files with their digests', ev.get('ok') == 1 and 'srv.bin' in names
      and 'lecar.ode' in names and all(len(f['sha256']) == 64 for f in ev['files']), str(ev)[:300])
refused = {n: file_cmd(op='put', name=n, data='eA==').get('ok') for n in ['../x', 'a/b', 'a\\b', '.hidden', '..', 'C:x', '']}
refused['a\\u0000b'] = file_cmd(op='put', name='a\x00b', data='eA==').get('ok')
refused['get ../lecar.ode'] = file_cmd(op='get', name='../' + os.path.basename(run) + '/lecar.ode').get('ok')
check('file put and get refuse anything but a base name', all(v == 0 for v in refused.values()), str(refused))
ev = file_cmd(op='put', name='bad.bin', data='not base64!')
check('file put refuses data that is not base64', ev.get('ok') == 0 and 'base64' in ev.get('error', ''), str(ev))
ev = file_cmd(op='get', name='none.bin')
check('file get of a missing file says so', ev.get('ok') == 0 and ev.get('error'), str(ev))
check('refused file commands leave nothing behind', sorted(os.listdir(run)) == sorted(before + ['srv.bin'])
      and stat_of(outside) == above, str(sorted(os.listdir(run))))

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
ev = (eq or {}).get('eigenvalues') or []
check('with its eigenvalues, one [re, im] per variable, as its counts say',
      len(ev) == len(eq['values']) and sum(1 for r, i in ev if r < 0) == eq['cminus'] + eq['rminus'], str(ev))
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
check('the animation window opens', any(e.get('ev') == 'window' and e.get('win') == 104
      and e.get('op') == 'create' for e in evs), str([e for e in evs if e.get('ev') == 'window']))
send(cmd='ani', op='close')
evs, _ = collect(is_idle)
check('the animation window closes', any(e.get('ev') == 'window' and e.get('op') == 'destroy' for e in evs))

send(cmd='plotvars', how=2, names=['V', 'W'])
evs, _ = collect(is_idle)
ap = [e for e in evs if e.get('ev') == 'aplot']
check('the IC arry button opens an array plot', ap and ap[-1]['nx'] == 2 and len(ap[-1]['cells']) == 2 * ap[-1]['ny'],
      str(ap[-1:])[:200])

# docs/ui-v2.md T12: `values`, the cells' numbers before XPP maps them to a
# colour, equal the browser's own numbers for the same rows and columns.
# optimize_aplot (core/graf_par.c) always starts at row 0 with ColSkip 1 and
# picks RowSkip so `ny` rows span the run (ndown = min(201, nrows), nskip =
# nrows // ndown, both integer division as the core computes them); the
# columns are V, W (plotvars picked them, in that order).
send(cmd='state')
evs, st = collect(is_state)
collect(is_idle)
nrows = st['rows']
send(cmd='browser', **{'from': 0, 'count': nrows, 'col': 1, 'ncol': 500})
evs, br = collect(lambda e: e.get('ev') == 'browser')
collect(is_idle)
iv, iw = br['cols'].index('V'), br['cols'].index('W')
nskip = max(1, nrows // ap[-1]['ny'])
want = []
for j in range(ap[-1]['ny']):
    row = br['data'][nskip * j]
    want += [f32(row[iv]), f32(row[iw])]
got = values({'data': ap[-1]['values']}, ap[-1].get('enc'))
check('aplot values equal the browser numbers for the same rows and columns',
      same_floats(got, want), '%s != %s' % (got[:6], want[:6]))

# the same, base64 float32, when the client last asked for it (reused from
# the "data" subscription's own "enc", core/plot_data_want_f32)
send(cmd='data', events=[], enc='f32')
collect(is_idle)
send(cmd='aplot', op='redraw')
evs, _ = collect(is_idle)
ap2 = [e for e in evs if e.get('ev') == 'aplot']
check('aplot values as base64 float32 when the client asked for it',
      ap2 and ap2[-1].get('enc') == 'f32' and isinstance(ap2[-1]['values'], str)
      and same_floats(values({'data': ap2[-1]['values']}, 'f32'), got), str(ap2[-1:])[:200])
send(cmd='data', events=[])
collect(is_idle)

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
send(cmd='auto', op='redraw')
evs, _ = collect(is_idle)
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


# The values panel and the run history of web2 (GitHub #18): hello's
# defaults, state's now, a batched set with rerun, the ICs from where the
# last run ended, the state at the start of a run, the series version, and
# the erase/redraw events of the Erase and Redraw commands only.
def check_values_protocol():
    pv, rv, sndv, colv, _ = launch_server()

    def after(**cmd):
        sndv(**cmd)
        return colv(is_idle, timeout=30)[0]

    def keys(key, *answers):
        sndv(cmd='key', key=key)
        got, pending = [], list(answers)
        while True:
            evs, e = colv(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            sndv(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))

    full = lambda evs: [e for e in evs if e.get('ev') == 'series' and 'op' not in e]
    pics = lambda evs: [(e['ev'], e['win']) for e in evs if e.get('ev') in ('erase', 'redraw')]
    par = lambda st, n: next(v for k, v in st['pars'] if k.lower() == n)
    ic = lambda st, n: next(v for k, v in st['ics'] if k.lower() == n)
    try:
        evs, _ = colv(is_idle)
        hv = next((e for e in evs if e.get('ev') == 'hello'), {})
        st0 = last_state(evs)
        d = hv.get('defaults', {})
        check('hello: defaults, one per parameter and IC, equal to the values as loaded',
              st0 and len(d.get('pars', [])) == len(st0['pars']) and len(d.get('ics', [])) == len(st0['ics'])
              and d['pars'] == [v for _, v in st0['pars']] and d['ics'] == [v for _, v in st0['ics']], str(d)[:200])
        check('state: no now before any run', st0 and 'now' not in st0, str(st0)[:200])
        after(cmd='data', events=['series'])
        evs = keys('i', {'key': 'g'})
        st, ser = last_state(evs), full(evs)
        cols = {c['col']: c['data'] for c in ser[-1]['columns']} if ser else {}
        check('a run: the full series carries the data version',
              ser and isinstance(ser[-1].get('version'), int), str(ser and {k: v for k, v in ser[-1].items() if k != 'columns'})[:200])
        check('state: now is where the run ended (the last stored row)',
              st and len(st.get('now', [])) == len(st['ics']) and ser
              and all(abs(st['now'][i] - cols[i + 1][-1]) < 1e-6 for i in range(len(st['ics'])) if i + 1 in cols),
              str(st and st.get('now')))
        v0 = ser[-1]['version'] if ser else None

        # one command sets several values and runs once
        evs = after(cmd='set', values=[{'kind': 'par', 'name': 'iapp', 'text': '0.1'},
                                       {'kind': 'ic', 'name': 'v', 'value': -0.2}], rerun=1)
        st, ser = last_state(evs), full(evs)
        v_col = ser[-1]['columns'][[c['col'] for c in ser[-1]['columns']].index(1)]['data'] if ser else []
        check('set values[]: both set, then one run from them (rerun)',
              st and abs(par(st, 'iapp') - 0.1) < 1e-12 and abs(ic(st, 'v') + 0.2) < 1e-12 and len(ser) == 1
              and ser[0]['rows'] == 601 and v_col and abs(v_col[0] - f32(-0.2)) < 1e-9 and ser[0]['version'] != v0,
              str(st and st['pars'][:3]) + str([(s['rows'], s.get('version')) for s in ser]))
        check('the Erase-free rerun sends no erase event', not pics(evs), str(pics(evs)))
        evs = after(cmd='set', values=[{'kind': 'par', 'name': 'iapp', 'text': '%nosuch+'}], rerun=1)
        check('set values[] with a bad formula: an error, and no run',
              any(e.get('ev') == 'message' and 'error' in e for e in evs) and not full(evs), str(evs)[:300])
        before = last_state(after(cmd='state'))
        evs = after(cmd='set', kind='ic', **{'from': 'last'})
        st = last_state(evs)
        check('set from last: the ICs become now, without a run',
              st and st['ics'] and [v for _, v in st['ics']] == before['now'] and not full(evs),
              str(st and st['ics']) + ' vs ' + str(before and before.get('now')))
        after(cmd='set', kind='ic', name='v', value=-0.3)
        prev_now = last_state(after(cmd='state'))['now']
        evs = keys('i', {'key': 'l'})
        states = [i for i, e in enumerate(evs) if is_state(e)]
        sers = [i for i, e in enumerate(evs) if e.get('ev') == 'series']
        first = evs[states[0]] if states else None
        check('Initialconds/Last: a state at the start of the run already has the ICs from where the last one ended',
              first and sers and states[0] < sers[-1] and [v for _, v in first['ics']] == prev_now,
              str(first and first['ics']) + ' vs ' + str(prev_now))
        evs = after(cmd='default', kind='par', rerun=1)
        st = last_state(evs)
        check('default with rerun: the model values, then a run',
              st and par(st, 'iapp') == d['pars'][[k.lower() for k, _ in st['pars']].index('iapp')] and len(full(evs)) == 1,
              str(st and st['pars'][:3]))

        evs = after(cmd='slide', name='iapp', value=0.07)
        check('a slider rerun sends no erase event', not pics(evs) and len(full(evs)) == 1, str(pics(evs)))
        evs = keys('e')
        check('Erase: one erase event for the active window, no series', pics(evs) == [('erase', 1)] and not full(evs),
              str(pics(evs)))
        evs = keys('r')
        check('Redraw: one redraw event for the active window, no series', pics(evs) == [('redraw', 1)] and not full(evs),
              str(pics(evs)))
        evs = after(cmd='redraw')
        check('the redraw command (a reconnect) sends no erase or redraw event', not pics(evs), str(pics(evs)))
    finally:
        stop_server(pv, rv, sndv)


check_values_protocol()


# Nullclines, direction fields and flows as data (docs/protocol.md "The
# plot as data", docs/ui-v2.md T7), in plot coordinates.
def check_phase_data():
    proc5, run5, send5, collect5, _ = launch_server()

    def command(key, *answers):
        send5(cmd='key', key=key)
        got, pending = [], list(answers)
        while True:
            evs, e = collect5(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            send5(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))

    def after(cmd):
        send5(**cmd)
        return collect5(is_idle, timeout=30)[0]

    of = lambda evs, name: [e for e in evs if e.get('ev') == name]

    def segments_ok(segs, v):
        """'' when segs [x1,y1,x2,y2,...] are whole segments inside the view (a little slack), else why"""
        if not segs or len(segs) % 4:
            return '%d values' % len(segs)
        dx, dy = (v['xhi'] - v['xlo']) * 0.01, (v['yhi'] - v['ylo']) * 0.01
        for k in range(0, len(segs), 2):
            x, y = segs[k], segs[k + 1]
            if not (v['xlo'] - dx <= x <= v['xhi'] + dx and v['ylo'] - dy <= y <= v['yhi'] + dy):
                return 'point %s outside the view %s' % ((x, y), v)
        return ''

    def grid_ok(grid, n, v):
        """'' when the arrows start on the n x n grid of the view, else why"""
        if len(grid) != 4 * n * n:
            return 'count %d arrows' % (len(grid) // 4)
        xs = sorted({round(grid[k], 6) for k in range(0, len(grid), 4)})
        ys = sorted({round(grid[k + 1], 6) for k in range(0, len(grid), 4)})
        if len(xs) != n or len(ys) != n:
            return '%d x %d distinct starts' % (len(xs), len(ys))
        if abs(xs[0] - v['xlo']) > 1e-4 * (v['xhi'] - v['xlo']) or abs(ys[0] - v['ylo']) > 1e-4 * (v['yhi'] - v['ylo']):
            return 'first start %s, view %s' % ((xs[0], ys[0]), v)
        return ''

    try:
        evs, _ = collect5(is_idle)
        hello5 = next((e for e in evs if e.get('ev') == 'hello'), {})
        check('hello lists the nullclines and dfield features',
              {'nullclines', 'dfield'} <= set(hello5.get('features', [])), str(hello5.get('features')))
        evs = after({'cmd': 'data', 'events': ['nullclines', 'dfield']})
        nc, df = of(evs, 'nullclines'), of(evs, 'dfield')
        check('data sends both at once, empty, for window 1',
              len(nc) == 1 and len(df) == 1 and nc[0]['win'] == 1 and nc[0]['x'] == [] and nc[0]['frozen'] == []
              and df[0]['grid'] == [] and df[0]['flows'] == [] and not of(evs, 'series'), str(evs)[:300])

        evs = command('n', {'key': 'n'})
        nc, v = of(evs, 'nullclines'), last_state(evs)['view']
        n0 = nc[0] if nc else {}
        check('Nullcline/New: the event names V and W, colours 2 and 7',
              len(nc) == 1 and n0['xname'] == 'V' and n0['yname'] == 'W' and n0['xcolor'] == 2 and n0['ycolor'] == 7,
              str(nc)[:300])
        why = (segments_ok(n0.get('x', []), v) or segments_ok(n0.get('y', []), v)) if n0 else 'no event'
        check('... its x- and y-nullclines are whole segments inside the view', not why, why)
        evs = after({'cmd': 'redraw'})
        check('a redraw draws them again and sends nothing', not of(evs, 'nullclines') and not of(evs, 'dfield'))

        evs = command('d', {'key': 's'}, {'value': '16'})
        df, v = of(evs, 'dfield'), last_state(evs)['view']
        d0 = df[0] if df else {}
        check('Dir.field/Scaled: dfield has a 17 x 17 grid, scaled, in the curve\'s colour',
              len(df) == 1 and d0['n'] == 17 and d0['scaled'] == 1 and d0['color'] == 0
              and len(d0['grid']) == 4 * 289 and len(d0['speed']) == 289, str(df)[:300])
        why = grid_ok(d0.get('grid', []), 17, v) if d0 else 'no event'
        check('... its arrows start on the 17 x 17 grid of the view', not why, why)
        check('... unit directions', all(abs(d0['grid'][k + 2] ** 2 + d0['grid'][k + 3] ** 2 - 1) < 1e-5
                                         for k in range(0, len(d0.get('grid', [])), 4)) if d0 else False)

        evs = command('n', {'key': 'f'}, {'key': 'f'})
        nc = of(evs, 'nullclines')
        ncx = nc[0]['x'] if nc else []
        check('Nullcline/Freeze/Freeze: the frozen nullclines are in the event',
              len(nc) == 1 and len(nc[0]['frozen']) == 1 and nc[0]['frozen'][0]['x'] == nc[0]['x']
              and nc[0]['frozen'][0]['y'] == nc[0]['y'], str(nc)[:300])

        evs = command('e')
        nc, df = of(evs, 'nullclines'), of(evs, 'dfield')
        check('Erase clears them: both events empty',
              len(nc) == 1 and nc[0]['x'] == [] and nc[0]['frozen'] == [] and len(df) == 1 and df[0]['grid'] == [],
              str(evs)[:300])
        evs = after({'cmd': 'redraw'})
        nc, df = of(evs, 'nullclines'), of(evs, 'dfield')
        check('a redraw draws the nullclines again (not the field Erase turned off)',
              len(nc) == 1 and len(nc[0]['x']) > 0 and len(nc[0]['frozen']) == 1 and not df, str(evs)[:300])

        evs = command('d', {'key': 'f'}, {'value': '5'})
        df = of(evs, 'dfield')
        fl = df[0]['flows'] if df else []
        xs = fl[0]['x'] if fl else []
        starts = [0] + [i + 1 for i, a in enumerate(xs) if a is None]
        check('Dir.field/Flow: one curve of 72 trajectories (6 x 6, forward and back), finite points',
              len(fl) == 1 and fl[0]['color'] == 0 and len(starts) == 72 and len(xs) == len(fl[0]['y'])
              and all(a is None or abs(a) < 1e6 for a in xs), '%d curves, %d trajectories, %d points'
              % (len(fl), len(starts), len(xs)))
        check('... each trajectory starts on the grid',
              bool(fl) and all(abs((xs[s] + 0.6) / 0.36 - round((xs[s] + 0.6) / 0.36)) < 1e-4 for s in starts),
              str([xs[s] for s in starts][:8]))

        evs = after({'cmd': 'data', 'events': ['nullclines', 'dfield'], 'enc': 'f32'})
        nc, df = of(evs, 'nullclines'), of(evs, 'dfield')
        same = False
        if nc and df and fl:
            same = (nc[0].get('enc') == 'f32' and df[0].get('enc') == 'f32'
                    and same_floats(values({'data': nc[0]['x']}, 'f32'), values({'data': ncx}, None))
                    and same_floats(values({'data': df[0]['flows'][0]['x']}, 'f32'), values({'data': xs}, None)))
        check('enc f32: the same values as base64 float32', same, str(evs)[:200])
        evs = after({'cmd': 'data', 'events': ['series']})
        check('not asked for: neither is sent', not of(evs, 'nullclines') and not of(evs, 'dfield'))
    finally:
        stop_server(proc5, run5, send5)


check_phase_data()


# "Use this view" (docs/ui-v2.md T9, GitHub issue #18): {"cmd":"view",
# "win":w,"xlo":..,"xhi":..,"ylo":..,"yhi":..} sets window w's axes exactly
# as Window/Window (graf_par.c update_view) would, so state.view, "plots"
# and a PostScript export all agree with it afterward. An invalid range
# (inverted or non-finite) or a window that does not exist is refused
# (message error) and changes nothing.
def check_view():
    proc6, run6, send6, collect6, _ = launch_server()
    collect6(is_idle)  # startup hello/state/idle
    send6(cmd='data', events=['plots'])
    collect6(is_idle)

    def view_cmd(**kw):
        """sends {"cmd":"view",...}; returns (plots or None, message-error
        text or None, state.view) of the command's events up to its idle"""
        send6(cmd='view', **kw)
        evs, _ = collect6(is_idle)
        pl = next((e for e in evs if e.get('ev') == 'plots'), None)
        msg = next((e.get('error') for e in evs if e.get('ev') == 'message' and 'error' in e), None)
        st = last_state(evs)
        return pl, msg, st and st.get('view')

    def axes(v):
        return v and (v['xlo'], v['xhi'], v['ylo'], v['yhi'])

    try:
        pl, msg, view = view_cmd(win=1, xlo=-100, xhi=100, ylo=-100, yhi=100)
        w = pl and next((x for x in pl['windows'] if x['win'] == 1), None)
        check('view sets the window\'s axes: plots reflects them',
              msg is None and axes(w) == (-100, 100, -100, 100), str(w))
        check('... and state.view matches, for the active window',
              view is not None and view['win'] == 1 and axes(view) == (-100, 100, -100, 100), str(view))

        # an inverted range (lo >= hi) is refused and changes nothing
        _, msg2, view2 = view_cmd(win=1, xlo=5, xhi=5, ylo=-1, yhi=1)
        check('an inverted range (lo >= hi) is refused (message error)', msg2 is not None, str(msg2))
        check('... and leaves the axes unchanged', axes(view2) == (-100, 100, -100, 100), str(view2))

        # a non-finite bound is refused too
        _, msg3, view3 = view_cmd(win=1, xlo=float('-inf'), xhi=100, ylo=-1, yhi=1)
        check('a non-finite bound is refused too', msg3 is not None, str(msg3))
        check('... and leaves the axes unchanged', axes(view3) == (-100, 100, -100, 100), str(view3))

        # a window that does not exist is refused
        _, msg4, view4 = view_cmd(win=7, xlo=0, xhi=1, ylo=0, yhi=1)
        check('a window that does not exist is refused (message error)', msg4 is not None, str(msg4))
        check('... and leaves the axes unchanged', axes(view4) == (-100, 100, -100, 100), str(view4))

        # Graphic stuff/Postscript (key g, then its submenu's p): a menu,
        # then a form for the PS parameters (answered with its own
        # defaults), then a file to write; the written file's axes come
        # from the same MyGraph the view command set, so its tick labels
        # ("%g" of the boundary, Box_axis/draw_xtics/draw_ytics in
        # axes2.c) are the boundary values themselves, -100 and 100 (a
        # symmetric [-100,100] range makes make_tics() choose a 20-wide
        # tic, which lands exactly on the boundary).
        send6(cmd='key', key='g')
        evs, ask = collect6(lambda e: e.get('ev') == 'ask')
        check('Graphic stuff opens its submenu (curves)',
              ask is not None and ask['kind'] == 'menu' and 'p' in ask.get('keys', ''), str(ask))
        if ask:
            send6(cmd='answer', id=ask['id'], key='p')
        evs, ask2 = collect6(lambda e: e.get('ev') == 'ask')
        check('Postscript asks for its PS parameters first',
              ask2 is not None and ask2['kind'] == 'form', str(ask2))
        if ask2:
            send6(cmd='answer', id=ask2['id'], ok=1, values=ask2['values'])
        evs, ask3 = collect6(lambda e: e.get('ev') == 'ask')
        check('... then a file to write it to (mode write, *.ps)',
              ask3 is not None and ask3['kind'] == 'file' and ask3.get('mode') == 'write'
              and ask3.get('wild') == '*.ps', str(ask3))
        psname = 't9view.ps'
        if ask3:
            send6(cmd='answer', id=ask3['id'], ok=1, file=psname)
        collect6(is_idle)
        pspath = os.path.join(run6, psname)
        ps = ''
        if os.path.exists(pspath):
            with open(pspath, encoding='latin-1') as f:
                ps = f.read()
        check('the PostScript file was written', ps != '')
        check("its axes use the new view: the boundary tick labels (-100, 100) appear in the file's text",
              '(-100) ' in ps and '(100) ' in ps, str(len(ps)))
    finally:
        stop_server(proc6, run6, send6)


# 3D plots turned by the client (docs/ui-v2.md T14, GitHub issue #18):
# {"cmd":"view3d","win":w,"theta":..,"phi":..} sets window w's angles
# directly and redraws (core/ui_json.cpp view3d_command), so "plots" and
# state.view.theta/phi agree with whatever web2 settled on after
# projecting the box itself and turning it locally (no need to replay
# rotate's pixel deltas). lorenz.ode sets axes=3d and phi=60 (theta stays
# the default 45).
def check_view3d():
    proc7, run7, send7, collect7, _ = launch_server(ode='examples/ode/lorenz.ode')
    # runnow=1 loads, then runs, as two command cycles (the load's own idle
    # comes first, at row 0; the run keeps going in the background and
    # ends with its own state/idle some time later): the first idle is
    # only the load's, so keep collecting (a short timeout: nothing more
    # means the run's own idle already came) until the stored rows stop
    # growing, so what follows starts from the settled run.
    evs0, _ = collect7(is_idle, timeout=30)
    rows0 = last_state(evs0) and last_state(evs0).get('rows')
    for _ in range(9):
        more, _ = collect7(is_idle, timeout=3)
        if not more:
            break
        evs0 = more
        rows = last_state(evs0) and last_state(evs0).get('rows')
        if rows == rows0:
            break
        rows0 = rows
    view0 = last_state(evs0) and last_state(evs0)['view']
    check('lorenz.ode opens a 3D window at its @ phi=60 (theta the default 45)',
          view0 is not None and view0['three'] == 1 and view0['theta'] == 45 and view0['phi'] == 60, str(view0))

    send7(cmd='data', events=['plots'])
    collect7(is_idle)

    def view3d_cmd(**kw):
        """sends {"cmd":"view3d",...}; returns (plots or None, message-error
        text or None, state.view) of the command's events up to its idle"""
        send7(cmd='view3d', **kw)
        evs, _ = collect7(is_idle)
        pl = next((e for e in evs if e.get('ev') == 'plots'), None)
        msg = next((e.get('error') for e in evs if e.get('ev') == 'message' and 'error' in e), None)
        st = last_state(evs)
        return pl, msg, st and st.get('view')

    def angles(v):
        return v and (v['theta'], v['phi'])

    try:
        pl, msg, view = view3d_cmd(win=1, theta=10, phi=-20)
        w = pl and next((x for x in pl['windows'] if x['win'] == 1), None)
        check('view3d sets the window\'s angles: plots reflects them',
              msg is None and w is not None and (w['theta'], w['phi']) == (10, -20), str(w))
        check('... and state.view.theta/phi matches, for the active window',
              view is not None and view['win'] == 1 and angles(view) == (10, -20), str(view))

        # a non-finite angle is refused and changes nothing
        _, msg2, view2 = view3d_cmd(win=1, theta=float('nan'), phi=0)
        check('a non-finite angle is refused (message error)', msg2 is not None, str(msg2))
        check('... and leaves the angles unchanged', angles(view2) == (10, -20), str(view2))

        # a window that does not exist is refused
        _, msg3, view3 = view3d_cmd(win=7, theta=0, phi=0)
        check('a window that does not exist is refused (message error)', msg3 is not None, str(msg3))
        check('... and leaves the angles unchanged', angles(view3) == (10, -20), str(view3))
    finally:
        stop_server(proc7, run7, send7)


# Marks as data (docs/protocol.md "The plot as data", docs/ui-v2.md T8): the
# equilibria Sing pts marks, Text,etc's text, arrows and markers, and frozen
# curves, as the classic window shows them: a redraw draws the labels,
# objects and frozen curves again but not the equilibria, Erase clears them
# all, a deleted one goes at once.
def check_marks():
    proc6, run6, send6, collect6, _ = launch_server()
    of = lambda evs, name: [e for e in evs if e.get('ev') == name]

    def command(key, answer):
        """a key, its asks answered by answer(ask, n) (n counts them), up to its idle"""
        send6(cmd='key', key=key)
        got, n = [], 0
        while True:
            evs, e = collect6(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            reply = answer(e, n) or {'ok': 0}
            n += 1
            send6(cmd='answer', id=e['id'], **reply)

    def keys(*replies):
        """answers in order: a str is a menu key, a dict the answer itself; then cancel"""
        def answer(e, n):
            if n >= len(replies):
                return None
            return {'key': replies[n]} if isinstance(replies[n], str) else replies[n]
        return answer

    def after(cmd):
        send6(**cmd)
        return collect6(is_idle, timeout=30)[0]

    def marks_of(evs):
        m = of(evs, 'marks')
        return m[-1] if len(m) == 1 else None

    def near(a, b, tol):
        return a is not None and b is not None and abs(a - b) <= tol

    try:
        evs, _ = collect6(is_idle)
        hello6 = next((e for e in evs if e.get('ev') == 'hello'), {})
        check('hello lists the marks feature', 'marks' in hello6.get('features', []), str(hello6.get('features')))
        evs = after({'cmd': 'data', 'events': ['marks', 'plots', 'series']})
        m = marks_of(evs)
        check('data sends marks at once, empty, for window 1',
              m is not None and m['win'] == 1 and m['equilibria'] == [] and m['text'] == [] and m['arrows'] == []
              and m['markers'] == [] and m['frozen'] == [], str(of(evs, 'marks'))[:300])
        curve = of(evs, 'plots')[0]['windows'][0]['curves'][0]

        evs = command('i', keys('g'))
        ser = of(evs, 'series')
        v = last_state(evs)['view']
        px = (v['xhi'] - v['xlo']) / abs(v['right'] - v['left']) * 1.5
        py = (v['yhi'] - v['ylo']) / abs(v['bottom'] - v['top']) * 1.5
        check('an integration sends no marks', bool(ser) and not of(evs, 'marks'))
        cols = {c['col']: c['data'] for c in ser[-1]['columns']} if ser else {}

        typed = r'\1a\0-point \{2*3}'
        evs = command('t', keys('t', {'value': typed}, {'value': '3'}, {'xd': -0.3, 'yd': 0.5}))
        m = marks_of(evs)
        t = m['text'] if m else []
        check('Text,etc/Text: a text mark with its text (\\{expr} filled in), size and position',
              len(t) == 1 and t[0]['text'] == r'\1a\0-point 6' and t[0]['size'] == 3 and t[0]['font'] == 0
              and near(t[0]['x'], -0.3, px) and near(t[0]['y'], 0.5, py), str(m)[:300])

        evs = command('t', keys('p', {'value': '0.2'}, {'value': '5'},
                                {'xd': 0.1, 'yd': 0.2, 'xd2': 0.6, 'yd2': 0.8}))
        m = marks_of(evs)
        a = m['arrows'] if m else []
        check('Text,etc/Pointer: an arrow mark from the first point to the second, its size and colour',
              len(a) == 1 and a[0]['kind'] == 'pointer' and a[0]['size'] == 0.2 and a[0]['color'] == 5
              and near(a[0]['x1'], 0.1, px) and near(a[0]['y1'], 0.2, py) and near(a[0]['x2'], 0.6, px)
              and near(a[0]['y2'], 0.8, py) and len(m['text']) == 1, str(m)[:300])

        evs = command('t', keys('m', {'values': ['3', '7', '2']}, {'xd': 0.9, 'yd': 0.1}))
        m = marks_of(evs)
        k = m['markers'] if m else []
        check('Text,etc/Marker: a marker mark with its shape, size, colour and position',
              len(k) == 1 and k[0]['shape'] == 'diamond' and k[0]['size'] == 2 and k[0]['color'] == 7
              and near(k[0]['x'], 0.9, px) and near(k[0]['y'], 0.1, py), str(m)[:300])

        evs = command('g', keys('f', 'f', {'values': ['4', 'first run', 'frz1']}))
        m = marks_of(evs)
        f = m['frozen'] if m else []
        check('Graphic stuff/Freeze: a frozen curve equal to the series at freeze time, its key and colour',
              len(f) == 1 and f[0]['key'] == 'first run' and f[0]['name'] == 'frz1' and f[0]['color'] == 4
              and f[0]['line'] == 1 and f[0]['x'] == cols.get(curve['x']) and f[0]['y'] == cols.get(curve['y'])
              and len(f[0]['x']) > 2, str(m)[:300])

        got = command('s', lambda e, n: {'key': 'g' if n == 0 else 'n'})
        eq = next(iter(of(got, 'equilibrium')), None)
        m = marks_of(got)
        q = m['equilibria'] if m else []
        vals = [val for _, val in eq['values']] if eq else []
        check("Sing pts: an equilibrium mark at the equilibrium's values of the plotted variables",
              eq is not None and len(q) == 1 and near(q[0]['x'], vals[curve['x'] - 1], 1e-12)
              and near(q[0]['y'], vals[curve['y'] - 1], 1e-12), '%s vs %s' % (q, vals))
        check('... its type and symbol as the stability says',
              len(q) == 1 and q[0]['symbol'] == {'stable': 'circle', 'unstable': 'box', 'saddle': 'triangle'}[q[0]['type']]
              and (q[0]['type'] == 'stable') == (eq['cplus'] + eq['rplus'] == 0)
              and (q[0]['type'] == 'saddle') == (eq['cplus'] + eq['rplus'] > 0 and eq['cminus'] + eq['rminus'] > 0),
              '%s, %s' % (q, eq and eq['type']))
        check('... the other marks unchanged', m is not None and len(m['text']) == 1 and len(m['frozen']) == 1)

        evs = after({'cmd': 'redraw'})
        m = marks_of(evs)
        check('a redraw draws text, objects and frozen curves again, not the equilibrium (as the classic window)',
              m is not None and m['equilibria'] == [] and len(m['text']) == 1 and len(m['arrows']) == 1
              and len(m['markers']) == 1 and len(m['frozen']) == 1, str(m)[:300])
        evs = after({'cmd': 'redraw'})
        check('... and a second one sends nothing', not of(evs, 'marks'))

        evs = after({'cmd': 'data', 'events': ['marks'], 'enc': 'f32'})
        m = marks_of(evs)
        check('enc f32: the frozen curve as base64 float32, the same values',
              m is not None and m.get('enc') == 'f32' and len(m['frozen']) == 1 and bool(f)
              and same_floats(values({'data': m['frozen'][0]['x']}, 'f32'), values({'data': f[0]['x']}, None)),
              str(m)[:200])
        after({'cmd': 'data', 'events': ['marks']})

        evs = command('g', keys('f', 'r'))
        m = marks_of(evs)
        check('Freeze/Remove all: the frozen curve goes at once', m is not None and m['frozen'] == []
              and len(m['text']) == 1, str(m)[:300])

        evs = command('e', keys())
        m = marks_of(evs)
        check('Erase clears every mark',
              m is not None and not any(m[k] for k in ('equilibria', 'text', 'arrows', 'markers', 'frozen')),
              str(m)[:300])
        evs = after({'cmd': 'redraw'})
        m = marks_of(evs)
        check('... a redraw brings the text and objects back', m is not None and len(m['text']) == 1
              and len(m['arrows']) == 1 and len(m['markers']) == 1, str(m)[:300])
        evs = command('t', keys('d'))
        m = marks_of(evs)
        check('Text,etc/Delete all deletes them', m is not None and m['text'] == [] and m['arrows'] == []
              and m['markers'] == [], str(m)[:300])
        evs = after({'cmd': 'data', 'events': ['series']})
        evs += command('s', lambda e, n: {'key': 'g' if n == 0 else 'n'})
        check('not asked for: no marks', not of(evs, 'marks'))
    finally:
        stop_server(proc6, run6, send6)


def check_ani_data():
    """The animation as data (docs/protocol.md "The animation as data",
    docs/ui-v2.md T13): tools/gui_test.ani (one of every command) on lecar.
    Each frame event's primitives are in unit coordinates of the dimension
    box, colours as XPP indices or #rrggbb; the player's step, seek and Go
    send the frames they show, Go at most 25 a second and always its last
    one."""
    proc6, run6, send6, collect6, _ = launch_server()
    shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'gui_test.ani'), run6)
    of = lambda evs, name: [e for e in evs if e.get('ev') == name]
    frames = lambda evs: [e for e in evs if e.get('ev') == 'ani' and e.get('op') == 'frame']
    states = lambda evs: [e for e in evs if e.get('ev') == 'ani' and 'op' not in e]

    def answered(pending):
        got = []
        while True:
            evs, e = collect6(lambda e: e.get('ev') in ('ask', 'idle'), timeout=60)
            got += evs
            if e is None or e['ev'] == 'idle':
                return got
            send6(cmd='answer', id=e['id'], **(pending.pop(0) if pending else {'ok': 0}))

    def command(key, *answers):
        send6(cmd='key', key=key)
        return answered(list(answers))

    def ani(op, *answers, **kw):
        send6(cmd='ani', op=op, **kw)
        return answered(list(answers))

    def colours_ok(f):
        """'' when every primitive's colour is an XPP index (0..10) or a colour map's #rrggbb, else why"""
        for p in f['prims']:
            c = p[4] if p[0] in ('dot', 'text') else p[5]
            if not ((isinstance(c, int) and 0 <= c <= 10) or (isinstance(c, str) and len(c) == 7 and c[0] == '#')):
                return 'colour %r of %s' % (c, p)
        return ''

    def in_unit(f):
        """None when every coordinate is in [0,1] but a line's (a line may leave the box), else what"""
        for p in f['prims']:
            if p[0] == 'line':
                continue
            pts = [(p[1], p[2])] + ([(p[3], p[4])] if p[0] == 'rect' else [])
            for u, v in pts:
                if not (0 <= u <= 1 and 0 <= v <= 1):
                    return '%s at %s' % (p, (u, v))
        return None

    try:
        evs, _ = collect6(is_idle)
        hello = of(evs, 'hello')
        check('ani data: hello lists the ani feature', bool(hello) and 'ani' in hello[0].get('features', []))
        send6(cmd='data', events=['ani'])
        evs, _ = collect6(is_idle)
        check('ani data: no frame before one is drawn', not frames(evs))
        command('i', {'key': 'g'})
        evs = command('v', {'key': 't'})
        check('ani data: Viewaxes/Toon opens the animation window',
              any(e.get('ev') == 'window' and e.get('win') == 104 and e.get('op') == 'create' for e in evs))
        evs = ani('file', {'file': 'gui_test.ani'})
        fr, st = frames(evs), states(evs)
        check('ani data: loading an animation shows its first frame', len(fr) == 1 and fr[0]['pos'] == 0
              and fr[0]['rows'] == 601 and bool(st) and st[-1]['loaded'] == 1, str(evs)[-400:])
        if not fr:
            return
        f0 = fr[0]
        check('ani data: the frame names its box, window, speed and time',
              f0['dim'] == [-0.6, -0.1, 0.4, 0.6] and (f0['w'], f0['h']) == (280, 350) and f0['speed'] == 5
              and f0['skip'] == 1 and f0['t'] == 0, str({k: v for k, v in f0.items() if k != 'prims'}))
        kinds = sorted({p[0] for p in f0['prims']})
        check('ani data: every kind of primitive (line, rect, circle, ellipse, dot, text)',
              kinds == ['circle', 'dot', 'ellipse', 'line', 'rect', 'text'], str(kinds))
        check('ani data: frame 0 has a primitive per drawing command of the .ani, colours as indices or #rrggbb',
              len(f0['prims']) == 13 and not colours_ok(f0), '%d primitives %s' % (len(f0['prims']), colours_ok(f0)))
        check('ani data: its coordinates are in [0,1] (only a line may leave the box)',
              in_unit(f0) is None, str(in_unit(f0)))
        spectral = [p for p in f0['prims'] if p[0] == 'circle' and isinstance(p[5], str)]
        check('ani data: a colour of the colour map is #rrggbb (fcircle ...;w)', len(spectral) == 1, str(spectral))

        evs = ani('step', n=1)
        fr = frames(evs)
        check('ani data: step sends the next frame, moved on from frame 0', len(fr) == 1 and fr[0]['pos'] == 1
              and fr[0]['t'] > 0 and fr[0]['prims'] != f0['prims'] and not colours_ok(fr[0]), str(fr)[:300])
        evs = ani('seek', pos=300)
        fr = frames(evs)
        bad = in_unit(fr[-1]) if fr else 'no frame'
        check('ani data: seek sends the frame it lands on, in unit coordinates',
              bool(fr) and fr[-1]['pos'] == 300 and bad is None, str(bad))
        evs = ani('speed', ms=20)
        check('ani data: speed sets the delay between frames', bool(states(evs)) and states(evs)[-1]['speed'] == 20,
              str(states(evs)))
        evs = ani('go')
        fr = frames(evs)
        # rows 300 to 600, 20 ms apart (6 s): at most 25 frames a second is far fewer than the 300 rows shown
        check('ani data: Go sends frames as it plays, at most 25 a second, and its last one',
              3 <= len(fr) < 300 and fr[-1]['pos'] == 600 and all(a['pos'] < b['pos'] for a, b in zip(fr, fr[1:])),
              '%d frames, last %s' % (len(fr), fr and fr[-1]['pos']))
        check('ani data: the last frame of Go is whole: in unit coordinates, its colours, the comets grown',
              bool(fr) and in_unit(fr[-1]) is None and not colours_ok(fr[-1])
              and len(fr[-1]['prims']) > len(f0['prims']),
              str(fr and (in_unit(fr[-1]), colours_ok(fr[-1]), len(fr[-1]['prims']))))

        # grab: the grab point's cross (the last two black lines), hit in unit coordinates
        evs = ani('grab')
        fr = frames(evs)
        check('ani data: Grab shows the grab points', bool(fr) and states(evs)[-1]['grab'] == 1, str(states(evs)))
        cross = [p for p in (fr[-1] if fr else f0)['prims'] if p[0] == 'line' and p[5] == 0][-2:]
        if len(cross) == 2:
            cx, cy = (cross[1][1] + cross[1][3]) / 2, (cross[1][2] + cross[1][4]) / 2
            ani('mouse', what='down', u=cx, v=cy)
            evs = ani('mouse', what='up', u=cx, v=cy)
            check('ani data: a press and release at the point in unit coordinates (u, v) grabs it',
                  bool(states(evs)) and states(evs)[-1]['grab'] == 0, str(states(evs)))
        else:
            check('ani data: a press and release at the point in unit coordinates (u, v) grabs it', False,
                  'no grab cross drawn')

        send6(cmd='data', events=[])
        collect6(is_idle)
        evs = ani('step', n=1)
        check('ani data: not asked for, no frame is sent', not frames(evs) and bool(states(evs)), str(evs)[:200])
    finally:
        stop_server(proc6, run6, send6)

check_view()
check_view3d()
check_marks()
check_ani_data()
# AUTO's info strip and stability circle as data (docs/protocol.md "The AUTO
# diagram as data", `autoinfo`), and a grab answered with a point of the
# diagram's data (`point`). lecar's steady-state branch from its "hopf" fixed
# point, then the periodic branch from the Hopf point, as
# examples/scripts/lecar_auto.jsonl does it.
def auto_scratch(p):
    """AUTO's private scratch directories of server process p (xpp_make_temp_dir)"""
    tmp = tempfile.gettempdir() if os.name == 'nt' else (os.environ.get('TMPDIR') or '/tmp')
    pre = 'xppautoX-%d-' % p.pid
    return [os.path.join(tmp, d) for d in os.listdir(tmp) if d.startswith(pre)]


NUM = r'([-+]?\d\.\d+E[-+]\d+)'


def printed_stability(p):
    """(branch, point) -> the eigenvalues or multipliers AUTO printed for it
    (its fort.9, kept as <model>.d in its scratch directory), the last printing"""
    out = {}
    for d in auto_scratch(p):
        for f in glob.glob(os.path.join(d, '*.d')):
            with open(f) as fh:
                for line in fh:
                    m = re.match(r'\s*(\d+)\s+(\d+)\s+(Eigenvalue|Multiplier)\s*(\d+)\s*' + NUM + r'\s*' + NUM + r'\s*$',
                                 line)
                    if m:
                        key = (int(m.group(1)), int(m.group(2)))
                        if int(m.group(4)) == (1 if m.group(3) == 'Eigenvalue' else 0):  # the first of a printing
                            out[key] = []
                        out.setdefault(key, []).append([float(m.group(5)), float(m.group(6))])
    return out


def strip_matches(info, diag):
    """None when autoinfo's point is the diagram data's point it names (branch, point, the
    parameter as the diagram's x), else why not"""
    if not 0 <= info['point'] < len(diag):
        return 'point %d of %d' % (info['point'], len(diag))
    p = diag[info['point']]
    if (abs(p[0]), p[1]) != (info['br'], info['pt']):
        return 'br/pt %s vs %s' % (p[:2], (info['br'], info['pt']))
    x = info['par'][0]['value']
    if abs(p[3] - x) > 1e-6 * max(1, abs(x)):
        return 'x %r vs %s %r' % (p[3], info['par'][0]['name'], x)
    return None


def close_pairs(a, b, tol):
    """two lists of [re, im] the same within tol (relative to 1 or more), in any order"""
    if a is None or b is None or len(a) != len(b):
        return False
    rest = list(b)
    for x in a:
        k = next((i for i, y in enumerate(rest) if abs(x[0] - y[0]) <= tol * max(1, abs(y[0]))
                  and abs(x[1] - y[1]) <= tol * max(1, abs(y[1]))), None)
        if k is None:
            return False
        rest.pop(k)
    return True


def rebuild_diagram(evs, pts):
    """the diagram data after these events: (br, pt, ty, x, y, y2, from) per point"""
    for e in evs:
        if e.get('ev') != 'diagram':
            continue
        if e['op'] == 'reset':
            del pts[e['keep']:]
        elif e['op'] == 'add':
            del pts[e['from']:]
            for r in e['runs']:
                for i, x in enumerate(r['x']):
                    pts.append((r['br'], r['pt'] + i, r['ty'], x, r['y'][i], (r.get('y2') or r['y'])[i],
                                r.get('from', 0) if i == 0 else 0))
    return pts


def lecar_to_auto(snd, col):
    """the "hopf" set, its fixed point as the IC, File/Auto, Run a steady state: the events"""
    def step(**c):
        snd(**c)
        return col(lambda e: is_idle(e) or e.get('ev') == 'ask', timeout=30)
    for c in ({'cmd': 'key', 'key': 'f'}, {'cmd': 'key', 'key': 'g'}, {'cmd': 'answer', 'key': 'd'},
              {'cmd': 'key', 'key': 's'}, {'cmd': 'answer', 'key': 'g'}, {'cmd': 'answer', 'key': 'n'},
              {'cmd': 'eqimport'}, {'cmd': 'key', 'key': 'f'}, {'cmd': 'key', 'key': 'a'}, {'cmd': 'auto', 'op': 'run'}):
        step(**c)
    snd(cmd='answer', key='s')
    evs, _ = col(is_idle, timeout=60)
    return evs


def infos(evs):
    return [e for e in evs if e.get('ev') == 'autoinfo']


def check_autoinfo():
    pa, ra, snda, cola, _ = launch_server()
    pb, rb, sndb, colb, _ = launch_server()
    try:
        cola(is_idle)
        colb(is_idle)
        snda(cmd='data', events=['autoinfo'])
        evs, _ = cola(is_idle)
        check('autoinfo: sent at once after data, empty before AUTO',
              [(e['info'], e['stab']) for e in infos(evs)] == [(None, None)], str(infos(evs)))
        evs = lecar_to_auto(snda, cola)
        diag_a = rebuild_diagram(evs, [])
        got = infos(evs)
        stab = got[-1]['stab'] if got else None
        last = diag_a[-1] if diag_a else None
        pr = printed_stability(pa).get(last[:2]) if last else None
        check("autoinfo: after a run, the circle is its last point's (e^eigenvalue), as AUTO printed it",
              stab is not None and got[-1]['info'] is None and stab['periodic'] == 0 and len(stab['circle']) == 2
              and close_pairs(stab['eig'], pr, 2e-5), '%s vs %s' % (got[-1:], pr))
        evs = lecar_to_auto(sndb, colb)
        diag_b = rebuild_diagram(evs, [])
        check('autoinfo: a client that did not ask gets none', not infos(evs))

        snda(cmd='auto', op='grab')
        evs, ask = cola(lambda e: e.get('ev') == 'ask')
        got = infos(evs)
        info = got[-1]['info'] if got else None
        check("autoinfo: Grab sends the strip of the point under the cursor (the first), the diagram data's point",
              info is not None and info['point'] == 0 and info['br'] == 1 and info['pt'] == 1
              and strip_matches(info, diag_a) is None, info and strip_matches(info, diag_a) or str(got))
        snda(cmd='answer', id=ask['id'], key='Tab')
        evs, ask = cola(lambda e: e.get('ev') == 'ask')
        got = infos(evs)
        info, stab = (got[-1]['info'], got[-1]['stab']) if got else (None, None)
        check('autoinfo: Tab to the Hopf point: its strip', info is not None and info['sym'] == 'HB'
              and info['lab'] > 0 and strip_matches(info, diag_a) is None, info and strip_matches(info, diag_a) or str(got))
        hb = info
        printed = printed_stability(pa)
        pr = printed.get((1, hb['pt'])) if hb else None
        check("autoinfo: the Hopf point's eigenvalues are what AUTO printed for it, a pair on the imaginary axis",
              stab is not None and stab['periodic'] == 0 and len(stab['circle']) == 2 and close_pairs(stab['eig'], pr, 2e-5)
              and all(abs(e[0]) < 1e-3 and abs(e[1]) > 0.1 for e in stab['eig']), '%s vs %s' % (stab, pr))
        check('autoinfo: the circle is e^eigenvalue', stab is not None and all(
            abs(complex(*z) - cmath.exp(complex(*e))) < 1e-9 for z, e in zip(stab['circle'], stab['eig'])))
        # a point of the data by its index moves the cursor there
        snda(cmd='answer', id=ask['id'], point=5)
        evs, ask = cola(lambda e: e.get('ev') == 'ask')
        got = infos(evs)
        info, stab = (got[-1]['info'], got[-1]['stab']) if got else (None, None)
        p5 = diag_a[5] if len(diag_a) > 5 else None
        pr = printed.get((info['br'], info['pt'])) if info else None
        check('grab by point: the cursor goes to point 5 of the data, its strip and eigenvalues',
              info is not None and p5 is not None and info['point'] == 5 and (info['br'], info['pt']) == p5[:2]
              and strip_matches(info, diag_a) is None and stab is not None and close_pairs(stab['eig'], pr, 2e-5),
              '%s %s %s' % (info, p5, pr))
        snda(cmd='answer', id=ask['id'], point=100000, key='Return')
        evs, ask = cola(lambda e: e.get('ev') == 'ask' or is_idle(e))
        check('grab by point: a point the data do not have is ignored, and so is its key',
              ask is not None and ask.get('ev') == 'ask' and not infos(evs), str(evs)[:300])
        # take the Hopf point by its index, in one answer; the other server by its keys
        snda(cmd='answer', id=ask['id'], point=hb['point'], key='Return')
        evs, end = cola(is_idle)
        check('grab by point: point and Return take it in one answer', end is not None
              and not any(e.get('ev') == 'ask' for e in evs), str(evs)[:300])
        sndb(cmd='auto', op='grab')
        for k in ['Tab', 'Return']:
            evs, ask = colb(lambda e: e.get('ev') == 'ask')
            sndb(cmd='answer', id=ask['id'], key=k)
        colb(is_idle)
        run_evs = []
        for snd, col, diag in ((snda, cola, diag_a), (sndb, colb, diag_b)):
            snd(cmd='auto', op='run')
            evs, ask = col(lambda e: e.get('ev') == 'ask', timeout=30)
            snd(cmd='answer', id=ask['id'], key='p')
            evs, _ = col(is_idle, timeout=120)
            rebuild_diagram(evs, diag)
            run_evs = run_evs or evs
        per = [p for p in diag_a if p[0] == 2]
        check('grab by point, then Run: the periodic branch is the one grabbing by keys gives',
              len(per) > 10 and diag_a == diag_b, '%d periodic points; %d vs %d points' % (len(per), len(diag_a), len(diag_b)))
        check('diagram: the periodic branch says it started from the Hopf label (from)',
              per and per[0][6] == hb['lab'] and all(p[6] == 0 for p in per[1:]), str(per[:2]))
        got = infos(run_evs)
        stab = got[-1]['stab'] if got else None
        pr = printed_stability(pa).get(per[-1][:2]) if per else None
        check("autoinfo: after the periodic run, the circle holds its last point's Floquet multipliers, as AUTO printed them",
              stab is not None and stab['periodic'] == 1 and 'eig' not in stab and close_pairs(stab['circle'], pr, 2e-5)
              and any(abs(complex(*z) - 1) < 1e-3 for z in stab['circle']), '%s vs %s' % (stab, pr))
        snda(cmd='redraw')
        evs, _ = cola(is_idle)
        check('autoinfo: a redraw sends none (nothing it shows changed)', not infos(evs), str(infos(evs))[:200])
        snda(cmd='auto', op='point', xd=0.125, yd=-0.25)
        evs, _ = cola(is_idle)
        hint = [e.get('auto') for e in evs if e.get('ev') == 'message' and 'auto' in e]
        check('auto point in data coordinates shows exactly those', hint and hint[-1] == 'x=0.125,y=-0.25', str(hint))
        snda(cmd='auto', op='close')
        evs, _ = cola(is_idle)
        got = infos(evs)
        check('autoinfo: closing AUTO empties it', got and got[-1]['info'] is None and got[-1]['stab'] is None, str(got))
    finally:
        stop_server(pa, ra, snda)
        stop_server(pb, rb, sndb)


check_autoinfo()

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
