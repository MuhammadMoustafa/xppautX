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

# Run per-file checks in parallel and collect output in order.
# xargs -P runs nproc worker processes in parallel; each line is a filename.
verbose="$1"
tmp=$(mktemp -d)
trap "rm -rf '$tmp'" EXIT

# Create a check script that outputs to $tmp based on input
cat > "$tmp/check.sh" << 'CHECKEOF'
#!/bin/sh
CC="$1"
FLAGS="$2"
f="$3"
verbose="$4"
tmp="$5"
# Use basename with path separator replaced to avoid nested dir issues
id=$(echo "$f" | tr '/' '_')
if $CC $FLAGS "$f" 2>/dev/null; then
  touch "$tmp/$id.ok"
else
  if [ "$verbose" = -v ]; then
    echo "needs X11: $f" >> "$tmp/needs"
  fi
fi
CHECKEOF
chmod +x "$tmp/check.sh"

# Collect files (excluding sbml2xpp.c) in glob order
for f in core/*.c; do
  [ "$f" = core/sbml2xpp.c ] && continue
  echo "$f"
done > "$tmp/files.txt"

# Run checks in parallel
nproc=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
cat "$tmp/files.txt" | xargs -P "$nproc" -I {} "$tmp/check.sh" "$CC" "$FLAGS" {} "$verbose" "$tmp"

# Count results (in original glob order)
ok=0
total=$(wc -l < "$tmp/files.txt")
for f in core/*.c; do
  [ "$f" = core/sbml2xpp.c ] && continue
  id=$(echo "$f" | tr '/' '_')
  [ -f "$tmp/$id.ok" ] && ok=$((ok+1))
done

# Emit "needs X11" lines in sorted order
if [ "$verbose" = -v ] && [ -f "$tmp/needs" ]; then
  sort "$tmp/needs"
fi

echo "X11-free: $ok / $total"
