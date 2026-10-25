"""W412 (#474): one archived revision's build and 20-point ModelDB run.

Run through tools/wslrun.sh, once per revision, never in parallel:
    uv run --locked python tools/auto-profile/revision.py COMMIT OUT [--tool newton|callgrind] [--own-tools]
The revision is archived into a temporary folder (no checkout or worktree) and
built there. The driver is this checkout's profile.py, or with --own-tools the
revision's own (W358's callgrind table was made by its commit's tools).
"""
import argparse, io, os, subprocess, sys, tarfile, tempfile
from pathlib import Path

here = Path(__file__).resolve().parent
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('commit')
ap.add_argument('out', type=Path)
ap.add_argument('--tool', choices=('newton', 'callgrind'), default='newton')
ap.add_argument('--own-tools', action='store_true', help="the revision's own tools/auto-profile/profile.py")
args = ap.parse_args()
commit = subprocess.check_output(['git', 'rev-parse', args.commit], text=True).strip()
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='w412-' + commit[:8] + '-') as name:
    tree = Path(name)
    with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(['git', 'archive', commit]))) as tar:
        tar.extractall(tree, filter='data')
    # ccache's own settings, since an older Makefile does not set them
    env = {**os.environ, 'CCACHE_BASEDIR': str(tree), 'CCACHE_NOHASHDIR': '1', 'CCACHE_MAXSIZE': '15G'}
    with (out / 'build.log').open('w') as log:
        subprocess.run(['make', '-j4', 'xppautx', 'WERROR=1', 'CC=ccache gcc', 'CXX=ccache g++', 'CCACHE='],
                       cwd=tree, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    driver = (tree / 'tools/auto-profile' if args.own_tools else here) / 'profile.py'
    with (out / 'profile.log').open('w') as log:
        subprocess.run([sys.executable, str(driver), str(tree / 'xppautX'), str(out), 'modeldb', '--nmax', '20',
                        '--tool', args.tool], stdout=log, stderr=subprocess.STDOUT, check=True)
    print(commit, 'complete', flush=True)
