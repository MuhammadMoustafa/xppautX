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
#                       linux job, whose source job runs them once
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
( cd "$tmp" && "$OLDPWD/xppautX" "$OLDPWD/examples/ode/lecar.ode" -silent >/dev/null 2>&1 )
sum=$(md5sum "$tmp/output.dat" 2>/dev/null | cut -d' ' -f1)
rows=$(wc -l < "$tmp/output.dat" 2>/dev/null || echo 0)
rm -rf "$tmp"
if [ "$sum" = "$BASELINE" ]; then
  echo "headless -silent ok: $rows rows, checksum matches baseline"
else
  echo "HEADLESS -silent MISMATCH: rows=$rows sum=$sum"
  exit 1
fi
# a failed allocation is loud: exit 1 and an ERROR naming the call site
# (core/xpp_mem.h; XPP_MEM_FAIL_AT makes the 5th allocation fail)
tmp=$(mktemp -d)
( cd "$tmp" && XPP_MEM_FAIL_AT=5 "$OLDPWD/xppautX" "$OLDPWD/examples/ode/lecar.ode" -silent > run.log 2>&1 )
st=$?
site=$(grep -oE 'out of memory: .* at core/[a-z0-9_]+\.(c|cpp):[0-9]+' "$tmp/run.log")
rm -rf "$tmp"
if [ $st -eq 1 ] && [ -n "$site" ]; then
  echo "allocation failure ok: exit 1, \"$site\""
else
  echo "ALLOCATION FAILURE NOT LOUD: exit $st, message \"$site\""
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
if command -v python3 >/dev/null; then
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
  if python3 tools/autocheck.py > build/autocheck.log 2>&1; then
    echo "auto checks ok: $(grep -c '^PASS' build/autocheck.log) checks"
  else
    grep -v '^PASS' build/autocheck.log
    echo "AUTO CHECK FAILED"
    exit 1
  fi
fi
# every example's output against tests/examples.md5: the numerics
if tools/examples_check.sh > build/examples.log 2>&1; then
  tail -1 build/examples.log
else
  tail -20 build/examples.log
  echo "EXAMPLES CHECK FAILED"
  exit 1
fi
# the conversion of the core to C++ (CLAUDE.md, "C and C++")
echo "C++: $(( $(ls core/*.cpp 2>/dev/null | wc -l) )) / $(( $(ls core/*.c core/*.cpp 2>/dev/null | wc -l) )) sources"
if [ $clean_warnings -eq 1 ]; then tools/warnings.sh; fi
