#!/bin/sh
# Build xppautX with AddressSanitizer, LeakSanitizer and
# UndefinedBehaviorSanitizer (make asan, into build/asan) and run the checks
# under it: the --silent smoke run (same checksum as verify.sh), the unit
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
# Usage: tools/asancheck.sh [--no-leaks] [--builddir DIR] [--skip-build]
#                            [--only LIST] [VAR=value ...]
#   --no-leaks      no LeakSanitizer (detect_leaks=0), where it does not exist
#   --builddir DIR  build into and run from DIR (default build/asan)
#   --skip-build    assume DIR is already built (a previous --only build run,
#                    e.g. downloaded from another CI job); never runs "make
#                    asan-link"
#   --only LIST     run only these comma-separated phases, in order:
#                    build,smoke,examples,unittests, then any of
#                    servercheck,webcheck,autocheck (or the "checks" alias
#                    for all three) -- these run side by side when more
#                    than one is named. autocheck=SECTION+SECTION... (the
#                    names tools/autocheck.py --list prints, '+'-joined)
#                    runs only those sections instead of its default full
#                    list. Default: build,smoke,examples,unittests,checks
#                    (everything). Lets a slow platform (W41:
#                    windows-clang-sanitizers) split the work across
#                    parallel CI jobs, each with its own build (a build is
#                    only 60-90s here, so unlike the checks a shard rarely
#                    needs --skip-build); Linux and macOS CI keep running
#                    it whole.
#   VAR=value       passed to make (the compilers, say)
#   $MAKE, $PYTHON  the make and python programs (default make, python3)
cd "$(dirname "$0")/.." || exit 1
top=$PWD
BASELINE=c281851de59ffd03b2a46428619a0c8f
leaks=1
bdir=build/asan
skip_build=0
only=build,smoke,examples,unittests,checks
while [ $# -gt 0 ]; do
  case $1 in
    --no-leaks) leaks=0 ;;
    --builddir) bdir=$2; shift ;;
    --skip-build) skip_build=1 ;;
    --only) only=$2; shift ;;
    *=*) break ;;
    *) echo "usage: tools/asancheck.sh [--no-leaks] [--builddir DIR] [--skip-build] [--only LIST] [VAR=value ...]"; exit 2 ;;
  esac
  shift
done
has_phase() {
  case ",$only," in *",$1,"*) return 0 ;; *) return 1 ;; esac
}
make=${MAKE:-make}
python=${PYTHON:-python3}
# the logs: build/asan-*.log for build/asan
log=build/$(basename "$bdir")
mkdir -p build || exit 1
if [ $skip_build -eq 1 ] || ! has_phase build; then
  echo "asan build skipped (--skip-build or --only without build): using $bdir as-is"
else
  if ! "$make" -j"${XPP_JOBS:-4}" BUILDDIR="$bdir" ASAN=1 "$@" asan-link > "$log-build.log" 2>&1; then
    grep -E ' error:' "$log-build.log" | head -20
    echo "ASAN BUILD FAILED"
    exit 1
  fi
  echo "asan build ok"
fi
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
# gcc's UBSan, built beside ASan, ignores log_path and reports on stderr
# only: the python checks keep each server's stderr here
# (tools/xppclient.py drain_stderr), and a report found in one is copied
# into $reports below
stderrs=$top/$bdir/stderr
rm -rf "$stderrs" && mkdir -p "$stderrs" || exit 1
XPP_CHECK_STDERR=$stderrs
command -v cygpath > /dev/null 2>&1 && XPP_CHECK_STDERR=$(cygpath -m "$stderrs")
export XPP_CHECK_STDERR
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

if has_phase smoke; then
  tmp=$(mktemp -d)
  ( cd "$tmp" && "$top/$bin" "$top/examples/ode/lecar.odex" --silent > run.log 2>&1 )
  st=$?
  # (CRs removed: Windows writes CRLF)
  sum=$( [ -e "$tmp/output.dat" ] && tr -d '\r' < "$tmp/output.dat" | md5 )
  if [ $st -eq 0 ] && [ "$sum" = "$BASELINE" ]; then
    echo "$bin --silent ok: checksum matches baseline"
  else
    head -20 "$tmp/run.log"
    echo "$bin --silent FAILED: exit $st, sum=$sum"
    fail=1
  fi
  rm -rf "$tmp"
fi

if has_phase examples; then
  # every example through xppautX --silent, sanitizers only (no output
  # comparison, just their verdict); a model that does not run by itself
  # exits non-zero without a report
  ex=$(mktemp -d)
  find examples -name '*.odex' | sort | xargs -P"$NPROC" -I{} sh -c '
    f=$1; run=$2/$(echo "$f" | tr / _); mkdir -p "$run"
    cp "$(dirname "$f")"/* "$run"/ 2>/dev/null
    cd "$run" && ${4:+$4 120} "$3" "$(basename "$f")" --silent > run.log 2>&1
    echo "$? $f" >> "$2/status"' sh {} "$ex" "$top/$bin" "$TMO"
  echo "examples run: $(wc -l < "$ex/status"), exit codes: $(cut -d' ' -f1 "$ex/status" | sort -n | uniq -c | tr -s ' \n' ' ')"
  rm -rf "$ex"
fi

if has_phase unittests; then
  if "$make" -j"$NPROC" BUILDDIR="$bdir" ASAN=1 "$@" test > "$log-unittest.log" 2>&1; then
    echo "unit tests ok"
  else
    grep -E 'FAIL|failed|ERROR' "$log-unittest.log" | head -20
    echo "UNIT TESTS FAILED"
    fail=1
  fi
fi

# servercheck, webcheck and autocheck can be selected individually (a
# platform whose combined "checks" is still too slow for one CI job
# splits them across shards, W41) or together as the "checks" alias
# (Linux and macOS CI's whole-script run); whichever of these are
# requested run side by side, each logged to its own file, their waits
# doubled for a machine sharing sanitized servers. Prints "NAME finished
# after Ns" as each one exits (not just its final verdict), so a job log
# shows which one is the long pole without needing to look at the .log
# files afterwards.
run_check() {
  name=$1; shift
  t0=$(date +%s)
  if XPP_CHECK_SLOW=${XPP_CHECK_SLOW:-2} "$@" > "$log-$name.log" 2>&1; then
    echo "$name ok: $(grep -c '^PASS' "$log-$name.log") checks" > "$log-$name.verdict"
  else
    { grep -v '^PASS' "$log-$name.log" | head -30; echo "$name FAILED"; } > "$log-$name.verdict"
  fi
  echo "$name finished after $(( $(date +%s) - t0 ))s"
}
# autocheck's sections (tools/autocheck.py --list), split roughly in half
# by count so autocheck can be its own shard's own two shards if running
# it whole is still the long pole (W41: unmeasured yet which of
# servercheck/autocheck it is; --only autocheck=SECTION+SECTION... picks a
# subset, '+'-joined so it doesn't collide with --only's own commas)
want=""
autosections=
old_ifs=$IFS; IFS=,
for tok in $only; do
  case "$tok" in
    checks) want="$want servercheck webcheck autocheck" ;;
    servercheck|webcheck) want="$want $tok" ;;
    autocheck) want="$want autocheck" ;;
    autocheck=*) want="$want autocheck"; autosections=$(printf '%s' "${tok#autocheck=}" | tr '+' ' ') ;;
  esac
done
IFS=$old_ifs
if [ -n "$want" ]; then
  for name in $want; do
    case $name in
      servercheck) run_check servercheck "$python" tools/servercheck.py --server "$bin" & ;;
      webcheck) run_check webcheck "$python" tools/webcheck.py --bin "$bin" & ;;
      # --report: the sanitizers slow everything down, so the latency
      # limits (which verify.sh checks) only measure here
      autocheck) run_check autocheck "$python" tools/autocheck.py --server "$bin" --report $autosections & ;;
    esac
  done
  wait
  for name in $want; do
    cat "$log-$name.verdict"
    grep -q ' FAILED$' "$log-$name.verdict" && fail=1
  done
fi

for f in "$stderrs"/*; do
  [ -e "$f" ] || continue
  grep -qE 'runtime error:|ERROR: (Address|Leak)Sanitizer' "$f" && cp "$f" "$reports/stderr-$(basename "$f")"
done
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
