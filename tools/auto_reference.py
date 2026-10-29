#!/usr/bin/env python3
"""W300/#358: write tests/auto/<description>.auto07p and .xppaut, the special points AUTO-07p
and XPPAUT 8.0 find for the problem a tests/auto description runs in xppautX.

This is the maintainer's manual tool (Linux or WSL). Only its output is committed;
tools/autocompare.py compares ours with those files on every push, without either
program. A description whose problem a program cannot run the same way is refused with
the reason (REFUSED), which the description keeps as auto07p_skip / xppaut_skip.
A curve's samples have one format per program (W539): an .auto07p curve row is [parameter, second parameter,
norm, period] (AUTO-07p's PERIOD column; 0 for an equilibrium), an .xppaut curve row is [parameter, second parameter, norm].

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

    uv run --locked python tools/auto_reference.py --program auto07p|xppaut [--only NAME,...] [--keep DIR]
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
import time

from autocompare import CODIM_TWO, HOMCONT, SPECIAL_TYPES, completed, read_description
from odex_model import Odex, Refused, INTRINSICS
from comparison_measure import clock, elapsed, run_foreground
from xppclient import Server, wait_until

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / 'tests/auto'
FORMAT = 1
# AUTO's index of the period among the parameters (PAR(11); core/auto_nox.h AUTO_PERIOD_INDEX is 0-based 10)
PERIOD = 11
# AUTO's reserved PAR(12): torus angle or fold-of-cycles normalization (autlib1.c init).
CYCLE_EXTRA = 12
RHS_RELATIVE_TOLERANCE = 1e-12  # of the sum of the absolute sizes of the terms (magnitude_bound), so a cancelling RHS is judged by its rounding, not by its small value


def fnum(v):
    """A Fortran double literal."""
    t = repr(float(v)).replace('e', 'd')
    return t if 'd' in t else t + 'd0'
# the constants xppautX fixes (core/auto_nox.cpp: advanced numerics and the run's own ISP, ILP, ISW)
FIXED = dict(IAD=3, IID=2, ITMX=8, ITNW=7, NWTN=3, IADS=1, ILP=1, ISW=1, A0=0, A1=1000)
DEFAULTS = dict(ncol=4, epsl=1e-4, epsu=1e-4, epss=1e-4, mxbf=5)
# a description's setting (xppautX's name, W531) to AUTO's own constant it is given as
AUTO_NAMES = dict(mesh_intervals='ntst', max_steps='nmax', output_every='npr', collocation_points='ncol', step_size='ds',
                  min_step_size='dsmin', max_step_size='dsmax', parameter_min='parmin', parameter_max='parmax',
                  parameter_tolerance='epsl', state_tolerance='epsu', special_point_tolerance='epss',
                  step_adapt_every='iads', max_branch_switches='mxbf')


def auto_constants(r):
    """The run's settings under AUTO's constants (MXBF's sign is the description's switch_one_direction)."""
    a = {**DEFAULTS, **{AUTO_NAMES[k]: v for k, v in r['auto'].items() if k in AUTO_NAMES}}
    if r['auto'].get('switch_one_direction'):
        a['mxbf'] = -a['mxbf']
    return a
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
    a = auto_constants(r)
    lines = ['NDIM=%d, IPS=%d, IRS=%d, ILP=%d' % (ndim, pr['ips'], irs, pr['ilp']),
             'ICP=%s' % icp,
             'NTST=%d, NCOL=%d, IAD=%d, ISP=%d, ISW=%d, IPLT=0, NBC=%d, NINT=0' %
             (a['ntst'], a['ncol'], FIXED['IAD'], pr['isp'], pr['isw'], nbc),
             'NMX=%d, NPR=%d, MXBF=%d, IID=%d, ITMX=%d, ITNW=%d, NWTN=%d, JAC=0' %
             (a['nmax'], a['npr'], a['mxbf'], FIXED['IID'], FIXED['ITMX'], FIXED['ITNW'], FIXED['NWTN']),
             'EPSL=%r, EPSU=%r, EPSS=%r' % (a['epsl'], a['epsu'], a['epss']),
             'DS=%r, DSMIN=%r, DSMAX=%r, IADS=%d' % (a['ds'], a['dsmin'], a['dsmax'], a.get('iads', FIXED['IADS'])),
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


NUMBER = re.compile(r'-?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?')


def magnitude_bound(toks):
    """The expression with every binary minus a plus, every sign dropped and each function call or number
    in abs(): its value is the sum of the absolute sizes of the terms (a bound on what rounding works on),
    where the RHS itself may cancel to almost nothing. A condition keeps its true value."""
    out, i, previous = [], 0, None
    while i < len(toks):
        t = toks[i]
        operand_before = previous is not None and (previous == ')' or NUMBER.fullmatch(previous) or previous.isidentifier())
        if t == '-':
            if operand_before:
                out.append('+')
            elif previous == '^':
                out.append('-')  # an exponent's sign is part of its value
            previous = t
            i += 1
        elif NUMBER.fullmatch(t):
            out.append(t.lstrip('-'))
            previous, i = t, i + 1
        elif t.isidentifier() and t not in ('if', 'then', 'else') and i + 1 < len(toks) and toks[i + 1] == '(':
            depth, j = 0, i + 1
            while True:
                depth += (toks[j] == '(') - (toks[j] == ')')
                if depth == 0:
                    break
                j += 1
            out += ['abs', '('] + toks[i:j + 1] + [')']
            previous, i = ')', j + 1
        elif t == 'if':
            groups, j = [], i + 1
            for _ in range(3):  # if ( C ) then ( A ) else ( B ), as Equations.conditionals writes it
                j += 1 if toks[j] in ('then', 'else') else 0
                start_group, depth = j, 0
                while True:
                    depth += (toks[j] == '(') - (toks[j] == ')')
                    j += 1
                    if depth == 0:
                        break
                groups.append(toks[start_group + 1:j - 1])
            condition, taken, other = groups
            out += ['if', '('] + condition + [')', 'then', '('] + magnitude_bound(taken) + [')', 'else', '('] + magnitude_bound(other) + [')']
            previous, i = ')', j
        else:
            out.append(t)
            previous, i = t, i + 1
    return out


def check_rhs(binary, eqs, start, folder):
    """W439: evaluate the generated FUNC and the same expanded RHS with
    xppautX's expression evaluator at the series' start. No Python math copy.
    Retain both vectors; a near-zero residual is not given an absolute floor.
    """
    folder.mkdir(parents=True)
    n, npar = len(start), max(eqs.npar, PERIOD)
    source = eqs.source(start, False)
    driver = ['program rhs_probe', 'implicit none',
              'integer :: icp(1), i',
              'double precision :: u(%d), par(%d), f(%d), dfdu(%d,%d), dfdp(%d,%d)' % (n, npar, n, n, n, n, npar),
              'par=0', 'icp=1', 'call stpnt(%d,u,par,0d0)' % n,
              'call func(%d,u,icp,par,0,f,dfdu,dfdp)' % n,
              "do i=1,%d; write(*,'(ES26.17E3)') f(i); end do" % n, 'end program rhs_probe']
    (folder / 'probe.f90').write_text(source + '\n' + '\n'.join(driver) + '\n')
    subprocess.run(['gfortran', '-ffp-contract=off', 'probe.f90', '-o', 'probe'], cwd=folder, check=True, capture_output=True)
    p = subprocess.run([str((folder / 'probe').resolve())], cwd=folder, check=True, capture_output=True, text=True)
    foreign = [float(v) for v in p.stdout.split()]
    (folder / 'probe.odex').write_text("par probe_value = 0\nz' = 0\n")
    assignments = dict(eqs.ordered_assigns())
    values = {**eqs.values, **dict(zip(eqs.o.states, start)), 't': 0.0}

    def expand(toks):
        out = []
        for t in eqs.expand(toks):
            if t in assignments:
                out += ['('] + expand(assignments[t]) + [')']
            elif t in values:
                out += ['(', repr(float(values[t])), ')']
            else:
                out.append(t)
        return out

    ours, scales = [], []
    run = folder / 'xppautx'
    run.mkdir()
    s = Server(binary, folder / 'probe.odex', run=str(run))
    try:
        completed(s)
        for name in eqs.o.states:
            expression = ' '.join(eqs.conditionals(expand(eqs.o.odes[name]), xppaut=True))
            events = completed(s, dict(cmd='value.set_by_formula', name='probe_value', formula=expression))
            state = next(e for e in reversed(events) if e.get('ev') == 'state')
            ours.append(dict(state['pars'])['probe_value'])
            events = completed(s, dict(cmd='value.set_by_formula', name='probe_value', formula=' '.join(magnitude_bound(expression.split()))))
            state = next(e for e in reversed(events) if e.get('ev') == 'state')
            scales.append(dict(state['pars'])['probe_value'])
    finally:
        s.close()
    relative = [abs(a-b)/scale if scale else float(a != b) for a, b, scale in zip(ours, foreign, scales)]
    result = dict(states=eqs.o.states, start=start, xppautx=ours, fortran=foreign, term_sizes=scales,
                  relative_errors=relative, relative_tolerance=RHS_RELATIVE_TOLERANCE,
                  passed=len(ours) == len(foreign) == n and all(math.isfinite(v) and v <= RHS_RELATIVE_TOLERANCE for v in relative))
    (folder / 'rhs-check.json').write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    return result


BLOCK = re.compile(r'^\s*(\d+)\s+(\d+)\s+(Eigenvalue|Multiplier)\s*(\d+):?\s+(\S+)\s+(\S+)(\s|$)')
# the row that ends a special point's output: BR, PT (negative when stable), TY, LAB; a location's
# iteration rows (BR, PT, IT) have no TY
SUMMARY = re.compile(r'^\s*(-?\d+)\s+(-?\d+)\s+[A-Z][A-Z0-9]{1,2}\s+(\d+)\s+-?\d\.\d+E[-+]\d+')


def fortran_number(text):
    """A number as AUTO-07p's Fortran prints it: with a three-digit exponent the E is dropped (1.79769+308)."""
    return float(re.sub(r'(\d)([-+]\d{3})$', r'\1E\2', text))


def stability(text):
    """d.<name>: {(branch, label): (kind, [[re, im], ...])}, the last block printed for a special point
    (the location of a special point prints the point again after each of its iterations). Keyed by
    label, not point: a run of more than 9999 steps prints a point's number again for a later point
    (AUTO-07p's NTOP=MOD(NTOT-1,9999)+1, ae.f90; XPPAUT's ntop=(ntot+1)%10000, autlib1.c).
    A block starts when the number of its line is not above the one before: AUTO-07p numbers
    eigenvalues and multipliers from 1, XPPAUT's multipliers from 0."""
    out, block, last = {}, None, None
    for line in text.splitlines():
        m = BLOCK.match(line)
        if m:
            key, number = (int(m.group(1)), int(m.group(2))), int(m.group(4))
            if block is None or block[0] != key or number <= last:
                block = (key, m.group(3), [])
            last = number
            block[2].append([fortran_number(m.group(5)), fortran_number(m.group(6))])
            continue
        m = SUMMARY.match(line)
        if m:
            branch, label = abs(int(m.group(1))), int(m.group(3))
            if block is not None and block[0] == (branch, abs(int(m.group(2)))):
                out[branch, label] = block[1:]
    return out


def curve_points(text, first, second=None):
    """[[PAR(first), PAR(second), L2-NORM, PERIOD]] of every step of a two-parameter run, from its bifurcation
    diagram file (b.*: a header line naming the columns, then one line per step, whatever NPR is). PERIOD is the
    PERIOD column; an equilibrium's file has none, and its samples then carry 0 (no period, no global end: W539)."""
    lines = text.splitlines()
    head = next(i for i, l in enumerate(lines) if 'L2-NORM' in l and 'PAR(' in l)
    # the header starts with a 0 where the lines have their branch number
    # a periodic solution's file names a column "MAX U(1)": one column, two words
    columns = {name: i + 1 for i, name in enumerate(re.sub(r'(MAX|MIN) ', r'\1', lines[head]).split()[1:])}
    wanted = [columns['PAR(%d)' % first], columns['PAR(%d)' % second] if second is not None else None, columns['L2-NORM'],
              columns.get('PERIOD')]
    rows = []
    for line in lines[head + 1:]:
        fields = line.split()
        if fields and fields[0] == '0':  # XPPAUT prepends runs; the next header starts the earlier run.
            break
        if len(fields) == len(columns) + 1:
            rows.append([float(fields[k]) if k is not None else 0.0 for k in wanted])
    return rows


def script(runs, eq, names, measure=False):
    """The .auto script: runs in order, every labelled solution printed as a JSON line."""
    lines = ['import json, os, subprocess', 'results = {}']
    compile_lines = []
    for e in ('eq', 'eqs') if eq['homcont'] else ('eq',):
        compile_lines += ["subprocess.run(['make', '-f', os.path.join(os.environ['AUTO_DIR'], 'cmds/cmds.make'), 'EQUATION_NAME=%s', 'CC=gcc -std=gnu17', '%s.exe'], check=True)" % (e, e),
                          "assert os.path.isfile('%s.exe'), 'AUTO make did not produce %s.exe'" % (e, e)]
    if measure:
        # Compile before the run() clocks. AUTO reuses the up-to-date object and
        # executable, so RUSAGE_CHILDREN below counts the executable, not gfortran.
        lines += ['import resource, time', *compile_lines,
                  "ready_self, ready_children = resource.getrusage(resource.RUSAGE_SELF), resource.getrusage(resource.RUSAGE_CHILDREN)",
                  "print('READY ' + json.dumps({'wall_seconds': time.perf_counter()-float(os.environ['W439_START']), 'cpu_seconds': ready_self.ru_utime+ready_self.ru_stime+ready_children.ru_utime+ready_children.ru_stime}))",
                  'def child_cpu():', '    r = resource.getrusage(resource.RUSAGE_CHILDREN)',
                  '    return r.ru_utime + r.ru_stime']
    else:
        lines += compile_lines
    for r, c in zip(runs, names):
        start = r['start']
        if start.get('state') == 'current':
            # a HomCont description's `eq` starts every run from its orbit; an equilibrium run starts from our start (`eqs`)
            src = "run(e='%s', c='%s')" % ('eqs' if eq['homcont'] and r['kind'] == 'equilibrium' else 'eq', c)
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
        if measure:
            at = lines.index('    results[%r] = %s' % (r['name'], src))
            lines[at:at] = ['    t0, c0 = time.perf_counter(), child_cpu()']
            lines.insert(at + 2, "    print('TIMING ' + json.dumps({'run': %r, 'wall_seconds': time.perf_counter()-t0, 'cpu_seconds': child_cpu()-c0}))" % r['name'])
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
    if program + '_skip' in d:  # a reason this program's reference is not kept (autocompare reads the same key)
        raise Refused('our description skips this program: ' + d[program + '_skip'])
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
        for parameter, _ in r.get('marks', []):
            if parameter != 'T' and parameter not in eqs.slot:
                raise Refused('mark parameter %s is not in the model' % parameter)
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


def execute_auto07p(d, odex, eqs, start, bvp, auto_dir, keep, name, measurements=None, exact_source=None, process_env=None):
    """{run: [point]}, {run: {(branch, point): spectrum}}, {run: reason} from AUTO-07p."""
    (ROOT / 'build').mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='w300-a07-', dir=ROOT / 'build'))
    try:
        hom = None
        if any(r['kind'] in HOMCONT for r in d['runs']):
            run, = (r for r in d['runs'] if r['kind'] in HOMCONT)
            known = {**odex.inits, **{k.lower(): v for k, v in d.get('initial_conditions', {}).items()}}
            hom = dict(interval=run['start']['settle'], step=run['start']['step'], ic=[known.get(x, 0.0) for x in odex.states],
                       equilibria=run['connection']['left'] + (run['connection']['right'] if run['kind'] == 'heteroclinic' else []))
        (work / 'eq.f90').write_text(eqs.source(start, bvp, hom), encoding='utf-8')
        if hom:
            (work / 'eqs.f90').write_text(eqs.source(start, bvp), encoding='utf-8')
        if exact_source:
            (work / 'eq.f90').unlink()
            shutil.copy2(exact_source, work / 'eq.c')
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
            text = constants(r, len(odex.states), pr, len(odex.bcs) if bvp else 0,
                             icp, 0, max(eqs.npar, HOM_EQUILIBRIUM - 1 + (2 if r['kind'] == 'heteroclinic' else 1) * len(odex.states)))
            zeros = {}
            for parameter, value in r.get('marks', []):
                slot = PERIOD if parameter == 'T' else eqs.slot[parameter]
                zeros.setdefault(slot, []).append(value)
            text += 'UZR=%r\n' % zeros
            if exact_source:
                text = text.replace('JAC=0', 'JAC=1')
            (work / ('c.' + c)).write_text(text)
        env = {**os.environ, **(process_env or {}), 'AUTO_DIR': auto_dir, 'PYTHONWARNINGS': 'ignore'}
        env['PATH'] = '%s/cmds:%s/bin:%s' % (auto_dir, auto_dir, env['PATH'])
        (work / 'run.auto').write_text(script(d['runs'], {'icp1': icp1, 'icp2': icp2, 'cycles': cycles, 'discrete': odex.discrete, 'homcont': hom is not None}, names, measurements is not None))
        env['W439_START'] = str(time.perf_counter())
        p = run_foreground(['auto', 'run.auto'], cwd=work, env=env, timeout=RUN_LIMIT * len(d['runs']))
        (work / 'wrapper.log').write_text(p.stdout + p.stderr)
        if measurements is not None:
            measurements['wrapper_wall_seconds'] = time.perf_counter() - float(env['W439_START'])
            measurements['runs'] = [json.loads(l[7:]) for l in p.stdout.splitlines() if l.startswith('TIMING ')]
            measurements['ready'] = next((json.loads(l[6:]) for l in p.stdout.splitlines() if l.startswith('READY ')), None)
        if p.returncode or 'POINT ' not in p.stdout:
            raise Refused('AUTO-07p failed: ' + (p.stderr or p.stdout)[-600:])
        curves = {r['name']: curve_points((work / ('b.' + c)).read_text(errors='replace'), icp1[r['name']], icp2.get(r['name']))
                  for c, r in zip(names, d['runs']) if (work / ('b.' + c)).exists()}
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
        if keep:
            shutil.copytree(work, keep / name, dirs_exist_ok=True)
        shutil.rmtree(work, ignore_errors=True)


# --- XPPAUT 8.0: its AUTO window on a private display, driven by the keys a user presses -------------
DISPLAY = os.environ.get('W300_DISPLAY', ':93')
# XPPAUT 8.0's AutoPer dialog has nine fields (auto_nox.c:706-729).
XPPAUT_MARK_ROWS = 9
MENU_MS = '300'  # ms xdotool waits between keys: XPPAUT opens a pop-up menu per key and reads the next key from it
TYPE_DELAY = '15'  # ms between the characters typed into a dialog
BACKSPACES = 40  # more than any field holds
RUN_LIMIT = 600  # s: bound one foreground run; an overrun is unavailable, not a numerical failure.
# the keys that pick a one-parameter run's start point by its type (Up, then the key, repeated by index)
GRAB_KEYS = {'HB': 'h', 'LP': 'l', 'BP': 'b', 'PD': 'p', 'TR': 't'}
ROW = re.compile(r'^\s*(-?\d+)\s+(-?\d+)\s+(?:([A-Z]{2})\s+)?(\d+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)(.*)$')


def xdo(*args):
    return subprocess.run(['xdotool', *args], env={**os.environ, 'DISPLAY': DISPLAY},
                          capture_output=True, text=True).stdout.strip()


def wait_for(what, found, timeout=RUN_LIMIT):
    """What `found()` returns once it is true (xppclient's wait_until polls)."""
    got = []
    if not wait_until(lambda: got.append(found()) or got[-1], timeout):
        raise Refused('XPPAUT: timed out waiting for %s after %d s' % (what, timeout))
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
    a = auto_constants(r)
    return [a['ntst'], a['nmax'], a['npr'], a['ds'], a['dsmin'], a['ncol'], a['epsl'], a['dsmax'], a['parmin'],
            a['parmax'], FIXED['A0'], FIXED['A1'], a['epsu'], a['epss'], FIXED['IAD'], a['mxbf'], FIXED['IID'],
            FIXED['ITMX'], FIXED['ITNW'], FIXED['NWTN'], a.get('iads', FIXED['IADS']), 0]


def constants_used(text):
    """The run's constants as the .b file header prints them (the first line is RL0 RL1 A0 A1)."""
    head = text.splitlines()[:9]
    used = dict(zip(('rl0', 'rl1', 'a0', 'a1'), (float(x) for x in head[0].split()[1:5])))
    used.update((k.lower(), float(v)) for k, v in re.findall(r'([A-Z]+)\s*=\s*([-+0-9.eE]+)', '\n'.join(head[1:])))
    return used


def execute_xppaut(d, odex, eqs, start, binary, keep, name, measurements=None, process_env=None):
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
        launched = time.perf_counter() if measurements is not None else None
        xpp = subprocess.Popen([os.path.abspath(binary), model.name], cwd=work, stdout=open(log, 'w'), stderr=subprocess.STDOUT,
                               env={**os.environ, **(process_env or {}), 'DISPLAY': DISPLAY, 'HOME': str(work), 'LC_ALL': 'C'})
        main = window('XPP')
        if measurements is not None:
            key_start = time.perf_counter()
        keys(main, 'f', 'a')
        if measurements is not None:
            key_pauses = time.perf_counter() - key_start
        auto = window('AUTO')
        if measurements is not None:
            measurements['ready'] = dict(wall_seconds=time.perf_counter() - launched - key_pauses,
                                         cpu_seconds=clock(xpp.pid)[1], excluded_key_driver_seconds=key_pauses)
            measurements['runs'] = []
        points, stab, failed, curves = {}, {}, {}, {}
        curve_driver = any(r['kind'] == 'two_parameter' for r in d['runs'])
        # Keep AUTO slots stable: saved solutions use these positions, not model parameter indices.
        selected = list(dict.fromkeys(n for r in d['runs'] for n in r.get('parameters', [r.get('parameter')]) if n))
        selected += [n for n in odex.pars if n not in selected]
        labels, seeds = [], {}
        npar = min(8, len(odex.pars))
        for r in d['runs']:
            if 'skip' in r:  # An explicit description skip also skips the foreign run.
                failed[r['name']] = 'our description skips it: ' + r['skip']
                continue
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
            marks = r.get('marks', [])
            if len(marks) > XPPAUT_MARK_ROWS:
                raise Refused('XPPAUT AutoPer has only %d mark fields' % XPPAUT_MARK_ROWS)
            if marks:
                mark_fields = ['%s=%r' % (parameter, value) for parameter, value in marks]
                # Inactive fields are ignored by NUZR, but AutoPer still edits all nine.
                mark_fields += ['T=0'] * (XPPAUT_MARK_ROWS - len(marks))
                fill_dialog(auto, (('u', str(len(marks))), 'AutoPer'), mark_fields)
            else:
                keys(auto, 'u', '0')
            start_run = r['start']
            if start_run.get('state') != 'current':
                if curve_driver or start_run['type'] not in GRAB_KEYS:
                    labelled = [pt for pt in points[start_run['run']] if pt['type'] == start_run['type']]
                    if len(labelled) < start_run['index']:
                        raise Refused('XPPAUT 8.0 run %s has no %s %d for %s' %
                                      (start_run['run'], start_run['type'], start_run['index'], r['name']))
                    point = labelled[start_run['index'] - 1]
                    label = seed if seed else point['label']
                    # Tab visits labelled points in diagram order (auto_x11.c traverse_diagram).
                    keys(auto, 'g', 'Home', *(['Tab'] * labels.index(label)), 'Return')
                else:
                    keys(auto, 'g', *(['Up', GRAB_KEYS[start_run['type']]] * start_run['index']), 'Return')
            if curve_driver:
                axes = r['parameters'] if curve else [r['parameter'], next(n for n in pars if n != r['parameter'])]
                a = auto_constants(r)
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
                run_keys = ['r', *(['t'] if start_run['type'] != 'LP' and not odex.discrete else [])]
            else:
                # a periodic run from a point of cycles extends them (Run's periodic_choice, auto_nox.c: Extend is 'e')
                extends = start_run.get('state') != 'current' and start_run['type'] != 'HB'
                run_keys = ['r', {'equilibrium': 's', 'periodic': 'e' if extends else 'p'}[r['kind']]]
                if r['kind'] == 'equilibrium' and start_run.get('type') == 'EP':
                    # E1's repeated branches: an ordinary labelled equilibrium
                    # extends directly, with no menu (XPPAUT 8.0 auto_nox.c:2006-2008).
                    run_keys = ['r']
            if measurements is None:
                keys(auto, *run_keys)
            else:
                # All menu delays precede the timed final key. Its xdotool
                # invocation has no --delay, so no fixed key pause is timed.
                if len(run_keys) > 1:
                    keys(auto, *run_keys[:-1])
                xdo('mousemove', '--window', auto, '20', '20')
                run_start = clock(xpp.pid)
                xdo('key', '--window', auto, '--delay', '0', run_keys[-1])

            def finished():
                rows = [ROW.match(l) for l in log.read_text().splitlines()[before:]]
                rows = [m for m in rows if m]
                # One-parameter runs print an initial EP; curves may print only their final row before NPR.
                # Equilibrium EP restarts do not print their seed again;
                # auto_run extends it directly (auto_nox.c:2006-2008).
                repeats_equilibrium = r['kind'] == 'equilibrium' and start_run.get('type') == 'EP'
                ends = sum(m.group(3) in ('EP', 'MX') for m in (rows if curve or repeats_equilibrium else rows[1:]))
                # Cycles first generate five starting-data steps, then do_auto restarts automatically.
                return rows if ends >= (2 if preparing else 1) and not (work / 'fort.9').exists() else None
            try:
                rows = wait_for('the end of run ' + r['name'], finished)
                if measurements is not None:
                    measurements['runs'].append(dict(run=r['name'], **elapsed(xpp.pid, run_start)))
            except Refused as error:
                # A bounded run is unavailable, not a refusal of all earlier results.
                # Escape is XPPAUT's own abort key (auto_x11.c:660-662); wait for
                # its normal end before another run edits the window or files.
                failed[r['name']] = str(error)
                keys(auto, 'Escape')
                rows = wait_for('the aborted end of run ' + r['name'], finished)
            labels.extend(int(m.group(4)) for m in rows if int(m.group(4)))
            if r['name'] in failed:
                continue
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
            want['nuzr'] = len(marks)
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
            curves[r['name']] = curve_points(btext, pars.index(axes[0]) if curve else pars.index(r['parameter']),
                                             pars.index(axes[1]) if curve else None)
            dtext = diag.read_text(errors='replace')
            # Every run is prepended; Extend reuses its source branch/point numbers.
            stab[r['name']] = stability(dtext[:len(dtext) - dsize])
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
        return points, stab, failed, curves
    finally:
        for p in (xpp, xvfb):
            if p is not None and p.poll() is None:
                p.kill()
                p.wait()
        if keep:
            shutil.copytree(work, keep / name, dirs_exist_ok=True)
        shutil.rmtree(work, ignore_errors=True)


def reference(name, d, program, args, inputs=None, measurements=None, exact_source=None, process_env=None):
    odex, eqs, start, bvp = inputs if inputs is not None else check_description(name, d, program)
    if program == 'auto07p':
        points, stab, failed, curves = execute_auto07p(d, odex, eqs, start, bvp, args.auto_dir, args.keep, name, measurements, exact_source, process_env)
    else:
        points, stab, failed, curves = execute_xppaut(d, odex, eqs, start, args.xppaut, args.keep, name, measurements, process_env)
        # one format per program: an .xppaut curve's samples are [par, par2, norm] (XPPAUT's file has no period to keep)
        curves = {n: [sample[:3] for sample in c] for n, c in curves.items()}
    runs = []
    for r in d['runs']:
        kept = []
        for pt in points.get(r['name'], []):
            if pt['type'] in POINT_TYPES:
                kind, vals = stab[r['name']].get((abs(pt['branch']), pt['label']), ('', []))
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
        try:
            d = read_description(BASE / (name + '.json'))
            write(BASE / (name + SUFFIX[args.program]), reference(name, d, args.program, args))
            print('WROTE', name)
        except (Refused, ValueError) as e:
            print('REFUSED %s: %s' % (name, e))
    return 0


if __name__ == '__main__':
    sys.exit(main())
