"""W598 (#664): where collocation threads start to pay, to check AutoxParallel::min_collocation_work.

Times one cycle run (a published model, tests/autox's description, mesh_intervals swept) at 1, 2 and 4 collocation
threads, REPEATS times interleaved, on a build whose min_collocation_work is 0 (so the threads are used at any size:
edit core/autox_parallel.h in a scratch build only). The size is ndim^2 * mesh_intervals * collocation points, the
quantity the program compares with min_collocation_work. Wall times are only measured, never asserted; timed runs need
tools/comparison-setup.sh --ready first.

    uv run --locked python tools/autox-profile/collocation_threads.py BINARY OUT [--repeats N]
"""
import argparse, copy, json, statistics, subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import autoxcompare as ac  # noqa: E402

THREADS = (1, 2, 4)
COLLOCATION_POINTS = 4  # autoX's default when a run does not set collocation_points
POINTS = 30  # steps of the cycle run: enough to time, the same at every size
# (description, run, states): published models; the run's runs before it stay (they find its start)
CASES = (('ode_lecar', 'cycles', 2), ('lrz_cycles', 'cycles_hb1', 3), ('hodgkin_huxley_hopf_pair', 'cycles', 4))
MESHES = (8, 12, 16, 32, 64, 128, 256)


def description(name, run, mesh):
    d = ac.read_description(ac.BASE / (name + '.json'))
    d['runs'] = d['runs'][:[r['name'] for r in d['runs']].index(run) + 1]
    d['runs'][-1]['autox'].update(mesh_intervals=mesh, max_steps=POINTS)
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('binary')
    ap.add_argument('out')
    ap.add_argument('--repeats', type=int, default=5)
    a = ap.parse_args()
    subprocess.run([str(ROOT / 'tools/comparison-setup.sh'), '--ready'], check=True)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    times, failed = {}, set()
    for rep in range(a.repeats):
        for name, run, states in CASES:
            for mesh in MESHES:
                for t in THREADS:
                    d = description(name, run, mesh)
                    began = time.monotonic()
                    try:
                        ac.compute(a.binary, copy.deepcopy(d), t)
                    except RuntimeError as e:  # a mesh too coarse for the cycle: its size is left out, never timed
                        failed.add((name, mesh))
                        print(name, mesh, t, 'left out:', str(e)[:90], flush=True)
                        continue
                    times.setdefault((name, mesh, t), []).append(time.monotonic() - began)
    rows = []
    for name, run, states in CASES:
        for mesh in [m for m in MESHES if (name, m) not in failed]:
            one = statistics.median(times[(name, mesh, 1)])
            row = dict(model=name, work=states * states * mesh * COLLOCATION_POINTS, mesh=mesh, wall={t: statistics.median(times[(name, mesh, t)]) for t in THREADS})
            row['speedup'] = {t: one / row['wall'][t] for t in THREADS}
            rows.append(row)
            print(name, 'work', row['work'], ' '.join('%d:%.3fs(x%.2f)' % (t, row['wall'][t], row['speedup'][t]) for t in THREADS))
    (out / 'collocation_threads.json').write_text(json.dumps(dict(raw={'%s/%d/%d' % k: v for k, v in times.items()}, rows=rows), indent=1))


main()
