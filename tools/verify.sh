#!/bin/sh
# Build, run the headless smoke test, compare against the known-good checksum,
# drive xppautX --server through its protocol (tools/servercheck.py) and
# its browser mode through HTTP (tools/webcheck.py), and print
# the X11-free metric. Run from repo root (WSL/Linux/macOS).
cd "$(dirname "$0")/.." || exit 1
BASELINE=c281851de59ffd03b2a46428619a0c8f
mkdir -p build || exit 1
make -j8 xppaut xppautx > build/last-build.log 2>&1
st=$?
tr -d '\r' < build/last-build.log > build/last-build.tmp && mv build/last-build.tmp build/last-build.log
if [ $st -ne 0 ] || grep -q ' error:' build/last-build.log; then
  grep -E ' error:' build/last-build.log | head -20
  echo "BUILD FAILED"
  exit 1
fi
echo "build ok, warnings: $(grep -c 'warning:' build/last-build.log), implicit decls: $(grep -c 'implicit declaration' build/last-build.log)"
tmp=$(mktemp -d)
( cd "$tmp" && "$OLDPWD/xppaut" "$OLDPWD/examples/ode/lecar.ode" -silent >/dev/null 2>&1 )
sum=$(md5sum "$tmp/output.dat" 2>/dev/null | cut -d' ' -f1)
rows=$(wc -l < "$tmp/output.dat" 2>/dev/null || echo 0)
rm -rf "$tmp"
if [ "$sum" = "$BASELINE" ]; then
  echo "smoke ok: $rows rows, checksum matches baseline"
else
  echo "SMOKE MISMATCH: rows=$rows sum=$sum"
  exit 1
fi
tmp=$(mktemp -d)
( cd "$tmp" && "$OLDPWD/xppautX" "$OLDPWD/examples/ode/lecar.ode" -silent >/dev/null 2>&1 )
sum=$(md5sum "$tmp/output.dat" 2>/dev/null | cut -d' ' -f1)
rm -rf "$tmp"
if [ "$sum" = "$BASELINE" ]; then
  echo "headless -silent ok: checksum matches baseline"
else
  echo "HEADLESS -silent MISMATCH: sum=$sum"
  exit 1
fi
if make test > build/unittest.log 2>&1; then
  echo "unit tests ok: $(grep -c 'checks,' build/unittest.log) files"
else
  grep -E 'FAIL|failed' build/unittest.log
  echo "UNIT TESTS FAILED"
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
fi
tools/x11free.sh
