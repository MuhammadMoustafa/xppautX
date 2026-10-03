#!/bin/sh
# The core's API is C++ (W109, maintainer 2026-09-30): C++ linkage, in
# namespace xpp, with C++ types at the boundary. extern "C" stays only
# where C really calls across: the Linux window library's one export
# (xpp_window_plugin_init: dlsym finds it by its C name; the tables it
# trades are C++, both sides built together, W172), and rand_s, which
# the C library header declares only under a macro (W173). This
# check fails on an extern "C" in core/ or tests/ (.cpp and .h, comments
# stripped: tools/strip_comments.awk) that the list below does not allow,
# by file and count: a file with more than its entry allows, or one with
# no entry, fails, naming its lines; a file with fewer fails too, so the
# list follows the stages down (lower or delete its entry).
#
# W109's stages are done (W109f, the last, 2026-10-01; docs/roadmap.md
# "W109: the core's API in C++, in stages"): what is listed is the
# permanent C boundary, and the vendored CVODE's headers, which keep their
# C API until W34 (#72) decides whether SUNDIALS replaces CVODE
# (maintainer, 2026-09-30); never add an entry for new code.
# tools/sourcecheck.sh runs this. Usage: tools/externcheck.sh
cd "$(dirname "$0")/.." || exit 1

# "file count|why"
ALLOW="core/xpp_window.cpp 1|xpp_window_plugin_init, the dlsym export
core/xpp_http.cpp 1|rand_s, the C library's, which stdlib.h declares only under _CRT_RAND_S
core/band.h 1|vendored CVODE, its own C API: W34 (#72) decides whether SUNDIALS replaces it
core/cvband.h 1|vendored CVODE: W34 (#72) decides
core/cvdense.h 1|vendored CVODE: W34 (#72) decides
core/cvode.h 1|vendored CVODE: W34 (#72) decides
core/dense.h 1|vendored CVODE: W34 (#72) decides
core/llnlmath.h 1|vendored CVODE: W34 (#72) decides
core/vector.h 1|vendored CVODE: W34 (#72) decides"

allowed() {
  printf '%s\n' "$ALLOW" | awk -v f="$1" '$1 == f { split($2, a, "|"); print a[1]; exit }'
}

tmp=$(mktemp) || exit 1
trap 'rm -f "$tmp"' EXIT

bad=0
for f in core/*.cpp core/*.h tests/*.cpp tests/*.h; do
  [ -f "$f" ] || continue
  awk -f tools/strip_comments.awk "$f" | grep -n 'extern[[:space:]]*"C"' > "$tmp"
  n=$(grep -c '' "$tmp")
  want=$(allowed "$f")
  want=${want:-0}
  if [ "$n" -gt "$want" ]; then
    echo "externcheck: $f has $n extern \"C\" (allowed $want):"
    while IFS=: read -r lineno _; do
      printf '  %s:%s: %s\n' "$f" "$lineno" "$(sed -n "${lineno}p" "$f")"
    done < "$tmp"
    bad=1
  elif [ "$n" -lt "$want" ]; then
    echo "externcheck: $f has $n extern \"C\", its entry allows $want: lower or delete the entry in tools/externcheck.sh"
    bad=1
  fi
done
# an entry for a file that is gone
printf '%s\n' "$ALLOW" | awk '{print $1}' | while read -r f; do
  [ -f "$f" ] || { echo "externcheck: $f is listed but does not exist: delete its entry"; exit 1; }
done || bad=1

total=$(printf '%s\n' "$ALLOW" | awk '{split($2, a, "|"); s += a[1]} END {print s}')
headers=$(printf '%s\n' "$ALLOW" | awk '$1 ~ /\.h$/' | wc -l | tr -d ' ')
if [ $bad -ne 0 ]; then
  echo "externcheck: the core's API is C++ (CLAUDE.md \"C and C++\"): extern \"C\" only where C calls across"
  exit 1
fi
echo "externcheck ok: $total extern \"C\" left, as listed ($headers headers)"
