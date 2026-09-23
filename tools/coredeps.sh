#!/bin/sh
# Link-level coupling metric: symbols that the X11-free objects import from
# objects that still need X11. Zero means the numerics can be linked into a
# library without the X11 front end. Run after a full build.
# Usage: tools/coredeps.sh [-v]   (-v lists symbol, defining file, importing file)
cd "$(dirname "$0")/.."
tools/x11free.sh -v | grep 'needs X11' | sed 's|needs X11: core/||; s|\.cpp$||; s|\.c$||' | sort > build/x11set.txt
ls core/*.c core/*.cpp 2>/dev/null | xargs -n1 basename | sed 's/\.cpp$//; s/\.c$//' | grep -vE 'sbml2xpp|xppaut_main' | sort | comm -23 - build/x11set.txt > build/pureset.txt
for f in $(cat build/x11set.txt); do nm --defined-only -g build/obj/$f.o 2>/dev/null | awk -v f=$f '{print $3" "f}'; done | sort -u > build/x11defs.txt
: > build/coredeps.txt
for f in $(cat build/pureset.txt); do
  nm -u build/obj/$f.o | awk '{print $2}' | sort -u | while read s; do
    d=$(grep -m1 "^$s " build/x11defs.txt | cut -d' ' -f2)
    [ -n "$d" ] && echo "$s $d $f" >> build/coredeps.txt
  done
done
[ "$1" = -v ] && awk '{print $1" ("$2")"}' build/coredeps.txt | sort | uniq -c | sort -rn
echo "core->ui imports: $(cut -d' ' -f1 build/coredeps.txt | sort -u | wc -l) symbols from $(cut -d' ' -f2 build/coredeps.txt | sort -u | wc -l) ui files, used by $(cut -d' ' -f3 build/coredeps.txt | sort -u | wc -l) core files"
