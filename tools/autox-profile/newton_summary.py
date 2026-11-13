"""W412 (#474): what profile.py --tool newton recorded, run by run.

    uv run --locked python tools/autox-profile/newton_summary.py [--history OUT.json] DIR...
Prints one CSV row per DIR (named by the folder): its stored points, BVP solves,
Newton corrections (step_boundary's solves) and Jacobian builds (setup_boundary_system), fold-direction
solves (fold_test_boundary's), the Newton corrections of regular steps and of special-point
searches (locate_special_boundary's), the searches and the tests that started them (the last test
evaluated before a search's first solve; empty for a trace without the tests' events),
the steps that went back along the branch (the period falls; it rises along this
branch), its first and last parameter and period,
and a hash of its stored meshes. --history also writes every periodic event, the
meshes replaced by their SHA256, to OUT.json.
"""
import argparse, collections, csv, hashlib, json, sys
from pathlib import Path

POINTS = 20  # profile.py's ModelDB cut (run_all.sh's default)
TESTS = ('uz', 'fold', 'bp', 'sp')  # newton.py's events for user_zero_test_boundary, fold_test_boundary, branch_point_test_boundary, secondary_bifurcation_test_boundary


def summary(folder):
    rows = [json.loads(line) for line in (folder / 'modeldb.newton.jsonl').read_text().splitlines()]
    rows = rows[next(i for i, row in enumerate(rows) if row['event'] == 'store_diagram_boundary'):]  # the periodic run
    solves = [row for row in rows if row['event'] == 'solve_boundary']
    counts = collections.Counter(row['event'] for row in rows)
    if counts['store_diagram_boundary'] != POINTS or counts['setup_boundary_system'] != sum(row['ifst'] == 1 for row in solves):
        raise ValueError(f'{folder}: not a {POINTS}-point trace, or a Jacobian build without its solve')
    meshes, stored, back, searching, test = hashlib.sha256(), [], 0, False, None
    started = collections.Counter()
    for row in rows:
        if row['event'] == 'store_diagram_boundary':
            mesh = json.dumps([row.pop('tm'), row.pop('ups')]).encode()
            meshes.update(mesh)
            row['mesh_sha256'] = hashlib.sha256(mesh).hexdigest()
            back += bool(stored) and row['period'] < stored[-1]['period']
            stored.append(row)
            searching = False
        elif row['event'] in TESTS:
            test = row['event']
        elif row['event'] == 'solve_boundary' and row['caller'] == 'step_boundary':
            if row['parent'] == 'locate_special_boundary' and not searching:
                started[test] += 1
            searching = row['parent'] == 'locate_special_boundary'
    newton = [row for row in solves if row['caller'] == 'step_boundary']
    located = sum(row['parent'] == 'locate_special_boundary' for row in newton)
    return {'run': folder.name, 'points': POINTS, 'solves': len(solves), 'newton': len(newton),
            'jacobians': counts['setup_boundary_system'], 'fold_solves': len(solves) - len(newton),
            'regular_newton': len(newton) - located, 'search_newton': located, 'searches': started.total(),
            'started_by': ' '.join(f'{t}:{started[t]}' for t in TESTS if started[t]) if counts['bp'] else '',
            'backward_steps': back, 'first_parameter': stored[0]['parameter'],
            'last_parameter': stored[-1]['parameter'], 'first_period': stored[0]['period'],
            'last_period': stored[-1]['period'], 'meshes_sha256': meshes.hexdigest()}, rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--history', type=Path)
    ap.add_argument('folders', nargs='+', type=Path)
    args = ap.parse_args()
    histories, out = {}, None
    for folder in args.folders:
        counts, rows = summary(folder)
        if out is None:
            out = csv.DictWriter(sys.stdout, list(counts), lineterminator='\n')
            out.writeheader()
        out.writerow(counts)
        histories[folder.name] = {**counts, 'history': rows}
    if args.history:
        args.history.write_text(json.dumps(histories, indent=1) + '\n')


if __name__ == '__main__':
    main()
