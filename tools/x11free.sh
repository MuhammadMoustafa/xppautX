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
ok=0; total=0
for f in core/*.c; do
  [ "$f" = core/sbml2xpp.c ] && continue
  total=$((total+1))
  if $CC $FLAGS "$f" 2>/dev/null; then ok=$((ok+1)); else [ "$1" = -v ] && echo "needs X11: $f"; fi
done
echo "X11-free: $ok / $total"
