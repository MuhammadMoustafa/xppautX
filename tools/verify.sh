#!/bin/sh
# Build, run the headless smoke test, compare against the known-good checksum,
# drive xppautX --server through its protocol (tools/servercheck.py) and
# its browser mode through HTTP (tools/webcheck.py), and print
# the C++ metric. Run from repo root (WSL/Linux/macOS).
# It also links with LTO (make ltocheck), which reports types
# that differ across files, and checks that a failed allocation is loud.
# The sanitizer build (tools/asancheck.sh) is slower and runs apart, in CI.
# Usage: tools/verify.sh [--clean-warnings]
cd "$(dirname "$0")/.." || exit 1
BASELINE=c281851de59ffd03b2a46428619a0c8f
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
if make test > build/unittest.log 2>&1; then
  echo "unit tests ok: $(grep -c 'checks,' build/unittest.log) files"
else
  grep -E 'FAIL|failed' build/unittest.log
  echo "UNIT TESTS FAILED"
  exit 1
fi
if ! python3 tools/utf8check.py; then
  echo "ENCODING CHECK FAILED"
  exit 1
fi
if ! sh tools/stdoutcheck.sh; then
  echo "STDOUT CHECK FAILED"
  exit 1
fi
if ! sh tools/formatcheck.sh; then
  echo "FORMAT CHECK FAILED"
  exit 1
fi
if make ltocheck > build/ltocheck.log 2>&1; then
  echo "lto link ok: no types differ across files"
else
  tail -30 build/ltocheck.log
  echo "LTO CHECK FAILED"
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
if [ "$1" = --clean-warnings ]; then tools/warnings.sh; fi
