#!/usr/bin/env bash
# W244 (#300): build the pinned XPPAUT 8.0 source natively on Windows the way
# xppautX.exe is built (MSYS2 UCRT gcc 16, 64-bit, -g -O2 -ffp-contract=off),
# for the paper's Windows comparison (W242, #298). From Git Bash:
#   paper/benchmarks/windows-build/build.sh OUTDIR
# The source is not patched: flags and shim headers stand in for what
# Windows lacks (STRUPR: the C runtime has strupr/strlwr; MAXW: extra.c
# defines it only under HAVEDLL; -UWIN32: the X headers' Windows path pulls
# in windows.h; win-include/: sys/wait.h, sys/resource.h), and
# x11-stubs.c gives the X11 functions and fork/wait the link needs, each
# aborting if called: -silent never opens a window.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd -W)
out=$1
repo=$(git -C "$here" rev-parse --show-toplevel)
legacy=c021b511eee95a43576227656c6530e9af78ca27  # five-model-screen/provenance.json
mkdir "$out"   # refuses an existing folder
git -C "$repo" archive "$legacy" | tar -x -C "$out"
cd "$out"
export PATH=/c/msys64/ucrt64/bin:$PATH
cc="gcc -std=gnu99 -D_XOPEN_SOURCE=600 -w -Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-error=incompatible-pointer-types -Wno-error=implicit-int -Wno-error=return-mismatch"
# WSL's legacy CFLAGS (provenance.json) without -DHAVEDLL, plus the X11 headers
cflags="-g -O2 -ffp-contract=off -fcommon -DNOERRNO -DNON_UNIX_STDIO -DAUTO -DCVODE_YES -DMYSTR1=8.0 -DMYSTR2=1 -DSTRUPR -DMAXW=50 -UWIN32 -I$here/x11-include -I$here/win-include"
$cc $cflags -c "$here/x11-stubs.c" -o x11-stubs.o
mingw32-make -j4 xppaut CC="$cc" CFLAGS="$cflags" LDFLAGS=-fcommon LIBS="x11-stubs.o -lm" > build.log 2>&1 || { tail -30 build.log; exit 1; }
mv xppaut xppaut.exe 2>/dev/null || true
gcc --version | head -1 > compiler.txt
sha256sum xppaut.exe
