#!/bin/sh
# W358 (#420): profile.py over W358's workloads for one build.
#   tools/auto-profile/run_all.sh BINARY OUT [WORKLOAD...]
# Default workloads: py_s1bf (periodic run cut at 100 points), modeldb (periodic cut at $MODELDB_NMAX, default 20: each
# of its points costs 25 times more) and the tests/auto descriptions that continue periodic orbits.
# Each is one valgrind process; run it through tools/heavy.sh (tools/wslrun.sh does).
set -e
binary=$1; out=$2; shift 2
[ $# -gt 0 ] || set -- py_s1bf modeldb ode_lecar tstauto_brper tstauto_frcp tstauto_lamom tstauto_lamom2 tstauto_lecar tstauto_lo tstauto_pp2 tstauto_torx
here=$(dirname "$0")
for w in "$@"; do
  case $w in
    py_s1bf) extra="--nmax 100" ;;
    modeldb) extra="--nmax ${MODELDB_NMAX:-20}" ;;
    *) extra="" ;;
  esac
  python3 "$here/profile.py" "$binary" "$out" "$w" $extra > /dev/null
  echo "profiled $w"
done
