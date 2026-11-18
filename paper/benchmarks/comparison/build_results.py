"""Build the comparison's one folder of record from the per-unit tree W699's conversion left (W807, #888).

    uv run --locked python paper/benchmarks/comparison/build_results.py --from ~/xppautx-comparison/w617-units --to ~/xppautx-comparison/results

`--from` is a results tree in the per-unit layout (`<study>/<unit>/attempt-NNN/`, `done.json`, `extra-runs/`,
`overlays/`) whose entries may be symlinks into other folders. Every file reachable from it, symlinks resolved, is
hard-linked (os.link) into `--to` under the same relative path: real files, one inode each, no space. A symlink is never
copied as a link, a dangling one is an error, and an existing `--to` is refused (never merged into, never overwritten).
The tree is built in a sibling folder and renamed into place at the end, so a failed build leaves no `--to`.
"""
import argparse
import os
import shutil
import sys
from pathlib import Path


def link_tree(source: Path, target: Path, visiting: set[Path]) -> int:
    """Hard-link every file under `source` (directory symlinks followed) into `target`; return the file count."""
    real = source.resolve(strict=True)
    if real in visiting:
        raise ValueError(f'{source}: a directory link loops back to {real}')
    visiting.add(real)
    target.mkdir()
    count = 0
    for entry in sorted(os.scandir(source), key=lambda e: e.name):
        path = Path(entry.path)
        resolved = path.resolve(strict=True)  # a dangling link raises here: an error, never skipped
        if resolved.is_dir():
            count += link_tree(path, target / entry.name, visiting)
        else:
            os.link(resolved, target / entry.name)
            count += 1
    visiting.discard(real)
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--from', dest='source', type=Path, required=True, help='the per-unit results tree to link from')
    parser.add_argument('--to', dest='target', type=Path, required=True, help='the folder of record to make (must not exist)')
    args = parser.parse_args()
    source, target = args.source.expanduser(), args.target.expanduser()
    if target.exists() or target.is_symlink():
        raise SystemExit(f'{target} exists: refusing to overwrite it')
    partial = target.with_name(target.name + '.partial')
    if partial.exists():
        raise SystemExit(f'{partial} exists (a failed build): remove it yourself, then rerun')
    try:
        count = link_tree(source, partial, set())
    except BaseException:
        shutil.rmtree(partial, ignore_errors=True)  # only what this run just made
        raise
    partial.rename(target)
    print(f'{target}: {count} files hard-linked from {source}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
