#!/bin/sh
# Code nothing reaches (W24): `make deadcode` builds every object at -O0
# with -ffunction-sections -fdata-sections -fno-common into build/deadcode
# and links xppautX from the objects themselves, the Linux window library
# and every unit test with --gc-sections; the linker's --print-gc-sections
# says which of our functions and file-scope data it dropped. A symbol is
# live if any of those links keeps it: gc-sections follows every
# relocation, so a function reached only through a table of pointers (the
# parser's builtins, the XppUi table, menus) is live. This lists the rest
# by source file ("whole file" when nothing of it is kept), and apart what
# only a unit test reaches: dead product code unless the test is why it
# exists, which an entry in ALLOW below says.
# -O0, since an optimizer inlines a function into its callers and drops its
# own copy, which would read as unreached. Not counted: inline functions
# and template instances (weak, or the standard library's), names with a
# dot (a function's static, a compiler clone: they go with it), and local
# read-only data (C++ folds a constant into its uses and drops its storage;
# gcc already warns about an unused static const in C).
# Linux only: MinGW's linker keeps every function its unwind tables name
# (all of them), and macOS's has no --print-gc-sections. Code only Windows
# or macOS compiles (_WIN32, __APPLE__) is thus never listed; code Linux
# compiles but only they call would be, with an entry below saying so; the
# Windows build's link is what fails if something it calls is removed.
# Usage: tools/deadcode.sh [--check] [--no-build] [make variables...]
#   --check     exit 1 when anything outside ALLOW is listed
#               (tools/sourcecheck.sh, which CI's source job runs)
#   --no-build  report from the last build
cd "$(dirname "$0")/.." || exit 1

# What stays although no Linux link reaches it: "file symbol|reason", the
# symbol as reported (C++ names demangled, without their parameters), or
# "file *|reason" for a whole file.
ALLOW="core/xpp_io.cpp xpp_strlcat_at|XPP_STRCAT/xpp_strlcat, the append of the xpp_io.h API CLAUDE.md prescribes; test_io
core/xpp_io.cpp (anon)::bounded_len|xpp_strlcat_at's helper
core/xpp_io.cpp xpp_token_reader_string|xpp_io.h's fscanf-%s counterpart CLAUDE.md prescribes; test_io
core/xpp_io.cpp xpp_writer_printf|xpp_io.h's fprintf over a writer, named in CLAUDE.md; test_io
core/auto_settings.cpp auto_settings_num_ok|test_auto_settings' view of the field check auto_settings_apply makes
core/auto_stop.cpp auto_stop_key|test_auto_stop's view of the keys auto_stop_last hands the protocol"

check=0
build=1
while [ $# -gt 0 ]; do
  case "$1" in
    --check) check=1 ;;
    --no-build) build=0 ;;
    *) break ;;
  esac
  shift
done
if [ "$(uname -s)" != Linux ]; then
  echo "deadcode: Linux only (the linker report it reads); skipped"
  exit 0
fi
MAKE=${MAKE:-make}
B=build/deadcode
if command -v nproc >/dev/null 2>&1; then
  NPROC=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
  NPROC=$(sysctl -n hw.ncpu)
else
  NPROC=4
fi
mkdir -p build || exit 1
if [ $build = 1 ]; then
  if ! $MAKE -j"$NPROC" deadcode "$@" > $B.log 2>&1; then
    grep -v 'removing unused section' $B.log | tail -30
    echo "deadcode: the build failed ($B.log)"
    exit 1
  fi
fi
[ -f $B/gc-xppautX.log ] || { echo "deadcode: no $B/gc-xppautX.log (run without --no-build)"; exit 1; }

# (object symbol class) of what the links dropped, from our own objects
# only: not the tests', not generated code (web_assets, window_lib, icons)
# nor the vendored web view's; class ro for read-only data
cat $B/gc-*.log | sed -n "s/.*removing unused section '\([^']*\)' in file '\($(echo $B | sed 's/[/.]/\\&/g')\/[^']*\.o\)'.*/\2 \1/p" |
  grep -v -e '/tests/' -e '/web_assets\.o ' -e '/window_lib\.o ' -e '/icon_assets\.o ' -e '/webview\.o ' |
  sed -E 's/ \.(rodata|data\.rel\.ro\.local|data\.rel\.ro)[.$]/ ro /; s/ \.(text|data|bss|tbss|tdata|data\.rel\.local|data\.rel)(\.(unlikely|startup|hot|exit))?[.$]/ x /' |
  awk 'NF == 3 && $3 !~ /[.]/ {print $1, $3, $2}' | sort -u > $B/dropped.txt

# the symbols each object defines, but the standard library's: "object
# symbol type". A name starting with __ is the compiler's or the library's
# (reserved), plain or as a mangled file-static name (_ZL18__...), e.g.
# libstdc++'s __gthread_active_p, which gcc 13 emits into
# an object and gcc 15 does not (CI's source job, 2026-09-25)
for o in $(cut -d' ' -f1 $B/dropped.txt | sort -u); do
  nm --defined-only "$o" | awk -v o="$o" '$2 ~ /^[TtDdBbRr]$/ && $3 !~ /[.]/ && $3 !~ /^_ZN?K?(St|9__gnu_cxx)/ && $3 !~ /^(_ZL?[0-9]+)?__/ {print o, $3, $2}'
done | sort -u > $B/defined.txt
# what a unit test keeps
for t in $B/tests/test_*; do
  case "$t" in *.o|*.d) continue ;; esac
  nm --defined-only "$t" | awk '{print $3}'
done | sort -u > $B/tests.txt

# dead: dropped and defined, but local read-only data; "tested" when a
# test keeps it
DEMANGLE=cat
command -v c++filt >/dev/null 2>&1 && DEMANGLE=c++filt
awk -v tests=$B/tests.txt -v defs=$B/defined.txt -v b=$B/ '
  BEGIN { while ((getline s < tests) > 0) intest[s] = 1
          while ((getline l < defs) > 0) { split(l, f, " "); type[f[1] " " f[2]] = f[3] } }
  { t = type[$1 " " $2]
    if (t == "" || ($3 == "ro" && t ~ /[a-z]/)) next
    o = $1; sub("^" b "(window/)?", "", o); sub(/\.o$/, "", o)
    print o, $2, ($2 in intest) ? "tested" : "dead" }' $B/dropped.txt > $B/dead.raw
# display names: C++ demangled, parameters dropped
cut -d' ' -f2 $B/dead.raw | $DEMANGLE | sed 's/(anonymous namespace)/(anon)/g; s/(\([^a]\|$\).*//; s/ /_/g' > $B/dead.names
paste -d' ' $B/dead.raw $B/dead.names > $B/dead.txt

# a whole file: everything it defines is dead (tested or not), local
# constants aside
awk -v b=$B/ '$3 != "r" { o = $1; sub("^" b "(window/)?", "", o); sub(/\.o$/, "", o); print o }' $B/defined.txt |
  sort | uniq -c | awk '{print $2, $1}' > $B/defined.count

# the source of an object: core/x.c or core/x.cpp
src() { if [ -f core/$1.cpp ]; then echo core/$1.cpp; else echo core/$1.c; fi; }

report=$B/report.txt
: > $report
for o in $(cut -d' ' -f1 $B/dead.txt | sort -u); do
  f=$(src "$o")
  total=$(awk -v o="$o" '$1 == o {print $2}' $B/defined.count)
  ndead=$(awk -v o="$o" '$1 == o' $B/dead.txt | wc -l | tr -d ' ')
  allowed_file=$(printf '%s\n' "$ALLOW" | grep -c -F "$f *|")
  if [ "$ndead" = "$total" ]; then
    tag="whole file"
  else
    tag="$ndead of $total"
  fi
  dead=$(awk -v o="$o" '$1 == o && $3 == "dead" {print $4}' $B/dead.txt | sort)
  tested=$(awk -v o="$o" '$1 == o && $3 == "tested" {print $4}' $B/dead.txt | sort)
  for s in $dead; do
    printf '%s\n' "$ALLOW" | grep -q -F "$f $s|" && continue
    [ "$allowed_file" = 0 ] || continue
    echo "$f $s" >> $report.new
  done
  for s in $tested; do
    printf '%s\n' "$ALLOW" | grep -q -F "$f $s|" && continue
    [ "$allowed_file" = 0 ] || continue
    echo "$f $s (only a unit test)" >> $report.new
  done
  [ -f $report.new ] || continue
  echo "$f ($tag):" >> $report
  sed 's/^[^ ]* /  /' $report.new | sort -u >> $report
  rm -f $report.new
done

# an entry that names nothing listed is stale: its code went or is reached
printf '%s\n' "$ALLOW" | sed 's/|.*//' | while read -r f s; do
  if [ "$s" = "*" ]; then
    [ -f "$f" ] && continue
  else
    awk -v f="$f" -v s="$s" '{ g = "core/" $1 } (g ".c" == f || g ".cpp" == f) && $4 == s { found = 1 } END { exit !found }' $B/dead.txt && continue
  fi
  echo "stale ALLOW entry: $f $s"
done >> $report

n=$(grep -c '^  \|^stale' $report)
if [ "$n" -gt 0 ]; then
  cat $report
  echo "deadcode: $n unreached or stale ($report)"
  [ $check = 1 ] && exit 1
else
  echo "deadcode: nothing unreached beyond the allowlist"
fi
exit 0
