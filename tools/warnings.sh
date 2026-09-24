#!/bin/sh
# Count the warnings of a clean build, by -W flag and by file (issue #5).
# Compiles every object of both programs into build/warn, links nothing,
# so the normal build and the binaries in the tree are left alone.
# The full list goes to build/warn/warnings.txt.
cd "$(dirname "$0")/.." || exit 1
out=build/warn
rm -rf "$out" && mkdir -p "$out" || exit 1
make -j"$(nproc 2>/dev/null || echo 4)" BUILDDIR="$out" objects > "$out/build.log" 2>&1 || {
  grep ' error:' "$out/build.log" | head; echo "warnings.sh: the build failed"; exit 1; }
grep 'warning:' "$out/build.log" | sort -u > "$out/warnings.txt"
echo "clean-build warnings: $(wc -l < "$out/warnings.txt")"
echo "by flag:"
sed -n 's/.*\(\[-W[^]]*\]\)$/\1/p; /\]$/!s/.*/(untagged)/p' "$out/warnings.txt" | sort | uniq -c | sort -rn
echo "top files:"
cut -d: -f1 "$out/warnings.txt" | sort | uniq -c | sort -rn | head -20
