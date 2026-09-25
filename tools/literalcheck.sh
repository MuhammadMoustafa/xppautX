#!/bin/sh
# A string literal is const char * (W28): the core never casts one to
# char *, `(char *)"..."` or `const_cast<char *>("...")`, which hides a
# function that should take const char * (or one that writes into its
# argument, which a literal must never reach). Make the parameter const
# instead. Comments are stripped first (tools/strip_comments.awk).
# tools/sourcecheck.sh runs this. Usage: tools/literalcheck.sh
cd "$(dirname "$0")/.." || exit 1

PATTERN='\([ \t]*char[ \t]*\*[ \t]*\)[ \t]*"|const_cast<[ \t]*char[ \t]*\*[ \t]*>[ \t]*\([ \t]*"'

bad=0
for f in core/*.cpp core/*.h; do
  [ -f "$f" ] || continue
  hits=$(awk -f tools/strip_comments.awk "$f" | grep -nE "$PATTERN")
  [ -n "$hits" ] || continue
  printf '%s\n' "$hits" | while IFS=: read -r lineno _; do
    printf '%s:%s: %s\n' "$f" "$lineno" "$(sed -n "${lineno}p" "$f")"
  done
  bad=1
done
if [ $bad -ne 0 ]; then
  echo "literalcheck: a string literal cast to char *; make the parameter const char * instead"
  exit 1
fi
echo "literalcheck ok: no string literal is cast to char *"
