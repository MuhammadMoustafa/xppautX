"""A client of `xppautX --server` (docs/protocol.md), Python standard library only.

    from xppautx import Client
    with Client('examples/ode/lecar.odex', binary='./xppautX') as x:
        x.command('value.set_parameters', values={'iapp': 80})
        x.command('run.initial')
        rows = x.command('data.page', from_row=0, count=5)['rows']

The program starts on a model, one JSON line goes in per command, and its events come out; a command is finished at
its `done`, never at a pause. The binary is the `binary` argument, else the XPPAUTX environment variable: nothing
searches the disk for it. A command the program refuses raises XppError, which keeps the error's fields (what, file,
line, col, source: docs/protocol.md "Errors"). An `ask` (a question the program puts) is answered or refused as the caller
says, never silently.
"""
import glob, json, os, queue, shutil, subprocess, tempfile, threading, time

# the safety bound on one command, seconds: it only fails a hang, never decides a result
COMMAND_TIMEOUT = 3600
# how long the process may take to end after session.quit, seconds
QUIT_TIMEOUT = 10
BINARY_VARIABLE = 'XPPAUTX'


class XppError(RuntimeError):
    """What the program reported as an error: str() is the error as the core renders it (`file:line:col: what`,
    xpp::Error::text); `.error` is the event's own fields (error, file, line, col, source, field)."""

    def __init__(self, text, error=None):
        super().__init__(text)
        self.error = error or {}


def placed(e):
    """An error event's text as the core renders an error (docs/protocol.md "Errors"): "file:line:col: error",
    leaving out what is not known; "" for an event with no error."""
    if not e.get('error'):
        return ''
    t, line, col = e.get('file') or '', e.get('line') or 0, e.get('col') or 0
    if line > 0:
        t += (':' if t else 'line ') + str(line)
        if col > 0:
            t += ':' + str(col)
    return (t + ': ' if t else '') + e['error']


def is_done(e):
    return e.get('ev') == 'done'


def is_ask(e):
    return e.get('ev') == 'ask'


def model_companions(source):
    return glob.glob(os.path.join(os.path.dirname(source), '*.anix'))


def copy_model(source, destination):
    """Stage a model and its animation companions together in a folder; the model's path there."""
    target = shutil.copy(source, destination)
    for animation in model_companions(source):
        copied = shutil.copy(animation, os.path.dirname(target))
        if os.path.basename(source) != os.path.basename(target):
            # these canonical fixtures name the model being renamed
            with open(copied, encoding='utf-8') as f: text = f.read()
            text = text.replace('model: "' + os.path.basename(source) + '"', 'model: "' + os.path.basename(target) + '"')
            with open(copied, 'w', encoding='utf-8', newline='') as f: f.write(text)
    return target


def _reject_non_finite(text):
    """json.loads' parse_constant: NaN/Infinity are not valid JSON (core/json_number.h); a strict parse fails on them
    instead of silently making a float."""
    raise ValueError('non-finite JSON constant: %s' % text)


def drain_stderr(proc, sink, then=None):
    """Reads the process's stderr on a thread, each line to sink(line) (the log must never fill the pipe), then()
    at its end."""
    def run():
        for line in proc.stderr:
            sink(line)
        if then:
            then()
    thread = threading.Thread(target=run, daemon=True)
    thread.start()
    return thread


class Client:
    """One `xppautX --server` process on a model: it runs in `cwd` (default the model's folder), where the program reads
    and writes its files, and is stopped by close() (or leaving a `with`). With scratch=True the model and its
    animation files are copied into `cwd` (a new temporary folder when none is given, removed at close), so what the
    program writes (exports, saves) never lands beside the user's model."""

    def __init__(self, model, binary=None, cwd=None, env=None, extra_args=(), scratch=False, parse_float=float, parse_int=int, drain=drain_stderr):
        binary = binary or os.environ.get(BINARY_VARIABLE)
        if not binary:
            raise XppError('no xppautX program: give binary= or set %s' % BINARY_VARIABLE)
        model = os.fspath(model)
        self.owns_cwd = scratch and cwd is None
        self.cwd = cwd or (tempfile.mkdtemp(prefix='xppserver') if scratch else os.path.dirname(os.path.abspath(model)))
        if scratch and os.path.abspath(model) != os.path.abspath(os.path.join(self.cwd, os.path.basename(model))):
            copy_model(model, self.cwd)
        self.parse_float, self.parse_int = parse_float, parse_int
        self.events = queue.Queue()
        self.log = []  # the process's stderr lines
        full = {**os.environ, **env} if env is not None else None
        self.started_at = time.monotonic()
        # argv is a list, never a shell string; the model is passed as a name in the folder the process runs in
        self.proc = subprocess.Popen([os.path.abspath(binary), '--server', os.path.basename(model), *extra_args], cwd=self.cwd,
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                     text=True, encoding='utf-8', bufsize=1, env=full)
        threading.Thread(target=self._read, daemon=True).start()
        self.log_thread = drain(self.proc, self.log.append)

    def __enter__(self):
        return self

    def __exit__(self, *exception):
        self.close()

    def observe(self, ev):
        """Called for each event, on the reader thread, before it is queued: for a subclass that keeps what it sees."""

    def _read(self):
        for line in self.proc.stdout:
            try:
                ev = json.loads(line, parse_float=self.parse_float, parse_int=self.parse_int, parse_constant=_reject_non_finite)
            except ValueError:
                ev = {'ev': 'bad', 'line': line[:300]}
            ev['_t'] = time.monotonic()
            self.observe(ev)
            self.events.put(ev)
        self.events.put({'ev': 'eof', '_t': time.monotonic()})

    def send(self, **cmd):
        """Sends one command (its fields as arguments) as one JSON line; the time it was sent."""
        self.proc.stdin.write(json.dumps(cmd) + '\n')
        self.proc.stdin.flush()
        return time.monotonic()

    def collect(self, until, timeout=COMMAND_TIMEOUT):
        """The events up to and including the first for which until(ev) is true: (events, that event); (events, None) on
        the timeout (a safety bound, not a speed) or at the end of the output."""
        got = []
        deadline = time.monotonic() + timeout
        while True:
            try:
                ev = self.events.get(timeout=max(0, deadline - time.monotonic()))
            except queue.Empty:
                return got, None
            got.append(ev)
            if until(ev):
                return got, ev
            if ev.get('ev') == 'eof':
                return got, None

    def start_events(self, timeout=COMMAND_TIMEOUT):
        """The load's own events, up to its first `done`. A model that does not load raises XppError with its place."""
        events, end = self.collect(is_done, timeout)
        failed = [e for e in events if e.get('ev') in ('session.error', 'error', 'eof')]
        if end is None or failed:
            errors = [e for e in failed if e.get('ev') != 'eof'] or failed  # the end of the output says nothing the error has not
            text = '; '.join(placed(e) or e.get('ev', '') for e in errors) or 'xppautX did not start'
            self.close()
            raise XppError(text, failed[0] if failed else {})
        return events

    def command(self, cmd, answers=None, timeout=COMMAND_TIMEOUT, **arguments):
        """One command, waited for until its `done`: its result (a dict); every event it sent is `.events_of_last`.
        answers maps an ask's kind to a function of the ask returning the answer's fields (docs/protocol.md "Asks");
        an ask not in it is cancelled and raises XppError (the caller did not say how to answer), and so does a command that fails."""
        self.send(cmd=cmd, **arguments)
        events = []
        while True:
            got, end = self.collect(lambda e: is_done(e) or is_ask(e), timeout)
            events += got
            if end is None:
                raise XppError('%s: no completion (the process ended or the safety timeout passed)' % cmd)
            if not is_ask(end):
                break
            reply = (answers or {}).get(end.get('kind'))
            if reply is None:  # refused: the question is cancelled (ok 0), so the program is not left waiting on it
                self.send(cmd='answer', id=end['id'], ok=0)
                events += self.collect(is_done, timeout)[0]
                raise XppError('%s: the program asked a question (%s) that was not answered; it was cancelled' % (cmd, end.get('kind')), end)
            self.send(cmd='answer', id=end['id'], **reply(end))
        self.events_of_last = events
        if end.get('status') != 'ok':
            raise XppError(placed(end) or '%s failed' % cmd, end)
        return end.get('result', {})

    def alive(self):
        return self.proc.poll() is None

    def quit(self):
        """Quits without saving (session.quit, save false) and waits for the process to end; True when it ended itself."""
        if not self.alive():
            return True
        try:
            self.send(cmd='session.quit', save=False)
            self.proc.wait(timeout=QUIT_TIMEOUT)
            return True
        except (OSError, subprocess.TimeoutExpired):
            return False

    def close(self):
        """Stops the process: asked to quit first, killed only when it does not end."""
        if not self.quit():
            self.proc.kill()
        self.proc.wait()
        self.log_thread.join(timeout=QUIT_TIMEOUT)
        for pipe in (self.proc.stdin, self.proc.stdout, self.proc.stderr):
            pipe.close()
        if self.owns_cwd:
            shutil.rmtree(self.cwd, ignore_errors=True)
