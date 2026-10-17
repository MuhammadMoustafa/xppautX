#!/usr/bin/env python3
"""W300/#358: write tests/auto/<description>.auto07p and .xppaut, the special points AUTO-07p
and XPPAUT 8.0 find for the problem a tests/auto description runs in xppautX.

This is the maintainer's manual tool (Linux or WSL). Only its output is committed;
tools/autocompare.py compares ours with those files on every push, without either
program. A description whose problem a program cannot run the same way is refused with
the reason (REFUSED), which the description keeps as auto07p_skip / xppaut_skip.

--program auto07p needs AUTO-07p 0.9.4 (never vendored; AUTO_DIR, default ~/auto-07p-0.9.4) and
gfortran. Each model's equations are translated from the .odex (the subset the descriptions
use: par, init, name' = expr, name = expr, fun, boundary, wiener, x[j] arrays) to a Fortran
equation file; the constants come from the description (NTST, NCOL, DS, DSMIN, DSMAX, EPSL,
EPSU, EPSS, NMX, NPR, MXBF as the description gives them, ITMX, ITNW, NWTN, IAD, IID, IADS as
xppautX's run fixes them, core/auto_nox.cpp); the starting point is the first point of our
diagram (the Newton-corrected start); a run from a Hopf point continues from the first HB of
the named run. A two_parameter run continues an LP or HB of an equilibrium run with ISW=2 (ICP the
two parameters; ILP=0 as xppautX, ISP=1 so that AUTO-07p labels BT, CP, ZH, GH) and records, besides
the labelled points, every step of the curve from AUTO-07p's b file: [parameter, second parameter, norm].
W402 (#464): a two_parameter run also continues an LP, PD or TR of a periodic run (IPS=2, ICP the two
parameters and the period, ISW=2, ISP=2: AUTO-07p labels R1 on a fold or torus curve, R2 on a PD or torus
curve, R3 and R4 on a torus curve), or of a map's fixed points (IPS=-1, ISP=1: also LPD, LTR, PTR, TTR, which
AUTO-07p detects for maps only, src/support.f90 CHECKSP and src/maps.f90). AUTO-07p first writes the curve's
starting data (an EP) from a PD, TR or LP of cycles, and the curve is the run restarted from that. A
run with `switch` is a branch switch (ISW=-1).

--program xppaut needs XPPAUT 8.0 built from reference/xppaut-8.0 (--xppaut), Xvfb and
xdotool. The same model is written as an .ode, XPPAUT's AUTO window is opened on a private
display and driven with the keys a user presses (Parameters and Numerics dialogs, Run, Grab,
Run); the constants it used are read back from the .b file header and must be the run's.
The point table is XPPAUT's own stdout, the spectra its diagnostics file. Two-parameter
curves use the Parameters/Axes dialogs and the grabbed point's Run menu; the shared
b-file reader records every step, with XPPAUT's zero-based parameter column indices.

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

from autocompare import CODIM_TWO, HOMCONT, SPECIAL_TYPES
from odex_model import Odex, Refused, INTRINSICS
from xppclient import wait_until

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / 'tests/auto'
FORMAT = 1
# AUTO's index of the period among the parameters (PAR(11); core/auto_nox.h AUTO_PERIOD_INDEX is 0-based 10)
PERIOD = 11
# AUTO's reserved PAR(12): torus angle or fold-of-cycles normalization (autlib1.c init).
CYCLE_EXTRA = 12


def fnum(v):
    """A Fortran double literal."""
    t = repr(float(v)).replace('e', 'd')
    return t if 'd' in t else t + 'd0'
# the constants xppautX fixes (core/auto_nox.cpp: advanced numerics and the run's own ISP, ILP, ISW)
FIXED = dict(IAD=3, IID=2, ITMX=8, ITNW=7, NWTN=3, IADS=1, ILP=1, ISW=1, A0=0, A1=1000)
DEFAULTS = dict(ncol=4, epsl=1e-4, epsu=1e-4, epss=1e-4, mxbf=5)
ISP = {'equilibrium': 1, 'periodic': 2, 'bvp': 2, 'homoclinic': 0, 'heteroclinic': 0}
IPS = {'equilibrium': 1, 'periodic': 2, 'bvp': 4, 'homoclinic': 9, 'heteroclinic': 9}
# PAR(11) is HomCont's truncated time interval, PAR(12...) the equilibrium (equilibria)
HOM_EQUILIBRIUM = 12
# A two-parameter curve of an LP or HB of equilibria is the equilibrium problem with ISW=2. xppautX runs it
# with ILP=0 and ISP=0 and detects no codim-2 point yet (core/auto_nox.cpp auto_2p_limit); the reference
# keeps ILP=0 and the equilibria's ISP=1, which makes AUTO-07p label BT, CP, ZH and GH: those labels are what
# W363 and W364 are judged against.
CURVE_ISW = 2
# A curve of cycles (an LP, PD or TR of a periodic run) is IPS=2 with ISW=2 and the cycle's detection
# (ISP=2: R1-R4 on the fold, PD and torus curves); ICP the two parameters and the period.
CYCLE_ISP = 2
# an .odex comparison in Fortran (the condition of an `if C then A else B`)
RELATIONS = {'<': '.LT.', '<=': '.LE.', '>': '.GT.', '>=': '.GE.', '==': '.EQ.', '!=': '.NE.'}
LINE = 96  #characters of an expression per Fortran line (free form allows 132)


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
        return self.o.expand(toks, unguarded=('ode_divisor',))

    def ordered_assigns(self):
        """The model's quantities, each after the quantities it reads: XPPAUT reads a constant ahead of its
        definition (ModelDB 267710's conn.inc conductances, used by the compartments before it), and a Fortran
        local read before it is set is garbage."""
        by_name, order, state = dict(self.o.assigns), [], {}

        def visit(name):
            if state.get(name) == 'done':
                return
            if state.get(name) == 'active':
                raise Refused('quantity %s depends on itself' % name)
            state[name] = 'active'
            for t in self.expand(by_name[name]):
                if t in by_name:
                    visit(t)
            state[name] = 'done'
            order.append((name, by_name[name]))

        for name in by_name:
            visit(name)
        return order

    @classmethod
    def conditionals(cls, toks, xppaut=False):
        """The tokens with each `if C then A else B` (C a comparison) as Fortran's MERGE(A, B, C), or with
        `xppaut` as XPPAUT's if(C)then(A)else(B). MERGE evaluates both branches: one that overflows or divides
        by zero where the other is taken is not trapped."""
        out, i = [], 0
        while i < len(toks):
            if toks[i] != 'if':
                out.append(toks[i])
                i += 1
                continue
            parts, k, depth, nested = [[], [], []], 0, 0, 0
            i += 1
            while i < len(toks):
                t = toks[i]
                if depth == 0 and t == 'then' and not nested:
                    k = 1
                elif depth == 0 and t == 'else' and not nested:
                    k = 2
                elif depth == 0 and t == ')':
                    break
                else:
                    depth += (t == '(') - (t == ')')
                    nested += (depth == 0 and t == 'if') - (depth == 0 and t == 'else' and nested > 0)
                    parts[k].append(t)
                i += 1
            if k != 2 or not any(t in RELATIONS for t in parts[0]):
                raise Refused('only `if COMPARISON then A else B` is translated')
            then, other, cond = (cls.conditionals(p, xppaut) for p in (parts[1], parts[2], parts[0]))
            out += (["if", "("] + cond + [")", "then", "("] + then + [")", "else", "("] + other + [")"] if xppaut
                    else ["MERGE", "("] + then + [","] + other + [","] + cond + [")"])
        return out

    def fortran(self, toks, boundary=False):
        toks = self.conditionals(self.expand(toks))
        out = []
        for j, t in enumerate(toks):
            if t == '^':
                out.append('**')
            elif t in RELATIONS:
                out.append(RELATIONS[t])
            elif t == 'MERGE':
                out.append(t)
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

    def orbit(self, hom):
        """STPNT's lines that set HomCont's parameters and call ORBIT, and ORBIT: RK4 from the initial state over T*interval,
        with the description's settle step, the step xppautX integrates its settle with: the run's first point is this
        starting data, which neither program corrects, and an orbit near a saddle magnifies a change of integrator."""
        body = ['  PAR(11) = ' + fnum(hom['interval'])]
        body += ['  PAR(%d) = %s' % (HOM_EQUILIBRIUM + i, fnum(v)) for i, v in enumerate(hom['equilibria'])]
        body.append('  CALL ORBIT(NDIM, PAR, T*PAR(11), U)')
        f = ['SUBROUTINE ORBIT(NDIM,PAR,TAU,U)', '  IMPLICIT NONE', '  INTEGER, INTENT(IN) :: NDIM',
              '  DOUBLE PRECISION, INTENT(IN) :: PAR(*), TAU', '  DOUBLE PRECISION, INTENT(OUT) :: U(NDIM)',
              '  DOUBLE PRECISION :: K1(NDIM), K2(NDIM), K3(NDIM), K4(NDIM), W(NDIM), DFDU(NDIM,NDIM), DFDP(NDIM,50), H',
              '  INTEGER :: I, STEPS, ICP(1)', '  ICP(1) = 1']
        f += ['  U(%d) = %s' % (i + 1, fnum(v)) for i, v in enumerate(hom['ic'])]
        f += ['  STEPS = MAX(1, NINT(TAU/%s))' % fnum(hom['step']), '  H = TAU/STEPS', '  DO I = 1, STEPS',
              '    CALL FUNC(NDIM,U,ICP,PAR,0,K1,DFDU,DFDP)', '    W = U + 0.5d0*H*K1',
              '    CALL FUNC(NDIM,W,ICP,PAR,0,K2,DFDU,DFDP)', '    W = U + 0.5d0*H*K2',
              '    CALL FUNC(NDIM,W,ICP,PAR,0,K3,DFDU,DFDP)', '    W = U + H*K3',
              '    CALL FUNC(NDIM,W,ICP,PAR,0,K4,DFDU,DFDP)', '    U = U + H*(K1 + 2d0*K2 + 2d0*K3 + K4)/6d0', '  END DO']
        return body, f + ['END SUBROUTINE ORBIT', '']

    @staticmethod
    def wrap(head, toks):
        lines, cur = [], head
        for t in toks:
            if len(cur) + len(t) > LINE:
                lines.append(cur + ' &')
                cur = '      '
            cur += t
        return '\n'.join(lines + [cur])

    def source(self, start, bvp, hom=None):
        """`hom`: the HomCont run's {'interval', 'equilibria', 'ic'}: STPNT integrates the orbit from ic over the interval."""
        o, n = self.o, len(self.o.states)
        f = ['SUBROUTINE FUNC(NDIM,U,ICP,PAR,IJAC,F,DFDU,DFDP)', '  IMPLICIT NONE',
             '  INTEGER, INTENT(IN) :: NDIM, IJAC, ICP(*)',
             '  DOUBLE PRECISION, INTENT(IN) :: U(NDIM), PAR(*)',
             '  DOUBLE PRECISION, INTENT(OUT) :: F(NDIM)',
             '  DOUBLE PRECISION, INTENT(INOUT) :: DFDU(NDIM,*), DFDP(NDIM,*)']
        if self.locals:
            f.append('  DOUBLE PRECISION ' + ', '.join('a_' + a for a in self.locals))
        for name, toks in self.ordered_assigns():
            f.append(self.wrap('  a_%s = ' % name, self.fortran(toks)))
        for i, s in enumerate(o.states):
            f.append(self.wrap('  F(%d) = ' % (i + 1), self.fortran(o.odes[s])))
        f += ['END SUBROUTINE FUNC', '', 'SUBROUTINE STPNT(NDIM,U,PAR,T)', '  IMPLICIT NONE',
              '  INTEGER, INTENT(IN) :: NDIM', '  DOUBLE PRECISION, INTENT(INOUT) :: U(NDIM), PAR(*)',
              '  DOUBLE PRECISION, INTENT(IN) :: T']
        for name, k in self.slot.items():
            f.append('  PAR(%d) = %s' % (k, repr(float(self.values[name])).replace('e', 'd') + ('d0' if 'e' not in repr(float(self.values[name])) else '')))
        if hom:
            body, routine = self.orbit(hom)
            f += body + ['END SUBROUTINE STPNT', ''] + routine
        else:
            f += ['  U(%d) = %s' % (i + 1, fnum(v)) for i, v in enumerate(start)] + ['END SUBROUTINE STPNT']
        f.append('') if not hom else ['END SUBROUTINE STPNT_END', '']
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


def problem(r, discrete, cycle=False):
    """The run's AUTO problem: ips, isp, isw, ilp (XPPAUT overrides curve ISP). `cycle`: a two-parameter curve of a periodic run's points."""
    curve = r['kind'] == 'two_parameter'
    kind = 'periodic' if cycle else 'equilibrium' if curve else r['kind']
    return dict(ips=-1 if discrete else IPS[kind], isp=1 if discrete else CYCLE_ISP if cycle else ISP[kind],
                isw=CURVE_ISW if curve else -1 if r.get('switch') else FIXED['ISW'], ilp=0 if curve else FIXED['ILP'])


def constants(r, ndim, pr, nbc, icp, irs, npar):
    a = {**DEFAULTS, **r['auto']}
    lines = ['NDIM=%d, IPS=%d, IRS=%d, ILP=%d' % (ndim, pr['ips'], irs, pr['ilp']),
             'ICP=%s' % icp,
             'NTST=%d, NCOL=%d, IAD=%d, ISP=%d, ISW=%d, IPLT=0, NBC=%d, NINT=0' %
             (a['ntst'], a['ncol'], FIXED['IAD'], pr['isp'], pr['isw'], nbc),
             'NMX=%d, NPR=%d, MXBF=%d, IID=%d, ITMX=%d, ITNW=%d, NWTN=%d, JAC=0' %
             (a['nmax'], a['npr'], a['mxbf'], FIXED['IID'], FIXED['ITMX'], FIXED['ITNW'], FIXED['NWTN']),
             'EPSL=%r, EPSU=%r, EPSS=%r' % (a['epsl'], a['epsu'], a['epss']),
             'DS=%r, DSMIN=%r, DSMAX=%r, IADS=%d' % (a['ds'], a['dsmin'], a['dsmax'], FIXED['IADS']),
             'RL0=%r, RL1=%r, A0=%r, A1=%r' % (a['parmin'], a['parmax'], FIXED['A0'], FIXED['A1']),
             'NPAR=%d, THL={%d:0.0}, THU={}' % (npar, PERIOD)]
    if 'connection' in r:  # HomCont: the equilibrium (equilibria) is solved for, IEQUIB=1 (homoclinic) or -2
        c = r['connection']
        lines.append('NUNSTAB=%d, NSTAB=%d, IEQUIB=%d, ITWIST=0, ISTART=5' % (c['nunstab'], c['nstab'], 1 if r['kind'] == 'homoclinic' else -2))
    return '\n'.join(lines) + '\n'


def first_point(name):
    with open(BASE / (name + '.diagram.csv'), newline='') as f:
        # the file starts with one `branch,...` line per branch (core/xpp_session_auto.h), then the points
        row = next(csv.DictReader(line for line in f if not line.startswith('branch,')))
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


def curve_points(text, first, second):
    """[[PAR(first), PAR(second), L2-NORM]] of every step of a two-parameter run, from its bifurcation
    diagram file (b.*: a header line naming the columns, then one line per step, whatever NPR is)."""
    lines = text.splitlines()
    head = next(i for i, l in enumerate(lines) if 'L2-NORM' in l and 'PAR(' in l)
    # the header starts with a 0 where the lines have their branch number
    # a periodic solution's file names a column "MAX U(1)": one column, two words
    columns = {name: i + 1 for i, name in enumerate(re.sub(r'(MAX|MIN) ', r'\1', lines[head]).split()[1:])}
    wanted = [columns['PAR(%d)' % first], columns['PAR(%d)' % second], columns['L2-NORM']]
    rows = []
    for line in lines[head + 1:]:
        fields = line.split()
        if fields and fields[0] == '0':  # XPPAUT prepends runs; the next header starts the earlier run.
            break
        if len(fields) == len(columns) + 1:
            rows.append([float(fields[k]) for k in wanted])
    return rows


def script(runs, eq, names):
    """The .auto script: runs in order, every labelled solution printed as a JSON line."""
    lines = ['import json', 'results = {}']
    for r, c in zip(runs, names):
        start = r['start']
        if start.get('state') == 'current':
            src = "run(e='eq', c='%s')" % c
        else:
            at = "results[%r]('%s%d')" % (start['run'], start['type'], start['index'])
            if eq['discrete'] and start['type'] == 'HB':
                # xppautX labels a map's torus bifurcations HB (and finds no flip bifurcation of a map)
                at = "results[%r]('TR%d')" % (start['run'], start['index'])
            src = "run(%s, c='%s')" % (at, c)
            if r['name'] in eq['cycles']:
                # the starting data (an EP) of the curve; the curve is the run restarted from it, in the direction
                # AUTO-07p takes for the description's DS (not necessarily the way xppautX's curve goes)
                src = "run(run(%s, c='%s'))" % (at, c)
        second = ", 'par2': s['PAR(%d)']" % eq['icp2'][r['name']] if r['name'] in eq['icp2'] else ''
        # a boundary value problem has no period (its PAR(11) is unused); every other run's solutions carry PAR(11)
        period = 'None' if r['kind'] == 'bvp' else "s['PAR(%d)']" % PERIOD
        lines += ['print("RUN %s")' % r['name'], 'try:', '    results[%r] = %s' % (r['name'], src),
                  "    save(results[%r], %r)" % (r['name'], c),
                  "    for s in results[%r]():" % r['name'],
                  "        print('POINT ' + json.dumps({'run': %r, 'type': s['TY name'], 'branch': s['BR'], 'label': s['LAB'],"
                  " 'point': s['PT'], 'par': s['PAR(%d)']%s, 'norm': s['L2-NORM'], 'period': %s}))"
                  % (r['name'], eq['icp1'][r['name']], second, period),
                  'except Exception as e:', "    print('FAILED %s ' + str(e).replace(chr(10), ' '))" % r['name']]
    return '\n'.join(lines) + '\n'


# the points a two-parameter curve can start from, by what the run it starts from continues
CURVE_START = {'equilibrium': ('LP', 'HB'), 'map': ('LP', 'HB', 'PD', 'TR'), 'periodic': ('LP', 'PD', 'TR')}
POINT_TYPES = SPECIAL_TYPES + CODIM_TWO
SOURCES = {'auto07p': 'AUTO-07p 0.9.4', 'xppaut': 'XPPAUT 8.0 (reference/xppaut-8.0, c021b511)'}
SUFFIX = {'auto07p': '.auto07p', 'xppaut': '.xppaut'}


def check_description(name, d, program):
    """The part of the problem both programs need (the Odex, the equations, the start); refused
    with its reason when the program cannot run it the same way."""
    if 'skip' in d:
        raise Refused('our description skips it: ' + d['skip'])
    odex = Odex(ROOT / d['model'])
    if odex.events or odex.networks:
        raise Refused('the model has events or networks, which the translation does not carry')
    if program == 'xppaut' and odex.discrete and any(r['kind'] != 'two_parameter' for r in d['runs'][1:]):
        raise Refused('XPPAUT multi-run maps: this driver only continues two-parameter curves of the initial fixed-point run')
    eqs = Equations(odex, d.get('parameters', {}))
    bvp = any(r['kind'] == 'bvp' for r in d['runs'])
    if bvp and len(d['runs']) != 1:
        raise Refused('BVP with several runs')
    kinds = {r['kind'] for r in d['runs']}
    if kinds - set(IPS) - {'two_parameter'}:
        raise Refused('run kind %s needs constants not mapped here' % sorted(kinds - set(IPS) - {'two_parameter'}))
    kind_of = {r['name']: r['kind'] for r in d['runs']}
    homs = [r for r in d['runs'] if r['kind'] in HOMCONT]
    if homs and (program == 'xppaut' or len(homs) != 1 or homs[0]['start'].get('state') != 'current' or 'step' not in homs[0]['start']):
        raise Refused('HomCont: only AUTO-07p, one run, from the integrated orbit (start current, settle, step)')
    for r in d['runs']:
        if r['kind'] == 'two_parameter':
            base = kind_of.get(r['start'].get('run'))
            base = 'map' if base == 'equilibrium' and odex.discrete else base
            if program == 'xppaut' and base == 'map' and r['start']['type'] == 'LP':
                raise Refused('XPPAUT 8.0 auto_2p_limit forces IPS=1 for a map LP (auto_nox.c:2498-2533)')
            if r['start'].get('type') not in CURVE_START.get(base, ()):
                raise Refused('two-parameter run %s: only a point of %s of %s run is continued' %
                              (r['name'], ' or '.join(CURVE_START.get(base, ('nothing',))), base))
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
        hom = None
        if any(r['kind'] in HOMCONT for r in d['runs']):
            run, = (r for r in d['runs'] if r['kind'] in HOMCONT)
            known = {**odex.inits, **{k.lower(): v for k, v in d.get('initial_conditions', {}).items()}}
            hom = dict(interval=run['start']['settle'], step=run['start']['step'], ic=[known.get(x, 0.0) for x in odex.states],
                       equilibria=run['connection']['left'] + (run['connection']['right'] if run['kind'] == 'heteroclinic' else []))
        (work / 'eq.f90').write_text(eqs.source(start, bvp, hom), encoding='utf-8')
        names, icp1, icp2, cycles = [], {}, {}, set()
        kind_of = {r['name']: r['kind'] for r in d['runs']}
        for i, r in enumerate(d['runs']):
            c = 'r%d' % i
            names.append(c)
            cycle = r['kind'] == 'two_parameter' and kind_of[r['start']['run']] == 'periodic'
            if cycle:
                cycles.add(r['name'])
            pr = problem(r, odex.discrete, cycle)
            if r['kind'] == 'two_parameter' or r['kind'] in HOMCONT:
                icp1[r['name']], icp2[r['name']] = (eqs.slot[n] for n in r['parameters'])
                icp = [icp1[r['name']], icp2[r['name']]] + ([PERIOD] if cycle else [])
            else:
                p = icp1[r['name']] = eqs.slot[r['parameter']]
                icp = [p] if r['kind'] in ('equilibrium', 'bvp') else [p, PERIOD]
            (work / ('c.' + c)).write_text(constants(r, len(odex.states), pr, len(odex.bcs) if bvp else 0,
                                                     icp, 0, max(eqs.npar, HOM_EQUILIBRIUM - 1 + (2 if r['kind'] == 'heteroclinic' else 1) * len(odex.states))))
        env = {**os.environ, 'AUTO_DIR': auto_dir, 'PYTHONWARNINGS': 'ignore'}
        env['PATH'] = '%s/cmds:%s/bin:%s' % (auto_dir, auto_dir, env['PATH'])
        (work / 'run.auto').write_text(script(d['runs'], {'icp1': icp1, 'icp2': icp2, 'cycles': cycles, 'discrete': odex.discrete}, names))
        p = subprocess.run(['auto', 'run.auto'], cwd=work, env=env, capture_output=True, text=True)
        if p.returncode or 'POINT ' not in p.stdout:
            raise Refused('AUTO-07p failed: ' + (p.stderr or p.stdout)[-600:])
        curves = {r['name']: curve_points((work / ('b.' + c)).read_text(errors='replace'), icp1[r['name']], icp2[r['name']])
                  for c, r in zip(names, d['runs']) if r['name'] in icp2 and (work / ('b.' + c)).exists()}
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
        return points, stab, failed, curves
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
    keys(opener, *title[0])
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
    lines += ['%s=%s' % (n, ' '.join(eqs.conditionals(eqs.expand(t), xppaut=True))) for n, t in eqs.ordered_assigns()]
    lines += ["%s'=%s" % (s, ' '.join(eqs.conditionals(eqs.expand(o.odes[s]), xppaut=True))) for s in o.states]
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
    (ROOT / 'build').mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='w428-xpp-', dir=ROOT / 'build'))
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
        points, stab, failed, curves = {}, {}, {}, {}
        curve_driver = any(r['kind'] == 'two_parameter' for r in d['runs'])
        # Keep AUTO slots stable: saved solutions use these positions, not model parameter indices.
        selected = list(dict.fromkeys(n for r in d['runs'] for n in r.get('parameters', [r.get('parameter')]) if n))
        selected += [n for n in odex.pars if n not in selected]
        labels, seeds = [], {}
        npar = min(8, len(odex.pars))
        for r in d['runs']:
            curve = r['kind'] == 'two_parameter'
            base_kind = next(v['kind'] for v in d['runs'] if v['name'] == r['start'].get('run')) if curve else r['kind']
            cycle_curve = curve and base_kind == 'periodic'
            seed_key = (r['start']['run'], r['start']['type'], r['start']['index'], tuple(r['parameters'])) if cycle_curve else None
            seed = seeds.get(seed_key)
            preparing = cycle_curve and seed is None
            pars = selected if curve_driver else [r['parameter']] + [n for n in odex.pars if n != r['parameter']]
            fill_dialog(auto, ('p', 'Parameters'), pars[:npar])
            values = numerics(r)
            fill_dialog(auto, ('n', 'AutoNum'), values)
            start_run = r['start']
            if start_run.get('state') != 'current':
                if curve_driver:
                    labelled = [pt for pt in points[start_run['run']] if pt['type'] == start_run['type']]
                    if len(labelled) < start_run['index']:
                        raise Refused('XPPAUT 8.0 run %s has no %s %d for %s' %
                                      (start_run['run'], start_run['type'], start_run['index'], r['name']))
                    point = labelled[start_run['index'] - 1]
                    label = seed if seed else point['label']
                    # Tab visits labelled points in diagram order (auto_x11.c traverse_diagram).
                    keys(auto, 'g', 'Home', *(['Tab'] * labels.index(label)), 'Return')
                else:
                    keys(auto, 'g', *(['Up', {'HB': 'h', 'LP': 'l', 'BP': 'b', 'PD': 'p', 'TR': 't'}[start_run['type']]] * start_run['index']), 'Return')
            if curve_driver:
                axes = r['parameters'] if curve else [r['parameter'], next(n for n in pars if n != r['parameter'])]
                a = r['auto']
                fill_dialog(auto, (('a', 't' if curve else 'h'), 'AutoPlot'),
                            [odex.states[0], *axes, a['parmin'], FIXED['A0'], a['parmax'], FIXED['A1']])
            before = len(log.read_text().splitlines())
            diag = work / 'model.ode.d'
            stamp = diag.stat().st_mtime_ns if diag.exists() else 0
            bfile = work / 'model.ode.b'
            bsize = len(bfile.read_text()) if bfile.exists() else 0
            dsize = len(diag.read_text()) if diag.exists() else 0
            if curve:
                # LP starts directly; HB/PD/TR offer Two Param (maps' HB starts directly too).
                keys(auto, 'r', *(['t'] if start_run['type'] != 'LP' and not odex.discrete else []))
            else:
                keys(auto, 'r', {'equilibrium': 's', 'periodic': 'p'}[r['kind']])

            def finished():
                rows = [ROW.match(l) for l in log.read_text().splitlines()[before:]]
                rows = [m for m in rows if m]
                # One-parameter runs print an initial EP; curves may print only their final row before NPR.
                ends = sum(m.group(3) in ('EP', 'MX') for m in (rows if curve else rows[1:]))
                # Cycles first generate five starting-data steps, then do_auto restarts automatically.
                return rows if ends >= (2 if preparing else 1) and not (work / 'fort.9').exists() else None
            rows = wait_for('the end of run ' + r['name'], finished)
            wait_for('the diagnostics of run ' + r['name'], lambda: diag.exists() and diag.stat().st_mtime_ns != stamp)
            sizes = []  # the file is complete once it stops growing

            def complete():
                sizes.append(diag.stat().st_size)
                return len(sizes) > 1 and sizes[-1] == sizes[-2]
            wait_for('the diagnostics of run %s to stop growing' % r['name'], complete)
            btext = bfile.read_text(errors='replace')
            # XPPAUT prepends the newest run when it appends its AUTO files (appendf).
            btext = btext[:len(btext) - bsize] if curve_driver else btext
            used = constants_used(btext)
            want = dict(zip(('ntst', 'nmx', 'npr', 'ds', 'dsmin', 'ncol', 'epsl', 'dsmax', 'rl0', 'rl1', 'a0', 'a1', 'epsu', 'epss',
                             'iad', 'mxbf', 'iid', 'itmx', 'itnw', 'nwtn'), values))
            bad = {k: (used.get(k), v) for k, v in want.items()
                   if not math.isclose(used.get(k, math.nan), float(v), rel_tol=1e-3, abs_tol=1e-12)}
            if bad:
                raise Refused('the Numerics dialog did not set %s (XPPAUT used, wanted)' % bad)
            if curve_driver:
                irs = (seed if seed else labelled[start_run['index'] - 1]['label']) if start_run.get('state') != 'current' else 0
                if preparing:
                    # The initial .b header verifies the grabbed label; the final one uses the generated EP.
                    headers = re.split(r'(?m)(?=^\s*0\s+[-+0-9.]+E[-+][0-9]+\s+[-+0-9.]+E)', btext)
                    preparation = constants_used(headers[2])
                    restarts = re.findall(r'RestartLabel=(\d+)', '\n'.join(log.read_text().splitlines()[before:]))
                    if preparation.get('irs') != irs or not restarts:
                        raise Refused('XPPAUT starting data did not restart the grabbed label %d' % irs)
                    irs = int(restarts[-1])
                    # Reverse from the same generated EP, as a user does; regenerating it with negative DS
                    # can reverse the saved tangent too, making both signs follow the same direction.
                    seeds[seed_key] = irs
                want.update(problem(r, odex.discrete, cycle_curve), irs=irs)
                if curve:
                    want['isp'] = 0  # XPPAUT disables detection on singular-point curves (auto_nox.c).
                bad.update({k: (used.get(k), v) for k, v in want.items() if k in ('irs', 'ips', 'isw', 'ilp', 'isp') and used.get(k) != v})
                if bad:
                    raise Refused('XPPAUT run constants differ (used, wanted): %s' % bad)
                active = re.search(r'Active continuation parameters?:\s*([0-9 ]+)', btext)
                slots = ([pars.index(n) for n in axes] if curve else [pars.index(r['parameter'])]) + (
                    [PERIOD - 1] if base_kind == 'periodic' else []) + (
                    [CYCLE_EXTRA - 1] if cycle_curve and start_run['type'] in ('LP', 'TR') else [])
                if not active or [int(n) for n in active[1].split()] != slots:
                    raise Refused('XPPAUT active continuation slots differ from %s' % slots)
                # The b-file's columns carry the actual free parameter slots.
                if curve:
                    curves[r['name']] = curve_points(btext, pars.index(axes[0]), pars.index(axes[1]))
                labels.extend(int(m.group(4)) for m in rows if int(m.group(4)))
            dtext = diag.read_text(errors='replace')
            stab[r['name']] = stability(dtext[:len(dtext) - dsize] if curve_driver else dtext)
            solutions = solution_parameters((work / 'model.ode.s').read_text(errors='replace'))
            points[r['name']] = []
            for m in rows:
                if m.group(3) in POINT_TYPES:
                    # the period (PAR(11)) means something at a Hopf point and on a periodic branch
                    period = solutions[int(m.group(4))][PERIOD - 1] if m.group(3) == 'HB' or r['kind'] == 'periodic' else None
                    pars_values = solutions[int(m.group(4))]
                    kind = m.group(3)
                    if curve_driver and odex.discrete and kind == 'HB':
                        # fnhbae logs map multipliers: a real flip has log(-1)=i*pi, hence period 2.
                        kind = 'PD' if period == 2.0 else 'TR'
                    points[r['name']].append(dict(type=kind, branch=int(m.group(1)), label=int(m.group(4)),
                                                  point=int(m.group(2)), par=float(m.group(5)), norm=float(m.group(6)), period=period,
                                                  **({'par2': pars_values[pars.index(axes[1])]} if curve else {})))
        if keep:
            shutil.copytree(work, keep / name, dirs_exist_ok=True)
        return points, stab, failed, curves
    finally:
        for p in (xpp, xvfb):
            if p is not None and p.poll() is None:
                p.kill()
                p.wait()
        shutil.rmtree(work, ignore_errors=True)


def reference(name, d, program, args):
    odex, eqs, start, bvp = check_description(name, d, program)
    if program == 'auto07p':
        points, stab, failed, curves = execute_auto07p(d, odex, eqs, start, bvp, args.auto_dir, args.keep, name)
    else:
        points, stab, failed, curves = execute_xppaut(d, odex, eqs, start, args.xppaut, args.keep, name)
    runs = []
    for r in d['runs']:
        kept = []
        for pt in points.get(r['name'], []):
            if pt['type'] in POINT_TYPES:
                kind, vals = stab[r['name']].get((abs(pt['branch']), abs(pt['point'])), ('', []))
                # AUTO-07p prints a map's fixed point's Jacobian eigenvalues, which are its multipliers
                kept.append({**pt, 'spectrum': 'multiplier' if odex.discrete and kind else kind.lower(), 'values': vals})
        names = {'parameters': r['parameters']} if 'parameters' in r else {'parameter': r['parameter']}
        runs.append({'name': r['name'], **names, 'points': kept,
                     **({'curve': curves[r['name']]} if r['name'] in curves else {}),
                     **({'unavailable': failed[r['name']]} if r['name'] in failed else {})})
    return {'format': FORMAT, 'source': SOURCES[program] + ' (tools/auto_reference.py)', 'model': d['model'], 'runs': runs}


def write(path, ref):
    lines = ['{', '  "format": %d,' % ref['format'], '  "source": %s,' % json.dumps(ref['source']),
             '  "model": %s,' % json.dumps(ref['model']), '  "runs": [']
    for i, r in enumerate(ref['runs']):
        axes = '"parameters": %s' % json.dumps(r['parameters']) if 'parameters' in r else '"parameter": %s' % json.dumps(r['parameter'])
        lines += ['    {"name": %s, %s,%s "points": [' % (
            json.dumps(r['name']), axes,
            (' "unavailable": %s,' % json.dumps(r['unavailable'])) if 'unavailable' in r else '')]
        lines += ['      ' + json.dumps(p) + (',' if j + 1 < len(r['points']) else '') for j, p in enumerate(r['points'])]
        if 'curve' in r:
            lines.append('    ], "curve": [')
            lines += ['      ' + json.dumps(p) + (',' if j + 1 < len(r['curve']) else '') for j, p in enumerate(r['curve'])]
        lines.append('    ]}' + (',' if i + 1 < len(ref['runs']) else ''))
    lines += ['  ]', '}', '']
    path.write_text('\n'.join(lines), encoding='utf-8')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--program', choices=sorted(SUFFIX), required=True)
    ap.add_argument('--only', help='comma-separated description names (without .json)')
    ap.add_argument('--keep', type=Path, help='keep the program\'s files here')
    ap.add_argument('--auto-dir', default=os.environ.get('AUTO_DIR', str(Path.home() / 'auto-07p-0.9.4')))
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
