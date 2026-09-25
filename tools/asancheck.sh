#!/bin/sh
# Build xppautX with AddressSanitizer, LeakSanitizer and
# UndefinedBehaviorSanitizer (make asan, into build/asan) and run the checks
# under it: the -silent smoke run (same checksum as verify.sh), the unit
# tests, the protocol (servercheck.py), browser mode (webcheck.py) and
# AUTO (autocheck.py).
#
# Any sanitizer report fails the script, whether or not the check that
# provoked it noticed: every report goes to a file in build/asan/reports
# (log_path), and that directory must end up empty. A leak is reported when
# a process exits, so the checks must let the programs exit (File/Quit),
# not kill them. tools/lsan.supp lists the only leaks tolerated, all in code
# we do not own.
#
# Slow (the checks run several times slower than in verify.sh), so not part
# of verify.sh; CI runs it: on Linux with gcc (LeakSanitizer too), on
# macOS with Apple clang (macos-sanitizers, --no-leaks: Apple Silicon
# runners have no LeakSanitizer) and on Windows with MSYS2's CLANG64 clang
# (W23: --no-leaks, no LeakSanitizer there), from its shell or Git Bash
# with C:\msys64\clang64\bin first on PATH:
#   MAKE=mingw32-make tools/asancheck.sh --no-leaks --builddir build/clang-asan CC=clang CXX=clang++
# Usage: tools/asancheck.sh [--no-leaks] [--builddir DIR] [VAR=value ...]
#   --no-leaks      no LeakSanitizer (detect_leaks=0), where it does not exist
#   --builddir DIR  build into and run from DIR (default build/asan)
#   VAR=value       passed to make (the compilers, say)
#   $MAKE, $PYTHON  the make and python programs (default make, python3)
cd "$(dirname "$0")/.." || exit 1
top=$PWD
BASELINE=c281851de59ffd03b2a46428619a0c8f
leaks=1
bdir=build/asan
while [ $# -gt 0 ]; do
  case $1 in
    --no-leaks) leaks=0 ;;
    --builddir) bdir=$2; shift ;;
    *=*) break ;;
    *) echo "usage: tools/asancheck.sh [--no-leaks] [--builddir DIR] [VAR=value ...]"; exit 2 ;;
  esac
  shift
done
make=${MAKE:-make}
python=${PYTHON:-python3}
# the logs: build/asan-*.log for build/asan
log=build/$(basename "$bdir")
mkdir -p build || exit 1
if ! "$make" -j8 BUILDDIR="$bdir" ASAN=1 "$@" asan-link > "$log-build.log" 2>&1; then
  grep -E ' error:' "$log-build.log" | head -20
  echo "ASAN BUILD FAILED"
  exit 1
fi
echo "asan build ok"
bin=$bdir/xppautX
[ -e "$bin.exe" ] && bin=$bin.exe
reports=$top/$bdir/reports
rm -rf "$reports" && mkdir -p "$reports" || exit 1
# the path as the program sees it (C:/... on Windows), quoted: its colon
# would end the option
rpath=$reports
command -v cygpath > /dev/null 2>&1 && rpath=$(cygpath -m "$reports")
export ASAN_OPTIONS="detect_leaks=$leaks:abort_on_error=1:log_path='$rpath/asan'"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1:log_path='$rpath/ubsan'"
if [ $leaks -eq 1 ]; then
  export LSAN_OPTIONS="suppressions=$top/tools/lsan.supp:print_suppressions=0"
fi
if command -v nproc >/dev/null 2>&1; then
  NPROC=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
  NPROC=$(sysctl -n hw.ncpu)
else
  NPROC=4
fi
if command -v timeout >/dev/null 2>&1; then
  TMO=timeout
elif command -v gtimeout >/dev/null 2>&1; then
  TMO=gtimeout
else
  TMO=
fi
md5() {
  if command -v md5sum >/dev/null 2>&1; then md5sum | cut -d' ' -f1; else md5 -q; fi
}
fail=0

tmp=$(mktemp -d)
( cd "$tmp" && "$top/$bin" "$top/examples/ode/lecar.ode" -silent > run.log 2>&1 )
st=$?
# (CRs removed: Windows writes CRLF)
sum=$( [ -e "$tmp/output.dat" ] && tr -d '\r' < "$tmp/output.dat" | md5 )
if [ $st -eq 0 ] && [ "$sum" = "$BASELINE" ]; then
  echo "$bin -silent ok: checksum matches baseline"
else
  head -20 "$tmp/run.log"
  echo "$bin -silent FAILED: exit $st, sum=$sum"
  fail=1
fi
rm -rf "$tmp"

# every example through xppautX -silent, sanitizers only (no output
# comparison, just their verdict); a model that does not run by itself
# exits non-zero without a report
ex=$(mktemp -d)
find examples -name '*.ode' | sort | xargs -P"$NPROC" -I{} sh -c '
  f=$1; run=$2/$(echo "$f" | tr / _); mkdir -p "$run"
  cp "$(dirname "$f")"/* "$run"/ 2>/dev/null
  cd "$run" && ${4:+$4 120} "$3" "$(basename "$f")" -silent > run.log 2>&1
  echo "$? $f" >> "$2/status"' sh {} "$ex" "$top/$bin" "$TMO"
echo "examples run: $(wc -l < "$ex/status"), exit codes: $(cut -d' ' -f1 "$ex/status" | sort -n | uniq -c | tr -s ' \n' ' ')"
rm -rf "$ex"

if "$make" BUILDDIR="$bdir" ASAN=1 "$@" test > "$log-unittest.log" 2>&1; then
  echo "unit tests ok"
else
  grep -E 'FAIL|failed|ERROR' "$log-unittest.log" | head -20
  echo "UNIT TESTS FAILED"
  fail=1
fi

# the name, then the command
run_check() {
  name=$1; shift
  if "$@" > "$log-$name.log" 2>&1; then
    echo "$name ok: $(grep -c '^PASS' "$log-$name.log") checks"
  else
    grep -v '^PASS' "$log-$name.log" | head -30
    echo "$name FAILED"
    fail=1
  fi
}
run_check servercheck "$python" tools/servercheck.py --server "$bin"
run_check webcheck "$python" tools/webcheck.py --bin "$bin"
# --report: the sanitizers slow everything down, so the latency limits
# (which verify.sh checks) only measure here
run_check autocheck "$python" tools/autocheck.py --server "$bin" --report

n=$(ls "$reports" | wc -l)
if [ "$n" -ne 0 ]; then
  for f in "$reports"/*; do
    echo "== $f"
    head -60 "$f"
  done
  echo "SANITIZER REPORTS: $n (in $reports)"
  fail=1
else
  if [ $leaks -eq 1 ]; then echo "sanitizers ok: no error or leak report"
  else echo "sanitizers ok: no error report (leaks not checked)"; fi
fi
if [ $fail -ne 0 ]; then
  echo "ASAN CHECK FAILED"
  exit 1
fi
echo "asan check ok"
