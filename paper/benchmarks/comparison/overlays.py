#!/usr/bin/env -S uv run --locked python
"""W553: every comparison run of a program plotted alone, overlaid on xppautX's, and their difference.

Per workload and per other program, one figure of three panels: that program's result alone, the same with xppautX's
overlaid, and the error (other minus xppautX) on a common grid. manifest.json lists every figure with the max and RMS error.
Integration studies read runs/<workload>/0/<engine>/run/output.dat (repetition 0: results are bit-identical across
repetitions, which the gates check); the continuation attempt reads each run's branch from xppautX's diagram.csv and from
the references of XPPAUT and AUTO-07p, the quantities tools/autocompare.py compares (parameter, norm, second parameter).
W555: --extra DIR (extra_runs.py's folder, <study>/<workload>/<program>/) supplies the data a program has no usable result for in the
series, from an untimed rerun, and the figure and manifest say so; a scaling workload no other program can run (XPPAUT cannot hold
a model that size, docs/xppaut-findings.md finding 45) is drawn for xppautX alone, with that reason.
"""
import argparse
import json
import math
from pathlib import Path
import re
import sys

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

from common import TOOLS, save  # puts tools/ on sys.path
import autocompare
from odex_model import Odex

# report-20261022's program colours; xppautX is always the solid blue line
PROGRAM_COLORS = {'xppautx': '#1f77b4', 'xppaut': '#ff7f0e', 'python': '#2ca02c', 'auto07p': '#9467bd'}
SOLVER_COLORS = {'gmres': '#d62728', 'legacy-band': '#8c564b', 'legacy-dense': '#7f7f7f'}  # xppautX's other linear solvers
PROGRAM_NAMES = {'xppautx': 'xppautX', 'xppaut': 'XPPAUT 8.0', 'python': 'Python', 'auto07p': 'AUTO-07p'}
MAX_FIGURE_BYTES = 300 * 1024
PLOT_POINTS = 4000  # a curve is drawn with at most this many points; the errors are computed on every row
LOG_DECADES = 2  # the error axis is logarithmic (absolute value) when its non-zero values span more decades than this
DPI = 80
STUDIES = ('screen', 'modeldb', 'research', 'izhikevich', 'scaling')  # their trajectories are runs/<workload>/0/<engine>/run
LABEL_LIMIT = 30  # labelled points written on a panel; the rest keep their marker
RERUN_NOTE = 'untimed rerun (W555)'  # said in a figure's title and its manifest entry when its data came from --extra
NO_PEER = {'scaling': 'XPPAUT cannot run a model of this size (docs/xppaut-findings.md finding 45), so there is no second program'}

plt.rcParams.update({'font.size': 8, 'axes.grid': True, 'grid.alpha': .3, 'axes.spines.top': False, 'axes.spines.right': False,
                     'legend.frameon': False, 'lines.linewidth': 1.1})


def color(program):
    return PROGRAM_COLORS.get(program) or SOLVER_COLORS[program]


def name_of(program):
    return PROGRAM_NAMES.get(program, 'xppautX, solver ' + program)


def safe(text):
    return re.sub(r'[^A-Za-z0-9._-]+', '_', text)


def thin(x, y, error=False):
    """At most PLOT_POINTS points; for an error the one of largest magnitude of each bin, so a spike is never lost."""
    if len(x) <= PLOT_POINTS:
        return x, y
    edges = np.linspace(0, len(x), PLOT_POINTS + 1).astype(int)
    if error:
        idx = [lo + int(np.nanargmax(np.abs(y[lo:hi]))) if np.isfinite(y[lo:hi]).any() else lo
               for lo, hi in zip(edges[:-1], edges[1:]) if hi > lo]
        return x[idx], y[idx]
    idx = np.unique(np.linspace(0, len(x) - 1, PLOT_POINTS).astype(int))
    return x[idx], y[idx]


def error_panel(ax, x, e, xlabel, label, extra=None, extra_label=''):
    """The error e on x: linear and signed, or its absolute value on a log axis when it spans decades."""
    finite = np.abs(e[np.isfinite(e)])
    nonzero = finite[finite > 0]
    ax.set_xlabel(xlabel)
    if not len(nonzero):
        ax.plot(*thin(x, np.zeros(len(x))), color='black')
        ax.text(.5, .5, 'identically zero' if len(finite) == len(e) else 'no finite error', transform=ax.transAxes, ha='center')
        ax.set_ylabel(label)
        return
    xs, es = thin(x, e, error=True)
    if nonzero.max() / nonzero.min() > 10 ** LOG_DECADES:
        ax.semilogy(xs, np.where(es == 0, np.nan, np.abs(es)), color='black', lw=.8, label='vertical')
        ax.set_ylabel('|' + label + '|')
        if extra is not None:
            ax.semilogy(*thin(x, np.where(extra == 0, np.nan, np.abs(extra)), error=True), color='tab:gray', lw=.7, label=extra_label)
    else:
        ax.plot(xs, es, color='black', lw=.8, label='vertical')
        ax.set_ylabel(label)
        if extra is not None:
            ax.plot(*thin(x, extra, error=True), color='tab:gray', lw=.7, label=extra_label)
    if extra is not None:
        ax.legend(loc='best', fontsize=6.5)


def finish(fig, path):
    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=DPI)
    plt.close(fig)
    size = path.stat().st_size
    if size > MAX_FIGURE_BYTES:
        raise ValueError('%s: %d bytes is over the %d-byte limit' % (path, size, MAX_FIGURE_BYTES))


# ---- integration ----------------------------------------------------------------------------------------------

def column_names(folder, columns):
    """time, the states, then the auxiliaries of the model's .odex beside the run; generic names when they do not add up."""
    try:
        model = Odex(next(folder.glob('*.odex')))
        names = [*model.states, *(a.split('=')[0].strip() for a in model.aux)]
        if len(names) == columns - 1:
            return names
    except (StopIteration, ValueError, OSError, KeyError, AttributeError):
        pass
    return ['column %d' % k for k in range(1, columns)]


def load(path):
    data = np.loadtxt(path, ndmin=2)
    if data.shape[0] < 2:
        raise ValueError('%s: fewer than two rows' % path)
    return data


def compare_trajectories(base, other):
    """other minus base on base's times: (times, errors per row and column, grid text). The same grid subtracts directly,
    otherwise the other program is interpolated linearly onto base's times over the times both cover."""
    if base.shape[1] != other.shape[1]:
        raise ValueError('%d columns against %d' % (base.shape[1], other.shape[1]))
    if base.shape == other.shape and np.array_equal(base[:, 0], other[:, 0]):
        return base[:, 0], other[:, 1:] - base[:, 1:], 'the shared time grid (%d rows)' % len(base)
    lo, hi = max(base[0, 0], other[0, 0]), min(base[-1, 0], other[-1, 0])
    keep = (base[:, 0] >= lo) & (base[:, 0] <= hi)
    t = base[keep, 0]
    interpolated = np.column_stack([np.interp(t, other[:, 0], other[:, k]) for k in range(1, other.shape[1])])
    return t, interpolated - base[keep, 1:], "xppautX's times (the other interpolated, %d of %d rows, t %.6g to %.6g)" % (keep.sum(), len(base), lo, hi)


def plot_integration(entry, base_path, other_path, program, out, stopped=False, rerun=False):
    base, other = load(base_path), load(other_path)
    names = column_names(base_path.parent, base.shape[1])
    t, err, grid = compare_trajectories(base, other)
    column_error = np.where(np.isfinite(err), np.abs(err), np.inf).max(axis=0)
    worst = int(np.argmax(column_error)) if err.size else 0
    chosen = [0] if worst == 0 or column_error[worst] == 0 else [0, worst]
    finite = np.isfinite(err)
    entry.update(program=program, quantity='all state and auxiliary columns', columns=len(names), grid=grid,
                 max_abs_error=float(np.abs(err[finite]).max()) if finite.any() else None,
                 rms_error=float(np.sqrt(np.mean(err[finite] ** 2))) if finite.any() else None,
                 non_finite_values=int((~finite).sum()), plotted=[names[k] for k in chosen], worst_variable=names[worst],
                 rows=[len(base), len(other)])
    if stopped:  # a run every program aborts: each output ends at the last row stored before its stop
        entry['last_stored_time'] = [float(base[-1, 0]), float(other[-1, 0])]
    if rerun:
        entry['data_source'] = RERUN_NOTE
    fig, axes = plt.subplots(3, len(chosen), figsize=(max(5 * len(chosen) + 1, 8), 8), squeeze=False)
    for j, k in enumerate(chosen):
        alone, over, ax_err = axes[:, j]
        tx, bx = thin(base[:, 0], base[:, k + 1])
        to, oy = thin(other[:, 0], other[:, k + 1])
        alone.plot(to, oy, color=color(program), label=name_of(program))
        over.plot(to, oy, color=color(program), ls='--', lw=1.4, label=name_of(program))
        over.plot(tx, bx, color=color('xppautx'), lw=1.0, label='xppautX')
        if stopped:
            for ax, rows in ((alone, [(other, program)]), (over, [(other, program), (base, 'xppautx')])):
                for data, who in rows:
                    ax.axvline(data[-1, 0], color=color(who), ls=':', lw=1.2, label='%s stops after t = %.6g' % (name_of(who), data[-1, 0]))
        for ax in (alone, over):
            ax.set_ylabel(names[k])
            ax.legend(loc='best')
        alone.set_title('%s: %s alone' % (names[k], name_of(program)))
        over.set_title('overlaid on xppautX')
        over.set_xlabel('time')
        alone.set_xlabel('time')
        error_panel(ax_err, t, err[:, k], 'time (error on %s)' % grid, '%s: %s minus xppautX' % (names[k], name_of(program)))
        ax_err.set_title('error, max %.3g (this variable)' % (np.nanmax(np.abs(err[:, k])) if finite[:, k].any() else float('nan')))
    which = 'first variable %s' % names[0] if len(chosen) == 1 and worst == 0 else 'first variable %s and largest-error variable %s' % (names[0], names[worst])
    if len(chosen) == 1 and column_error[worst] == 0:
        which = 'first variable %s (errors are zero everywhere)' % names[0]
    fig.suptitle('%s: %s against xppautX, %s\nall columns: max |error| %s, RMS %s%s' % (
        entry['workload'], name_of(program), which, '%.3g' % entry['max_abs_error'] if entry['max_abs_error'] is not None else 'n/a',
        '%.3g' % entry['rms_error'] if entry['rms_error'] is not None else 'n/a',
        ('\nthe run is aborted by every program; dotted lines mark where each stops' if stopped else '') + ('\ndata from an ' + RERUN_NOTE if rerun else '')), fontsize=9)
    finish(fig, out)


def plot_alone(entry, base_path, reason, out):
    """xppautX's first variable alone, for a workload no other program can run; the reason is the figure's title and the manifest's."""
    base = load(base_path)
    names = column_names(base_path.parent, base.shape[1])
    entry.update(program='xppautx', quantity='first state variable', columns=len(names), rows=[len(base)], plotted=[names[0]], alone_reason=reason)
    fig, ax = plt.subplots(figsize=(8, 3.5))
    x, y = thin(base[:, 0], base[:, 1])
    ax.plot(x, y, 'o-' if len(x) < 30 else '-', color=color('xppautx'), label='xppautX')
    ax.set_xlabel('time')
    ax.set_ylabel(names[0])
    ax.legend(loc='best')
    fig.suptitle('%s: xppautX alone, variable %s (%d rows)\n%s' % (entry['workload'], names[0], len(base), reason), fontsize=8.5)
    finish(fig, out)


def integration_jobs(series, only=None, extra=None):
    """(study, workload label, xppautX's output, [(program, output)]) of every trajectory the series kept, and skip notes."""
    jobs, skipped = [], []
    for study in (*STUDIES, 'python'):
        runs = series / study / 'attempt-001' / 'runs'
        if not runs.is_dir():
            skipped.append((study, 'no runs folder'))
            continue
        for folder in sorted(runs.iterdir()):
            run = folder / '0'
            base = run / 'xppautx' / 'run' / 'output.dat'
            if not base.exists():
                skipped.append((study + '/' + folder.name, 'no xppautX output.dat in repetition 0'))
                continue
            others = [(p.name, p / 'run' / 'output.dat') for p in sorted(run.iterdir()) if p.name != 'xppautx']
            if extra:  # a program with no output in the series: its untimed rerun's, when there is one
                others = [(p, f if f.exists() or not (extra / study / folder.name / p / 'output.dat').exists() else extra / study / folder.name / p / 'output.dat')
                          for p, f in others]
            found = [(p, f) for p, f in others if f.exists()]
            skipped += [(study + '/%s/%s' % (folder.name, p), 'no output.dat') for p, f in others if not f.exists()]
            if found:
                jobs.append((study, folder.name, base, found))
            elif study in NO_PEER:
                jobs.append((study, folder.name, base, []))
            else:
                skipped.append((study + '/' + folder.name, 'no other program ran it'))
        if study == 'python':  # the ladder's finest rung: every program at the tolerance of the last rung
            ladder = series / study / 'attempt-001' / 'ladder'
            for folder in sorted(p for p in ladder.iterdir() if p.is_dir()) if ladder.is_dir() else []:
                rungs = sorted((r for r in folder.iterdir() if r.is_dir() and r.name != 'reference'), key=lambda r: float(r.name))
                if not rungs:
                    continue
                finest = rungs[0]
                base = finest / 'xppautx' / 'output.dat'
                found = [(p.name, p / 'output.dat') for p in sorted(finest.iterdir()) if p.name != 'xppautx' and (p / 'output.dat').exists()]
                if base.exists() and found:
                    jobs.append(('python-ladder', '%s@tolerance-%s' % (folder.name, finest.name), base, found))
                else:
                    skipped.append(('python-ladder/' + folder.name, 'no complete finest rung'))
    terman = series / 'terman' / 'attempt-001' / 'convergence'
    for folder in sorted(terman.iterdir()) if terman.is_dir() else []:
        base, other = folder / 'xppautx' / 'output.dat', folder / 'xppaut' / 'output.dat'
        if base.exists() and other.exists():
            jobs.append(('terman', folder.name, base, [('xppaut', other)]))
        else:
            skipped.append(('terman/' + folder.name, 'xppautX or XPPAUT output.dat missing'))
    stiff = series / 'large-stiff' / 'attempt-001' / 'results' / 'runs'
    for folder in sorted(stiff.iterdir()) if stiff.is_dir() else []:
        base = folder / '0-klu' / 'trajectory.dat'
        found = [(s, folder / ('0-' + s) / 'trajectory.dat') for s in ('gmres', 'legacy-band', 'legacy-dense')
                 if (folder / ('0-' + s) / 'trajectory.dat').exists()]
        if base.exists() and found:
            jobs.append(('large-stiff', folder.name, base, found))
        else:
            skipped.append(('large-stiff/' + folder.name, 'no klu trajectory or no other solver'))
    return [j for j in jobs if only is None or j[0] in only], skipped


def integration(series, out, manifest, skipped, only, extra=None):
    jobs, notes = integration_jobs(series, only, extra)
    skipped += ['%s: %s' % n for n in notes]
    for study, workload, base, others in jobs:
        if not others:
            entry = dict(study=study, workload='%s/%s' % (study, workload), kind='integration')
            figure = out / 'integration' / study / ('%s__alone.png' % safe(workload))
            plot_alone(entry, base, NO_PEER[study], figure)
            entry['figure'] = str(figure.relative_to(out))
            manifest.append(entry)
            print('integration', entry['workload'], 'alone', flush=True)
        stopped = study == 'python' and (series / 'python/attempt-001/diverged' / (workload + '.json')).exists()
        for program, path in others:
            entry = dict(study=study, workload='%s/%s' % (study, workload), kind='integration')
            figure = out / 'integration' / study / ('%s__%s.png' % (safe(workload), program))
            try:
                plot_integration(entry, base, path, program, figure, stopped, bool(extra) and extra in path.parents)
            except (ValueError, OSError) as e:
                why = 'every program aborts before a second row is stored: the run ends at its first row (t = 0)' if stopped and 'fewer than two rows' in str(e) else e
                skipped.append('%s/%s %s: %s' % (study, workload, program, why))
                continue
            entry['figure'] = str(figure.relative_to(out))
            manifest.append(entry)
            print('integration', entry['workload'], program, entry['max_abs_error'], flush=True)


# ---- continuation ---------------------------------------------------------------------------------------------

def xppautx_branch(rows, two_parameter):
    """(x, y, labelled points) of one run's stored rows: the continued parameter against the norm, or against the second
    parameter for a two-parameter run, the way autocompare reads them; labelled are the special points by their code."""
    keep = [r for r in rows if not two_parameter or 'continued_parameter2' in r]
    x = np.array([float(r['continued_parameter']) for r in keep])
    y = np.array([float(r['continued_parameter2'] if two_parameter else r['norm']) for r in keep])
    marks = []
    for i, r in enumerate(keep):
        code = autocompare.code_of(r['itp'])
        if code and (code != 'EP' or i in (0, len(keep) - 1)):  # an EP is a branch end: the references label the start and the end
            marks.append((code, x[i], y[i]))
    return x, y, marks


def reference_branch(ref_run, two_parameter):
    curve = np.array([c[:3] for c in ref_run['curve']], dtype=float)
    x, y = curve[:, 0], curve[:, 1] if two_parameter else curve[:, 2]
    marks = [(p['type'], float(p['par']), float(p['par2'] if two_parameter and 'par2' in p else p['norm'])) for p in ref_run['points']]
    return x, y, marks


TURN_FRACTION = 1e-3  # a parameter reversal smaller than this fraction of the branch's range is noise, not a turning point


def segments(x):
    """Index ranges [lo, hi] of the branch between its turning points (a fold turns the parameter back): the parameter
    must come back by TURN_FRACTION of its range from the extreme before the extreme counts as a turn."""
    tol = TURN_FRACTION * ((np.nanmax(x) - np.nanmin(x)) or 1.0)
    cuts, direction, extreme = [0], 0, 0
    for i in range(1, len(x)):
        if direction == 0:
            if abs(x[i] - x[0]) > tol:
                direction, extreme = int(np.sign(x[i] - x[0])), i
        elif direction * (x[i] - x[extreme]) > 0:
            extreme = i
        elif direction * (x[extreme] - x[i]) > tol:
            cuts.append(extreme)
            direction, extreme = -direction, i
    cuts.append(len(x) - 1)
    return [(lo, hi) for lo, hi in zip(cuts[:-1], cuts[1:]) if hi > lo]


def branch_error(a, b):
    """The error of branch b against branch a, along the parameter, per segment between turning points: segment i of one is
    matched to segment i of the other, and b is interpolated linearly onto a's parameter values over the range both cover.
    Returns (x, error, error x cos of the branch's angle, notes); the notes say what could not be matched."""
    (xa, ya), (xb, yb) = a, b
    sa, sb = segments(xa), segments(xb)
    notes = []
    if len(sa) != len(sb):
        notes.append('%d turning-point segments in xppautX against %d: the first %d are compared' % (len(sa), len(sb), min(len(sa), len(sb))))
    rx, ry = (np.ptp(xa) or 1.0), (np.ptp(ya) or 1.0)  # the axes' scales: the branch's own range
    xs, es, ns, covered = [], [], [], 0
    for (alo, ahi), (blo, bhi) in zip(sa, sb):
        pa, pb = slice(alo, ahi + 1), slice(blo, bhi + 1)
        xa_, ya_, xb_, yb_ = xa[pa], ya[pa], xb[pb], yb[pb]
        if xa_[0] > xa_[-1]:
            xa_, ya_ = xa_[::-1], ya_[::-1]
        if xb_[0] > xb_[-1]:
            xb_, yb_ = xb_[::-1], yb_[::-1]
        lo, hi = max(xa_[0], xb_[0]), min(xa_[-1], xb_[-1])
        inside = (xa_ >= lo) & (xa_ <= hi)
        if lo > hi or not inside.any():
            continue
        covered += int(inside.sum())
        xs.append(xa_[inside])
        es.append(np.interp(xa_[inside], xb_, yb_) - ya_[inside])
        slope = np.gradient(ya_[inside] / ry, xa_[inside] / rx) if inside.sum() > 1 else np.zeros(1)
        ns.append(es[-1] / np.sqrt(1 + np.nan_to_num(slope, nan=0.0, posinf=1e12, neginf=-1e12) ** 2))
        for part in (xs, es, ns):
            part.append(np.array([np.nan]))  # a gap between segments, so no line joins them
    total = sum(hi - lo + 1 for lo, hi in sa)
    if covered < total:
        notes.append('shared parameter range only: %d of %d xppautX points have the other branch beside them' % (covered, total))
    if not xs:
        return np.array([]), np.array([]), np.array([]), notes + ['no shared range']
    return np.concatenate(xs), np.concatenate(es), np.concatenate(ns), notes


def mark_points(ax, marks):
    for k, (code, px, py) in enumerate(marks):
        ax.plot(px, py, 'o', ms=3.5, mfc='none', color='black', zorder=5)
        if k < LABEL_LIMIT:
            ax.annotate(code, (px, py), textcoords='offset points', xytext=(3, 3), fontsize=6.5)


def plot_continuation(entry, run, program, base, other, out, rerun=False):
    two = run['kind'] == 'two_parameter'
    ylabel = '%s (second parameter)' % run['parameters'][1] if two else 'norm (AUTO L2 norm)'
    xlabel = run['parameters'][0] if two else run['parameter']
    x, e, en, notes = branch_error(base[:2], other[:2])
    finite = np.isfinite(e)
    entry.update(program=program, quantity=('%s against %s' % (run['parameters'][1], run['parameters'][0])) if two else 'norm against %s' % run['parameter'],
                 max_abs_error=float(np.abs(e[finite]).max()) if finite.any() else None,
                 rms_error=float(np.sqrt(np.mean(e[finite] ** 2))) if finite.any() else None,
                 max_abs_error_normal=float(np.abs(en[finite]).max()) if finite.any() else None,
                 matched_points=int(finite.sum()), points=[len(base[0]), len(other[0])], notes=notes, labelled=[len(base[2]), len(other[2])])
    if rerun:
        entry['data_source'] = RERUN_NOTE
        notes = notes + ['data from an ' + RERUN_NOTE]
    fig, (alone, over, err) = plt.subplots(3, 1, figsize=(8, 10))
    alone.plot(other[0], other[1], '-', color=color(program), label=name_of(program))
    mark_points(alone, other[2])
    over.plot(other[0], other[1], '--', color=color(program), lw=1.5, label=name_of(program))
    over.plot(base[0], base[1], '-', color=color('xppautx'), lw=1.0, label='xppautX')
    mark_points(over, base[2])
    for ax in (alone, over):
        ax.set_xlabel(xlabel)
        ax.set_ylabel(ylabel)
        ax.legend(loc='best')
    alone.set_title('%s alone (%d points, %d labelled)' % (name_of(program), len(other[0]), len(other[2])))
    over.set_title('overlaid on xppautX (%d points, %d labelled)' % (len(base[0]), len(base[2])))
    error_panel(err, x, e, '%s (error at xppautX\'s values, other branch interpolated, per segment between turning points)' % xlabel,
                '%s: %s minus xppautX' % (ylabel.split(' (')[0], name_of(program)),
                extra=en if finite.any() and np.abs(en[finite]).max() < .5 * np.abs(e[finite]).max() else None,
                extra_label='x cos(branch angle), axes scaled to the branch (a steep part conditions the vertical error badly)')
    err.set_title('error, max %s, RMS %s (vertical); max %s x cos(branch angle)' % (
        '%.3g' % entry['max_abs_error'] if finite.any() else 'n/a', '%.3g' % entry['rms_error'] if finite.any() else 'n/a',
        '%.3g' % entry['max_abs_error_normal'] if finite.any() else 'n/a'))
    fig.suptitle('%s, run "%s" (%s): %s against xppautX, %s%s' % (entry['workload'], run['name'], run['kind'], name_of(program), entry['quantity'],
                 ('\n' + '; '.join(notes)) if notes else ''), fontsize=8.5)
    finish(fig, out)


def usable(ref):
    return ref is not None and 'unavailable' not in ref and len(ref.get('curve', [])) >= 2


def continuation(attempt, out, manifest, skipped, extra=None):
    for wdir in sorted(p for p in attempt.iterdir() if p.is_dir() and p.name != 'models'):
        for mdir in sorted(p for p in wdir.iterdir() if (p / 'description.json').exists()):
            label = '%s/%s' % (wdir.name, mdir.name)
            desc = json.loads((mdir / 'description.json').read_text())
            ranges_path, diagram = mdir / 'pilot-xppautx' / 'run-ranges.json', mdir / 'pilot-xppautx' / 'diagram.csv'
            if not ranges_path.exists() or not diagram.exists():
                skipped.append('continuation %s: no xppautX diagram or run ranges' % label)
                continue
            stored = autocompare.stored_rows(diagram.read_bytes(), json.loads(ranges_path.read_text()))
            refs = {}
            for program, folder in (('xppaut', 'pilot-xppaut'), ('auto07p', 'pilot-auto07p')):
                path = mdir / folder / 'reference.json'
                if path.exists():
                    refs[program] = {r['name']: r for r in json.loads(path.read_text())['runs']}
                else:
                    skipped.append('continuation %s %s: no reference.json' % (label, program))
            for run in desc['runs']:
                if 'skip' in run:
                    skipped.append('continuation %s run %s: the description skips it (%s)' % (label, run['name'], run['skip']))
                    continue
                two = run['kind'] == 'two_parameter'
                if run['name'] not in stored:
                    skipped.append('continuation %s run %s: xppautX stored no rows' % (label, run['name']))
                    continue
                base = xppautx_branch(stored[run['name']], two)
                if len(base[0]) < 2:
                    skipped.append('continuation %s run %s: fewer than two xppautX points' % (label, run['name']))
                    continue
                for program, runs in refs.items():
                    ref, rerun = runs.get(run['name']), False
                    kept = extra / 'continuation' / label / program / 'reference.json' if extra else None
                    if not usable(ref) and kept and kept.exists():  # no curve in the series: the untimed rerun's
                        ref, rerun = next((r for r in json.loads(kept.read_text())['runs'] if r['name'] == run['name']), None), True
                    if not usable(ref):
                        skipped.append('continuation %s run %s %s: %s' % (label, run['name'], program, (ref or {}).get('unavailable', 'no curve in its reference')))
                        continue
                    entry = dict(study='continuation', workload=label, run=run['name'], kind='continuation')
                    figure = out / 'continuation' / ('%s__%s__%s.png' % (safe(label), safe(run['name']), program))
                    try:
                        plot_continuation(entry, run, program, base, reference_branch(ref, two), figure, rerun)
                    except (ValueError, OSError) as e:
                        skipped.append('continuation %s run %s %s: %s' % (label, run['name'], program, e))
                        continue
                    entry['figure'] = str(figure.relative_to(out))
                    manifest.append(entry)
                    print('continuation', label, run['name'], program, entry['max_abs_error'], flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--series', type=Path, required=True, help='the series folder holding the integration studies (screen, modeldb, ...)')
    p.add_argument('--continuation', type=Path, required=True, help="the continuation attempt folder")
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--extra', type=Path, help="extra_runs.py's folder: untimed reruns that supply the data a program has none of in the series")
    p.add_argument('--only', help='comma-separated integration studies (screen, python, terman, large-stiff, ...) or "continuation"')
    a = p.parse_args()
    only = set(a.only.split(',')) if a.only else None
    extra = a.extra.resolve() if a.extra else None
    manifest, skipped = [], []
    if only is None or only - {'continuation'}:
        integration(a.series.resolve(), a.out.resolve(), manifest, skipped, only, extra)
    if only is None or 'continuation' in only:
        continuation(a.continuation.resolve(), a.out.resolve(), manifest, skipped, extra)
    save(a.out / 'manifest.json', dict(figures=manifest, skipped=skipped))
    print('%d figures, %d skipped' % (len(manifest), len(skipped)))
    for s in skipped:
        print('skipped:', s)


if __name__ == '__main__':
    main()
