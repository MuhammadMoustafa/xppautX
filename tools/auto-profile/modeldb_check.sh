#!/bin/sh
# W429 (#491), W549 (#614): ModelDB 267710's 20-point periodic run (examples/ode/motoneuron6c.odex) makes no fold
# search and no step back along the branch, in the default, LIBM=system and LAPACK builds. W429 expected one search
# (AUTO-07p's: its LP at the third point), but that point is not a fold: the parameter rises monotonically from the
# Hopf point, (a - HB) / amplitude^2 = 5.15e-4 at points 2, 3 and 4 and 5.34e-4 at point 20 (HB 0.5727814553, the
# model's Jacobian eigenvalues by numpy/scipy), so a search is a spurious one, from the start's second step; W458
# removed it (docs/auto-references.md, "W549"). A build that searches, or goes back, has lost the W458 rule or
# its Hopf location (W412's finding on the Hopf: the start 5e-6 off the Hopf made the searches).
# A step of the checks after a wave of AUTO cards, beside diagram_diff.py with the LAPACK build. In WSL from the
# checkout's root (through tools/wslrun.sh on the Windows laptop), into a new folder OUT (profile.py uses this
# checkout's paper/benchmarks/auto driver helpers, tracked since W543; --paper points elsewhere):
#   tools/wslrun.sh tools/auto-profile/modeldb_check.sh build/modeldb-check
# Writes OUT/runs.csv (newton_summary.py); exits 1 naming the build whose searches or backward_steps are not 0.
set -eu
out=${1:?usage: modeldb_check.sh OUT}
here=tools/auto-profile
mkdir "$out"
make -j4 xppautx
make -j4 LIBM=system BUILDDIR=build/libm xppautx
make -j4 LINALG=lapack BUILDDIR=build/lapack xppautx
runs=''
for build in default:xppautX system:build/libm/xppautX lapack:build/lapack/xppautX; do
  uv run --locked python "$here/profile.py" "${build#*:}" "$out/${build%%:*}" modeldb --nmax 20 --tool newton \
    > "$out/${build%%:*}.log" 2>&1
  runs="$runs $out/${build%%:*}"
done
# the list holds folder names without spaces, one argument each
# shellcheck disable=SC2086
uv run --locked python "$here/newton_summary.py" $runs > "$out/runs.csv"
uv run --locked python -c 'import csv, sys
bad = [r["run"] for r in csv.DictReader(open(sys.argv[1])) if (r["searches"], r["backward_steps"]) != ("0", "0")]
print("modeldb_check: " + ("FAIL, a search or a step back in " + ", ".join(bad) if bad else "no search and no backward step in all three builds"))
sys.exit(1 if bad else 0)' "$out/runs.csv"
