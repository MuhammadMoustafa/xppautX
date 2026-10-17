#!/bin/sh
# W429 (#491): ModelDB 267710's 20-point periodic run (examples/ode/motoneuron6c.odex) makes one fold search and no
# step back along the branch, in the default, LIBM=system and LAPACK builds (W412's finding: AUTO-07p makes
# one search; a build that makes more, or goes back, has lost its test histories or its Hopf location).
# A step of the checks after a wave of AUTO cards, beside diagram_diff.py with the LAPACK build. In WSL from the
# checkout's root, through tools/wslrun.sh, into a new folder OUT (profile.py needs paper/benchmarks/auto's driver
# helpers, git-ignored: the maintainer's checkout has them; --paper in profile.py points elsewhere):
#   tools/wslrun.sh tools/auto-profile/modeldb_check.sh build/modeldb-check
# Writes OUT/runs.csv (newton_summary.py); exits 1 naming the build whose searches are not 1 or backward_steps not 0.
set -eu
out=${1:?usage: modeldb_check.sh OUT}
here=tools/auto-profile
mkdir "$out"
make -j4 xppautx
make -j4 LIBM=system BUILDDIR=build/libm xppautx
make -j4 LINALG=lapack BUILDDIR=build/lapack xppautx
runs=''
for build in default:xppautX system:build/libm/xppautX lapack:build/lapack/xppautX; do
  python3 "$here/profile.py" "${build#*:}" "$out/${build%%:*}" modeldb --nmax 20 --tool newton \
    > "$out/${build%%:*}.log" 2>&1
  runs="$runs $out/${build%%:*}"
done
# the list holds folder names without spaces, one argument each
# shellcheck disable=SC2086
python3 "$here/newton_summary.py" $runs > "$out/runs.csv"
python3 -c 'import csv, sys
bad = [r["run"] for r in csv.DictReader(open(sys.argv[1])) if (r["searches"], r["backward_steps"]) != ("1", "0")]
print("modeldb_check: " + ("FAIL, searches not 1 or backward_steps not 0 in " + ", ".join(bad) if bad else "one search and no backward step in all three builds"))
sys.exit(1 if bad else 0)' "$out/runs.csv"
