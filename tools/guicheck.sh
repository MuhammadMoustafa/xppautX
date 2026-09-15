#!/bin/sh
# Drive the X11 GUI through tools/gui_keys.txt twice, once with a binary
# built from a git ref (default HEAD) and once with ./xppaut, and compare
# the screenshots. Every menu pop-up and a few commands are covered, so a
# refactor that changes what the user sees shows up as a DIFF.
# Needs a display (WSLg, Xvfb, a desktop). Run after `make xppaut`.
# Usage: tools/guicheck.sh [REF]     (screenshots kept in build/guicheck/)
cd "$(dirname "$0")/.." || exit 1
REF=${1:-HEAD}
top=$PWD
out=$top/build/guicheck
rm -rf "$out" && mkdir -p "$out/base" "$out/new" "$out/src" || exit 1
git archive "$REF" | tar -x -C "$out/src" || exit 1
( cd "$out/src" && make -j8 xppaut > build.log 2>&1 ) || { echo "baseline build failed"; exit 1; }
${CC:-gcc} -O1 -o "$out/xdrive" tools/xdrive.c -lX11 || exit 1
for tag in base new; do
  [ $tag = base ] && bin=$out/src/xppaut || bin=$top/xppaut
  run=$out/run_$tag && mkdir -p "$run" && cp examples/ode/lecar.ode "$run/"
  ( cd "$run" && exec "$bin" lecar.ode > run.log 2>&1 ) &
  pid=$!
  "$out/xdrive" tools/gui_keys.txt "$out/$tag"
  kill $pid 2>/dev/null; wait $pid 2>/dev/null
done
same=0; total=0
for f in "$out"/base/*; do
  n=$(basename "$f"); total=$((total+1))
  if cmp -s "$f" "$out/new/$n"; then same=$((same+1)); else echo "DIFF: $n"; fi
done
echo "gui screens identical to $REF: $same / $total"
[ $total -gt 0 ] && [ $same -eq $total ]
