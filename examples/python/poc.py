"""A proof of concept: drive xppautX from Python through its `--server` protocol (docs/protocol.md), with the standard
library only (python/xppautx). The model is a published XPPAUT demo of examples/ode, the Morris-Lecar model by default.

    uv run --locked python examples/python/poc.py [--bin ./xppautX] [MODEL]

Every step is one registry command, the one the window's menu runs; a command that fails raises XppError, which this
script prints with its place (file, line) before it exits 1. The program runs in a temporary copy of the model's folder.
"""
import argparse, glob, os, shutil, sys, tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'python'))
from xppautx import Client, XppError

parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
parser.add_argument('model', nargs='?', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'ode', 'lecar.odex'))
parser.add_argument('--bin', help='the xppautX program (else the XPPAUTX environment variable)')
args = parser.parse_args()

folder = tempfile.mkdtemp(prefix='xppautx-poc-')  # the program writes beside the model: keep the original untouched
try:
    shutil.copy(args.model, folder)
    for companion in glob.glob(os.path.join(os.path.dirname(args.model), '*.anix')):
        shutil.copy(companion, folder)
    with Client(os.path.join(folder, os.path.basename(args.model)), binary=args.bin) as x:
        x.start_events()
        print('1. load: %s' % os.path.basename(args.model))

        state = x.command('session.get_state')
        parameters, states = dict(state['pars']), [name for name, _ in state['ics']]
        print('2. parameters: %s' % ', '.join('%s=%g' % tuple(p) for p in state['pars']))
        print('   variables:  %s' % ', '.join(states))

        # the parameter and the variable this script moves: the first of each
        parameter, variable = state['pars'][0][0], states[0]
        print('3. set %s=%g and the initial %s=%g' % (parameter, 0.1, variable, -0.2))
        x.command('value.set_parameters', values={parameter: 0.1})
        x.command('initial_conditions.set', values={variable: -0.2})

        def maximum():
            """a run from the initial data, and the largest value of `variable` in its rows (data.page: the table as lists)"""
            run = x.command('run.initial')
            rows = x.command('data.page', from_row=0, count=2000, columns=len(states))['rows']
            return run, rows, max(row[1 + states.index(variable)] for row in rows)

        run, rows, top = maximum()
        print('4. integrate: %d steps to t=%g' % (run['steps'], run['time']))
        print('5. rows: %d x %d (T, %s); first %s, last %s' % (len(rows), len(rows[0]), ', '.join(states), rows[0], rows[-1]))

        print('6. sweep %s, the maximum of %s over the run (the same server, one run per value):' % (parameter, variable))
        for value in (0.0, 0.05, 0.1, 0.15, 0.2):
            x.command('value.set_parameters', values={parameter: value})
            print('   %s=%-5g max %s = %.6g' % (parameter, value, variable, maximum()[2]))

        x.command('value.set_parameters', values={parameter: parameters[parameter]})
        found = x.command('equilibrium.find')
        print('7. equilibrium at %s=%g: %s, %s' % (parameter, parameters[parameter],
              ', '.join('%s=%.6g' % item for item in found['equilibrium'].items()), found['stability']))
        print('   eigenvalues: %s' % ', '.join('%.4g%+.4gi' % (e['real'], e['imaginary']) for e in found['eigenvalues']))

        saved = x.command('session.save_as', file='poc.snapx')
        print('8. saved the session as %s (%d bytes)' % (saved['file'], os.path.getsize(os.path.join(folder, saved['file']))))
    print('9. quit: the program ended with status %d' % x.proc.returncode)
except XppError as error:
    print('error: %s' % error, file=sys.stderr)
    sys.exit(1)
finally:
    shutil.rmtree(folder, ignore_errors=True)
