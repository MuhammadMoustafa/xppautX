#!/bin/sh
# The core's API is C++ (W109, maintainer 2026-09-30): C++ linkage, in
# namespace xpp, with C++ types at the boundary. extern "C" stays only
# where C really calls across: the Linux window library's one export and
# its table (xpp_window_plugin.h: dlsym finds it by its C name), data the
# build generates as C (tools/embed.c's web_assets.c, tools/embed_bytes.c's
# icon and window library), a function of the C library that a header
# declares only under a macro, and a callback a C library calls. This
# check fails on an extern "C" in core/ or tests/ (.cpp and .h, comments
# stripped: tools/strip_comments.awk) that the list below does not allow,
# by file and count: a file with more than its entry allows, or one with
# no entry, fails, naming its lines; a file with fewer fails too, so the
# list follows the stages down (lower or delete its entry).
#
# Until W109's stages finish (docs/roadmap.md, "W109: the core's API in
# C++, in stages"), the C API they have not reached yet is listed with
# the stage card that converts it; never add an entry for new code. The
# vendored CVODE's headers keep theirs until W34 (#72) decides whether
# SUNDIALS replaces CVODE (maintainer, 2026-09-30).
# tools/sourcecheck.sh runs this. Usage: tools/externcheck.sh
cd "$(dirname "$0")/.." || exit 1

# "file count|why"
ALLOW="core/xpp_window_plugin.h 1|the window library's C ABI: libxppwindow.so's one export, found by dlsym by its C name, and the tables it trades (W13e)
core/xpp_window.cpp 3|xpp_window_plugin_init, that export; xpp_icon_png and its length, C data tools/embed_bytes.c generates
core/xpp_window_loader.cpp 2|xpp_window_lib and its length, C data tools/embed_bytes.c generates
core/xpp_http.cpp 2|xpp_web_assets, C data tools/embed.c generates; rand_s, the C library's, which stdlib.h declares only under _CRT_RAND_S
core/band.h 1|vendored CVODE, its own C API: W34 (#72) decides whether SUNDIALS replaces it
core/cvband.h 1|vendored CVODE: W34 (#72) decides
core/cvdense.h 1|vendored CVODE: W34 (#72) decides
core/cvode.h 1|vendored CVODE: W34 (#72) decides
core/dense.h 1|vendored CVODE: W34 (#72) decides
core/llnlmath.h 1|vendored CVODE: W34 (#72) decides
core/vector.h 1|vendored CVODE: W34 (#72) decides
core/xpp_http.h 1|W109f: the threads and the window's edges (tests/test_job.c, C, calls xpp_job.h)
core/xpp_inbox.h 1|W109f
core/xpp_job.h 1|W109f
core/xpp_webview.h 1|W109f
core/xpp_win32.h 1|W109f
core/xpp_window.h 1|W109f"

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
