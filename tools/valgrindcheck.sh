#!/bin/sh
# Run the checks under valgrind's memcheck, for what the sanitizers cannot
# see: a read of memory never written (ASan does not track that, and MSan
# needs every library rebuilt). Builds xppautX at -O1 without sanitizers
# (make vg, into build/vg), then runs the -silent smoke run (same checksum
# as verify.sh), every example, the unit tests, the protocol
# (servercheck.py) and AUTO (autocheck.py) under it.
#
# Any memcheck error fails the script, whether or not the check that
# provoked it noticed: each process writes its report to its own file in
# build/vg/reports, and those files must all end up empty. Leaks are
# LeakSanitizer's (tools/asancheck.sh), so memcheck does not look for them.
# tools/valgrind.supp lists the only errors tolerated, all in code we do
# not own.
#
# Slow (memcheck runs the programs 20-50 times slower), so not part of
# verify.sh. Linux only; valgrind is `apt install valgrind`.
# The checks run side by side after the examples: servercheck and each of
# autocheck's sections in a process of its own. About 25 minutes on 32
# threads; --origins (memcheck's --track-origins, about twice as slow)
# also says where an uninitialised value came from, for a report to fix.
# Usage: tools/valgrindcheck.sh [--origins]
cd "$(dirname "$0")/.." || exit 1
origins=
case $1 in
  --origins) origins=--track-origins=yes ;;
  "") ;;
  *) echo "usage: tools/valgrindcheck.sh [--origins]"; exit 2 ;;
esac
top=$PWD
BASELINE=c281851de59ffd03b2a46428619a0c8f
if ! command -v valgrind > /dev/null; then
  echo "valgrind not found (sudo apt-get install valgrind)"
  exit 1
fi
mkdir -p build || exit 1
if ! make -j8 vg > build/vg-build.log 2>&1; then
  grep -E ' error:' build/vg-build.log | head -20
  echo "VALGRIND BUILD FAILED"
  exit 1
fi
echo "valgrind build ok"
reports=$top/build/vg/reports
rm -rf "$reports" && mkdir -p "$reports" || exit 1
# one report file per process (%p); an error also makes the process exit 99
vg="valgrind -q --error-exitcode=99 --leak-check=no $origins
  --num-callers=30 --suppressions=$top/tools/valgrind.supp
  --log-file=$reports/vg.%p"
# the checks start "xppautX": this runs it under memcheck
wrap=$top/build/vg/xppautX-memcheck
printf '#!/bin/sh\nexec %s "%s" "$@"\n' "$(echo $vg)" "$top/build/vg/xppautX" > "$wrap"
chmod +x "$wrap"
# the checks wait this many times longer for the slow server
# (tools/xppclient.py); xpp_malloc leaves memory as the C library gives it
# (core/xpp_mem.h), which is what memcheck needs to see a read of what was
# never written
export XPP_CHECK_SLOW=30 XPP_MEM_INIT=0
fail=0

tmp=$(mktemp -d)
( cd "$tmp" && "$wrap" "$top/examples/ode/lecar.ode" -silent > run.log 2>&1 )
st=$?
sum=$(md5sum "$tmp/output.dat" 2>/dev/null | cut -d' ' -f1)
if [ $st -eq 0 ] && [ "$sum" = "$BASELINE" ]; then
  echo "xppautX -silent ok: checksum matches baseline"
else
  head -20 "$tmp/run.log"
  echo "xppautX -silent FAILED: exit $st, sum=$sum"
  fail=1
fi
rm -rf "$tmp"

# every example through xppautX -silent, memcheck's verdict only (no output
# comparison); a model that does not run by itself exits non-zero without
# a report
ex=$(mktemp -d)
find examples -name '*.ode' | sort | xargs -P"$(nproc 2>/dev/null || echo 4)" -I{} sh -c '
  f=$1; run=$2/$(echo "$f" | tr / _); mkdir -p "$run"
  cp "$(dirname "$f")"/* "$run"/ 2>/dev/null
  cd "$run" && timeout 900 "$3" "$(basename "$f")" -silent > run.log 2>&1
  echo "$? $f" >> "$2/status"' sh {} "$ex" "$wrap"
echo "examples run: $(wc -l < "$ex/status"), exit codes: $(cut -d' ' -f1 "$ex/status" | sort -n | uniq -c | tr -s ' \n' ' ')"
grep -E '^(99|124) ' "$ex/status" | sed 's/^99 /memcheck error: /; s/^124 /timed out: /'
rm -rf "$ex"

if make BUILDDIR=build/vg VALGRIND=1 TEST_RUNNER="$(echo $vg)" test > build/vg-unittest.log 2>&1; then
  echo "unit tests ok"
else
  grep -E 'FAIL|failed|ERROR' build/vg-unittest.log | head -20
  echo "UNIT TESTS FAILED"
  fail=1
fi

# servercheck and autocheck's sections side by side, each logged to
# build/vg-<name>.log; then each one's exit status
python3 tools/servercheck.py --server "$wrap" > build/vg-servercheck.log 2>&1 &
jobs="servercheck:$!"
# --report: memcheck slows everything down, so the latency limits (which
# verify.sh checks) only measure here
for sec in $(python3 tools/autocheck.py --list); do
  python3 tools/autocheck.py --server "$wrap" --report "$sec" > "build/vg-auto-$sec.log" 2>&1 &
  jobs="$jobs auto-$sec:$!"
done
passes=0
for job in $jobs; do
  name=${job%:*}
  log=build/vg-$name.log
  if wait "${job#*:}"; then
    passes=$((passes + $(grep -c '^PASS' "$log")))
  else
    grep -v '^PASS' "$log" | head -30
    echo "$name FAILED"
    fail=1
  fi
done
echo "servercheck and autocheck: $passes checks passed"

n=0
for f in "$reports"/*; do
  [ -s "$f" ] || continue
  n=$((n + 1))
  echo "== $f"
  head -60 "$f"
done
if [ "$n" -ne 0 ]; then
  echo "MEMCHECK REPORTS: $n (in $reports)"
  fail=1
else
  echo "memcheck ok: no error report"
fi
if [ $fail -ne 0 ]; then
  echo "VALGRIND CHECK FAILED"
  exit 1
fi
echo "valgrind check ok"
