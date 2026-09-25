#!/bin/sh
# The core allocates through core/xpp_mem.h (xpp_malloc, xpp_calloc,
# xpp_realloc, xpp_strdup, xpp_free), never the C library's malloc,
# calloc, realloc, strdup or free directly (W21): those return NULL
# unchecked where xpp_* fail loudly, they escape xpp_mem's counts, and
# xpp_free must only ever see what xpp_* allocated. Comments are stripped
# first (tools/strip_comments.awk), so dead code needs no entry.
# tools/sourcecheck.sh runs this. Usage: tools/alloccheck.sh
cd "$(dirname "$0")/.." || exit 1

# xpp_mem.cpp is the allocator itself. A new exception (memory a library
# allocates or frees) is listed here and in xpp_mem.h's comment.
EXCLUDE="core/xpp_mem.cpp"

# a call, not a member (x.free(, p->free() or a longer name (xpp_free()
PATTERN='(^|[^a-zA-Z0-9_.>])(malloc|calloc|realloc|strdup|free)[ \t]*\('

bad=0
for f in core/*.cpp core/*.h; do
  [ -f "$f" ] || continue
  case " $EXCLUDE " in *" $f "*) continue ;; esac
  hits=$(awk -f tools/strip_comments.awk "$f" | grep -nE "$PATTERN")
  [ -n "$hits" ] || continue
  printf '%s\n' "$hits" | while IFS=: read -r lineno _; do
    printf '%s:%s: %s\n' "$f" "$lineno" "$(sed -n "${lineno}p" "$f")"
  done
  bad=1
done
if [ $bad -ne 0 ]; then
  echo "alloccheck: the C library's allocator called directly; use xpp_malloc/xpp_calloc/xpp_realloc/xpp_strdup/xpp_free (core/xpp_mem.h)"
  exit 1
fi
echo "alloccheck ok: every core allocation goes through xpp_mem.h"
