"""W598 (#664): the speed-up of W596's thread jobs (follow_all_branches) at 1, 2, 4 and 8 threads.

Runs each follow_all_branches workload through tools/autoxcompare.py's compute() at each thread count,
REPEATS times, interleaved (so drift hits every count alike), and writes the raw wall times, each run's point
and CPU times, point and job counts (one job per branch) to OUT/thread_jobs.json and the speed-up, against the ideal
min(threads, jobs) of each model, to OUT/thread_jobs.png. The wall time is only measured, never asserted.
Timed runs need tools/comparison-setup.sh --ready first (--dry skips that and times nothing meaningful).

The workloads are the tests/autox descriptions that set follow_all_branches, and VARIANTS: the comparison's published
models (ModelDB 267710, mu26) as a steady run with follow_all_branches on, long enough to time.

    uv run --locked python tools/autox-profile/thread_jobs.py BINARY OUT [--repeats N] [--only A,B] [--dry]
"""
import argparse, copy, json, resource, statistics, subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import autoxcompare as ac  # noqa: E402

THREADS = (1, 2, 4, 8)
# name: (description, its run, autox settings changed): the comparison's own models with every branch followed
VARIANTS = {
    'mu26_follow_all': ('ode_mu26_control', 'steady', dict(max_steps=300)),
    'motoneuron6c_follow_all': ('ode_motoneuron6c', 'steady', dict(max_steps=600)),
}
MODELS = [p.stem for p in sorted((ROOT / 'tests/autox').glob('*.json')) if '"follow_all_branches": true' in p.read_text()] + list(VARIANTS)


def description(name):
    if name not in VARIANTS:
        return ac.read_description(ac.BASE / (name + '.json'))
    source, run, changes = VARIANTS[name]
    d = ac.read_description(ac.BASE / (source + '.json'))
    d['runs'] = [r for r in d['runs'] if r['name'] == run]
    d['runs'][0]['autox'].update(follow_all_branches=True, **changes)
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('binary')
    ap.add_argument('out')
    ap.add_argument('--repeats', type=int, default=7)
    ap.add_argument('--dry', action='store_true')
    ap.add_argument('--only')
    a = ap.parse_args()
    if not a.dry:
        subprocess.run([str(ROOT / 'tools/comparison-setup.sh'), '--ready'], check=True)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    models = a.only.split(',') if a.only else MODELS
    saved = out / 'thread_jobs.json'
    # a series longer than one command's time adds each call's repetitions to the file
    runs = json.loads(saved.read_text()) if saved.exists() and not a.dry else []
    for rep in range(1 if a.dry else a.repeats):
        for m in models:
            d = description(m)
            for t in THREADS:
                cpu0, began = resource.getrusage(resource.RUSAGE_CHILDREN), time.monotonic()
                data, count, _, _ = ac.compute(a.binary, copy.deepcopy(d), t)
                wall, cpu1 = time.monotonic() - began, resource.getrusage(resource.RUSAGE_CHILDREN)
                runs.append(dict(model=m, threads=t, wall=wall, cpu=cpu1.ru_utime + cpu1.ru_stime - cpu0.ru_utime - cpu0.ru_stime, points=count,
                                 jobs=len({row['ibr'] for row in ac.diagram_rows(data)})))  # one job per branch: the ideal's other bound
    saved.write_text(json.dumps(runs, indent=1))

    def of(m, t, key):
        return [r[key] for r in runs if r['model'] == m and r['threads'] == t]
    for m in [m for m in MODELS if of(m, 1, 'wall')]:
        base = statistics.median(of(m, 1, 'wall'))
        print(m, ' '.join('%d:%.2fs(x%.2f, cpu %.2fs, %d pts, %d jobs)' % (t, statistics.median(of(m, t, 'wall')), base / statistics.median(of(m, t, 'wall')),
                                                                            statistics.median(of(m, t, 'cpu')), of(m, t, 'points')[0], of(m, t, 'jobs')[0]) for t in THREADS))
    if not a.dry:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        fig, ax = plt.subplots()
        for m in [m for m in MODELS if of(m, 1, 'wall')]:
            base = statistics.median(of(m, 1, 'wall'))
            line, = ax.plot(THREADS, [base / statistics.median(of(m, t, 'wall')) for t in THREADS], marker='o', label=m)
            ax.plot(THREADS, [min(t, of(m, 1, 'jobs')[0]) for t in THREADS], '--', color=line.get_color(), alpha=0.5)
        ax.plot([], [], 'k--', alpha=0.5, label='ideal, min(threads, jobs), per model')
        ax.set_xlabel('threads (jobs at once)'); ax.set_ylabel('speed-up over 1'); ax.legend(fontsize=7)
        fig.savefig(out / 'thread_jobs.png', dpi=150)


main()
