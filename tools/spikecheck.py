#!/usr/bin/env python3
"""W416 (#478): the firing behaviour of every examples/ode/izh_*.odex model, from its output data.

Each model is run with `xppautX --silent` in a temporary copy of its file, and the spike times are
read from the reset itself: the model's `ts` (the time of the last reset) at each row where its
spike count `n` grew. A check states what defines the behaviour (one spike, a growing interval, a
burst of several spikes, a spike after the pulse ended ...), never a picture. A model that loses its
behaviour fails; the rows of the output are the model's, so the check never reads a clock.

Usage: tools/spikecheck.py [--server PATH] [--only NAME,...]
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from xppclient import Checker

ROOT = Path(__file__).resolve().parent.parent
FOLDER = ROOT / 'examples/ode'
# a run's safety limit, never a result (these runs take a second)
RUN_SECONDS = 120
# the model's output columns: t, v, u, n (spikes so far), ts (time of the last), I (the input)
T, V, U, N, TS, I = range(6)
# a burst is a group of spikes whose intervals stay below this; a longer interval ends it (ms)
BURST_GAP = 15


def run_model(binary, model):
    """The rows of output.dat of one model's silent run, each a list of numbers."""
    with tempfile.TemporaryDirectory(prefix='xpp-spike-') as folder:
        copy = Path(folder) / model.name
        copy.write_bytes(model.read_bytes())
        done = subprocess.run([binary, copy.name, '--silent'], cwd=folder, capture_output=True, text=True,
                              timeout=RUN_SECONDS)
        output = Path(folder) / 'output.dat'
        if done.returncode != 0 or not output.exists():
            raise ValueError('%s: xppautX --silent exited %d: %s' % (model.name, done.returncode, done.stderr[-300:]))
        return [[float(x) for x in line.split()] for line in output.read_text().splitlines() if line.strip()]


def spike_times(rows):
    """The reset times, from `ts` at each row where `n` grew; None if a row held two resets."""
    times = []
    for before, row in zip(rows, rows[1:]):
        grew = round(row[N] - before[N])
        if grew > 1:
            return None
        if grew == 1:
            times.append(row[TS])
    return times


def intervals(times):
    return [b - a for a, b in zip(times, times[1:])]


def increasing(xs):
    return all(b > a for a, b in zip(xs, xs[1:]))


def decreasing(xs):
    return all(b < a for a, b in zip(xs, xs[1:]))


def steady(xs, tolerance):
    """All of xs within `tolerance` (a fraction) of their mean."""
    mean = sum(xs) / len(xs)
    return all(abs(x - mean) <= tolerance * mean for x in xs)


def bursts(times):
    """The spikes in groups, split where an interval exceeds BURST_GAP."""
    groups = []
    for time in times:
        if groups and time - groups[-1][-1] <= BURST_GAP:
            groups[-1].append(time)
        else:
            groups.append([time])
    return groups


def maxima(rows, start, stop):
    """The values of v at the local maxima of its stored samples between two times."""
    points = [r for r in rows if start <= r[T] <= stop]
    return [points[i][V] for i in range(1, len(points) - 1) if points[i][V] > points[i - 1][V] and points[i][V] >= points[i + 1][V]]


def after_potential(rows):
    """Panel Q: v after the reset rises 5 mV or more above it (the map's rows are its steps)."""
    for index in range(1, len(rows)):
        if round(rows[index][N] - rows[index - 1][N]) == 1:
            return max(r[V] for r in rows[index:index + 20]) >= rows[index][V] + 5
    return False


# Each behaviour: (what it states, a function of (spike times, rows) that is true while the shape holds).
# The times in them are the paper's protocol, the model's own parameters (the pulse's start and width).
BEHAVIOURS = {
    '2004a_tonic_spiking': [
        ('at least 5 spikes, the first after the step at 10', lambda s, r: len(s) >= 5 and s[0] > 10),
        ('the last intervals are equal (regular firing)', lambda s, r: steady(intervals(s)[-2:], 0.02))],
    '2004b_phasic_spiking': [
        ('exactly one spike, after the step at 20', lambda s, r: len(s) == 1 and s[0] > 20)],
    '2004c_tonic_bursting': [
        ('at least 3 bursts of 3 or more spikes, repeating', lambda s, r: len(bursts(s)) >= 3 and all(len(g) >= 3 for g in bursts(s))),
        ('the last bursts start at equal periods', lambda s, r: steady(intervals([g[0] for g in bursts(s)])[-2:], 0.03))],
    '2004d_phasic_bursting': [
        ('one burst of 3 or more spikes, with growing intervals', lambda s, r: len(bursts(s)) == 1 and len(s) >= 3 and increasing(intervals(s))),
        ('then silence for the last 100 ms', lambda s, r: r[-1][T] - s[-1] > 100)],
    '2004e_mixed_mode': [
        ('a burst of 3 spikes, then single spikes at a long, steady interval',
         lambda s, r: len(s) >= 6 and max(intervals(s)[:2]) < 5 and min(intervals(s)[2:]) > 20 and steady(intervals(s)[-3:], 0.05))],
    '2004f_spike_frequency_adaptation': [
        ('at least 5 spikes, every interval longer than the one before', lambda s, r: len(s) >= 5 and increasing(intervals(s))),
        ('the last interval is more than 10 times the first', lambda s, r: intervals(s)[-1] > 10 * intervals(s)[0])],
    '2004g_class1_excitable': [
        ('at least 8 spikes, each interval shorter than the one before', lambda s, r: len(s) >= 8 and decreasing(intervals(s))),
        ('the first interval is 35 ms or longer: firing starts at a low frequency',
         lambda s, r: intervals(s)[0] >= 35 and intervals(s)[0] > 2.5 * intervals(s)[-1])],
    '2004h_class2_excitable': [
        ('at least 10 spikes, each interval shorter than the one before', lambda s, r: len(s) >= 10 and decreasing(intervals(s))),
        ('the first interval is under 25 ms: firing starts at a high frequency', lambda s, r: intervals(s)[0] < 25)],
    '2004i_spike_latency': [
        ('one spike, after the 3 ms pulse (10 to 13) ended', lambda s, r: len(s) == 1 and s[0] > 13)],
    '2004j_subthreshold_oscillations': [
        ('one spike, then no more', lambda s, r: len(s) == 1 and 20 < s[0] < 40),
        ('at least 3 oscillations of v below -55 after it', lambda s, r: len(maxima(r, 50, r[-1][T])) >= 3 and max(maxima(r, 50, r[-1][T])) < -55)],
    '2004k_resonator': [
        ('no spike for the pairs of pulses 20 apart or the first of the pair 40 apart', lambda s, r: all(x > 320 for x in s)),
        ('one spike after the second pulse of the pair 40 apart (320)', lambda s, r: len(s) == 1)],
    '2004l_integrator': [
        ('one spike after the second pulse of the pair 5 apart (ends at 16.1)', lambda s, r: len(s) == 1 and 16.1 < s[0] < 30)],
    '2004m_rebound_spike': [
        ('one spike, after the inhibitory pulse (20 to 25) ended', lambda s, r: len(s) == 1 and s[0] > 25)],
    '2004n_rebound_burst': [
        ('no spike before the inhibitory pulse ended (25)', lambda s, r: all(x > 25 for x in s)),
        ('a burst of at least 5 spikes with growing intervals, then silence',
         lambda s, r: len(bursts(s)) == 1 and len(s) >= 5 and increasing(intervals(s)) and r[-1][T] - s[-1] > 50)],
    '2004o_threshold_variability': [
        ('the pulse at 10 gives no spike, the same pulse at 80 after an inhibitory one gives one', lambda s, r: len(s) == 1 and s[0] > 80)],
    '2004p_bistability': [
        ('no spike before the first pulse (37.5)', lambda s, r: all(x > 37.5 for x in s)),
        ('at least 4 regular spikes after it, from one pulse', lambda s, r: len([x for x in s if x < 207]) >= 4 and steady(intervals([x for x in s if x < 207]), 0.02)),
        ('none after the second pulse (207): it switched the firing off', lambda s, r: all(x < 207 for x in s))],
    '2004q_depolarizing_afterpotential': [
        ('one spike, after the pulse began (9)', lambda s, r: len(s) == 1 and s[0] > 9),
        ('after the reset (v = -60) v rises by 5 mV or more: the after-potential', lambda s, r: after_potential(r))],
    '2004r_accommodation': [
        ('no spike during the slow ramp or the rest (before 300)', lambda s, r: all(x > 300 for x in s)),
        ('one spike during the fast ramp (300 to 312.5)', lambda s, r: len(s) == 1 and s[0] < 330)],
    '2004s_inhibition_induced_spiking': [
        ('no spike before the current is lowered (50), though it is higher then', lambda s, r: all(x > 50 for x in s)),
        ('at least 3 spikes while it is lowered, none after the release has settled (260)', lambda s, r: len([x for x in s if x < 250]) >= 3 and all(x < 260 for x in s))],
    '2004t_inhibition_induced_bursting': [
        ('no spike before the current is lowered (50)', lambda s, r: all(x > 50 for x in s)),
        ('at least 2 bursts of 3 or more spikes, all before the release (250)',
         lambda s, r: len(bursts(s)) >= 2 and all(len(g) >= 3 for g in bursts(s)) and all(x < 250 for x in s))],
    '2003_rs': [
        ('at least 4 spikes, the intervals grow (none shrinks by 1 %) to more than twice the first: adaptation', lambda s, r: len(s) >= 4 and all(b > 0.99 * a for a, b in zip(intervals(s), intervals(s)[1:])) and intervals(s)[-1] > 2 * intervals(s)[0])],
    '2003_ib': [
        ('a burst of 3 spikes, then single spikes at a long, steady interval',
         lambda s, r: len(s) >= 6 and max(intervals(s)[:2]) < 5 and min(intervals(s)[2:]) > 25 and steady(intervals(s)[-3:], 0.05))],
    '2003_ch': [
        ('at least 3 bursts of 5 or more closely spaced spikes, repeating', lambda s, r: len(bursts(s)) >= 3 and all(len(g) >= 5 for g in bursts(s))),
        ('their intervals are under 8 ms', lambda s, r: max(max(intervals(g)) for g in bursts(s)) < 8)],
    '2003_fs': [
        ('at least 15 spikes at a mean interval under 10 ms', lambda s, r: len(s) >= 15 and sum(intervals(s)) / len(intervals(s)) < 10),
        ('the last 10 intervals are equal: no adaptation', lambda s, r: steady(intervals(s)[-10:], 0.01))],
    '2003_lts': [
        ('at least 10 spikes, the first interval under a third of the last', lambda s, r: len(s) >= 10 and intervals(s)[0] < intervals(s)[-1] / 3),
        ('the last intervals are equal: it settles', lambda s, r: steady(intervals(s)[-5:], 0.01))],
    '2003_tc_depolarized': [
        ('at least 30 spikes, regular: neighbouring intervals differ by under 5 %',
         lambda s, r: len(s) >= 30 and all(abs(b / a - 1) < 0.05 for a, b in zip(intervals(s), intervals(s)[1:])))],
    '2003_tc_hyperpolarized': [
        ('no spike while held at -87 mV (before 20)', lambda s, r: all(x > 20 for x in s)),
        ('a burst of at least 4 spikes within 10 ms of the release, with growing intervals',
         lambda s, r: len(s) >= 4 and s[0] < 30 and increasing(intervals(s)[:4]))],
    '2003_rz': [
        ('at least 2 oscillations of v below -55 after the small step (20 to 100), no spike', lambda s, r: len(maxima(r, 20, 100)) >= 2 and max(maxima(r, 20, 100)) < -55 and all(x > 100 for x in s)),
        ('after the brief pulse (100) at least 3 spikes, at equal intervals', lambda s, r: len(s) >= 3 and steady(intervals(s)[-2:], 0.02))],
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', default='./xppautX.exe' if (ROOT / 'xppautX.exe').exists() else './xppautX')
    parser.add_argument('--only', help='comma-separated model names without izh_ and .odex')
    args = parser.parse_args()
    os.chdir(ROOT)
    check = Checker(detail_sep=': ')
    models = sorted(p.name[len('izh_'):-len('.odex')] for p in FOLDER.glob('izh_*.odex'))
    check('every izh_ model has its behaviour checked, and every one checked is a model', models == sorted(BEHAVIOURS),
          'missing %s, extra %s' % (sorted(set(models) - set(BEHAVIOURS)), sorted(set(BEHAVIOURS) - set(models))))
    for name in args.only.split(',') if args.only else models:
        rows = run_model(str(Path(args.server).resolve()), FOLDER / ('izh_' + name + '.odex'))
        spikes = spike_times(rows)
        check(name + ': every reset is told apart (no row holds two)', spikes is not None)
        if spikes is None:
            continue
        for what, test in BEHAVIOURS[name]:
            try:
                ok = bool(test(spikes, rows))
            except (IndexError, ValueError, ZeroDivisionError):
                ok = False
            check('%s: %s' % (name, what), ok, 'spikes at %s' % [round(x, 2) for x in spikes[:12]])
    print('%d checks, %d failed' % (check.checks, check.failures))
    return 1 if check.failures else 0


if __name__ == '__main__':
    sys.exit(main())
