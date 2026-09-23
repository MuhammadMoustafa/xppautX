#!/bin/sh
# Clean build and count compiler warnings by category.
# Uses BUILDDIR=build/warn to avoid disturbing the normal build.
# Outputs: summary to stdout, full warning list to build/warn/warnings.txt
# Usage: tools/warnings.sh
cd "$(dirname "$0")/.." || exit 1

BUILDDIR="build/warn"
mkdir -p "$BUILDDIR" || exit 1

# Clean the warning build directory to force a full rebuild
rm -rf "$BUILDDIR"/* 2>/dev/null || true
mkdir -p "$BUILDDIR" || exit 1

# Save the state of root binaries: we'll verify they aren't modified
XPPAUT_EXISTS=0
XPPAUTX_EXISTS=0
if [ -f xppaut ]; then
  XPPAUT_EXISTS=1
fi
if [ -f xppautX ]; then
  XPPAUTX_EXISTS=1
fi

# Capture all compiler output (stdout + stderr) from a clean build.
# We compile all sources: core + X11 files (via lib) and server files.
# The Makefile's dependency rules will compile all .c files to the BUILDDIR.
echo "Building with BUILDDIR=$BUILDDIR for warning analysis..."
make -j"$(nproc)" BUILDDIR="$BUILDDIR" lib 2>&1 | tee "$BUILDDIR/build.log"

# Also compile the server/xppautx sources (ui_json, xppautx_main, xpp_http)
# These files go into SERVER_SOURCES and get compiled but not linked
make -j"$(nproc)" BUILDDIR="$BUILDDIR" \
  build/warn/xppautx_main.o build/warn/ui_json.o build/warn/xpp_http.o build/warn/web_assets.o 2>&1 | tee -a "$BUILDDIR/build.log"

# Verify we haven't disturbed root binaries
if [ $XPPAUT_EXISTS -eq 1 ] && [ ! -f xppaut ]; then
  echo "WARNING: xppaut was deleted by the build" >&2
fi
if [ $XPPAUTX_EXISTS -eq 1 ] && [ ! -f xppautX ]; then
  echo "WARNING: xppautX was deleted by the build" >&2
fi

# Extract all warning lines (format: file:line:col: warning: message [-Wflag])
# Create two files: one with full warnings, one for processing by flag
grep 'warning:' "$BUILDDIR/build.log" | sort > "$BUILDDIR/warnings.txt" || true

# Count total warnings
total=$(wc -l < "$BUILDDIR/warnings.txt" 2>/dev/null || echo 0)

# Create flag summary by extracting [-W...] patterns
# Lines without a [-W...] tag are tagged as "(untagged)"
awk '
  /warning:/ {
    match($0, /\[-W[^]]*\]/)
    if (RSTART > 0) {
      flag = substr($0, RSTART, RLENGTH)
    } else {
      flag = "(untagged)"
    }
    count[flag]++
  }
  END {
    for (flag in count) {
      printf "%5d %s\n", count[flag], flag
    }
  }
' "$BUILDDIR/build.log" | sort -rn > "$BUILDDIR/flag_counts.txt" || true

# Create file summary
grep -oE '^[^:]+' "$BUILDDIR/warnings.txt" 2>/dev/null | sort | uniq -c | sort -rn > "$BUILDDIR/file_counts.txt" || true

# Print summary
echo ""
echo "=== Clean-build warning summary ==="
echo "Total warnings: $total"
echo ""
echo "Warnings by category:"
awk '{printf "  %s  %s\n", $1, $2}' "$BUILDDIR/flag_counts.txt"

echo ""
echo "Top 20 files by warning count:"
head -20 "$BUILDDIR/file_counts.txt" | awk '{printf "  %s  %s\n", $1, $2}'

echo ""
echo "Full warning list: $BUILDDIR/warnings.txt"
