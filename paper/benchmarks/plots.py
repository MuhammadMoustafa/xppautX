#!/usr/bin/env -S uv run --locked python
"""Paper figures from the saved benchmark evidence (no runs): paper/figures/.

  terman-platforms   Terman's episodic.ode: XPPAUT 8.0 built three ways gives
                     three trajectories; xppautX on Linux and Windows gives one
                     (windows-build/terman-platforms, W244 #300)
  auto-speed         AUTO periodic-run times of XPPAUT, xppautX and AUTO-07p, and
                     xppautX's profile (auto/results/wsl, W246 #302)
  size-scaling       integration speed ratio and start-up time against model
                     size (size-scaling/results/<platform>, W241 #297, W242 #298)
"""
import gzip, json, sys
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / 'figures'
# dataviz reference palette (light): categorical slots 1-3, which pass every
# pair for colour-vision deficiency; text and grid tokens
BLUE, ORANGE, AQUA = '#2a78d6', '#eb6834', '#1baf7a'
INK, INK2, MUTED, GRID = '#0b0b0b', '#52514e', '#8a8984', '#e4e3df'
plt.rcParams.update({'font.size': 8, 'axes.edgecolor': MUTED, 'axes.labelcolor': INK2,
                     'xtick.color': INK2, 'ytick.color': INK2, 'axes.spines.top': False,
                     'axes.spines.right': False, 'axes.grid': True, 'grid.color': GRID,
                     'grid.linewidth': .6, 'legend.frameon': False, 'lines.linewidth': 1.2,
                     'pdf.fonttype': 42})
VG1 = 61  # GPe cell 1's voltage: output columns follow the .ode (v, h, n, r, ca, s, then vg)


def load(path):
    with gzip.open(path, 'rt') as f:
        return np.loadtxt(f)


def save(fig, name):
    for ext in ('pdf', 'png'):
        fig.savefig(OUT / f'{name}.{ext}', dpi=200, bbox_inches='tight')
    print('wrote', OUT / f'{name}.pdf')


def terman_platforms():
    R, W = HERE / 'five-model-screen/results', HERE / 'windows-build/terman-platforms'
    xpp = {'Linux (glibc)': load(R / 'episodic-legacy-1.dat.gz'),
           'Windows x64 (UCRT)': load(W / 'episodic-xppaut-win-ucrt64.dat.gz'),
           'Windows 32-bit (distributed)': load(W / 'episodic-xppaut-win-cygwin32.dat.gz')}
    xx = {'Linux': load(R / 'episodic-modern-1.dat.gz'), 'Windows': load(W / 'episodic-xppautx-win-ucrt64.dat.gz')}
    t = xx['Linux'][:, 0]
    assert np.array_equal(xx['Linux'], xx['Windows'])  # the claim the figure draws
    window = (t >= 1592) & (t <= 1596.4)  # the spike where the XPPAUT builds differ most

    fig, (a, b) = plt.subplots(2, 1, figsize=(6.6, 4.8), gridspec_kw=dict(height_ratios=[1.1, 1]))
    for (name, d), c in zip(xpp.items(), [BLUE, ORANGE, AQUA]):
        a.plot(t[window], d[window, VG1], color=c, marker='o', ms=4, label=f'XPPAUT 8.0, {name}')
    a.plot(t[window], xx['Linux'][window, VG1], color=INK, lw=1.4, ls=(0, (1, 1.5)),
           label='xppautX, Linux and Windows\n(identical)')
    a.set_xlabel('time (ms), saved every 0.4 ms')
    a.set_ylabel('GPe cell 1 voltage (mV)')
    a.set_title('A  One spike at 1594 ms: same model, same XPPAUT source, three platforms',
                loc='left', fontsize=8.5, color=INK)
    a.legend(loc='upper left', bbox_to_anchor=(1.01, 1), fontsize=7.5)

    names = list(xpp)
    for (p, q), c in zip([(0, 1), (0, 2), (1, 2)], [BLUE, ORANGE, AQUA]):
        diff = np.abs(xpp[names[p]][:, 1:] - xpp[names[q]][:, 1:]).max(1)
        b.plot(t, np.where(diff > 0, diff, np.nan), color=c, lw=.8,
               label=f'XPPAUT: {names[p].split(" (")[0]} vs {names[q].split(" (")[0]}')
    b.set_yscale('log')
    b.set_ylim(1e-6, 20)
    b.set_xlim(0, t[-1])
    b.set_xlabel('time (ms)')
    b.set_ylabel('largest difference,\nany of the 120 states')
    b.set_title('B  Difference between builds of the same program, whole run (log scale)',
                loc='left', fontsize=8.5, color=INK)
    handles, labels = b.get_legend_handles_labels()
    handles.append(Line2D([], [], color='none'))
    labels.append('xppautX: Linux vs Windows\n0 at all 4,998 saved times\n(nothing to draw on a log scale)')
    b.legend(handles, labels, loc='upper left', bbox_to_anchor=(1.01, 1), fontsize=7.5)
    fig.tight_layout()
    fig.text(0, -.03, 'Terman et al. (2002) STN-GPe network, ModelDB 182758, with its own settings (QualRK, tolerance 1e-3). '
             'Gaps in B: the builds agree exactly there; ~1e-5 is the\nsaved precision (six digits). '
             'The differences are spike-timing shifts of a fraction of a millisecond, large in voltage only on a '
             'spike\'s upstroke.', fontsize=6.5, color=MUTED)
    save(fig, 'terman-platforms')


FAMILY_COLOR = {'kuramoto': BLUE, 'lecdiff': ORANGE, 'amari': AQUA}
FAMILY_LABEL = {'kuramoto': 'Kuramoto (all-to-all sum)', 'lecdiff': 'Morris-Lecar chain',
                'amari': 'Amari field (convolution)'}
PLATFORM_TITLE = {'wsl': 'Linux (WSL), gcc 15.2', 'windows': 'Windows, MSYS2 UCRT gcc 16'}


def size_scaling():
    base = HERE / 'size-scaling/results'
    platforms = [(p, json.loads((base / p / 'summary.json').read_text()))
                 for p in PLATFORM_TITLE if (base / p / 'summary.json').exists()]
    fig, axes = plt.subplots(2, len(platforms), figsize=(3.3 * len(platforms) + 1.6, 4.8),
                             squeeze=False, sharex=True, sharey='row')
    for k, (platform, rows) in enumerate(platforms):
        a, b = axes[0, k], axes[1, k]
        a.axhline(1, color=MUTED, lw=.8)
        for fam, c in FAMILY_COLOR.items():
            rr = sorted((r for r in rows if r['family'] == fam and r.get('complete')), key=lambda r: r['states'])
            if not rr:
                continue
            n = [r['states'] for r in rr]
            a.errorbar(n, [r['integration_ratio'] for r in rr], yerr=[r['integration_ratio_se'] for r in rr],
                       color=c, marker='o', ms=4, capsize=2, label=FAMILY_LABEL[fam])
            b.plot(n, [r['legacy_fixed_s'] * 1e3 for r in rr], color=c, ls=(0, (3, 2)), marker='o', ms=3.5,
                   mfc='white')
            b.plot(n, [r['modern_fixed_s'] * 1e3 for r in rr], color=c, marker='o', ms=3.5)
        a.set_xscale('log')
        a.set_title(PLATFORM_TITLE[platform], loc='left', fontsize=8.5, color=INK)
        a.text(1.0, .98, 'above 1: xppautX faster', transform=a.transAxes, ha='right', va='top',
               fontsize=6.5, color=MUTED)
        b.set_xscale('log')
        b.set_yscale('log')
        b.set_xlabel('model size (state variables)')
    axes[0, 0].set_ylabel('integration speed\n(XPPAUT time / xppautX time)')
    axes[1, 0].set_ylabel('start-up and load (ms)\n(one-step run)')
    axes[0, -1].legend(loc='upper left', bbox_to_anchor=(1.02, 1), fontsize=7.5)
    axes[1, -1].legend([Line2D([], [], color=INK2, marker='o', ms=3.5),
                        Line2D([], [], color=INK2, ls=(0, (3, 2)), marker='o', ms=3.5, mfc='white')],
                       ['xppautX', 'XPPAUT 8.0'], loc='upper left', bbox_to_anchor=(1.02, 1), fontsize=7.5)
    fig.tight_layout()
    fig.text(0, -.02, 'Each point: five fresh processes per run length and program after a warm-up, one pinned CPU. '
             'Integration cost = (mean 2T run - mean T run) / T;\nerror bars one standard error. '
             'The two programs\' outputs are identical at every size.', fontsize=6.5, color=MUTED)
    save(fig, 'size-scaling')


def auto_speed():
    base = HERE / 'auto/results/wsl'
    s = json.loads((base / 'summary.json').read_text())
    rows = [('XPPAUT 8.0', 'xppaut', 'cpu', BLUE), ('xppautX', 'xppautx', 'cpu', ORANGE),
            ('AUTO-07p, 1 thread', 'auto07p-1t', 'cpu', AQUA), ('AUTO-07p, 2 threads', 'auto07p-2t', 'wall', AQUA),
            ('AUTO-07p, 4 threads', 'auto07p-4t', 'wall', AQUA)]
    fig, (a, b) = plt.subplots(1, 2, figsize=(7.4, 2.6), gridspec_kw=dict(width_ratios=[1.6, 1]))
    y = np.arange(len(rows))[::-1]
    for yi, (label, key, kind, c) in zip(y, rows):
        v, se = s[key][f'periodic_{kind}_mean'], s[key].get(f'periodic_{kind}_se', 0)
        a.barh(yi, v, xerr=se, color=c, height=.6, error_kw=dict(ecolor=INK2, capsize=2, lw=.8))
        a.text(v + se + .5, yi, f'{v:.2f} s' + (' (wall)' if kind == 'wall' else ''), va='center', fontsize=7, color=INK2)
    a.set_yticks(y, [r[0] for r in rows])
    a.set_xlabel('periodic run (s): CPU time, or wall time for several threads')
    a.set_xlim(0, 28)
    a.grid(axis='y', visible=False)
    a.set_title('A  Same 336-point periodic branch in each program', loc='left', fontsize=8.5, color=INK)

    parts = [('model right-hand side\n(expression interpreter)', 84, ORANGE), ('block linear algebra\n(brbd, conpar)', 12, BLUE),
             ('rest', 4, MUTED)]
    left = 0
    for label, share, c in parts:
        b.barh(0, share, left=left, color=c, height=.5, edgecolor='white', linewidth=2)
        small = share < 8  # too narrow to centre a label under: put it past the bar's end
        b.text(left + share + 1 if small else left + share / 2, -.42 if not small else 0, f'{share}%',
               ha='left' if small else 'center', va='top' if not small else 'center', fontsize=7, color=INK2)
        left += share
    b.legend(handles=[Line2D([], [], color=c, lw=6) for _, _, c in parts], labels=[p[0] for p in parts],
             loc='upper left', bbox_to_anchor=(0, 1.02), fontsize=6.5, ncol=1)
    b.set_xlim(0, 108)
    b.set_xticks(range(0, 101, 20))
    b.set_ylim(-1, 2.2)
    b.set_yticks([])
    b.set_xlabel("share of xppautX's AUTO instructions (%)")
    b.grid(False)
    b.set_title("B  Where xppautX's AUTO time goes", loc='left', fontsize=8.5, color=INK)
    fig.tight_layout()
    fig.text(0, -.06, 'PY_S1Bf, Hopf at Iapp = 16409 pA continued to 200 pA (Ntst 100, Ncol 4, the same constants in all three). '
             'Five runs after a warm-up, one pinned CPU (AUTO-07p: n CPUs);\nerror bars one standard error. '
             'B: callgrind instruction counts, a shortened run (Nmax 60).', fontsize=6.5, color=MUTED)
    save(fig, 'auto-speed')


if __name__ == '__main__':
    OUT.mkdir(exist_ok=True)
    for name in (sys.argv[1:] or ['terman-platforms', 'size-scaling', 'auto-speed']):
        globals()[name.replace('-', '_')]()
