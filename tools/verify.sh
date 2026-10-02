#!/bin/sh
# Build, run the headless smoke test, compare against the known-good checksum,
# drive xppautX --server through its protocol (tools/servercheck.py) and
# its browser mode through HTTP (tools/webcheck.py), and print
# the C++ metric. Run from repo root (WSL/Linux/macOS).
# It also runs the checks about the source (tools/sourcecheck.sh: the LTO
# type check among them) and checks that a failed allocation is loud.
# The sanitizer build (tools/asancheck.sh) is slower and runs apart, in CI.
# Usage: tools/verify.sh [--clean-warnings] [--no-source-checks]
#   --no-source-checks  skip tools/sourcecheck.sh (encoding, script modes,
#                       stdoutcheck, formatcheck, the LTO type check): CI's
#                       linux-core job, whose source job runs them once
cd "$(dirname "$0")/.." || exit 1
clean_warnings=0
source_checks=1
for arg in "$@"; do
  case "$arg" in
    --clean-warnings) clean_warnings=1 ;;
    --no-source-checks) source_checks=0 ;;
    *) echo "verify.sh: unknown option $arg"; exit 2 ;;
  esac
done
BASELINE=c281851de59ffd03b2a46428619a0c8f
# the behaviour checks are python: without it the gate would pass having
# run only part of itself, so it refuses to start (review 2026-10-01, W146)
if ! command -v python3 >/dev/null 2>&1; then
  echo "verify.sh: python3 is required (servercheck, autocheck, goldencheck, manualcheck)"
  exit 2
fi
# one verify.sh at a time on this machine, whatever the checkout (W84):
# runs side by side over WSL's /mnt/c each took 40+ minutes instead of
# ~12, so a second run waits for the first (flock, where there is one;
# macOS has none, and CI runs one per job anyway)
if command -v flock >/dev/null 2>&1; then
  exec 9>"${TMPDIR:-/tmp}/xppautx-verify.lock"
  if ! flock -n 9; then
    echo "verify.sh: another verify.sh is running; waiting for it"
    flock 9
  fi
fi
# make test used to build on one core (60 s on CI); run it with the
# machine's core count instead
if command -v nproc >/dev/null 2>&1; then
  NPROC=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
  NPROC=$(sysctl -n hw.ncpu)
else
  NPROC=4
fi
mkdir -p build || exit 1
make -j8 WERROR=1 xppautx > build/last-build.log 2>&1
st=$?
tr -d '\r' < build/last-build.log > build/last-build.tmp && mv build/last-build.tmp build/last-build.log
if [ $st -ne 0 ] || grep -q ' error:' build/last-build.log; then
  grep -E ' error:' build/last-build.log | head -20
  echo "BUILD FAILED"
  exit 1
fi
echo "build ok, warnings: $(grep -c 'warning:' build/last-build.log), implicit decls: $(grep -c 'implicit declaration' build/last-build.log)"
# the tree builds with 0 warnings (CLAUDE.md): any warning fails the gate
# (W27b's conversion let 11 through while this only counted them)
if grep -q 'warning:' build/last-build.log; then
  grep 'warning:' build/last-build.log | sort -u | head -20
  echo "WARNINGS: the build must have none"
  exit 1
fi
# header dependencies: the core objects must depend on the headers they
# include (their .d files, which a Makefile slip once stopped loading for
# every core source, so incremental builds mixed old and new struct
# layouts); read from make's rule database, nothing is built or touched
if make -pn WERROR=1 xppautx 2>/dev/null | grep -E '^build/obj/xpp_session\.o:' | grep -q 'core/xpp_ui\.h'; then
  echo "header dependencies ok: core objects rebuild when a header they include changes"
else
  echo "HEADER DEPENDENCIES FAILED: build/obj/xpp_session.o does not depend on core/xpp_ui.h"
  exit 1
fi
tmp=$(mktemp -d)
( cd "$tmp" && "$OLDPWD/xppautX" "$OLDPWD/examples/ode/lecar.odex" --silent >/dev/null 2>&1 )
sum=$(md5sum "$tmp/output.dat" 2>/dev/null | cut -d' ' -f1)
rows=$(wc -l < "$tmp/output.dat" 2>/dev/null || echo 0)
rm -rf "$tmp"
if [ "$sum" = "$BASELINE" ]; then
  echo "headless --silent ok: $rows rows, checksum matches baseline"
else
  echo "HEADLESS --silent MISMATCH: rows=$rows sum=$sum"
  exit 1
fi
if make -j"$NPROC" test > build/unittest.log 2>&1; then
  echo "unit tests ok: $(grep -c 'checks,' build/unittest.log) files"
else
  grep -E 'FAIL|failed' build/unittest.log
  echo "UNIT TESTS FAILED"
  exit 1
fi
# checks about the source, not the build: once per push in CI (its
# `source` job, tools/sourcecheck.sh), every time in the local gate
if [ $source_checks -eq 1 ] && ! sh tools/sourcecheck.sh; then
  exit 1
fi
if python3 tools/servercheck.py > build/servercheck.log 2>&1; then
  echo "server protocol ok: $(grep -c '^PASS' build/servercheck.log) checks"
else
  grep -v '^PASS' build/servercheck.log
  echo "SERVER CHECK FAILED"
  exit 1
fi
if python3 tools/webcheck.py > build/webcheck.log 2>&1; then
  echo "web front end ok: $(grep -c '^PASS' build/webcheck.log) checks"
else
  grep -v '^PASS' build/webcheck.log
  echo "WEB CHECK FAILED"
  exit 1
fi
if sh tools/modecheck.sh > build/modecheck.log 2>&1; then
  echo "command-line modes ok: $(grep -c '^PASS' build/modecheck.log) checks"
else
  grep -v '^PASS' build/modecheck.log
  echo "MODE CHECK FAILED"
  exit 1
fi
if sh tools/associatecheck.sh > build/associatecheck.log 2>&1; then
  tail -1 build/associatecheck.log
else
  cat build/associatecheck.log
  echo "ASSOCIATE CHECK FAILED"
  exit 1
fi
if python3 tools/autocheck.py > build/autocheck.log 2>&1; then
  echo "auto checks ok: $(grep -c '^PASS' build/autocheck.log) checks"
else
  grep -v '^PASS' build/autocheck.log
  echo "AUTO CHECK FAILED"
  exit 1
fi
# a differing output is kept in build/golden-differ (CI uploads it); the
# examples still run after a golden failure, so one run says both
rm -rf build/golden-differ
golden_failed=0
if python3 tools/goldencheck.py --keep build/golden-differ > build/goldencheck.log 2>&1; then
  echo "golden outputs ok: $(grep -c '^PASS' build/goldencheck.log) files"
else
  grep -v '^PASS' build/goldencheck.log
  echo "GOLDEN CHECK FAILED"
  golden_failed=1
fi
# every example's output against tests/examples.md5: the numerics
if tools/examples_check.sh > build/examples.log 2>&1; then
  tail -1 build/examples.log
else
  tail -20 build/examples.log
  echo "EXAMPLES CHECK FAILED"
  exit 1
fi
[ "$golden_failed" -eq 0 ] || exit 1
# every example converted to .odex and run: the same md5 (W74, docs/odex.md)
if sh tools/odexcheck.sh > build/odexcheck.log 2>&1; then
  tail -1 build/odexcheck.log
else
  tail -20 build/odexcheck.log
  echo "ODEX CHECK FAILED"
  exit 1
fi
# every whole model the manual shows still loads, and every command-line
# option it names is real (W81)
if python3 tools/manualcheck.py > build/manualcheck.log 2>&1; then
  tail -1 build/manualcheck.log
else
  cat build/manualcheck.log
  echo "MANUAL CHECK FAILED"
  exit 1
fi
# the conversion of the core to C++ (CLAUDE.md, "C and C++")
echo "C++: $(( $(ls core/*.cpp 2>/dev/null | wc -l) )) / $(( $(ls core/*.c core/*.cpp 2>/dev/null | wc -l) )) sources"
# the move from C++ to safe C++ (CLAUDE.md, "C and C++"; the W29 cards)
if sh tools/unsafecheck.sh --check > build/unsafecheck.log 2>&1; then
  grep '^unsafe C idioms:' build/unsafecheck.log
else
  cat build/unsafecheck.log
  echo "UNSAFECHECK FAILED"
  exit 1
fi
if [ $clean_warnings -eq 1 ]; then tools/warnings.sh; fi
