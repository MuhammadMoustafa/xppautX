#!/bin/sh
# Global state audit (W47a, W120; docs/roadmap.md): counts, per core
# source, the mutable data its object defines, in two kinds, and compares
# them with a baseline each:
#   external  what nm calls B (.bss), D (.data) and C (common), thread-local
#             data too: a global other files can reach
#             (tests/globals.baseline)
#   internal  b and d: a file-scope static, an anonymous-namespace variable
#             or a static inside a function, which no other file reaches
#             but which holds state all the same (tests/globals_internal.baseline,
#             W120)
# Neither counts the symbols in .data.rel.ro, const data the loader
# relocates (a const table of pointers), a static local's guard variable
# (its static is counted) or <iostream>'s own ios_base::Init object. A
# value nothing writes after its initialisation is const, what a load
# makes is the Model's and what a run changes the Session's (CLAUDE.md
# "No global state"); what is left is listed in the baselines, each line
# `FILE COUNT REASON`, the reason saying why the file keeps it or which
# card takes it.
#   tools/globalcheck.sh              the table: each file's symbols
#   tools/globalcheck.sh --check      fail when a file has more than its
#                                     baseline count, or a baseline line
#                                     has no reason
#   tools/globalcheck.sh --update     rewrite the counts in the baselines
#                                     (a file's reason is kept; a new file
#                                     gets "REASON?", which --check fails
#                                     until it is written)
#   --builddir DIR                    read the objects already built in DIR
#                                     (tools/sourcecheck.sh passes
#                                     build/deadcode, which deadcode.sh has
#                                     just built); without it, build/obj is
#                                     brought up to date with make first
# Linux/WSL (GNU nm's sysv format names each symbol's section).
cd "$(dirname "$0")/.." || exit 1

mode="table"
builddir=""
while [ $# -gt 0 ]; do
  case "$1" in
    --check) mode="check" ;;
    --update) mode="update" ;;
    --builddir) shift; builddir="$1" ;;
    *) echo "usage: tools/globalcheck.sh [--check|--update] [--builddir DIR]" >&2; exit 2 ;;
  esac
  shift
done

if [ "$(uname -s)" != Linux ]; then
  echo "globalcheck: Linux only (GNU nm's section column); skipped"
  exit 0
fi
if [ -z "$builddir" ]; then
  builddir=build/obj
  if command -v nproc >/dev/null 2>&1; then NPROC=$(nproc); else NPROC=4; fi
  make -s -j"$NPROC" xppautx >/dev/null || { echo "globalcheck: build failed"; exit 1; }
fi

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

# each line: KIND FILE SYMBOL (KIND external or internal)
: > "$tmp/syms.txt"
for src in core/*.cpp; do
  base=$(basename "$src" .cpp)
  for obj in "$builddir/$base.o" "$builddir/window/$base.o"; do
    [ -f "$obj" ] || continue
    # sysv: name|value|class|type|size|line|section
    nm --defined-only -f sysv "$obj" 2>/dev/null | awk -F'|' -v f="$src" '
      NF >= 7 {
        name = $1; cls = $3; sec = $7
        gsub(/ /, "", name); gsub(/ /, "", cls); gsub(/ /, "", sec)
        if (sec ~ /^\.data\.rel\.ro/) next
        if (cls == "B" || cls == "D" || cls == "C") kind = "external"
        else if (cls == "b" || cls == "d") kind = "internal"
        else next
        if (name ~ /^_ZGV/ || name == "_ZStL8__ioinit") next
        print kind, f, name
      }' >> "$tmp/syms.txt"
  done
done
if [ ! -s "$tmp/syms.txt" ] && ! ls "$builddir"/*.o >/dev/null 2>&1; then
  echo "globalcheck: no objects in $builddir"
  exit 1
fi
sort -u "$tmp/syms.txt" > "$tmp/sorted.txt"

baseline_of() {
  if [ "$1" = external ]; then echo tests/globals.baseline; else echo tests/globals_internal.baseline; fi
}

failed=0
for kind in external internal; do
  BASELINE=$(baseline_of $kind)
  awk -v k=$kind '$1 == k { print $2, $3 }' "$tmp/sorted.txt" > "$tmp/$kind.txt"
  awk '{ n[$1]++ } END { for (f in n) print f, n[f] }' "$tmp/$kind.txt" | sort > "$tmp/$kind.counts"
  total=$(wc -l < "$tmp/$kind.txt" | tr -d ' ')
  case "$mode" in
    table)
      while read -r f s; do
        printf '%s %s %s\n' "$kind" "$f" "$(echo "$s" | c++filt)"
      done < "$tmp/$kind.txt"
      echo "--- $kind mutable data symbols: $total"
      ;;
    update)
      [ -f "$BASELINE" ] || : > "$BASELINE"
      # keep each file's reason; a new file's line says REASON?
      awk 'FILENAME == ARGV[1] { f = $1; $1 = ""; $2 = ""; sub(/^ +/, ""); reason[f] = $0; next }
           { r = ($1 in reason && reason[$1] != "") ? reason[$1] : "REASON?"; print $1, $2, r }' \
        "$BASELINE" "$tmp/$kind.counts" > "$tmp/$kind.new"
      cp "$tmp/$kind.new" "$BASELINE"
      echo "$kind mutable data symbols: $total (baseline updated: $BASELINE)"
      ;;
    check)
      [ -f "$BASELINE" ] || { echo "globalcheck: missing $BASELINE (run tools/globalcheck.sh --update)"; exit 1; }
      baseline_total=$(awk '{ s += $2 } END { print s + 0 }' "$BASELINE")
      awk 'NF < 3 || $3 == "REASON?" { print FILENAME ": " $1 " has no reason" }' "$BASELINE" > "$tmp/noreason.txt"
      if [ -s "$tmp/noreason.txt" ]; then
        sed 's/^/globalcheck: /' "$tmp/noreason.txt"
        failed=1
      fi
      awk 'FILENAME == ARGV[1] { base[$1] = $2; next }
           { b = base[$1] + 0; if ($2 > b) print $1, b, $2 }' "$BASELINE" "$tmp/$kind.counts" > "$tmp/grew.txt"
      if [ -s "$tmp/grew.txt" ]; then
        while read -r f b n; do
          echo "globalcheck: $f grew: $b -> $n $kind mutable data symbols:"
          awk -v f="$f" '$1 == f { print $2 }' "$tmp/$kind.txt" | c++filt | sed 's/^/  /'
        done < "$tmp/grew.txt"
        failed=1
      fi
      if [ "$total" -lt "$baseline_total" ]; then
        echo "$kind mutable data symbols: $total (baseline $baseline_total): $((baseline_total - total)) fewer than the baseline: run tools/globalcheck.sh --update"
      else
        echo "$kind mutable data symbols: $total (baseline $baseline_total)"
      fi
      ;;
  esac
done
if [ "$mode" = check ]; then
  if [ "$failed" != 0 ]; then
    echo "globalcheck FAILED: new global state; put it in its owner's struct (the Model's or the Session's), make it const, or say in the baseline why it stays (CLAUDE.md \"No global state\")"
    exit 1
  fi
  echo "globalcheck ok: no file has more global state than its baseline, and every baseline line has its reason"
fi
