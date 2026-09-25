#!/bin/sh
# Counts unsafe-C idioms still left in the (now all-.cpp) core, as a
# ratchet for the move from "the core is C++" to "the core is safe C++"
# (CLAUDE.md, "C and C++"; the W29 cards). Comments are stripped first
# (tools/strip_comments.awk, as tools/formatcheck.sh does), then every
# core/*.cpp and core/*.h is counted per category:
#   memory  - xpp_malloc/xpp_calloc/xpp_realloc/xpp_free/xpp_strdup calls,
#             and new[]/delete[] (core/xpp_mem.h's own idioms, not yet
#             std::vector/std::string/std::unique_ptr)
#   buffers - fixed-size `char name[N];` declarations (locals or members)
#   text    - xpp_strlcpy/xpp_strlcat/xpp_snprintf/XPP_SPRINTF/XPP_STRCPY/
#             XPP_STRCAT, plus the raw strcpy/strcat/strncpy/sprintf/
#             strtok tools/formatcheck.sh and tools/sourcecheck.sh already
#             hold at 0 (counted here too, so a regression shows in the
#             same ratchet), and printf-style fprintf/vfprintf
#   files   - fopen/fclose/fscanf/fgets/sscanf/feof (core/xpp_io.h's
#             line/token readers and xpp::Writer are the replacement)
#   casts   - a C-style cast: "(type)" or "(type *)"/"(type **)"
#             immediately followed by an identifier, a digit or "(", for
#             a fixed list of builtin/typedef'd types
#             (tools/unsafe_count.awk's re["casts"], documented there).
#             This is a heuristic, not a parse: it can miss a cast to a
#             project struct/typedef not in that list, and does not try
#             every spacing; it is meant to not grow, not to reach 0.
# The actual counting is tools/unsafe_count.awk, loaded together with
# tools/strip_comments.awk so every file is read, and its comments
# stripped, only once.
#
# Modes:
#   (default)  print the per-file table and the grand totals
#   --check    compare against tests/unsafe.baseline, fail if a
#              file/category count grew (a file absent from the baseline
#              starts at 0); a count that dropped is fine
#   --update   rewrite tests/unsafe.baseline from the current tree
#
# Usage: tools/unsafecheck.sh [--check|--update]
cd "$(dirname "$0")/.." || exit 1

BASELINE="tests/unsafe.baseline"
tab="$(printf '\t')"

mode="table"
case "$1" in
  --check) mode="check" ;;
  --update) mode="update" ;;
  "") mode="table" ;;
  *) echo "usage: tools/unsafecheck.sh [--check|--update]" >&2; exit 2 ;;
esac

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

awk -f tools/strip_comments.awk -f tools/unsafe_count.awk core/*.cpp core/*.h \
  | grep "^U${tab}" > "$tmp/raw.txt"

grandtotal=$(awk -F"$tab" '$2=="GRANDTOTAL"{print $3}' "$tmp/raw.txt")
grep "${tab}GRAND${tab}" "$tmp/raw.txt" > "$tmp/grand.txt"
awk -F"$tab" '$2!="GRAND" && $2!="GRANDTOTAL"{print}' "$tmp/raw.txt" > "$tmp/details.txt"

# details.txt: "U<TAB>file<TAB>category<TAB>count". Per-file totals,
# sorted highest first, for the default table.
awk -F"$tab" '{ total[$2]+=$4 } END { for (f in total) printf "%d\t%s\n", total[f], f }' "$tmp/details.txt" \
  | sort -t"$tab" -k1,1nr -k2,2 > "$tmp/totals.txt"

case "$mode" in
  table)
    awk -v tab="$tab" '
      NR == FNR { order++; ofile[order] = $2; ototal[order] = $1; nfiles++; next }
      { n = split($0, a, tab); detail[$2] = detail[$2] " " $3 "=" $4 }
      END {
        for (i = 1; i <= nfiles; i++)
          print ofile[i] " total=" ototal[i] detail[ofile[i]]
      }
    ' FS="$tab" "$tmp/totals.txt" "$tmp/details.txt"
    echo "---"
    while IFS="$tab" read -r _ _ cat n; do
      echo "$cat: $n"
    done < "$tmp/grand.txt"
    echo "total: $grandtotal"
    ;;
  update)
    awk -F"$tab" '{ print $2, $3, $4 }' "$tmp/details.txt" | sort > "$BASELINE"
    echo "unsafe C idioms: $grandtotal (baseline updated: $BASELINE)"
    ;;
  check)
    [ -f "$BASELINE" ] || { echo "unsafecheck: missing $BASELINE (run tools/unsafecheck.sh --update)"; exit 1; }
    baseline_total=$(awk '{s+=$3} END{print s+0}' "$BASELINE")

    # One awk pass comparing every current file/category count against
    # the baseline (0 when a file/category is not in it), instead of a
    # shell loop that would spawn an awk per baseline lookup.
    awk -v tab="$tab" '
      NR == FNR { n = split($0, a, " "); base[a[1] SUBSEP a[2]] = a[3]; next }
      {
        n = split($0, a, tab)
        fn = a[2]; cat = a[3]; cur = a[4] + 0
        b = base[fn SUBSEP cat] + 0
        if (cur > b) print fn "\t" cat "\t" b "\t" cur
      }
    ' "$BASELINE" "$tmp/details.txt" > "$tmp/grew.txt"

    grew=0
    if [ -s "$tmp/grew.txt" ]; then
      grew=1
      while IFS="$tab" read -r fn cat b n; do
        echo "unsafecheck: $fn $cat grew: $b -> $n"
      done < "$tmp/grew.txt"
    fi

    delta=$((grandtotal - baseline_total))
    if [ $grew -eq 1 ]; then
      echo "unsafe C idioms: $grandtotal (baseline $baseline_total, delta $delta)"
      echo "unsafecheck FAILED: unsafe C idioms grew; fix them, or if intended, run tools/unsafecheck.sh --update"
      exit 1
    fi
    if [ "$delta" -lt 0 ]; then
      echo "unsafe C idioms: $grandtotal (baseline $baseline_total): $((0 - delta)) fewer than the baseline: run tools/unsafecheck.sh --update"
    else
      echo "unsafe C idioms: $grandtotal (baseline $baseline_total, delta $delta)"
    fi
    echo "unsafecheck ok: no category grew past the baseline"
    ;;
esac
