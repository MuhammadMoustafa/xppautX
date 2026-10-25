#!/bin/sh
# W412 (#474): ModelDB's 20-point periodic run (profile.py --tool newton) in the three builds, and
# AUTO-07p's run of it: at the model's tolerances, with EPSL/EPSU/EPSS 1e-8 between two marks (and
# Ntst 200), and with Ds moved by 0.5-1%. In WSL from the checkout's root, through tools/wslrun.sh,
# into a new folder OUT:
#   AUTO_DIR=~/auto-07p-0.9.4 tools/auto-profile/modeldb_newton.sh OUT
# Writes OUT/runs.csv and OUT/histories.json (newton_summary.py), OUT/accuracy.json
# (mesh_accuracy.py) and OUT/auto07p.json (auto07p.py's summaries). Measurements, never baselines.
set -eu
out=${1:?usage: modeldb_newton.sh OUT}
here=tools/auto-profile
mkdir "$out"
make -j4 xppautx
make -j4 LIBM=system BUILDDIR=build/libm xppautx
make -j4 LINALG=lapack BUILDDIR=build/lapack xppautx
marks='[["ampPar",0.5728],["ampPar",0.57284]]' # inside every build's 20-point span, past the searches near the HB
tight='"epsl":1e-8,"epsu":1e-8,"epss":1e-8'
runs='' tight_runs='' auto_runs=''
trace() { # NAME BINARY [SETTINGS]
  uv run --locked python "$here/profile.py" "$2" "$out/$1" modeldb --nmax 20 --tool newton ${3:+--periodic-settings "$3"} \
    > "$out/$1.log" 2>&1
  runs="$runs $out/$1"
  case $1 in *-tight*) tight_runs="$tight_runs $out/$1" ;; esac
}
auto07p() { # NAME [ARGS]
  name=$1
  shift
  uv run --locked python "$here/auto07p.py" "$out/$name" "$@" > "$out/$name.log" 2>&1
  auto_runs="$auto_runs $out/$name"
}
for build in default:xppautX system:build/libm/xppautX lapack:build/lapack/xppautX; do
  name=${build%%:*} binary=${build#*:}
  trace "$name" "$binary"
  trace "$name-tight" "$binary" "{\"numerics\":{$tight},\"marks\":$marks}"
done
trace default-tight-200 xppautX "{\"numerics\":{\"ntst\":200,$tight},\"marks\":$marks}"
auto07p auto07p
auto07p auto07p-tight --marks 0.5728 0.57284 --constants '{"EPSL":1e-8,"EPSU":1e-8,"EPSS":1e-8}'
for ds in 0.0199 0.0201 0.0202; do
  trace "default-ds$ds" xppautX "{\"numerics\":{\"ds\":-$ds}}"
  auto07p "auto07p-ds$ds" --constants "{\"DS\":-$ds}"
done
# the lists hold folder names without spaces, one argument each
# shellcheck disable=SC2086
uv run --locked python "$here/newton_summary.py" --history "$out/histories.json" $runs > "$out/runs.csv"
# shellcheck disable=SC2086
uv run --locked python "$here/mesh_accuracy.py" "$out/accuracy.json" $tight_runs --at 0.5728 0.57284
# shellcheck disable=SC2086
uv run --locked python -c 'import json, sys
json.dump({p.split("/")[-1]: json.load(open(p + "/summary.json")) for p in sys.argv[2:]}, open(sys.argv[1], "w"), indent=1)' \
  "$out/auto07p.json" $auto_runs
