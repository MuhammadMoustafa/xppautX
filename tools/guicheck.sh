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
# both runs use the same directory, so file dialogs show the same path;
# files the session writes (clone, save as, ...) are compared too
run=$out/run
for tag in base new; do
  [ $tag = base ] && bin=$out/src/xppaut || bin=$top/xppaut
  rm -rf "$run" && mkdir -p "$run" && cp examples/ode/lecar.ode "$run/"
  ( cd "$run" && exec "$bin" lecar.ode > ../$tag.log 2>&1 ) &
  pid=$!
  "$out/xdrive" tools/gui_keys.txt "$out/$tag"
  kill $pid 2>/dev/null; wait $pid 2>/dev/null
  mkdir -p "$out/files_$tag" && cp -r "$run"/. "$out/files_$tag/"
  # a clone starts with its creation time
  [ -f "$out/files_$tag/clone.ode" ] && sed -i '1d' "$out/files_$tag/clone.ode"
done
same=0; total=0
for f in "$out"/base/*; do
  n=$(basename "$f"); total=$((total+1))
  if cmp -s "$f" "$out/new/$n"; then same=$((same+1)); else echo "DIFF: $n"; fi
done
echo "gui screens identical to $REF: $same / $total"
fsame=0; ftotal=0
for f in "$out"/files_base/*; do
  n=$(basename "$f"); ftotal=$((ftotal+1))
  if cmp -s "$f" "$out/files_new/$n"; then fsame=$((fsame+1)); else echo "DIFF file: $n"; fi
done
echo "files written identical: $fsame / $ftotal ($(ls "$out"/files_base | tr '\n' ' '))"
[ $total -gt 0 ] && [ $same -eq $total ] && [ $fsame -eq $ftotal ]
