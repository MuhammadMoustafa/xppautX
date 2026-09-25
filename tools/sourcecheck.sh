#!/bin/sh
# The checks about the source rather than about a platform's build (W17):
# the same on every platform, so CI runs them once, in its `source` job,
# and tools/verify.sh (the local gate) runs them every time unless given
# --no-source-checks (CI's linux job). Run from anywhere (WSL/Linux).
#   - every text file is UTF-8 (tools/utf8check.py)
#   - every committed .sh is executable in git
#   - the core never prints to stdout/stderr directly (tools/stdoutcheck.sh)
#   - no sprintf/strcpy into a fixed buffer (tools/formatcheck.sh)
#   - no extern whose type differs from its definition (make ltocheck)
# Usage: tools/sourcecheck.sh [--warnings]
#   --warnings  also count a clean build's warnings by flag and file
#               (tools/warnings.sh; a count, it fails only if the build does)
cd "$(dirname "$0")/.." || exit 1
if command -v nproc >/dev/null 2>&1; then
  NPROC=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
  NPROC=$(sysctl -n hw.ncpu)
else
  NPROC=4
fi
mkdir -p build || exit 1
if ! python3 tools/utf8check.py; then
  echo "ENCODING CHECK FAILED"
  exit 1
fi
# a script committed from Windows loses its executable bit: CI's checkout
# then cannot run it ("Permission denied"), which a Windows or WSL run on
# /mnt/c never shows
noexec=$(git ls-files -s -- '*.sh' 2>/dev/null | awk '$1 != "100755" {print $4}')
if [ -n "$noexec" ]; then
  echo "scripts not executable in git (git update-index --chmod=+x):" $noexec
  echo "SCRIPT MODE CHECK FAILED"
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
# make ltocheck's own sub-make, invoked via $(MAKE), shares the jobserver
# this -j sets up, so its build/lto compile parallelizes too
if make -j"$NPROC" ltocheck > build/ltocheck.log 2>&1; then
  echo "lto link ok: no types differ across files"
else
  tail -30 build/ltocheck.log
  echo "LTO CHECK FAILED"
  exit 1
fi
if [ "$1" = --warnings ]; then
  sh tools/warnings.sh || exit 1
fi
