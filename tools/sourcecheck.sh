#!/bin/sh
# The checks about the source rather than about a platform's build (W17):
# the same on every platform, so CI runs them once, in its `source` job,
# and tools/verify.sh (the local gate) runs them every time unless given
# --no-source-checks (CI's linux-core job). Run from anywhere (WSL/Linux).
#   - every text file is UTF-8 (tools/utf8check.py)
#   - every committed .sh is executable in git
#   - the core never prints to stdout/stderr directly (tools/stdoutcheck.sh)
#   - no sprintf/strcpy into a fixed buffer (tools/formatcheck.sh)
#   - no malloc/free but through xpp_mem.h (tools/alloccheck.sh)
#   - no more direct fopen/remove/rename/mkdir/... than tests/files.baseline
#     allows: files go through xpp_files.h (tools/filecheck.sh)
#   - no string literal cast to char * (tools/literalcheck.sh)
#   - no delay outside condition polling or named lower bounds (tools/sleepcheck.sh, W166)
#   - no direct exp, log, pow, sin, ... of the C library: xpp::math's, the same
#     bits on every CPU (tools/mathcheck.sh, W159)
#   - no more errors reported with no place (file, line) than
#     tests/errors.baseline allows (tools/errorcheck.py, W140)
#   - no extern "C" but where C calls across (tools/externcheck.sh, W109)
#   - no current Session or Model read outside the session list's owners
#     (tools/sessioncheck.sh, W47d6)
#   - no extern whose type differs from its definition (make ltocheck)
#   - no function or file-scope datum nothing reaches (tools/deadcode.sh;
#     Linux: elsewhere it says so and passes)
#   - no more external mutable data symbols per file than
#     tests/globals.baseline allows (tools/globalcheck.sh)
#   - no new duplicated function/block/struct (tools/dupcheck.sh)
#   - no second name for one of our names (tools/aliascheck.sh, W113)
#   - no dead declaration: an unused macro, type or field, a declaration
#     with no definition or repeated in a second header, #if 0, commented-
#     out code, a header nothing includes (tools/deadcheck.py)
#   - one letter, one command, every key in a menu: no key handled that no
#     menu lists, no two keys for one command (tools/keycheck.py, W60)
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
# the core is C++ (W27, 2026-09-25), and its unit tests with it (W109f):
# a new source there is a .cpp
csrc=$(ls core/*.c tests/*.c 2>/dev/null)
if [ -n "$csrc" ]; then
  echo "C sources in core/ or tests/ (they are C++: git mv to .cpp):" $csrc
  echo "C SOURCE CHECK FAILED"
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
if ! sh tools/alloccheck.sh; then
  echo "ALLOC CHECK FAILED"
  exit 1
fi
if ! sh tools/filecheck.sh --check; then
  echo "FILE CHECK FAILED"
  exit 1
fi
if ! python3 tools/errorcheck.py --check > build/errorcheck.out 2>&1; then
  tail -40 build/errorcheck.out
  echo "ERROR PLACE CHECK FAILED"
  exit 1
fi
tail -1 build/errorcheck.out
if ! sh tools/literalcheck.sh; then
  echo "LITERAL CHECK FAILED"
  exit 1
fi
if ! sh tools/sleepcheck.sh; then
  echo "SLEEP CHECK FAILED"
  exit 1
fi
if ! sh tools/mathcheck.sh; then
  echo "MATH CHECK FAILED"
  exit 1
fi
if ! sh tools/externcheck.sh; then
  echo "EXTERN C CHECK FAILED"
  exit 1
fi
if ! sh tools/sessioncheck.sh; then
  echo "SESSION CHECK FAILED"
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
if ! sh tools/deadcode.sh --check > build/deadcode.out 2>&1; then
  tail -40 build/deadcode.out
  echo "DEAD CODE CHECK FAILED"
  exit 1
fi
tail -1 build/deadcode.out
# the objects deadcode.sh just built in build/deadcode
if ! sh tools/globalcheck.sh --check --builddir build/deadcode > build/globalcheck.out 2>&1; then
  tail -40 build/globalcheck.out
  echo "GLOBAL STATE CHECK FAILED"
  exit 1
fi
tail -1 build/globalcheck.out
if ! sh tools/dupcheck.sh --check > build/dupcheck.out 2>&1; then
  tail -60 build/dupcheck.out
  echo "DUPLICATION CHECK FAILED"
  exit 1
fi
tail -1 build/dupcheck.out
if ! sh tools/aliascheck.sh --check > build/aliascheck.out 2>&1; then
  tail -40 build/aliascheck.out
  echo "ALIAS CHECK FAILED"
  exit 1
fi
tail -1 build/aliascheck.out
if ! python3 tools/deadcheck.py --check > build/deadcheck.out 2>&1; then
  tail -40 build/deadcheck.out
  echo "DEAD DECLARATION CHECK FAILED"
  exit 1
fi
tail -1 build/deadcheck.out
if ! python3 tools/keycheck.py > build/keycheck.out 2>&1; then
  cat build/keycheck.out
  echo "KEY CHECK FAILED"
  exit 1
fi
tail -1 build/keycheck.out
if [ "$1" = --warnings ]; then
  sh tools/warnings.sh || exit 1
fi
