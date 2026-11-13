#!/bin/sh
# Build, run the headless smoke test, compare against the known-good checksum,
# drive xppautX --server through its protocol (tools/servercheck.py) and
# its browser mode through HTTP (tools/webcheck.py), and print
# the C++ metric. Run from repo root (WSL/Linux/macOS).
# It also runs the checks about the source (tools/sourcecheck.sh: the LTO
# type check among them) and checks that a failed allocation is loud.
# The sanitizer build (tools/asancheck.sh) is slower and runs apart, in CI.
# Every check after the unit tests runs side by side (tools/side_by_side.sh, at
# most JOBS busy at once); a failed one is named with the tail of its log, and the
# exit status is non-zero if any failed.
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
BASELINE=58fed7a175a635010b7698da91dc4e4c
# the behaviour checks are python: without it the gate would pass having
# run only part of itself, so it refuses to start (review 2026-10-01, W146)
if ! command -v uv >/dev/null 2>&1; then
  echo "verify.sh: uv is required (servercheck, autoxcheck, goldencheck, manualcheck)"
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
. tools/jobs.sh
mkdir -p build || exit 1
make -j"$JOBS" xppautx > build/last-build.log 2>&1
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
if make -pn xppautx 2>/dev/null | grep -E '^build/obj/xpp_session\.o:' | grep -q 'core/xpp_ui\.h'; then
  echo "header dependencies ok: core objects rebuild when a header they include changes"
else
  echo "HEADER DEPENDENCIES FAILED: build/obj/xpp_session.o does not depend on core/xpp_ui.h"
  exit 1
fi
tmp=$(mktemp -d)
# the md5 is the same interpreted or compiled: the log says the model's code ran native (W714: MIR)
( cd "$tmp" && "$OLDPWD/xppautX" "$OLDPWD/examples/ode/lecar.odex" --silent --verbose 2>&1 >/dev/null | grep -q "Compiled 11 of 11 programs" ) || { echo "HEADLESS --silent: lecar was not compiled"; exit 1; }
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
if make -j"$JOBS" test > build/unittest.log 2>&1; then
  echo "unit tests ok: $(grep -c 'checks,' build/unittest.log) files"
else
  grep -E 'FAIL|failed' build/unittest.log
  echo "UNIT TESTS FAILED"
  exit 1
fi
# the checks, side by side (tools/side_by_side.sh: a log each in build/check-NAME.log,
# the verdicts at the end). The machine's bound holds (W289, W418): at most JOBS
# processes busy at once. Round one is the checks that run one process at a time,
# JOBS of them at once, longest first; round two is the checks that fill JOBS
# themselves (the compiles of sourcecheck, the models of examples, odexcheck and
# autoxcompare), one at a time. Nothing they write is shared: each has its own log,
# build folder (golden-differ, auto-differ, lto, deadcode) or mktemp folder, and
# the servers take port 0. A differing golden output is kept in build/golden-differ
# (CI uploads it); the examples still run after a golden failure, so one run says
# both.
rm -rf build/golden-differ build/auto-differ
fail=0
tools/side_by_side.sh --prefix build/check- --jobs "$JOBS" \
  autoxcheck uv run --locked python tools/autoxcheck.py -- \
  servercheck uv run --locked python tools/servercheck.py -- \
  webcheck uv run --locked python tools/webcheck.py -- \
  goldencheck uv run --locked python tools/goldencheck.py --keep build/golden-differ -- \
  modecheck sh tools/modecheck.sh -- \
  steadycheck uv run --locked python tools/steadycheck.py -- \
  spikecheck uv run --locked python tools/spikecheck.py -- \
  livestatecheck uv run --locked python tools/livestatecheck.py -- \
  associatecheck sh tools/associatecheck.sh -- \
  checkcheck uv run --locked python tools/checkcheck.py -- \
  embedcheck uv run --locked python tools/embedcheck.py --build-dir build/obj -- \
  manualcheck uv run --locked python tools/manualcheck.py -- \
  unsafecheck sh tools/unsafecheck.sh --check || fail=1
# checks about the source, not the build: once per push in CI (its
# `source` job, tools/sourcecheck.sh), every time in the local gate
set --
if [ $source_checks -eq 1 ]; then set -- sourcecheck sh tools/sourcecheck.sh --; fi
tools/side_by_side.sh --prefix build/check- --jobs 1 "$@" \
  examples tools/examples_check.sh -- \
  odexcheck sh tools/odexcheck.sh -- \
  autoxcompare uv run --locked python tools/autoxcompare.py --keep build/auto-differ || fail=1
grep '^unsafe C idioms:' build/check-unsafecheck.log
# the conversion of the core to C++ (CLAUDE.md, "C and C++")
echo "C++: $(( $(ls core/*.cpp 2>/dev/null | wc -l) )) / $(( $(ls core/*.c core/*.cpp 2>/dev/null | wc -l) )) sources"
[ $fail -eq 0 ] || exit 1
if [ $clean_warnings -eq 1 ]; then tools/warnings.sh; fi
