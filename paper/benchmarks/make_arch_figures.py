#!/usr/bin/env -S uv run --locked python
"""Paper figures of the architecture and the native code (W823, #904): paper/figures/.

  architecture      fig:arch, two panels: (a) XPPAUT 8.0's one layer of C files
                    and shared globals, counted from its own source; (b) xppautX's
                    onion rings and front ends, from tools/depgraph.py's MODULES
                    and LEVELS (imported, never copied)
  native-code       the compilation: model -> Generator -> one IR -> interpreter,
                    TinyCC, MIR, and one run's tier timeline (a schematic of the
                    ski-rental rule in core/model_native.cpp, not measured times)

  uv run --locked python paper/benchmarks/make_arch_figures.py [--xppaut DIR]

The XPPAUT source is git-ignored (reference/xppaut-8.0 in the main checkout, read
only); from a worktree the default finds it through git's common directory.
Counts: the C files its Makefile's SOURCES lists (the program), the extern lines
that declare data (no '(' on the line), in every .c and .h of the top folder.
"""
import argparse, importlib.util, re, subprocess, sys, textwrap
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]                       # this checkout (a worktree or the main one)
OUT = HERE.parent / 'figures'
BLUE, ORANGE, AQUA = '#2a78d6', '#eb6834', '#1baf7a'
INK, INK2, MUTED, GRID = '#0b0b0b', '#52514e', '#8a8984', '#e4e3df'
FILL, FILL2 = '#f4f3ef', '#e9f1fb'
plt.rcParams.update({'font.size': 7.5, 'pdf.fonttype': 42, 'text.color': INK,
                     'axes.edgecolor': MUTED, 'axes.labelcolor': INK2})
X11 = re.compile(r'#\s*include\s*[<"](X11/|Xm/|Xlib|Xutil|Xos)')
EXTERN = re.compile(r'^\s*extern\b')


def main_checkout():
    """The main checkout's folder: the parent of git's common directory (the worktree's .git is a file)."""
    try:
        common = subprocess.run(['git', '-C', str(ROOT), 'rev-parse', '--path-format=absolute', '--git-common-dir'],
                                capture_output=True, text=True, check=True).stdout.strip()
        return Path(common).parent
    except (OSError, subprocess.CalledProcessError):
        return ROOT


def xppaut_counts(src):
    """Counts of XPPAUT 8.0's own program (the SOURCES of its Makefile) and its data externs."""
    mk = (src / 'Makefile').read_text(errors='replace')
    block = re.search(r'^SOURCES\s*=(.*?)(?=^\S)', mk, re.S | re.M).group(1)
    names = sorted(set(re.findall(r'([\w.-]+\.c)\b', block.replace('\\\n', ' '))))
    files = [src / n for n in names]
    missing = [f.name for f in files if not f.is_file()]
    if missing:
        raise SystemExit('make_arch_figures: SOURCES names files not in ' + str(src) + ': ' + ', '.join(missing))
    lines = sum(len(f.read_text(errors='replace').splitlines()) for f in files)
    x11 = [f for f in files if X11.search(f.read_text(errors='replace'))]
    headers = sorted(list(src.glob('*.h')) + files)
    ext_data = ext_fn = 0
    for f in headers:
        for ln in f.read_text(errors='replace').splitlines():
            if EXTERN.match(ln):
                if '(' in ln:
                    ext_fn += 1
                else:
                    ext_data += 1
    return dict(c_files=len(files), c_lines=lines, x11=len(x11), x11_names=[f.name for f in x11],
                extern_data=ext_data, extern_fn=ext_fn, files=files)


def depgraph_module():
    spec = importlib.util.spec_from_file_location('depgraph', ROOT / 'tools' / 'depgraph.py')
    d = importlib.util.module_from_spec(spec)
    sys.path.insert(0, str(ROOT / 'tools'))
    spec.loader.exec_module(d)
    return d


def module_sizes(d):
    """Files (.h and .cpp of core/) per module, placed by depgraph's own rule."""
    n = {m: 0 for m in d.MODULES}
    for f in sorted((ROOT / 'core').iterdir()):
        if f.suffix in ('.h', '.cpp'):
            m = d.place('core/' + f.name, d.MODULES)
            if m is not None:
                n[m] += 1
    return n


def save(fig, name):
    for ext in ('pdf', 'png'):
        fig.savefig(OUT / f'{name}.{ext}', dpi=200, bbox_inches='tight')
    print('wrote', OUT / f'{name}.pdf')


def box(ax, x, y, w, h, text, fc=FILL, ec=MUTED, lw=0.8, fs=7, color=INK, bold=False):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle='round,pad=0.0,rounding_size=0.6', fc=fc, ec=ec, lw=lw))
    ax.text(x + w / 2, y + h / 2, text, ha='center', va='center', fontsize=fs, color=color,
            fontweight='bold' if bold else 'normal', linespacing=1.15)


def architecture(xp, d, sizes):
    H = 7.4
    fig = plt.figure(figsize=(6.6, H))
    # ---- (a) XPPAUT 8.0: one layer of C files, the globals every one of them shares
    a = fig.add_axes([0.0, 1 - 1.7 / H, 1.0, 1.7 / H])
    a.set_xlim(0, 100); a.set_ylim(-5, 30); a.axis('off')
    a.text(0, 30, '(a) XPPAUT 8.0: %d C files, %s lines, one layer' % (xp['c_files'], f"{xp['c_lines']:,}"),
           fontsize=8.5, fontweight='bold', va='top')
    a.add_patch(Rectangle((0, 23.4), 2.4, 2.4, fc=ORANGE, ec=MUTED, lw=0.3))
    a.text(3.4, 24.6, 'includes X11 headers (%d files)' % xp['x11'], fontsize=7, va='center', color=INK2)
    a.add_patch(Rectangle((44, 23.4), 2.4, 2.4, fc=FILL2, ec=MUTED, lw=0.3))
    a.text(47.4, 24.6, 'no X11 include (%d files)' % (xp['c_files'] - xp['x11']), fontsize=7, va='center',
           color=INK2)
    cols, pitch_x, pitch_y, cw = 20, 4.8, 4.2, 3.6
    for i, f in enumerate(xp['files']):
        r, c = divmod(i, cols)
        a.add_patch(Rectangle((c * pitch_x, 21.0 - r * pitch_y - cw), cw, cw,
                              fc=ORANGE if f.name in xp['x11_names'] else FILL2, ec=MUTED, lw=0.3))
    a.add_patch(Rectangle((0, -4.2), 100, 3.4, fc=FILL, ec=INK2, lw=0.8, hatch='////'))
    a.text(50, -2.5, '%d global data declarations (extern) in the sources and headers, shared by every file'
           % xp['extern_data'], ha='center', va='center', fontsize=7, color=INK,
           bbox=dict(fc='white', ec='none', pad=1.5))

    # ---- (b) xppautX: the rings of depgraph's LEVELS, each module with its file count
    b = fig.add_axes([0.0, 0.0, 1.0, 5.5 / H]); b.set_xlim(0, 100); b.set_ylim(0, 100); b.axis('off')
    b.text(0, 100, '(b) xppautX: five rings of %d core modules' % len(d.MODULES), fontsize=8.5,
           fontweight='bold', va='top')
    by_ring = {}
    for mod, ring, _why in d.LEVELS:
        by_ring.setdefault(ring, []).append(mod)
    rings = ['outside', 'application', 'domain services', 'domain model', 'kernel']
    band_h, band_gap = 14.4, 2.0
    x0, x1 = 18.5, 77.0
    fx, fw = 81.0, 19.0
    b.text(fx + fw / 2, 96.5, 'front ends', ha='center', va='top', fontsize=7.5, fontweight='bold', color=INK2)
    y = 92.0
    seam_box = None
    for name in rings:
        mods = by_ring.get(name, [])
        b.add_patch(FancyBboxPatch((0.5, y - band_h), x1 - 0.5, band_h,
                                   boxstyle='round,pad=0.0,rounding_size=1.2', fc='#fbfbfa', ec=GRID, lw=0.8))
        b.text(1.8, y - band_h / 2, name.replace(' ', '\n'), fontsize=7.5, fontweight='bold', color=INK2,
               va='center', ha='left')
        n = len(mods)
        bw = (x1 - x0 - 1.0 * (n - 1) - 1.0) / n
        for i, m in enumerate(mods):
            hot = m == 'ui seam'
            bx = x0 + 0.5 + i * (bw + 1.0)
            label = textwrap.fill(m, 11)
            box(b, bx, y - band_h + 2.2, bw, band_h - 4.4, '%s\n%d files' % (label, sizes.get(m, 0)),
                fc='#fde3d6' if hot else FILL2, ec=ORANGE if hot else MUTED, lw=1.8 if hot else 0.6, fs=7.5,
                bold=hot)
            if hot:
                seam_box = (bx + bw, y - band_h / 2)
        y -= band_h + band_gap
    # front ends: each one goes through the seam, drawn as one bus from the column to the seam
    fe = ['desktop window', 'browser', '--server', '--silent']
    fy = 92.0
    centers = []
    for name in fe:
        box(b, fx, fy - 10.0, fw, 10.0, name, fc='white', ec=AQUA, lw=0.9, fs=7.5)
        centers.append(fy - 5.0)
        fy -= 12.6
    bus_x = (x1 + fx) / 2
    for yc in centers:
        b.plot([bus_x, fx], [yc, yc], color=ORANGE, lw=0.9)
    b.plot([bus_x, bus_x, seam_box[0]], [centers[0], seam_box[1], seam_box[1]], color=ORANGE, lw=0.9,
           ls=(0, (3, 2)))
    b.text(fx, 1.0, 'XppUi seam\n(core/xpp_ui.h): every\ncall from the core to\na front end goes through it',
           ha='left', fontsize=6.5, color=INK2, va='bottom')
    save(fig, 'architecture')
    plt.close(fig)


def native_code():
    fig = plt.figure(figsize=(6.6, 4.8))
    # ---- the pipeline: model -> parser -> programs -> Generator -> one IR -> three back ends
    a = fig.add_axes([0, 0.521, 1, 0.479]); a.set_xlim(0, 100); a.set_ylim(0, 104); a.axis("off")
    a.text(0, 104, 'From a model to native code: one Generator, one IR, three back ends', fontsize=8.5,
           fontweight='bold', va='top')
    top = (84, 12)
    xs = [0, 26, 52, 78]
    labels = ['model\n(.ode, .odex)', 'parser\n(model and loading)', 'expression programs\n(RPN, expr_*.cpp)',
              'Generator\ncore/model_native.cpp']
    for i, (x, text) in enumerate(zip(xs, labels)):
        gen = i == 3
        box(a, x, top[0], 22, top[1], text, fc=FILL2 if gen else FILL, ec=BLUE if gen else MUTED,
            lw=1.2 if gen else 0.8, fs=7, bold=gen)
        if i:
            a.annotate('', xy=(x, top[0] + top[1] / 2), xytext=(xs[i - 1] + 22, top[0] + top[1] / 2),
                       arrowprops=dict(arrowstyle='-|>', color=MUTED, lw=0.8))
    box(a, 78, 62, 22, 12, 'one IR\ncore/expr_ir.h', fc=FILL2, ec=BLUE, lw=1.2, fs=7, bold=True)
    a.annotate('', xy=(89, 74), xytext=(89, top[0]), arrowprops=dict(arrowstyle='-|>', color=MUTED, lw=0.8))
    a.plot([89, 89, 15.5], [62, 58.5, 58.5], color=MUTED, lw=0.8)
    backs = [(0, 'interpreter\nexpr_eval.cpp\nthe reference, and the last fallback', FILL, MUTED),
             (34, 'TinyCC\nxpp_tcc.cpp, xpp_tcc_lower.cpp\nlowers to C, compiled in memory at load', FILL2, BLUE),
             (68, 'MIR\nxpp_mir.cpp\ncompiled on a spare processor, or once\nTinyCC\'s work pays for it', FILL2, BLUE)]
    for x, text, fc, ec in backs:
        box(a, x, 31, 31, 21, text, fc=fc, ec=ec, lw=1.0, fs=6.8)
        cx = x + 15.5
        a.annotate('', xy=(cx, 52), xytext=(cx, 58.5), arrowprops=dict(arrowstyle='-|>', color=MUTED, lw=0.8))
    a.text(0, 28, 'The Generator writes no text: every name is an index and every number a double, so the three back\n'
           'ends differ only in how they lower the same IR. TinyCC and MIR are compiled into the program.',
           fontsize=6.8, color=INK2, va='top')

    # ---- one run's tiers, schematic
    fig.text(0.0, 0.531, 'One run: which code runs when', fontsize=8.5, fontweight='bold')
    t = fig.add_axes([0.21, 0.104, 0.76, 0.396]); t.set_xlim(0, 100); t.set_ylim(0, 10.5); t.set_yticks([])
    t.set_xticks([]); [t.spines[s].set_visible(False) for s in ('top', 'right', 'left')]
    t.set_xlabel('time of one run, from load to the end (schematic, not to scale)', fontsize=7, color=INK2)
    for label, y in [('interpreter', 7.2), ('TinyCC', 4.4), ('MIR', 1.6)]:
        t.text(-1.5, y, label, ha='right', va='center', fontsize=7, color=INK2)
    # interpreter: a short bar, then dotted: the label sits right beside the bar
    t.add_patch(Rectangle((0, 6.6), 3, 1.2, fc=MUTED, ec='none'))
    t.plot([3, 100], [7.2, 7.2], color=MUTED, lw=0.8, ls=(0, (1, 2)))
    t.text(4.5, 7.9, 'before TinyCC\'s code is ready; then only the fallback', fontsize=6.5, color=MUTED,
           va='bottom')
    # TinyCC: compiles at load (dark), then runs from the end of its compile
    t.add_patch(Rectangle((0, 3.8), 3, 1.2, fc=BLUE, ec='none'))
    t.text(0, 5.2, 'TinyCC compiles', fontsize=6.5, color=INK2, va='bottom')
    t.add_patch(Rectangle((3, 3.8), 57, 1.2, fc=BLUE, alpha=0.45, ec='none'))
    t.text(31.5, 4.4, 'the run on TinyCC\'s code (from its compile\'s end)', ha='center', va='center',
           fontsize=6.8, color=INK)
    # MIR: its compile starts where TinyCC's compile ends, then it takes over at the switch
    t.add_patch(Rectangle((3, 1.0), 57, 1.2, fc=AQUA, alpha=0.35, ec='none'))
    t.text(31.5, 1.6, 'MIR compiles, starting once TinyCC\'s code is placed', ha='center', va='center',
           fontsize=6.8, color=INK)
    t.add_patch(Rectangle((60, 1.0), 40, 1.2, fc=AQUA, ec='none'))
    t.text(80, 1.6, 'the run on MIR\'s code', ha='center', va='center', fontsize=6.8, color='white')
    t.plot([60, 60], [0.6, 9.6], color=INK, lw=0.9, ls='--')
    t.text(61, 9.6, 'switch: the next right-hand side\nuses MIR\'s code', fontsize=6.5, color=INK2, va='top')
    save(fig, 'native-code')
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--xppaut', type=Path, default=None, help='XPPAUT 8.0 source (default: the main checkout\'s reference/xppaut-8.0)')
    args = ap.parse_args()
    src = args.xppaut or (main_checkout() / 'reference' / 'xppaut-8.0')
    OUT.mkdir(parents=True, exist_ok=True)
    xp = xppaut_counts(src)
    xp['x11_files'] = [f for f in xp['files'] if f.name in xp['x11_names']]
    print('XPPAUT 8.0 (%s): %d C files, %d lines, %d include X11 headers, %d extern data declarations, %d extern functions'
          % (src, xp['c_files'], xp['c_lines'], xp['x11'], xp['extern_data'], xp['extern_fn']))
    d = depgraph_module()
    architecture(xp, d, module_sizes(d))
    native_code()


if __name__ == '__main__':
    main()
