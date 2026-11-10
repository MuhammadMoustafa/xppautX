"""xppautX from Python (W306, #365): open a model or an .autox script, set values, integrate, run autoX and read the
diagram and the data as numpy arrays, over the same `--server` protocol every front end uses (docs/protocol.md).
Grown from xppclient.py, whose Server does the process and the events; this is the part a script or a notebook uses.

    from xppautx import Session
    with Session('examples/tstauto/ab.odex', binary='./xppautX') as s:
        s.set_parameters(p1=14)
        s.autox_axes('p0')
        s.autox_run('equilibrium', replace_diagram=True, step_size=0.02, parameter_max=1.5, follow_all_branches=False)
        d = s.diagram()
        d['param1'], d['type'], d['stability']

    with Session('analysis.autox', binary='./xppautX') as s:   # the model it names, its runs listed, nothing computed
        s.script()['runs']
        s.run_script()                                          # every run, or run_script('cycles')

A command that fails raises XppError, which keeps the error as the core reports it (its file, line and source line).
Every method is one registry command (docs/protocol.md), so what a script does is what the window's menus do.
"""
import csv, io, os, uuid
import numpy as np
from xppclient import Server, is_done, is_ask, placed

# the safety bound on one command, seconds (a long run is the user's own: this only fails a hang)
COMMAND_TIMEOUT = 3600


class XppError(RuntimeError):
    """A command the program refused or could not finish: str() is the error as the core renders it
    (file:line:col: what); `.error` the `done` event's own fields (error, file, line, col, source)."""

    def __init__(self, text, error=None):
        super().__init__(text)
        self.error = error or {}


def table(text):
    """CSV text (a header line, then rows) as a dict of numpy arrays: a column of numbers is float, any other text"""
    rows = list(csv.reader(io.StringIO(text)))
    names, body = rows[0], rows[1:]
    out = {}
    for i, name in enumerate(names):
        column = [r[i] for r in body]
        try:
            out[name] = np.array([float(v) for v in column])
        except ValueError:
            out[name] = np.array(column, dtype=str)
    return out


class Session:
    """One xppautX --server process on a model (or an .autox script, which names its model)."""

    def __init__(self, model, binary='./xppautX', timeout=COMMAND_TIMEOUT, extra_args=()):
        self.timeout = timeout
        # a model runs in a scratch copy; an .autox script in its own folder, beside the model it names, as the window opens it
        in_place = model.endswith('.autox')
        self.server = Server(binary, model, extra_args=extra_args, follow_one_branch=False,
                             run=os.path.dirname(os.path.abspath(model)) if in_place else None)
        self.model = os.path.basename(model)
        # the load's own events, up to its first completed command
        self.hello, end = self.server.collect(is_done, timeout=timeout)
        failed = [e for e in self.hello if e.get('ev') in ('session.error', 'error', 'eof')]
        if end is None or failed:
            text = '; '.join(placed(e) or e.get('ev', '') for e in failed) or 'xppautX did not start'
            self.server.close()
            raise XppError(text, failed[0] if failed else {})

    def __enter__(self):
        return self

    def __exit__(self, *exception):
        self.close()

    def close(self):
        self.server.close()

    def command(self, cmd, **arguments):
        """One registry command, waited for: its result (a dict). The events it sent are `.events`."""
        self.server.send(cmd=cmd, **arguments)
        events, end = self.server.collect(lambda e: is_done(e) or is_ask(e), timeout=self.timeout)
        self.events = events
        if end is None:
            raise XppError('%s: no completion (the process ended or the safety timeout passed)' % cmd)
        if is_ask(end):
            raise XppError('%s: the program asked a question (%s) that a script cannot answer' % (cmd, end.get('kind')), end)
        if end.get('status') != 'ok':
            raise XppError(placed(end) or '%s failed' % cmd, end)
        return end.get('result', {})

    # values
    def set_parameters(self, **values):
        return self.command('value.set_parameters', values=values)['updated']

    def set_initial(self, **values):
        return self.command('initial_conditions.set', values=values)['applied']

    # integration
    def integrate(self, **settings):
        """The integration from the initial conditions (run.configure's `settings` first, such as total_time and dt),
        its data table as a dict of numpy arrays by column name."""
        if settings:
            self.command('run.configure', **settings)
        self.command('run.initial')
        return self.data()

    def data(self):
        """The data table as the NPZ export writes it (docs/protocol.md): a dict of numpy arrays, `T.npy`'s as 'T'"""
        with self._exported('data.export', '.npz', format='npz', target='table') as path:
            with np.load(path) as z:
                return {name: z[name] for name in z.files}

    # autoX
    def autox_axes(self, *parameters, **arguments):
        """The one or two parameters the diagram continues in (and any other autox.configure_axes argument)"""
        names = dict(zip(('parameter1', 'parameter2'), parameters))
        if len(parameters) == 2:
            arguments.setdefault('plot_type', 'two_parameter')
        return self.command('autox.configure_axes', **names, **arguments)

    def autox_run(self, kind, **arguments):
        """One autoX run (autox.run): its kind, its start (label=, or type= with index=, or the current state, settled first
        by decimals=, hold=, maximum=) and its settings for this run only, step_size's sign the direction. Opens the
        autoX window when it is not open. The result: points_computed and branches_tracked."""
        self._open_autox()
        return self.command('autox.run', kind=kind, **arguments)

    def run_script(self, run=None):
        """The open .autox script's runs in order, or the one named (autox.run_script)"""
        self._open_autox()
        return self.command('autox.run_script', **({'run': run} if run is not None else {}))

    def script(self):
        """The open script: file, model, runs (names in order) and completed (the ones that have run)"""
        return self.command('autox.script')

    def diagram(self):
        """The diagram's points as a dict of numpy arrays (branch, point, type, label, stability, the parameters' values, the
        solution's extremes, ...), in the order they were stored: what autox.export_csv writes"""
        return self._export_diagram()[0]

    def eigenvalues(self):
        """The eigenvalues or multipliers of each stored point: branch, point, index, re, im, kind"""
        return self._export_diagram()[1]

    def _open_autox(self):
        if not self.command('autox.open')['open']:
            raise XppError('autoX could not be opened for this model')

    def _export_diagram(self):
        with self._exported('autox.export_csv', '.csv') as path:
            with open(path, encoding='utf-8', newline='') as f, open(path[:-4] + '_eig.csv', encoding='utf-8', newline='') as g:
                return table(f.read()), table(g.read())

    def _exported(self, cmd, extension, **arguments):
        """A file the command writes in the session's folder, under a name of its own, removed after"""
        return _Exported(self, cmd, extension, arguments)


class _Exported:
    def __init__(self, session, cmd, extension, arguments):
        self.session, self.cmd, self.arguments = session, cmd, arguments
        self.name = 'xppautx_%s%s' % (uuid.uuid4().hex[:12], extension)

    def __enter__(self):
        self.session.command(self.cmd, file=self.name, **self.arguments)
        self.path = os.path.join(self.session.server.run, self.name)
        return self.path

    def __exit__(self, *exception):
        for path in (self.path, self.path[:-4] + '_eig.csv'):
            try:
                os.remove(path)
            except OSError:
                pass
