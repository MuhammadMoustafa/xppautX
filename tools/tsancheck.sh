#!/bin/sh
# Build xppautX with ThreadSanitizer (make tsan, into build/tsan) and run the
# protocol (servercheck.py) and browser mode (webcheck.py) checks under it:
# the reader threads (core/xpp_http.cpp, the --server stdin reader) must touch
# only the inbox (core/xpp_inbox.cpp) and xpp_job's atomics; then the AUTO
# unit test (tests/test_session_auto.cpp), whose periodic runs on 1, 2 and 4
# collocation workers (core/auto_parallel.cpp, W247) must touch only their own
# AutoLib and disjoint output blocks.
#
# Any ThreadSanitizer report fails the script, whether or not the check that
# provoked it noticed: every report goes to a file in build/tsan/reports
# (log_path), and that directory must end up empty. tools/tsan.supp lists the
# only races tolerated, all in code we do not own.
#
# Slow (minutes), so not part of verify.sh or CI yet. Linux only (gcc's or
# clang's ThreadSanitizer).
# Usage: tools/tsancheck.sh [--builddir DIR] [--skip-build] [VAR=value ...]
#   $MAKE, $PYTHON  the make and python programs (default make, python3)
cd "$(dirname "$0")/.." || exit 1
top=$PWD
bdir=build/tsan
skip_build=0
while [ $# -gt 0 ]; do
  case $1 in
    --builddir) bdir=$2; shift ;;
    --skip-build) skip_build=1 ;;
    *=*) break ;;
    *) echo "usage: tools/tsancheck.sh [--builddir DIR] [--skip-build] [VAR=value ...]"; exit 2 ;;
  esac
  shift
done
make=${MAKE:-make}
python=${PYTHON:-python3}
log=build/$(basename "$bdir")
mkdir -p build || exit 1
if [ $skip_build -eq 1 ]; then
  echo "tsan build skipped (--skip-build): using $bdir as-is"
else
  if ! "$make" -j4 BUILDDIR="$bdir" TSAN=1 "$@" asan-link "$bdir/tests/test_session_auto" > "$log-build.log" 2>&1; then
    grep -E ' error:' "$log-build.log" | head -20
    echo "TSAN BUILD FAILED"
    exit 1
  fi
  echo "tsan build ok"
fi
bin=$bdir/xppautX
reports=$top/$bdir/reports
rm -rf "$reports" && mkdir -p "$reports" || exit 1
export TSAN_OPTIONS="log_path=$reports/tsan:suppressions=$top/tools/tsan.supp:print_suppressions=0:halt_on_error=0:second_deadlock_stack=1"
fail=0
run_check() {
  name=$1; shift
  t0=$(date +%s)
  if XPP_CHECK_SLOW=${XPP_CHECK_SLOW:-2} "$@" > "$log-$name.log" 2>&1; then
    # the python checks print a PASS line per check, a unit test its count
    n=$(grep -c '^PASS' "$log-$name.log")
    [ "$n" -eq 0 ] && n=$(sed -n 's/.* \([0-9][0-9]*\) checks, 0 failed$/\1/p' "$log-$name.log" | tail -1)
    echo "$name ok: $n checks" > "$log-$name.verdict"
  else
    { grep -v '^PASS' "$log-$name.log" | head -30; echo "$name FAILED"; } > "$log-$name.verdict"
  fi
  echo "$name finished after $(( $(date +%s) - t0 ))s"
}
run_check servercheck "$python" tools/servercheck.py --server "$bin" &
run_check webcheck "$python" tools/webcheck.py --bin "$bin" &
wait
run_check auto_threads "$bdir/tests/test_session_auto"
for name in servercheck webcheck auto_threads; do
  cat "$log-$name.verdict"
  grep -q ' FAILED$' "$log-$name.verdict" && fail=1
done
n=$(ls "$reports" | wc -l)
if [ "$n" -ne 0 ]; then
  for f in "$reports"/*; do
    echo "== $f"
    head -60 "$f"
  done
  echo "TSAN REPORTS: $n (in $reports)"
  fail=1
else
  echo "tsan ok: no report"
fi
if [ $fail -ne 0 ]; then
  echo "TSAN CHECK FAILED"
  exit 1
fi
echo "tsan check ok"
