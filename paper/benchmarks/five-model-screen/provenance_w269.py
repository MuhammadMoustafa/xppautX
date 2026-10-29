#!/usr/bin/env -S uv run --locked python
"""W269 (#326): the machine, compilers and programs a W269 result folder was measured with,
written as provenance.json into each folder named (Linux/WSL: the builds of build.py
--provenance provenance-w269.json; Windows: the binaries named with --legacy/--modern).

    uv run --locked python provenance_w269.py --legacy XPPAUT --modern XPPAUTX [--auto07p DIR] FOLDER...
"""
import argparse, hashlib, json, os, platform, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent


def out(*cmd):
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        return (p.stdout + p.stderr).strip()
    except (OSError, subprocess.TimeoutExpired) as e:
        return f'unavailable: {e}'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--legacy', required=True)
    ap.add_argument('--modern', required=True)
    ap.add_argument('--auto07p', help='the AUTO-07p build folder (AUTO_DIR)')
    ap.add_argument('folders', nargs='+', type=Path)
    a = ap.parse_args()
    pinned = json.loads((HERE / 'provenance-w269.json').read_text())
    record = dict(
        card='W269 #326', written_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
        legacy_commit=pinned['legacy_commit'], modern_commit=pinned['modern_commit'],
        build_commands=pinned['build_commands'] if os.name != 'nt' else
        dict(legacy='paper/benchmarks/windows-build/build.sh (W244), the binary W242 used',
             modern='mingw32-make -j4 xppautx BUILDDIR=build/ucrt WERROR=1 XPPAUTX_VERSION=paper-w269 (OPT -g -O2)'),
        platform=platform.platform(), python=sys.version,
        uname=out('uname', '-a') if os.name != 'nt' else platform.uname()._asdict(),
        compiler=out('gcc', '--version').splitlines()[0],
        cpu=out('lscpu') if os.name != 'nt' else platform.processor(),
        binaries={k: dict(path=str(Path(v).resolve()), sha256=sha(v),
                          version=out(v, '-version' if k == 'legacy' else '--version'))
                  for k, v in (('legacy', a.legacy), ('modern', a.modern))})
    if a.auto07p:
        record['auto07p'] = dict(dir=a.auto07p, version=next((l for l in open(Path(a.auto07p) / 'CHANGELOG')
                                                              if l.strip()), '').strip(),
                                 configure=out('head', '-n', '8', str(Path(a.auto07p) / 'config.log')),
                                 gfortran=out('gfortran', '--version').splitlines()[0])
    for folder in a.folders:
        target = folder / 'provenance.json'
        if target.exists():
            raise FileExistsError(target)
        target.write_text(json.dumps(record, indent=1) + '\n')
        print('wrote', target)


if __name__ == '__main__':
    main()
