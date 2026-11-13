#!/usr/bin/env python3
"""W358 (#420): the tables of docs/studies/w358-auto-profile.md from tools/autox-profile/profile.py's
callgrind files (written with --compress-strings=no --compress-pos=no).

    uv run --locked python tools/autox-profile/tables.py OUT [--top N | --summary | --compare OUT2]

Per workload and phase (OUT/<workload>.<phase>.cg, points from the matching .json): the total
instructions per stored point; the instructions by kind of routine (self cost, so each
instruction is counted once); the routines' call counts per point (Newton iterations,
Jacobian builds, right-hand-side calls); the top routines. --compare prints the totals of a second
set of files (another build of the same workloads) beside the first, per phase.
"""
import argparse, collections, json, re
from pathlib import Path

ANON = r'(\(anonymous namespace\)::)?'
# kind of routine, by (object, function name); the first rule that matches decides
KINDS = [
    ('math functions (xpp::math: CORE-MATH, or the C library under LIBM=system)',
     lambda o, f: f.startswith('cr_') or 'libm.so' in o),
    ('model right-hand side (compiled code, interpreter)',
     lambda o, f: (o == '???' and f.startswith('0x')) or f.startswith(('pure_rhs', 'xpp::evaluate', 'my_rhs'))),
    ('condense_parameters (condensation of parameters)', lambda o, f: f.startswith('(anonymous namespace)::conpar_process')),
    ('Jacobian and residual assembly (setup_boundary_system, build_right_hand_side, icps, fnps, problem_function, user_right_hand_side)',
     lambda o, f: re.match(ANON + r'(setup_boundary_system|build_right_hand_side|icps|fnps|problem_function|user_right_hand_side|continued_constants|user_start|bcps|icf)', f)),
    ('block linear solve (reduce_condensation, determine_local_variables, condense_right_hand_side, back_substitute, solve_end_system, solve_bordered_block_diagonal, gauss_eliminate, matrix_null_vector)',
     lambda o, f: re.match(r'(reduce_condensation|determine_local_variables|condense_right_hand_side|back_substitute|solve_end_system|rhsrge|copy_right_hand_side|solve_bordered_block_diagonal|gauss_eliminate|matrix_null_vector|copy_condensed_system|solve_boundary)\(', f.removeprefix('xpp::'))),
    ('eigenvalue routines (EISPACK, floquet_multipliers)',
     lambda o, f: re.match(r'(hessenberg_qr_values|hessenberg_qr_vectors|hessenberg_reduce_householder|balance_matrix|balance_back_transform|hessenberg_reduce_elimination|hessenberg_transform_elimination|qz_iterate|qz_reduce_hessenberg|qz_eigenvalues|qzvec|rg|rgg|eig|floquet_multipliers|cdiv|csroot|hessenberg_transform_orthogonal|comqr|cbal|corth|comqr2|htribk|ch|cg)\(', f.removeprefix('xpp::'))),
    ('allocation and zeroing (malloc, free, new, memset, memcpy)',
     lambda o, f: re.match(r'(malloc|free|calloc|realloc|_int_|operator new|operator delete|__memset|__memcpy|__memmove|memset|memcpy|memmove|cfree|__libc_|unlink_chunk|tcache)', f)),
    ('output, files, session (formatting, compression)',
     lambda o, f: re.match(r'(tdefl|tinfl|std::__format|std::format|xpp::format|xpp::print|xpp::json|xpp::Writer|xpp::log|fwrite|fputs|vfprintf|__vfprintf|_IO_)', f)),
]

COUNTS = [  # what a point costs in routines: the callee's name prefix
    ('solve_boundary (Newton iterations of a BVP/periodic point)', 'solve_boundary('),
    ('setup_boundary_system (Jacobian builds)', 'setup_boundary_system('),
    ('build_right_hand_side (residual only, modified Newton)', 'build_right_hand_side('),
    ('problem_function (right-hand side with or without derivatives)', 'problem_function('),
    ('user_right_hand_side (the model, one evaluation)', 'user_right_hand_side('),
    ('gauss_eliminate (dense solves)', 'gauss_eliminate('),
    ('floquet_multipliers (multipliers)', 'floquet_multipliers('),
    ('solve_algebraic (Newton iterations of an equilibrium point)', 'solve_algebraic('),
    ('malloc', 'malloc'),
    ('operator new', 'operator new(unsigned long)'),
]


def parse(path):
    """{'self': {fn: Ir}, 'calls': {callee: N}, 'obj': {fn: object}, 'total': Ir}"""
    self_cost = collections.Counter()
    calls = collections.Counter()
    pairs = collections.Counter()  # (caller, callee): calls
    callee_cost = collections.Counter()  # fn: inclusive cost of the calls it makes
    obj = {}
    fn = ob = cob = cfn = None
    pending = None
    total = 0
    for line in open(path, errors='replace'):
        line = line.rstrip('\n')
        if line.startswith(('summary:', 'totals:')):
            total = int(line.split()[1])
        elif line.startswith('ob='):
            ob = line[3:]
        elif line.startswith('cob='):
            cob = line[4:]
        elif line.startswith('fn='):
            fn = line[3:]
            obj[fn] = ob
        elif line.startswith('cfn='):
            cfn = line[4:]
            obj.setdefault(cfn, cob or ob)
        elif line.startswith('calls='):
            pending = int(line[6:].split()[0])
            calls[cfn] += pending
            pairs[(fn, cfn)] += pending
        elif line and (line[0].isdigit() or line[0] in '+-*'):
            if pending is not None:  # the line after calls= is the call's inclusive cost, not fn's own
                callee_cost[fn] += int(line.split()[-1])
                pending = None
            elif fn is not None:
                self_cost[fn] += int(line.split()[-1])
    return {'self': self_cost, 'calls': calls, 'pairs': pairs, 'callee_cost': callee_cost, 'obj': obj, 'total': total}


def short(name):
    if name.startswith('(anonymous namespace)::'):
        return '(anonymous namespace)::' + name.split('::')[1].split('(')[0]
    return name.split('(')[0]


def kind_of(profile, fn):
    o = profile['obj'].get(fn) or ''
    for label, test in KINDS:
        if test(o, fn):
            return label
    return 'the rest'


def points(out, workload, phase):
    p = json.loads((out / f'{workload}.{phase}.json').read_text())['points']
    return p if isinstance(p, int) else len(p)


def calls_of(profile, prefix):
    return sum(n for f, n in profile['calls'].items() if f.removeprefix('xpp::').startswith(prefix))


INCLUSIVE = [  # the routines whose whole cost (with everything they call) a stage 3 card can target
    'solve_boundary(', 'setup_boundary_system(', '(anonymous namespace)::setubv_make_aa_bb', '(anonymous namespace)::setubv_make_fa',
    'build_right_hand_side(', '(anonymous namespace)::conpar_process', 'reduce_condensation(', 'step_boundary(', 'problem_function(', 'user_right_hand_side(', 'gauss_eliminate(', 'floquet_multipliers(',
    'output_parameters_boundary(', 'solve_bordered_block_diagonal(',
]
CALLEES = ['problem_function(', 'user_right_hand_side(', 'gauss_eliminate(', 'malloc', 'memset', '__memset_avx2_unaligned_erms']  # whose callers are listed


def inclusive(prof, prefix):
    return sum(prof['self'][f] + prof['callee_cost'][f] for f in set(prof['self']) | set(prof['callee_cost'])
               if f.removeprefix('xpp::').startswith(prefix))


def callers(prof, callee, k=6):
    c = collections.Counter()
    for (a, b), n in prof['pairs'].items():
        if b.removeprefix('xpp::').startswith(callee):
            c[short(a)] += n
    return c.most_common(k)


def report(out, top):
    lines = []
    for cg in sorted(out.glob('*.cg')):
        workload, phase = cg.name[:-3].rsplit('.', 1)
        if phase == 'setup':
            continue
        n = points(out, workload, phase)
        prof = parse(cg)
        total = prof['total'] or sum(prof['self'].values())
        lines.append(f'\n#### {workload}, {phase}: {n} points, {total / 1e9:.3f} G instructions, '
                     f'{total / n / 1e6:.3f} M per point\n')
        by_kind = collections.Counter()
        for fn, c in prof['self'].items():
            by_kind[kind_of(prof, fn)] += c
        lines.append('| kind of routine | M instr per point | share |\n|---|---:|---:|')
        for k, c in by_kind.most_common():
            lines.append(f'| {k} | {c / n / 1e6:.3f} | {100 * c / total:.1f}% |')
        lines.append('\n| call counts | per point |\n|---|---:|')
        for label, pre in COUNTS:
            lines.append(f'| {label} | {calls_of(prof, pre) / n:.2f} |')
        lines.append('\n| inclusive (with callees) | M instr per point | share |\n|---|---:|---:|')
        for pre in INCLUSIVE:
            c = inclusive(prof, pre)
            if c:
                lines.append(f'| `{short(pre)}` | {c / n / 1e6:.3f} | {100 * c / total:.1f}% |')
        lines.append('\n| callee | callers: calls per point |\n|---|---|')
        for pre in CALLEES:
            lines.append(f'| `{short(pre)}` | ' + ', '.join(f'{a} {c / n:.1f}' for a, c in callers(prof, pre)) + ' |')
        lines.append('\n| top routines (self) | M instr per point | share |\n|---|---:|---:|')
        for fn, c in prof['self'].most_common(top):
            lines.append(f'| `{short(fn)[:70]}` | {c / n / 1e6:.3f} | {100 * c / total:.1f}% |')
    return '\n'.join(lines)


def per_point(out, cg):
    """(instructions per stored point, the profile) of a phase's file"""
    workload, phase = cg.name[:-3].rsplit('.', 1)
    prof = parse(cg)
    return (prof['total'] or sum(prof['self'].values())) / points(out, workload, phase), prof


def math_per_point(out, cg, prof):
    workload, phase = cg.name[:-3].rsplit('.', 1)
    return sum(c for f, c in prof['self'].items() if kind_of(prof, f) == KINDS[0][0]) / points(out, workload, phase)


def compare(a, b):
    """the same workloads under two builds: instructions per stored point (the points of the two may
    differ in number and place: it is the cost of a point that is compared), and the math functions'"""
    lines = ['| workload, phase | points this / other | M instr per point, this | other | other / this | math functions, M per point: this | other |',
             '|---|---|---:|---:|---:|---:|---:|']
    for cg in sorted(a.glob('*.cg')):
        o = b / cg.name
        if not o.exists() or cg.name.endswith('.setup.cg'):
            continue
        (x, px), (y, py) = per_point(a, cg), per_point(b, o)
        workload, phase = cg.name[:-3].rsplit('.', 1)
        lines.append(f'| {workload}, {phase} | {points(a, workload, phase)} / {points(b, workload, phase)} | {x / 1e6:.3f} | '
                     f'{y / 1e6:.3f} | {y / x:.3f} | {math_per_point(a, cg, px) / 1e6:.3f} | {math_per_point(b, o, py) / 1e6:.3f} |')
    return '\n'.join(lines)


SUMMARY_KINDS = ['math', 'model right-hand side', 'condense_parameters', 'Jacobian and residual', 'block linear', 'eigenvalue', 'allocation']


def summary(out):
    """one row per workload and phase: points, instructions per point, the share of each kind of
    routine, and the routines called per point"""
    lines = ['| workload, phase | points | M instr per point | ' + ' | '.join(SUMMARY_KINDS) +
             ' | solve_boundary | setup_boundary_system | build_right_hand_side | problem_function | func | malloc |', '|---|---:|---:|' + '---:|' * (len(SUMMARY_KINDS) + 6)]
    for cg in sorted(out.glob('*.cg')):
        workload, phase = cg.name[:-3].rsplit('.', 1)
        if phase == 'setup':
            continue
        n = points(out, workload, phase)
        prof = parse(cg)
        total = prof['total'] or sum(prof['self'].values())
        share = collections.Counter()
        for fn, c in prof['self'].items():
            share[kind_of(prof, fn)] += c
        cells = [f"{100 * sum(c for k, c in share.items() if k.startswith(key)) / total:.0f}%" for key in SUMMARY_KINDS]
        counts = [f'{calls_of(prof, pre) / n:.1f}' for pre in ('solve_boundary(', 'setup_boundary_system(', 'build_right_hand_side(', 'problem_function(', 'user_right_hand_side(', 'malloc')]
        lines.append(f'| {workload}, {phase} | {n} | {total / n / 1e6:.2f} | ' + ' | '.join(cells) + ' | ' + ' | '.join(counts) + ' |')
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('out')
    ap.add_argument('--top', type=int, default=14)
    ap.add_argument('--compare')
    ap.add_argument('--summary', action='store_true')
    args = ap.parse_args()
    out = Path(args.out)
    print(compare(out, Path(args.compare)) if args.compare else summary(out) if args.summary else report(out, args.top))


if __name__ == '__main__':
    main()
