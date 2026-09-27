#!/bin/sh
# Counts the file operations the core still makes directly instead of
# through core/xpp_files (W32b; CLAUDE.md "Single source": files and
# folders are xpp_files', reading and writing text xpp_io's): calls of
# fopen, freopen, remove, rename, unlink, mkdir, rmdir, opendir and
# tmpfile in core/*.cpp and core/*.h, comments and string literals
# stripped first (tools/strip_comments.awk, as tools/formatcheck.sh does).
# xpp_files.cpp and xpp_io.cpp (the modules that own them) and
# xpp_win32.cpp (the Windows API's file) are not counted. A member call
# (x.remove(), p->rename()) or a longer name (xpp_files_remove) is not a
# match; std::fopen and ::remove are.
#
# The replacements: xpp::Writer (write, binary, append; a replace only at
# commit), xpp::LineReader/TokenReader and xpp::open_read for reading,
# xpp_files_open_stream for a stream kept open across calls,
# xpp_files_copy/prepend/move/remove/exists/dir_writable, and the
# xpp_files temp folders.
#
# tests/files.baseline is the count per file this started from: the W33
# sweeps' to-do list. A file that grows past it fails --check; one that
# drops below it asks for --update.
#
# Modes:
#   (default)  print the per-file counts and the total
#   --check    compare against tests/files.baseline (sourcecheck.sh)
#   --update   rewrite tests/files.baseline from the current tree
# Usage: tools/filecheck.sh [--check|--update]
cd "$(dirname "$0")/.." || exit 1
LC_ALL=C
export LC_ALL

BASELINE="tests/files.baseline"
mode="table"
case "$1" in
  --check) mode="check" ;;
  --update) mode="update" ;;
  "") mode="table" ;;
  *) echo "usage: tools/filecheck.sh [--check|--update]" >&2; exit 2 ;;
esac

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

for f in core/*.cpp core/*.h; do
  case "$f" in
    core/xpp_files.cpp|core/xpp_io.cpp|core/xpp_win32.cpp) continue ;;
  esac
  awk -f tools/strip_comments.awk "$f" | awk -v file="$f" '
    {
      line = " " $0
      gsub(/"([^"\\]|\\.)*"/, "\"\"", line)
      while (match(line, /[^A-Za-z0-9_.>](fopen|freopen|remove|rename|unlink|mkdir|rmdir|opendir|tmpfile)[ \t]*\(/)) {
        n++
        line = substr(line, RSTART + RLENGTH)
      }
    }
    END { if (n > 0) print file, n }'
done | sort > "$tmp/now.txt"

total=$(awk '{ s += $2 } END { print s + 0 }' "$tmp/now.txt")

case "$mode" in
  table)
    sort -k2,2nr -k1,1 "$tmp/now.txt"
    echo "---"
    echo "direct file operations: $total in $(wc -l < "$tmp/now.txt" | tr -d ' ') files"
    ;;
  update)
    cp "$tmp/now.txt" "$BASELINE"
    echo "direct file operations: $total (baseline updated: $BASELINE)"
    ;;
  check)
    [ -f "$BASELINE" ] || { echo "filecheck: missing $BASELINE (run tools/filecheck.sh --update)"; exit 1; }
    base_total=$(awk '{ s += $2 } END { print s + 0 }' "$BASELINE")
    awk '
      NR == FNR { base[$1] = $2; next }
      { b = base[$1] + 0; if ($2 > b) print "filecheck: " $1 " grew: " b " -> " $2 }
    ' "$BASELINE" "$tmp/now.txt" > "$tmp/grew.txt"
    if [ -s "$tmp/grew.txt" ]; then
      cat "$tmp/grew.txt"
      echo "direct file operations: $total (baseline $base_total)"
      echo "filecheck FAILED: open, copy, move and delete files through core/xpp_files.h (and xpp_io.h's handles)"
      exit 1
    fi
    if [ "$total" -lt "$base_total" ]; then
      echo "direct file operations: $total (baseline $base_total): $((base_total - total)) fewer than the baseline: run tools/filecheck.sh --update"
    else
      echo "direct file operations: $total (baseline $base_total)"
    fi
    echo "filecheck ok: no file grew past the baseline"
    ;;
esac
