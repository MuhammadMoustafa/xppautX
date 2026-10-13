#!/usr/bin/env python3
"""W300/#358: write tests/auto/<description>.auto07p and .xppaut, the special points AUTO-07p
and XPPAUT 8.0 find for the problem a tests/auto description runs in xppautX.

This is the maintainer's manual tool (Linux or WSL). Only its output is committed;
tools/autocompare.py compares ours with those files on every push, without either
program. A description whose problem a program cannot run the same way is refused with
the reason (REFUSED), which the description keeps as auto07p_skip / xppaut_skip.

--program auto07p needs AUTO-07p 0.9.3 (never vendored; AUTO_DIR, default ~/auto-07p) and
gfortran. Each model's equations are translated from the .odex (the subset the descriptions
use: par, init, name' = expr, name = expr, fun, boundary, wiener, x[j] arrays) to a Fortran
equation file; the constants come from the description (NTST, NCOL, DS, DSMIN, DSMAX, EPSL,
EPSU, EPSS, NMX, NPR, MXBF as the description gives them, ITMX, ITNW, NWTN, IAD, IID, IADS as
xppautX's run fixes them, core/auto_nox.cpp); the starting point is the first point of our
diagram (the Newton-corrected start); a run from a Hopf point continues from the first HB of
the named run.

--program xppaut needs XPPAUT 8.0 built from reference/xppaut-8.0 (--xppaut), Xvfb and
xdotool. The same model is written as an .ode, XPPAUT's AUTO window is opened on a private
display and driven with the keys a user presses (Parameters and Numerics dialogs, Run, Grab,
Run); the constants it used are read back from the .b file header and must be the run's.
The point table is XPPAUT's own stdout, the spectra its diagnostics file.

    python3 tools/auto_reference.py --program auto07p|xppaut [--only NAME,...] [--keep DIR]
"""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from xppclient import wait_until

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / 'tests/auto'
FORMAT = 1
# AUTO's index of the period among the parameters (PAR(11); core/auto_nox.h AUTO_PERIOD_INDEX is 0-based 10)
PERIOD = 11
# the constants xppautX fixes (core/auto_nox.cpp: advanced numerics and the run's own ISP, ILP, ISW)
FIXED = dict(IAD=3, IID=2, ITMX=8, ITNW=7, NWTN=3, IADS=1, ILP=1, ISW=1, A0=0, A1=1000)
DEFAULTS = dict(ncol=4, epsl=1e-4, epsu=1e-4, epss=1e-4, mxbf=5)
ISP = {'equilibrium': 1, 'periodic': 2, 'bvp': 2}
IPS = {'equilibrium': 1, 'periodic': 2, 'bvp': 4}
INTRINSICS = {'exp', 'sin', 'cos', 'tan', 'tanh', 'cosh', 'sinh', 'sqrt', 'max', 'min', 'abs', 'log', 'atan'}
TOKEN = re.compile(r"\d+\.?\d*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?|[A-Za-z_][A-Za-z_0-9]*'?|\^|[()+\-*/,]")
LINE = 96  # characters of an expression per Fortran line (free form allows 132)


class Refused(Exception):
    """A problem AUTO-07p cannot be given the same way: the reason is the message."""


def tokens(text):
    out = TOKEN.findall(text)
    if ''.join(out) != re.sub(r'\s+', '', text):
        raise Refused('expression not understood: ' + text)
    return out


def split_args(toks, i):
    """toks[i] is '(': the argument token lists up to its ')' and the index after it."""
    depth, args, cur = 0, [], []
    while i < len(toks):
        t = toks[i]
        if t == '(':
            depth += 1
            if depth > 1:
                cur.append(t)
        elif t == ')':
            depth -= 1
            if depth == 0:
                args.append(cur)
                return args, i + 1
            cur.append(t)
        elif t == ',' and depth == 1:
            args.append(cur)
            cur = []
        else:
            cur.append(t)
        i += 1
    raise Refused('unbalanced parentheses')


class Odex:
    """The parts of an .odex the descriptions use."""
    def __init__(self, path):
        self.pars, self.inits, self.odes, self.assigns, self.funs, self.bcs = {}, {}, {}, [], {}, []
        self.discrete = False
        depth = 0
        for raw in path.read_text(encoding='utf-8').splitlines():
            line = raw.split('#')[0].strip()
            if depth:
                depth += line.count('{') - line.count('}')
                self.discrete |= bool(re.search(r'method:\s*discrete', line))
                continue
            if not line or line.startswith('"') or line == 'legacy':
                continue
            if re.match(r'(options|set\s+\w+)\s*\{', line):
                depth = line.count('{') - line.count('}')
                self.discrete |= bool(re.search(r'method:\s*discrete', line))
                continue
            self.line(line)
        if not self.odes:
            raise Refused('no differential equations')
        self.states = list(self.odes)

    def line(self, line):
        m = re.match(r'(.*?)\s+for\s+j\s+in\s+(\d+)\.\.(\d+)$', line)
        if m:
            for k in range(int(m.group(2)), int(m.group(3)) + 1):
                self.line(m.group(1).replace('[j]', '_%d' % k))
            return
        m = re.match(r'wiener\s+(\w+)$', line)
        if m:
            self.assigns.append((m.group(1), ['0']))  # deterministic: the noise term is zero
            return
        m = re.match(r'(par|init)\s+(.*)$', line)
        if m:
            target = self.pars if m.group(1) == 'par' else self.inits
            for item in m.group(2).split(','):
                name, value = item.split('=')
                target[name.strip()] = float(value)
            return
        m = re.match(r'fun\s+(\w+)\s*\(([^)]*)\)\s*=\s*(.*)$', line)
        if m:
            if m.group(1) != 'ode_divisor':
                self.funs[m.group(1)] = ([a.strip() for a in m.group(2).split(',')], tokens(m.group(3)))
            return
        m = re.match(r'boundary\s+(.*)$', line)
        if m:
            self.bcs.append(tokens(m.group(1)))
            return
        m = re.match(r"(\w+)('?)\s*=\s*(.*)$", line)
        if m and not line.startswith('aux '):
            if m.group(2):
                self.odes[m.group(1)] = tokens(m.group(3))
            else:
                self.assigns.append((m.group(1), tokens(m.group(3))))
            return
        if line.startswith('aux ') or re.match(r'(done|table|wiener|markov|number|global|dnet)\b', line):
            return
        raise Refused('model line not understood: ' + line)


class Equations:
    """Fortran for one model; pars are the slots (PAR(k)) the translation uses."""
    def __init__(self, odex, parameter_values):
        self.o = odex
        names = list(odex.pars)
        self.slot = {}
        k = 0
        for n in names:
            k += 1
            if k == PERIOD:
                k += 1
            self.slot[n] = k
        self.values = {**odex.pars, **parameter_values}
        for n in self.values:
            if n not in self.slot:
                raise Refused('description parameter %s is not a model parameter' % n)
        self.npar = max(self.slot.values())
        self.locals = [n for n, _ in odex.assigns]
        clash = set(self.slot) | set(odex.states) | set(self.locals)
        if len(clash) != len(self.slot) + len(odex.states) + len(self.locals):
            raise Refused('a name is used twice (parameter, variable, formula)')
        if len({n.lower() for n in clash}) != len(clash):
            raise Refused('names differing only by case')

    def expand(self, toks):
        out, i = [], 0
        while i < len(toks):
            t = toks[i]
            if t == 'ode_divisor' and i + 1 < len(toks) and toks[i + 1] == '(':
                args, i = split_args(toks, i + 1)
                out += ['('] + self.expand(args[0]) + [')']
            elif t in self.o.funs and i + 1 < len(toks) and toks[i + 1] == '(':
                params, body = self.o.funs[t]
                args, i = split_args(toks, i + 1)
                if len(args) != len(params):
                    raise Refused('call of %s with %d arguments' % (t, len(args)))
                args = [self.expand(a) for a in args]
                sub = []
                for b in body:
                    sub += ['('] + args[params.index(b)] + [')'] if b in params else [b]
                out += ['('] + self.expand(sub) + [')']
            else:
                out.append(t)
                i += 1
        return out

    def fortran(self, toks, boundary=False):
        toks = self.expand(toks)
        out = []
        for j, t in enumerate(toks):
            if t == '^':
                out.append('**')
            elif re.fullmatch(r'\d+\.?\d*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?', t):
                if j and toks[j - 1] == '^' and re.fullmatch(r'\d+', t):
                    out.append(t)
                else:
                    out.append(re.sub(r'[eE]', 'd', t) if re.search('[eE]', t) else (t if '.' in t else t + '.') + 'd0')
            elif t == 'pi':
                out.append('3.141592653589793d0')
            elif t in self.slot and not (t in self.o.states):
                out.append('PAR(%d)' % self.slot[t])
            elif boundary and t.rstrip("'") in self.o.states:
                out.append('%s(%d)' % ('UR' if t.endswith("'") else 'UL', self.o.states.index(t.rstrip("'")) + 1))
            elif t in self.o.states:
                out.append('U(%d)' % (self.o.states.index(t) + 1))
            elif t in self.locals:
                out.append('a_' + t)
            elif t.lower() in INTRINSICS or t in '()+-*/,':
                out.append(t.upper() if t.isalpha() else t)
            else:
                raise Refused('unknown name in the equations: ' + t)
        return out

    @staticmethod
    def wrap(head, toks):
        lines, cur = [], head
        for t in toks:
            if len(cur) + len(t) > LINE:
                lines.append(cur + ' &')
                cur = '      '
            cur += t
        return '\n'.join(lines + [cur])

    def source(self, start, bvp):
        o, n = self.o, len(self.o.states)
        f = ['SUBROUTINE FUNC(NDIM,U,ICP,PAR,IJAC,F,DFDU,DFDP)', '  IMPLICIT NONE',
             '  INTEGER, INTENT(IN) :: NDIM, IJAC, ICP(*)',
             '  DOUBLE PRECISION, INTENT(IN) :: U(NDIM), PAR(*)',
             '  DOUBLE PRECISION, INTENT(OUT) :: F(NDIM)',
             '  DOUBLE PRECISION, INTENT(INOUT) :: DFDU(NDIM,*), DFDP(NDIM,*)']
        if self.locals:
            f.append('  DOUBLE PRECISION ' + ', '.join('a_' + a for a in self.locals))
        for name, toks in o.assigns:
            f.append(self.wrap('  a_%s = ' % name, self.fortran(toks)))
        for i, s in enumerate(o.states):
            f.append(self.wrap('  F(%d) = ' % (i + 1), self.fortran(o.odes[s])))
        f += ['END SUBROUTINE FUNC', '', 'SUBROUTINE STPNT(NDIM,U,PAR,T)', '  IMPLICIT NONE',
              '  INTEGER, INTENT(IN) :: NDIM', '  DOUBLE PRECISION, INTENT(INOUT) :: U(NDIM), PAR(*)',
              '  DOUBLE PRECISION, INTENT(IN) :: T']
        for name, k in self.slot.items():
            f.append('  PAR(%d) = %s' % (k, repr(float(self.values[name])).replace('e', 'd') + ('d0' if 'e' not in repr(float(self.values[name])) else '')))
        for i, v in enumerate(start):
            f.append('  U(%d) = %s' % (i + 1, repr(float(v)).replace('e', 'd') + ('d0' if 'e' not in repr(float(v)) else '')))
        f += ['END SUBROUTINE STPNT', '']
        if bvp:
            f += ['SUBROUTINE BCND(NDIM,PAR,ICP,NBC,UL,UR,FB,IJAC,DBC)', '  IMPLICIT NONE',
                  '  INTEGER, INTENT(IN) :: NDIM, ICP(*), NBC, IJAC',
                  '  DOUBLE PRECISION, INTENT(IN) :: PAR(*), UL(NDIM), UR(NDIM)',
                  '  DOUBLE PRECISION, INTENT(OUT) :: FB(NBC)', '  DOUBLE PRECISION, INTENT(INOUT) :: DBC(NBC,*)']
            for i, b in enumerate(o.bcs):
                f.append(self.wrap('  FB(%d) = ' % (i + 1), self.fortran(b, boundary=True)))
            f += ['END SUBROUTINE BCND']
        else:
            f += ['SUBROUTINE BCND', 'END SUBROUTINE BCND']
        f += ['SUBROUTINE ICND', 'END SUBROUTINE ICND', 'SUBROUTINE FOPT', 'END SUBROUTINE FOPT',
              'SUBROUTINE PVLS', 'END SUBROUTINE PVLS', '']
        return '\n'.join(f)


def constants(r, ndim, kind, discrete, nbc, icp, irs, npar):
    a = {**DEFAULTS, **r['auto']}
    ips = -1 if discrete else IPS[kind]
    lines = ['NDIM=%d, IPS=%d, IRS=%d, ILP=%d' % (ndim, ips, irs, FIXED['ILP']),
             'ICP=%s' % icp,
             'NTST=%d, NCOL=%d, IAD=%d, ISP=%d, ISW=%d, IPLT=0, NBC=%d, NINT=0' %
             (a['ntst'], a['ncol'], FIXED['IAD'], ISP[kind] if not discrete else 1, FIXED['ISW'], nbc),
             'NMX=%d, NPR=%d, MXBF=%d, IID=%d, ITMX=%d, ITNW=%d, NWTN=%d, JAC=0' %
             (a['nmax'], a['npr'], a['mxbf'], FIXED['IID'], FIXED['ITMX'], FIXED['ITNW'], FIXED['NWTN']),
             'EPSL=%r, EPSU=%r, EPSS=%r' % (a['epsl'], a['epsu'], a['epss']),
             'DS=%r, DSMIN=%r, DSMAX=%r, IADS=%d' % (a['ds'], a['dsmin'], a['dsmax'], FIXED['IADS']),
             'RL0=%r, RL1=%r, A0=%r, A1=%r' % (a['parmin'], a['parmax'], FIXED['A0'], FIXED['A1']),
             'NPAR=%d, THL={%d:0.0}, THU={}' % (npar, PERIOD)]
    return '\n'.join(lines) + '\n'


def first_point(name):
    with open(BASE / (name + '.diagram.csv'), newline='') as f:
        row = next(csv.DictReader(f))
    return [float(row[k]) for k in row if k.startswith('u0.')]


BLOCK = re.compile(r'^\s*(\d+)\s+(\d+)\s+(Eigenvalue|Multiplier)\s*(\d+):?\s+(\S+)\s+(\S+)(\s|$)')


def stability(text):
    """d.<name>: {(branch, point): (kind, [[re, im], ...])}, the last block printed for a point (the
    location of a special point prints the point again after each of its iterations). A block
    starts when the number of its line is not above the one before: AUTO-07p numbers eigenvalues
    and multipliers from 1, XPPAUT's multipliers from 0."""
    out, last = {}, {}
    for line in text.splitlines():
        m = BLOCK.match(line)
        if m:
            key, number = (int(m.group(1)), int(m.group(2))), int(m.group(4))
            if key not in out or number <= last[key]:
                out[key] = (m.group(3), [])
            last[key] = number
            out[key][1].append([float(m.group(5)), float(m.group(6))])
    return out


def script(runs, eq, names):
    """The .auto script: runs in order, every labelled solution printed as a JSON line."""
    lines = ['import json', 'results = {}', 'def period(s):', '    try:', "        return s['PAR(%d)']" % PERIOD,
             '    except KeyError:', '        return None  # a boundary value problem has no period']
    for r, c in zip(runs, names):
        start = r['start']
        if start.get('state') == 'current':
            src = "run(e='eq', c='%s')" % c
        else:
            src = "run(results[%r]('%s%d'), c='%s')" % (start['run'], start['type'], start['index'], c)
        lines += ['print("RUN %s")' % r['name'], 'try:', '    results[%r] = %s' % (r['name'], src),
                  "    save(results[%r], %r)" % (r['name'], c),
                  "    for s in results[%r]():" % r['name'],
                  "        print('POINT ' + json.dumps({'run': %r, 'type': s['TY name'], 'branch': s['BR'], 'label': s['LAB'],"
                  " 'point': s['PT'], 'par': s['PAR(%d)'], 'norm': s['L2-NORM'], 'period': period(s)}))"
                  % (r['name'], eq['icp1'][r['name']]),
                  'except Exception as e:', "    print('FAILED %s ' + str(e).replace(chr(10), ' '))" % r['name']]
    return '\n'.join(lines) + '\n'


POINT_TYPES = ('EP', 'MX', 'BP', 'LP', 'HB', 'PD', 'TR')
SOURCES = {'auto07p': 'AUTO-07p 0.9.3', 'xppaut': 'XPPAUT 8.0 (reference/xppaut-8.0, c021b511)'}
SUFFIX = {'auto07p': '.auto07p', 'xppaut': '.xppaut'}


def check_description(name, d, program):
    """The part of the problem both programs need (the Odex, the equations, the start); refused
    with its reason when the program cannot run it the same way."""
    if 'skip' in d:
        raise Refused('our description skips it: ' + d['skip'])
    odex = Odex(ROOT / d['model'])
    if odex.discrete and len(d['runs']) != 1:
        raise Refused('discrete model with several runs')
    eqs = Equations(odex, d.get('parameters', {}))
    bvp = any(r['kind'] == 'bvp' for r in d['runs'])
    if bvp and len(d['runs']) != 1:
        raise Refused('BVP with several runs')
    kinds = {r['kind'] for r in d['runs']}
    if kinds - set(IPS):
        raise Refused('run kind %s needs constants not mapped here' % sorted(kinds - set(IPS)))
    start = first_point(name)
    if bvp:
        if program == 'xppaut':
            raise Refused('XPPAUT\'s boundary value run starts from the integrated solution in its '
                          'storage, which the keys of this driver do not produce')
        if any(start):
            raise Refused('BVP start is a nonzero integrated profile')
        if len(odex.bcs) != len(odex.states):
            raise Refused('boundary conditions %d for %d variables' % (len(odex.bcs), len(odex.states)))
    return odex, eqs, start, bvp


def execute_auto07p(d, odex, eqs, start, bvp, auto_dir, keep, name):
    """{run: [point]}, {run: {(branch, point): spectrum}}, {run: reason} from AUTO-07p."""
    work = Path(tempfile.mkdtemp(prefix='w300-a07-'))
    try:
        (work / 'eq.f90').write_text(eqs.source(start, bvp), encoding='utf-8')
        names, icp1 = [], {}
        for i, r in enumerate(d['runs']):
            c = 'r%d' % i
            names.append(c)
            p = eqs.slot[r['parameter']]
            icp1[r['name']] = p
            icp = [p] if r['kind'] in ('equilibrium', 'bvp') else [p, PERIOD]
            (work / ('c.' + c)).write_text(constants(r, len(odex.states), r['kind'], odex.discrete,
                                                     len(odex.bcs) if bvp else 0, icp, 0, eqs.npar))
        (work / 'run.auto').write_text(script(d['runs'], {'icp1': icp1}, names))
        env = {**os.environ, 'AUTO_DIR': auto_dir, 'PYTHONWARNINGS': 'ignore'}
        env['PATH'] = '%s/cmds:%s/bin:%s' % (auto_dir, auto_dir, env['PATH'])
        p = subprocess.run(['auto', 'run.auto'], cwd=work, env=env, capture_output=True, text=True)
        if p.returncode or 'POINT ' not in p.stdout:
            raise Refused('AUTO-07p failed: ' + (p.stderr or p.stdout)[-600:])
        if keep:
            shutil.copytree(work, keep / name, dirs_exist_ok=True)
        stab = {r['name']: (stability((work / ('d.' + c)).read_text(errors='replace'))
                            if (work / ('d.' + c)).exists() else {}) for c, r in zip(names, d['runs'])}
        failed = {l.split()[1]: l.split(None, 2)[2] for l in p.stdout.splitlines() if l.startswith('FAILED ')}
        points = {r['name']: [] for r in d['runs']}
        for line in p.stdout.splitlines():
            if line.startswith('POINT '):
                pt = json.loads(line[6:])
                points[pt.pop('run')].append(pt)
        return points, stab, failed
    finally:
        shutil.rmtree(work, ignore_errors=True)


# --- XPPAUT 8.0: its AUTO window on a private display, driven by the keys a user presses -------------
DISPLAY = os.environ.get('W300_DISPLAY', ':93')
MENU_MS = '300'  # ms xdotool waits between keys: XPPAUT opens a pop-up menu per key and reads the next key from it
TYPE_DELAY = '15'  # ms between the characters typed into a dialog
BACKSPACES = 40  # more than any field holds
RUN_LIMIT = 600  # s: a run still going after this has hung (a safety limit, not a result)
ROW = re.compile(r'^\s*(-?\d+)\s+(-?\d+)\s+(?:([A-Z]{2})\s+)?(\d+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)(.*)$')


def xdo(*args):
    return subprocess.run(['xdotool', *args], env={**os.environ, 'DISPLAY': DISPLAY},
                          capture_output=True, text=True).stdout.strip()


def wait_for(what, found, timeout=RUN_LIMIT):
    """What `found()` returns once it is true (xppclient's wait_until polls)."""
    got = []
    if not wait_until(lambda: got.append(found()) or got[-1], timeout):
        raise Refused('XPPAUT: no %s within %d s' % (what, timeout))
    return got[-1]


def window(title):
    return wait_for('window ' + title, lambda: (xdo('search', '--onlyvisible', '--name', title).split() or [None])[-1], 30)


def keys(win, *ks):
    """XPPAUT reads the key under the pointer (no window manager): move there, then press."""
    xdo('mousemove', '--window', win, '20', '20')
    xdo('key', '--window', win, '--delay', MENU_MS, *ks)


def fill_dialog(opener, title, values):
    """Open a dialog with the key `opener` in the AUTO window, type every field, finish it (Return
    goes to the next field, Tab finishes)."""
    keys(opener, title[0])
    win = window(title[1])
    xdo('mousemove', '--window', win, '20', '20')
    for i, v in enumerate(values):
        xdo('key', '--window', win, '--repeat', str(BACKSPACES), '--delay', '5', 'BackSpace')
        xdo('type', '--window', win, '--delay', TYPE_DELAY, '--', str(v))  # '--': a value may start with '-'
        xdo('key', '--window', win, 'Tab' if i == len(values) - 1 else 'Return')
    wait_for('the dialog to close', lambda: not xdo('search', '--onlyvisible', '--name', title[1]).split(), 30)


def solution_parameters(text):
    """{label: [PAR(1), ...]} from AUTO's solution file (fort.8, XPPAUT's .s): a header of twelve
    integers (IBR NTOT ITP LAB NFPR ISW NTPL NAR NROWPR NTST NCOL NPARX), NROWPR rows, the last of them
    the parameters, seven to a row."""
    rows, i, out = text.splitlines(), 0, {}
    while i < len(rows) and rows[i].strip():
        head = [int(x) for x in rows[i].split()]
        label, nrowpr, nparx = head[3], head[8], head[11]
        block = rows[i + 1:i + 1 + nrowpr]
        last = block[len(block) - (-(-nparx // 7)):]  # ceil(nparx / 7) rows
        out[label] = [float(x) for row in last for x in row.split()][:nparx]
        i += 1 + nrowpr
    return out


def ode_text(eqs, start, discrete):
    """XPPAUT's .ode for the model, with every fun and ode_divisor expanded."""
    o = eqs.o
    lines = ['par %s=%r' % (n, float(eqs.values[n])) for n in o.pars]
    lines += ['init %s=%r' % (s, float(v)) for s, v in zip(o.states, start)]
    lines += ['%s=%s' % (n, ' '.join(eqs.expand(t))) for n, t in o.assigns]
    lines += ["%s'=%s" % (s, ' '.join(eqs.expand(o.odes[s]))) for s in o.states]
    if discrete:
        lines.append('@ meth=discrete')
    return '\n'.join(lines + ['done', ''])


def numerics(r):
    a = {**DEFAULTS, **r['auto']}
    return [a['ntst'], a['nmax'], a['npr'], a['ds'], a['dsmin'], a['ncol'], a['epsl'], a['dsmax'], a['parmin'],
            a['parmax'], FIXED['A0'], FIXED['A1'], a['epsu'], a['epss'], FIXED['IAD'], a['mxbf'], FIXED['IID'],
            FIXED['ITMX'], FIXED['ITNW'], FIXED['NWTN'], FIXED['IADS'], 0]


def constants_used(text):
    """The run's constants as the .b file header prints them (the first line is RL0 RL1 A0 A1)."""
    head = text.splitlines()[:9]
    used = dict(zip(('rl0', 'rl1', 'a0', 'a1'), (float(x) for x in head[0].split()[1:5])))
    used.update((k.lower(), float(v)) for k, v in re.findall(r'([A-Z]+)\s*=\s*([-+0-9.eE]+)', '\n'.join(head[1:])))
    return used


def execute_xppaut(d, odex, eqs, start, binary, keep, name):
    work = Path(tempfile.mkdtemp(prefix='w300-xpp-'))
    xvfb = xpp = None
    try:
        model = work / 'model.ode'
        eqs.source(start, False)  # every name in the expressions is a parameter, variable, formula or intrinsic
        model.write_text(ode_text(eqs, start, odex.discrete), encoding='utf-8')
        xvfb = subprocess.Popen(['Xvfb', DISPLAY, '-screen', '0', '1600x1200x24', '-nolisten', 'tcp'],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        wait_for('Xvfb', lambda: subprocess.run(['xdpyinfo', '-display', DISPLAY], capture_output=True).returncode == 0, 30)
        log = work / 'xppaut.log'
        xpp = subprocess.Popen([os.path.abspath(binary), model.name], cwd=work, stdout=open(log, 'w'), stderr=subprocess.STDOUT,
                               env={**os.environ, 'DISPLAY': DISPLAY, 'HOME': str(work), 'LC_ALL': 'C'})
        keys(window('XPP'), 'f', 'a')
        auto = window('AUTO')
        points, stab, failed = {}, {}, {}
        npar = min(8, len(odex.pars))
        for r in d['runs']:
            pars = [r['parameter']] + [n for n in odex.pars if n != r['parameter']]
            fill_dialog(auto, ('p', 'Parameters'), pars[:npar])
            values = numerics(r)
            fill_dialog(auto, ('n', 'AutoNum'), values)
            start_run = r['start']
            if start_run.get('state') != 'current':
                keys(auto, 'g', *(['Up', {'HB': 'h', 'LP': 'l', 'BP': 'b', 'PD': 'p', 'TR': 't'}[start_run['type']]] * start_run['index']), 'Return')
            before = len(log.read_text().splitlines())
            diag = work / 'model.ode.d'
            stamp = diag.stat().st_mtime_ns if diag.exists() else 0
            keys(auto, 'r', {'equilibrium': 's', 'periodic': 'p'}[r['kind']])

            def finished():
                rows = [ROW.match(l) for l in log.read_text().splitlines()[before:]]
                rows = [m for m in rows if m]
                return rows if any(m.group(3) in ('EP', 'MX') for m in rows) else None
            rows = wait_for('the end of run ' + r['name'], finished)
            wait_for('the diagnostics of run ' + r['name'], lambda: diag.exists() and diag.stat().st_mtime_ns != stamp)
            sizes = []  # the file is complete once it stops growing

            def complete():
                sizes.append(diag.stat().st_size)
                return len(sizes) > 1 and sizes[-1] == sizes[-2]
            wait_for('the diagnostics of run %s to stop growing' % r['name'], complete)
            used = constants_used((work / 'model.ode.b').read_text(errors='replace'))
            want = dict(zip(('ntst', 'nmx', 'npr', 'ds', 'dsmin', 'ncol', 'epsl', 'dsmax', 'rl0', 'rl1', 'a0', 'a1', 'epsu', 'epss',
                             'iad', 'mxbf', 'iid', 'itmx', 'itnw', 'nwtn'), values))
            bad = {k: (used.get(k), v) for k, v in want.items()
                   if not math.isclose(used.get(k, math.nan), float(v), rel_tol=1e-3, abs_tol=1e-12)}
            if bad:
                raise Refused('the Numerics dialog did not set %s (XPPAUT used, wanted)' % bad)
            stab[r['name']] = stability(diag.read_text(errors='replace'))
            pars = solution_parameters((work / 'model.ode.s').read_text(errors='replace'))
            points[r['name']] = []
            for m in rows:
                if m.group(3) in POINT_TYPES:
                    # the period (PAR(11)) means something at a Hopf point and on a periodic branch
                    period = pars[int(m.group(4))][PERIOD - 1] if m.group(3) == 'HB' or r['kind'] == 'periodic' else None
                    points[r['name']].append(dict(type=m.group(3), branch=int(m.group(1)), label=int(m.group(4)),
                                                  point=int(m.group(2)), par=float(m.group(5)), norm=float(m.group(6)), period=period))
        if keep:
            shutil.copytree(work, keep / name, dirs_exist_ok=True)
        return points, stab, failed
    finally:
        for p in (xpp, xvfb):
            if p is not None and p.poll() is None:
                p.kill()
                p.wait()
        shutil.rmtree(work, ignore_errors=True)


def reference(name, d, program, args):
    odex, eqs, start, bvp = check_description(name, d, program)
    if program == 'auto07p':
        points, stab, failed = execute_auto07p(d, odex, eqs, start, bvp, args.auto_dir, args.keep, name)
    else:
        points, stab, failed = execute_xppaut(d, odex, eqs, start, args.xppaut, args.keep, name)
    runs = []
    for r in d['runs']:
        kept = []
        for pt in points.get(r['name'], []):
            if pt['type'] in POINT_TYPES:
                kind, vals = stab[r['name']].get((abs(pt['branch']), abs(pt['point'])), ('', []))
                kept.append({**pt, 'spectrum': kind.lower(), 'values': vals})
        runs.append({'name': r['name'], 'parameter': r['parameter'], 'points': kept,
                     **({'unavailable': failed[r['name']]} if r['name'] in failed else {})})
    return {'format': FORMAT, 'source': SOURCES[program] + ' (tools/auto_reference.py)', 'model': d['model'], 'runs': runs}


def write(path, ref):
    lines = ['{', '  "format": %d,' % ref['format'], '  "source": %s,' % json.dumps(ref['source']),
             '  "model": %s,' % json.dumps(ref['model']), '  "runs": [']
    for i, r in enumerate(ref['runs']):
        lines += ['    {"name": %s, "parameter": %s,%s "points": [' % (
            json.dumps(r['name']), json.dumps(r['parameter']),
            (' "unavailable": %s,' % json.dumps(r['unavailable'])) if 'unavailable' in r else '')]
        lines += ['      ' + json.dumps(p) + (',' if j + 1 < len(r['points']) else '') for j, p in enumerate(r['points'])]
        lines.append('    ]}' + (',' if i + 1 < len(ref['runs']) else ''))
    lines += ['  ]', '}', '']
    path.write_text('\n'.join(lines), encoding='utf-8')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--program', choices=sorted(SUFFIX), required=True)
    ap.add_argument('--only', help='comma-separated description names (without .json)')
    ap.add_argument('--keep', type=Path, help='keep the program\'s files here')
    ap.add_argument('--auto-dir', default=os.environ.get('AUTO_DIR', str(Path.home() / 'auto-07p')))
    ap.add_argument('--xppaut', default=str(Path.home() / 'w300xpp/xppaut'), help='XPPAUT 8.0 built from reference/xppaut-8.0')
    args = ap.parse_args()
    names = args.only.split(',') if args.only else [p.stem for p in sorted(BASE.glob('*.json'))]
    for name in names:
        d = json.loads((BASE / (name + '.json')).read_text(encoding='utf-8'))
        try:
            write(BASE / (name + SUFFIX[args.program]), reference(name, d, args.program, args))
            print('WROTE', name)
        except Refused as e:
            print('REFUSED %s: %s' % (name, e))
    return 0


if __name__ == '__main__':
    sys.exit(main())
