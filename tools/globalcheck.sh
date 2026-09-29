#!/bin/sh
# Global state audit (W47a, docs/roadmap.md): counts, per core source, the
# external mutable data symbols its object defines -- what nm calls B
# (.bss), D (.data) and C (common), and thread-local data too, but not the
# D symbols in .data.rel.ro, which are const data the loader relocates (a
# const table of pointers) -- and compares them with tests/globals.baseline.
# A global nothing writes after its initialisation is const, and one file's
# own state has internal linkage (CLAUDE.md "No global state"), so neither
# counts here; what is left is state W47b-d move into xpp::Model and
# xpp::Session.
#   tools/globalcheck.sh              the table: each file's symbols
#   tools/globalcheck.sh --check      fail when a file has more than its
#                                     baseline count
#   tools/globalcheck.sh --update     rewrite the baseline
#   --builddir DIR                    read the objects already built in DIR
#                                     (tools/sourcecheck.sh passes
#                                     build/deadcode, which deadcode.sh has
#                                     just built); without it, build/obj is
#                                     brought up to date with make first
# Linux/WSL (GNU nm's sysv format names each symbol's section).
cd "$(dirname "$0")/.." || exit 1

BASELINE="tests/globals.baseline"
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

: > "$tmp/syms.txt"
for src in core/*.cpp; do
  base=$(basename "$src" .cpp)
  for obj in "$builddir/$base.o" "$builddir/window/$base.o"; do
    [ -f "$obj" ] || continue
    # sysv: name|value|class|type|size|line|section
    nm -g --defined-only -f sysv "$obj" 2>/dev/null | awk -F'|' -v f="$src" '
      NF >= 7 {
        name = $1; cls = $3; sec = $7
        gsub(/ /, "", name); gsub(/ /, "", cls); gsub(/ /, "", sec)
        if (cls != "B" && cls != "D" && cls != "C") next
        if (sec ~ /^\.data\.rel\.ro/) next
        print f, name
      }' >> "$tmp/syms.txt"
  done
done
if [ ! -s "$tmp/syms.txt" ] && ! ls "$builddir"/*.o >/dev/null 2>&1; then
  echo "globalcheck: no objects in $builddir"
  exit 1
fi

sort -u "$tmp/syms.txt" > "$tmp/sorted.txt"
awk '{ n[$1]++ } END { for (f in n) print f, n[f] }' "$tmp/sorted.txt" | sort > "$tmp/counts.txt"
total=$(wc -l < "$tmp/sorted.txt" | tr -d ' ')

case "$mode" in
  table)
    awk '{ print $1 " " $2 }' "$tmp/sorted.txt" | while read -r f s; do
      printf '%s %s\n' "$f" "$(echo "$s" | c++filt)"
    done
    echo "---"
    echo "external mutable data symbols: $total"
    ;;
  update)
    cp "$tmp/counts.txt" "$BASELINE"
    echo "external mutable data symbols: $total (baseline updated: $BASELINE)"
    ;;
  check)
    [ -f "$BASELINE" ] || { echo "globalcheck: missing $BASELINE (run tools/globalcheck.sh --update)"; exit 1; }
    baseline_total=$(awk '{ s += $2 } END { print s + 0 }' "$BASELINE")
    awk 'FILENAME == ARGV[1] { base[$1] = $2; next }
         { b = base[$1] + 0; if ($2 > b) print $1, b, $2 }' "$BASELINE" "$tmp/counts.txt" > "$tmp/grew.txt"
    if [ -s "$tmp/grew.txt" ]; then
      while read -r f b n; do
        echo "globalcheck: $f grew: $b -> $n external mutable data symbols:"
        awk -v f="$f" '$1 == f { print $2 }' "$tmp/sorted.txt" | c++filt | sed 's/^/  /'
      done < "$tmp/grew.txt"
      echo "external mutable data symbols: $total (baseline $baseline_total)"
      echo "globalcheck FAILED: new global state; put it in its owner's struct, make it const, or give it internal linkage (CLAUDE.md \"No global state\")"
      exit 1
    fi
    if [ "$total" -lt "$baseline_total" ]; then
      echo "external mutable data symbols: $total (baseline $baseline_total): $((baseline_total - total)) fewer than the baseline: run tools/globalcheck.sh --update"
    else
      echo "external mutable data symbols: $total (baseline $baseline_total)"
    fi
    echo "globalcheck ok: no file has more global state than its baseline"
    ;;
esac
