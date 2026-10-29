#!/usr/bin/env -S uv run --locked python
"""W269 (#326): how many of each model's programs xppautX compiles (W258/W259/W266:
TinyCC at load), why any is not compiled, and whether it has an exact Jacobian
(W260/W275). Not a timing run.

Each model is loaded once with --server --verbose in a copy of its folder; the load's
INFO line "Compiled N of M programs" and the WARN lines of programs left to the
interpreter are read from the log, then the Numerics' Jacobian is set to exact: the
answer is either the INFO line "Compiled the exact Jacobian" or the refusal's reason.

    uv run --locked python compile_counts.py XPPAUTX OUT.json MODEL.odex [MODEL.odex ...]
"""
import json, re, shutil, sys, tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'tools'))
from xppclient import Server, is_done, is_ask
from autocompare import ensure_completed  # noqa: E402

COMPILED = re.compile(r'Compiled (\d+) of (\d+) programs')
JACOBIAN = re.compile(r'Compiled the exact Jacobian: (\d+) values, (\d+) derivatives')
LOAD_LIMIT = 300  # s: a load still going after this has hung (never a speed criterion)


def count(binary, model):
    with tempfile.TemporaryDirectory(prefix='w269-compile-') as tmp:
        # the model's whole folder: a model with #include reads its .inc files
        run = Path(tmp) / 'model'
        shutil.copytree(model.parent, run)
        home = Path(tmp) / 'home'
        (home / 'config').mkdir(parents=True)
        s = Server(binary, str(run / model.name), run=str(run), extra_args=['--verbose'],
                   env={'HOME': str(home), 'XPP_CONFIG_DIR': str(home / 'config'), 'LC_ALL': 'C'})
        try:
            events, end = s.collect(is_done, timeout=LOAD_LIMIT)
            ensure_completed(events, end)
            load_log = list(s.log)
            s.send(cmd='set', kind='num', name='jacobian', text='exact')
            evs, end = s.collect(lambda e: is_done(e) or is_ask(e), timeout=LOAD_LIMIT)
            if end is None:
                raise RuntimeError('Jacobian setting did not complete')
        finally:
            s.close()
    found = [m for line in load_log for m in [COMPILED.search(line)] if m]
    jac = [m for line in s.log for m in [JACOBIAN.search(line)] if m]
    refusals = [e.get('error') or e.get('text') or e for e in evs
                if e.get('ev') in ('session.error', 'session.message') and e.get('kind', 'error') == 'error']
    return dict(model=model.name,
                compiled=int(found[-1].group(1)) if found else None,
                programs=int(found[-1].group(2)) if found else None,
                not_compiled=[line.strip() for line in load_log if 'not compiled' in line],
                exact_jacobian=dict(values=int(jac[-1].group(1)), derivatives=int(jac[-1].group(2))) if jac else None,
                exact_jacobian_refused=refusals,
                load_log=[line.rstrip() for line in load_log])


def main():
    binary, out, models = sys.argv[1], Path(sys.argv[2]), [Path(m).resolve() for m in sys.argv[3:]]
    rows = [count(binary, m) for m in models]
    out.write_text(json.dumps(rows, indent=1) + '\n')
    for r in rows:
        print(f"{r['model']:28} compiled {r['compiled']} of {r['programs']}; "
              f"not compiled: {len(r['not_compiled'])}; exact Jacobian: "
              f"{r['exact_jacobian'] or r['exact_jacobian_refused']}")


if __name__ == '__main__':
    main()
