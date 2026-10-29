"""A small client for xppautX --server, shared by the protocol checks.

    s = Server('./xppautX', 'examples/ode/lecar.odex')
    s.collect(is_done)
    s.send(cmd='run.initial')
    evs, end = s.collect(is_done)

Each Server runs in its own scratch directory (the model is copied there),
reads events on a thread and hands them out with collect().
"""
import glob
import atexit, base64, copy, hashlib, json, os, queue, re, shutil, struct, subprocess, tempfile, threading, time

# No check touches the user's own per-user settings (keymap.json, W211;
# preferences.setx, W232): every xppautX a check script that imports this starts
# reads and writes a folder of its own (XPP_CONFIG_DIR, core/xpp_files.h),
# removed at exit
CONFIG_ROOT = tempfile.mkdtemp(prefix='xppconfig')
os.environ['XPP_CONFIG_DIR'] = os.path.join(CONFIG_ROOT, 'xppautX')
atexit.register(shutil.rmtree, CONFIG_ROOT, ignore_errors=True)

# XPP_CHECK_SLOW=F multiplies every wait by F, for a server under a slow
# tool (tools/valgrindcheck.sh: memcheck runs it some 30 times slower)
SLOW = float(os.environ.get('XPP_CHECK_SLOW', '1'))
POLL_SECONDS = 0.05  # yield between observations where the OS provides no event reader
WAIT_SECONDS = 10 * SLOW  # safety ceiling for an external condition, not a speed assertion


class Checker:
    """The PASS/FAIL line and counter every check script prints (verify.sh counts the ^PASS lines).
    detail_sep joins a failed check's detail to its name."""

    def __init__(self, detail_sep='  '):
        self.checks = self.failures = 0
        self.detail_sep = detail_sep

    def __call__(self, name, ok, detail=''):
        self.checks += 1
        self.failures += not ok
        print(('PASS ' if ok else 'FAIL ') + name + ('' if ok else self.detail_sep + str(detail)))


def wait_until(predicate, timeout=WAIT_SECONDS):
    deadline = time.monotonic() + timeout
    while True:
        if predicate():
            return True
        if time.monotonic() >= deadline:
            return False
        time.sleep(POLL_SECONDS)


def macos_thread_states(output):
    """ps(1)'s -M uses its thread table, even with -o state=; read STAT.

    Format: USER PID TT %CPU STAT PRI STIME UTIME COMMAND (Apple adv_cmds,
    ps/ps.c mfmt). Keep the header so a changed format is a shown error.
    """
    lines = output.splitlines()
    if not lines:
        return []
    header = lines[0].split()
    if 'STAT' not in header:
        raise ValueError('ps -M output:1: missing STAT column: ' + lines[0])
    # Further threads leave USER, PID, TT and COMMAND blank; alignment stays.
    column = lines[0].index('STAT')
    states = []
    for number, line in enumerate(lines[1:], 2):
        if len(line) <= column or line[column].isspace():
            raise ValueError('ps -M output:%d: missing thread state: %s' % (number, line))
        states.append(line[column:].split()[0])
    return states


class LogLines(list):
    """stderr's reader signals each appended line; checks never delay for logging."""
    def __init__(self):
        super().__init__()
        self.changed = threading.Condition()

    def append(self, line):
        with self.changed:
            super().append(line)
            self.changed.notify_all()

    def wait_for(self, predicate, timeout=WAIT_SECONDS):
        with self.changed:
            return self.changed.wait_for(lambda: predicate(self), timeout)


def _reject_non_finite(text):
    """json.loads' parse_constant: NaN/Infinity/-Infinity are not valid
    JSON (W35a, core/json_number.h); a strict parse must fail on them
    instead of silently making a Python float, so a "-nan" that leaks
    past the core's writer fails a check instead of round-tripping."""
    raise ValueError('non-finite JSON constant: %s' % text)


# the one format number of the protocol and every file of ours (core/format_version.h)
FORMAT_VERSION = 4


def versioned(text):
    """a model's text as its reader takes it: `version N` first (every position the core reports is then one line
    further); text that has its own, a converted model, is left as it is"""
    if re.match(r'(?:[ \t]*(?:#[^\n]*)?\r?\n)*[ \t]*version[ \t]+\d', text):
        return text
    return 'version %d\n' % FORMAT_VERSION + text


def versioned_anix(text):
    """an animation's: its version is a statement of its own, `version: N`"""
    return text if re.match(r'[ \t]*version[ \t]*:', text) else 'version: %d\n' % FORMAT_VERSION + text


def is_done(e):
    return e.get('ev') == 'done'


# every heavy data event a client subscribes to (hello's `features`)
ALL_EVENTS = ['plot.series', 'plot.shown', 'nullcline.curves', 'dirfield.field', 'plot.marks', 'animation.frame', 'auto.info', 'auto.settings', 'run.numerics', 'window.list', 'recording.player']


def save_permission(cmd):
    """Checks authorize their scratch saves explicitly (W129); replace=0 or
    confirm=False exercises the core's question instead. Answers never
    change policy."""
    kind, op = cmd.get('cmd'), cmd.get('op')
    if kind in ('equilibrium.export', 'nullcline.export', 'dirfield.export', 'session.save_as', 'session.save_copy',
                'session.export_info', 'recording.stop', 'recording.set_step_note'):
        cmd.setdefault('confirm', True)
        return cmd
    if op in {
            'session': ('save',),
            'aplot': ('print', 'gif'),
            'ani': ('go',)}.get(kind, ()):
        cmd.setdefault('replace', 1)
    return cmd


def is_ask(e):
    return e.get('ev') == 'ask'


def placed(e):
    """An error event's text as the core renders an error (xpp::Error::text,
    docs/protocol.md "Errors"): "file:line:col: error", leaving out what is
    not known; "" for an event with no error."""
    if not e.get('error'):
        return ''
    t, line, col = e.get('file') or '', e.get('line') or 0, e.get('col') or 0
    if line > 0:
        t += (':' if t else 'line ') + str(line)
        if col > 0:
            t += ':' + str(col)
    return (t + ': ' if t else '') + e['error']


def is_state(e):
    return e.get('ev') == 'session.state'


def drain_stderr(proc, sink=None):
    """Reads a server's stderr on a thread, each line to sink(line) when
    given. With XPP_CHECK_STDERR (a directory; tools/asancheck.sh sets it)
    every line also goes to DIR/server-PID.log: gcc's UBSan, built beside
    ASan, ignores log_path and reports only on stderr, which asancheck
    reads back from there."""
    keep = os.environ.get('XPP_CHECK_STDERR')

    def run():
        out = open(os.path.join(keep, 'server-%d.log' % proc.pid), 'a', errors='replace') if keep else None
        for line in proc.stderr:
            if out:
                out.write(line)
                out.flush()
            if sink:
                sink(line)
        if out:
            out.close()

    thread = threading.Thread(target=run, daemon=True)
    thread.start()
    return thread


def series_values(col, enc):
    """a series column's data as Python floats, float32-rounded (NaN for
    null), whatever the encoding"""
    d = col['data']
    if enc == 'f32':
        raw = base64.b64decode(d)
        return list(struct.unpack('<%df' % (len(raw) // 4), raw))
    return [float('nan') if v is None else struct.unpack('<f', struct.pack('<f', v))[0] for v in d]


def decoded_series(ev):
    """a full series event with its columns' data as floats (series_values), no `enc`"""
    h = dict(ev, columns=[dict(c, data=series_values(c, ev.get('enc'))) for c in ev['columns']])
    h.pop('enc', None)
    return h


class SeriesMirror:
    """What a client holds of each plot window's series (docs/protocol.md
    "The plot as data"): the last full `series` and the appends after it. A
    live run ends with an `end` instead of the whole series again; feed()
    gives that event the whole series as `held`, with a full series' fields
    (its columns' data as floats, no `enc`), so a check reads a run's series
    the same way whichever way it came (whole_series)."""

    def __init__(self):
        self.held = {}

    def feed(self, ev):
        if ev.get('ev') != 'plot.series':
            return
        op, win = ev.get('op'), ev.get('win')
        if op is None:
            self.held[win] = decoded_series(ev)
            return
        h = self.held.get(win)
        if h is None:
            return
        if op == 'append':
            have = {c['col']: c for c in h['columns']}
            for c in ev['columns']:
                if c['col'] in have:
                    have[c['col']]['data'] = have[c['col']]['data'][:ev['from']] + series_values(c, ev.get('enc'))
            h['rows'] = ev['rows']
        elif op == 'end' and h['rows'] == ev['rows']:
            h['version'] = ev['version']
            ev['held'] = copy.deepcopy(h)


def whole_series(evs, win=None):
    """the whole series that arrived in evs, in order: each full `series`
    event, and each live run's end as the series the client then holds
    (SeriesMirror), all with their data as floats (decoded_series); only
    window win's when given"""
    return [e['held'] if 'held' in e else decoded_series(e) for e in evs if e.get('ev') == 'plot.series' and e.get('op') in (None, 'end')
            and (win is None or e.get('win') == win)]


def model_companions(source):
    return glob.glob(os.path.join(os.path.dirname(source), '*.anix'))


def staged_model_names(source):
    return sorted([os.path.basename(source), *map(os.path.basename, model_companions(source))])


def copy_model(source, destination):
    """Stage a model and its animation companions together in a check's scratch folder."""
    target = shutil.copy(source, destination)
    for animation in model_companions(source):
        copied = shutil.copy(animation, os.path.dirname(target))
        if os.path.basename(source) != os.path.basename(target):
            # These canonical fixtures name the model being renamed by the check.
            with open(copied, encoding='utf-8') as f: text = f.read()
            text = text.replace('model: "' + os.path.basename(source) + '"', 'model: "' + os.path.basename(target) + '"')
            with open(copied, 'w', encoding='utf-8', newline='') as f: f.write(text)
    return target


class Server:
    def __init__(self, binary, ode, env=None, verbose=False, stdin=subprocess.PIPE, run=None, extra_args=(),
                 parse_float=float, parse_int=int):
        self.verbose = verbose
        self.parse_float = parse_float
        self.parse_int = parse_int
        self.owns_run = run is None
        self.run = run or tempfile.mkdtemp(prefix='xppserver')
        if os.path.abspath(ode) != os.path.abspath(os.path.join(self.run, os.path.basename(ode))):
            copy_model(ode, self.run)
        full = None
        if env is not None:
            full = dict(os.environ)
            full.update(env)
        self.started_at = time.monotonic()  # process launch, after scratch/model staging (comparison time-to-ready)
        self.proc = subprocess.Popen([os.path.abspath(binary), '--server', os.path.basename(ode), *extra_args], cwd=self.run,
                                     stdin=stdin, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                     text=True, encoding='utf-8', bufsize=1, env=full)
        self.events = queue.Queue()
        self.series = SeriesMirror()
        self.sent_at = {}
        self.grabbed_label = self.grabbed_type = None  # the label and type of the point the last AUTO grab took: AUTO runs start from it
        threading.Thread(target=self._read, daemon=True).start()
        self.log = []
        self.log_thread = drain_stderr(self.proc, self.log.append)

    def auto_dirs(self):
        """AUTO's scratch directories of this process (xpp::files::make_temp_dir)"""
        tmp = tempfile.gettempdir() if os.name == 'nt' else (os.environ.get('TMPDIR') or '/tmp')
        pre = 'xppautoX-%d-' % self.proc.pid
        return [os.path.join(tmp, d) for d in os.listdir(tmp) if d.startswith(pre)]

    def _read(self):
        for line in self.proc.stdout:
            try:
                ev = json.loads(line, parse_float=self.parse_float, parse_int=self.parse_int,
                                parse_constant=_reject_non_finite)
            except ValueError:
                ev = {'ev': 'bad', 'line': line[:300]}
            ev['_t'] = time.monotonic()
            result = ev.get('result') if ev.get('ev') == 'done' else None
            if isinstance(result, dict) and result.get('grabbed') and isinstance(result.get('label'), int):
                self.grabbed_label, self.grabbed_type = result['label'], result.get('type')
            self.series.feed(ev)
            self.events.put(ev)
        self.events.put({'ev': 'eof', '_t': time.monotonic()})

    def send(self, **cmd):
        save_permission(cmd)
        if self.verbose:
            print('  >', json.dumps(cmd))
        self.proc.stdin.write(json.dumps(cmd) + '\n')
        self.proc.stdin.flush()
        return time.monotonic()

    def collect(self, until, timeout=10 * SLOW):
        """events up to and including the first for which until(ev) is true;
        (events, None) on timeout or end of output"""
        got = []
        deadline = time.monotonic() + timeout  # safety bound, including a stream that never finishes
        while True:
            try:
                ev = self.events.get(timeout=max(0, deadline - time.monotonic()))
            except queue.Empty:
                return got, None
            if self.verbose:
                s = json.dumps(ev)
                print('  <', s[:160] + ('...' if len(s) > 160 else ''))
            got.append(ev)
            if until(ev):
                return got, ev
            if ev.get('ev') == 'eof':
                return got, None

    def answer_asks(self, until, replies, timeout=20 * SLOW):
        """collect up to until(ev), answering asks whose kind is in replies
        (kind -> function of the ask returning the answer's fields)"""
        got = []
        while True:
            evs, e = self.collect(lambda e: is_ask(e) or until(e), timeout)
            got += evs
            if e is None or not is_ask(e) or e['kind'] not in replies:
                return got, e
            self.send(cmd='answer', id=e['id'], **replies[e['kind']](e))

    def alive(self):
        return self.proc.poll() is None

    def close(self):
        if self.alive():
            try:
                self.proc.stdin.close()
                self.proc.wait(timeout=5 * SLOW)
            except (OSError, subprocess.TimeoutExpired):
                self.proc.kill()
        self.proc.wait(timeout=5 * SLOW)
        self.log_thread.join(timeout=5 * SLOW)
        if self.owns_run:
            shutil.rmtree(self.run, ignore_errors=True)


def open_auto(s):
    """File/AUTO, waiting for its window and the command's completion."""
    s.send(cmd='auto.open')
    events, end = s.collect(is_done)
    if end is None or not any(e.get('ev') == 'window.created' and e.get('win') == 'diagram_1' for e in events):
        raise RuntimeError('File/AUTO did not open its window: ' + str(events))
    return events


def run_any(s, key, timeout=120 * SLOW):
    """Run the registry's named AUTO kind from the point the last grab took (its label, until the next grab),
    answering only a real continuation question; the events to done."""
    kind = {'s':'equilibrium','p':'periodic','e':'periodic','t':'two_parameter','b':'bvp'}[key]
    if key == 'p' and s.grabbed_type == 'fold': kind = 'two_parameter'  # the run a fold point's menu offered
    if key == 'e':  # extend the branch of the label: the kind of the branch it is on, as autoinfo's start says the point allows
        s.send(cmd='events.subscribe', events=['auto.info'])
        shown, _ = s.collect(is_done)
        start = next((e['start'] for e in reversed(shown) if e.get('ev') == 'auto.info' and e.get('start')), None)
        kinds = start['kinds'] if start else []
        kind = 'equilibrium' if 'equilibrium' in kinds else kinds[0] if kinds else kind
    label = s.grabbed_label
    s.send(cmd='auto.run', kind=kind, **({'label': label} if label else {}))
    events,_ = s.answer_asks(is_done, {'choice':lambda e:{'key':'y'}}, timeout=timeout)
    return events


def run_commands(binary, ode, lines, run, flags=()):
    """Drive the protocol, waiting for its ask/done conditions before each line.
    outcome is error accounting for the commands; --server itself exits 0.
    This replaces file scripts in protocol checks, including pixels asks."""
    s = Server(binary, ode, run=run, extra_args=flags)
    events, _ = s.collect(is_done)
    try:
        for command in lines:
            s.send(**command)
            got, end = s.collect(lambda e: is_ask(e) or is_done(e) or e.get('ev') == 'session.bye', timeout=60 * SLOW)
            events += got
            if end is None or end.get('ev') == 'session.bye':
                break
    finally:
        s.close()
    result = subprocess.CompletedProcess([], s.proc.returncode,
        ''.join(json.dumps(e, separators=(',', ':')) + '\n' for e in events), ''.join(s.log))
    result.outcome = int(any(e.get('error') for e in events))
    return result


def silent_rows(binary, model, arguments=(), edit=None, timeout=None):
    """The rows of the output.dat that `xppautX MODEL --silent ARGUMENTS` writes, run on a copy of the model (its text
    passed through edit, if given) in a folder of its own: each row a list of floats."""
    with tempfile.TemporaryDirectory(prefix='xpp-silent-') as folder:
        copy = os.path.join(folder, os.path.basename(model))
        if edit:
            copy_model(model, copy)
            with open(model, encoding='utf-8') as f, open(copy, 'w', encoding='utf-8') as g:
                g.write(edit(f.read()))
        else:
            copy_model(model, copy)
        done = subprocess.run([os.path.abspath(binary), copy, '--silent', *arguments], cwd=folder, capture_output=True,
                              text=True, timeout=timeout)
        output = os.path.join(folder, 'output.dat')
        if done.returncode != 0 or not os.path.exists(output):
            raise ValueError('%s: xppautX --silent exited %d: %s' % (os.path.basename(model), done.returncode, done.stderr[-300:]))
        with open(output) as f:
            return [[float(x) for x in line.split()] for line in f if line.strip()]


def recording_steps(lines):
    """Write the .recx step spec: command, its answers, its abort.
    Bare aborts between commands interrupt nothing and need no replay step."""
    steps = []
    for original in lines:
        c = save_permission(dict(original))
        if 'rgb' in c:
            continue  # pixels come from the interface, never the recording
        if c['cmd'] == 'run.abort':
            if steps and 'at' in c:
                steps[-1]['abort'] = c['at']
        elif c['cmd'] == 'answer' and steps:
            value = {k: v for k, v in c.items() if k not in ('cmd', 'id')}
            steps[-1].setdefault('answers', []).append(None if c.get('ok') == 0 else
                c['value'] if 'value' in c else c['values'] if 'values' in c else value)
        else:
            steps.append({'cmd': c})
    return steps


def file_bytes(s, name):
    """the model folder's file name, through the file command (the player's
    folder is its own)"""
    s.send(cmd='file', op='get', name=name)
    _, ev = s.collect(lambda e: e.get('ev') == 'file.result')
    s.collect(is_done)
    return base64.b64decode(ev['data']) if ev and ev.get('ok') else None


def read_recx(text):
    """a .recx's parts (docs/protocol.md "Recordings"): the header lines, the
    embedded files {name: lines}, the steps [(step, note)], the fingerprint
    written and the one its files and steps give"""
    lines = text.split('\n')
    n = lines.index('')  # the header, in the settings syntax (W285), ends at the first blank line
    header, files, steps, hashed, note = lines[:n], {}, [], [], []
    name, body, in_steps, written = None, None, False, None
    for l in lines[n:]:
        if body is not None:
            hashed.append(l)
            if l == '@end':
                if name is not None:
                    files[name] = body
                body = None
            else:
                body.append(l[1:] if l.startswith('@@') else l)
        elif l.startswith('@file ') or l.startswith('@binary '):
            name, body = l.split(' ', 1)[1], []
            hashed.append(l)
        elif l == '@snapshot':  # the session it began from (W59d): recx_snapshot reads it
            name, body = None, []
            hashed.append(l)
        elif l == '@steps':
            in_steps = True
        elif l.startswith('fingerprint: '):
            written = l[len('fingerprint: '):]
        elif in_steps and l.startswith('#'):
            note.append(l[2:] if l.startswith('# ') else l[1:])
        elif in_steps and l.strip():
            hashed.append(l)
            steps.append((json.loads(l), '\n'.join(note)))
            note = []
    got = hashlib.sha256(''.join(h + '\n' for h in hashed).encode('utf-8')).hexdigest()
    return header, files, steps, written, got


def recording_text(template, steps, files=()):
    """Keep the core-recorded snapshot; append file sections and spec steps.
    The fingerprint covers sections and steps, exactly as docs/protocol.md."""
    prefix = template.split('@steps\n', 1)[0]
    for name, data in files:
        prefix += '@binary ' + name + '\n' + base64.b64encode(data).decode() + '\n@end\n\n'
    section_count = sum(l.startswith(('@file ', '@binary ')) for l in prefix.splitlines())
    body = []
    for step in steps:
        step = dict(step)
        step.setdefault('files', list(range(section_count)))
        body.append(json.dumps(step, ensure_ascii=False))
    raw = prefix + '@steps\n' + '\n'.join(body) + '\n'
    fingerprint = read_recx(raw)[4]
    return raw + 'fingerprint: ' + fingerprint + '\n'


def make_recording(binary, ode, lines, run):
    """The core records the starting snapshot; checks write steps to its spec."""
    s = Server(binary, ode, run=run)
    try:
        s.collect(is_done)
        s.send(cmd='recording.start')
        s.collect(is_done)
        s.send(cmd='recording.stop', file='template.recx')
        s.collect(is_done)
    finally:
        s.close()
    with open(os.path.join(run, 'template.recx'), encoding='utf-8') as f:
        template = f.read()
    files = []
    for name in os.listdir(run):
        if name != os.path.basename(ode) and not name.endswith('.recx') and os.path.isfile(os.path.join(run, name)):
            with open(os.path.join(run, name), 'rb') as f:
                files.append((name, f.read()))
    path = os.path.join(run, 'run.recx')
    steps = recording_steps(lines)
    # An existing movie is an output precondition, separate from file reads.
    steps = [{'cmd': {'cmd': 'file', 'op': 'put', 'name': name,
                     'data': base64.b64encode(data).decode()}}
             for name, data in files if name.endswith('.gif')] + steps
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(recording_text(template, steps, files))
    return path


def replay_recording(binary, ode, path, run, pixel_answers=()):
    """Silent replay supplies the actual exit status and empty stdout check.
    The same player over --server supplies events and copies its scratch
    output files through the existing file API before the process closes."""
    quiet = None if pixel_answers else subprocess.run(
        [os.path.abspath(binary), os.path.abspath(path), '--silent', '--yes'],
        cwd=run, capture_output=True, text=True, encoding='utf-8', timeout=120 * SLOW)
    s = Server(binary, ode, run=run)
    events = []
    try:
        s.collect(is_done)
        s.send(cmd='recording.open', play=False, file=os.path.abspath(path))
        evs, _ = s.answer_asks(is_done, {'choice': lambda e: {'key': 'd'}})
        pl = next((e for e in evs if e.get('ev') == 'recording.player'), None)
        if pl:
            n = len(pl['steps'])
            s.send(cmd='recording.seek', step=n, play=True)
            # Fast-forward shares the player's inputs and stop arming with
            # silent playback, with no display pacing or time-based assertion.
            pending = iter(pixel_answers)
            asked = False
            while True:
                evs, end = s.collect(lambda e: is_ask(e) or 'the player stopped' in e.get('error', '') or (is_state(e) and
                    ((e.get('player') or {}).get('step') == n or
                     ((e.get('player') or {}).get('step', 0) > 0 and
                      not (e.get('player') or {}).get('playing') and
                      (e.get('player') or {}).get('running') == -1))), timeout=120 * SLOW)
                events += evs
                if end and 'the player stopped' in end.get('error', ''):
                    if asked:
                        s.send(cmd='answer', ok=0)
                    events += s.collect(is_done)[0]
                    break
                if end and is_ask(end) and end.get('kind') == 'pixels':
                    s.send(**next(pending))
                    continue
                if end and is_ask(end):
                    asked = True
                    continue  # the player answers its own user prompts
                break
            if end and is_state(end):
                events += s.collect(is_done)[0]
            s.send(cmd='file', op='list')
            got, listing = s.collect(lambda e: e.get('ev') == 'file.result')
            s.collect(is_done)
            for f in (listing or {}).get('files', []):
                if f.get('folder'):
                    continue
                data = file_bytes(s, f['name'])
                if data is not None:
                    with open(os.path.join(run, f['name']), 'wb') as out:
                        out.write(data)

    finally:
        s.close()
    result = subprocess.CompletedProcess([], quiet.returncode if quiet else s.proc.returncode,
        ''.join(json.dumps(e, separators=(',', ':')) + '\n' for e in events),
        quiet.stderr if quiet else ''.join(s.log))
    result.quiet_stdout = quiet.stdout if quiet else ''
    if result.quiet_stdout:
        raise AssertionError('silent recording emitted interface events: ' + quiet.stdout[:200])
    return result
