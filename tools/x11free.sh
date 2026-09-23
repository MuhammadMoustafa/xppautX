#!/bin/sh
# Count core/*.c files that compile with X11 headers made unavailable.
# This is the progress metric for splitting numerics from the X11 UI.
# Usage: tools/x11free.sh [-v]   (-v lists the files that still need X11)
set -e
cd "$(dirname "$0")/.."
stub=build/nox11/X11
mkdir -p "$stub"
for h in Xlib.h Xutil.h Xos.h Xatom.h keysym.h cursorfont.h Xresource.h Xproto.h; do
  printf '#error "X11 header %s pulled in"\n' "$h" > "$stub/$h"
done
CC=${CC:-gcc}
FLAGS="-std=c99 -D_XOPEN_SOURCE=600 -w -DNOERRNO -DNON_UNIX_STDIO -DAUTO -DCVODE_YES -DHAVEDLL -DMYSTR1=0 -DMYSTR2=0 -Ibuild/nox11 -I. -Icore -Icore/bitmaps -fcommon -fsyntax-only"
# one compiler per core; the results are listed in file order
ls core/*.c | grep -v sbml2xpp | xargs -P "$(nproc 2>/dev/null || echo 4)" -n 1 sh -c \
  'if '"$CC $FLAGS"' "$0" 2>/dev/null; then echo "ok $0"; else echo "needs X11: $0"; fi' | sort -k 3 > build/nox11/result.txt
[ "$1" = -v ] && grep '^needs' build/nox11/result.txt
echo "X11-free: $(grep -c '^ok' build/nox11/result.txt) / $(wc -l < build/nox11/result.txt)"
