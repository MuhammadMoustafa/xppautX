#!/bin/sh
# The core formats into and copies text through core/xpp_io.h, never
# sprintf/strcpy/vsprintf into a fixed buffer (issue: W11 step 2): those
# write with no length, so an oversized name or expression silently
# overflows; XPP_SPRINTF/XPP_STRCPY (a real array destination) and
# xpp_snprintf/xpp_strlcpy (an explicit size, for a pointer destination)
# never do. tools/verify.sh runs this. Usage: tools/formatcheck.sh
cd "$(dirname "$0")/.." || exit 1

# xpp_http.cpp is excluded: W7 converts it separately (CLAUDE.md).
# xpp_io.cpp is the module itself -- the one place allowed to call the C
# library's vsnprintf/memcpy directly.
EXCLUDE="core/xpp_http.cpp core/xpp_io.cpp"

# Comments are stripped (see strip_comments below) before matching, so a
# sprintf/strcpy left inside dead/commented-out code does not need an
# entry here. Each entry is "file|substring of the matching line|why it
# stays direct". The substring only has to be unique enough to identify
# that one call; it is matched literally (grep -F), so a line-number
# shift needs no edit here.
ALLOW="core/browse_data.cpp|strcpy(ode_names[j-1],ode_names[j]);|ode_names[j-1]/[j] are both pointers allocated elsewhere at a length tied to that variable's own formula text, not visible here and not necessarily >= the other slot's; W11 report"

PATTERN='(^|[^a-zA-Z_])(sprintf|strcpy|vsprintf)[ \t]*\('

# Comments blanked, line numbers kept
# (tools/strip_comments.awk)
strip_comments() {
  awk -f tools/strip_comments.awk "$1"
}

is_excluded() {
  f="$1"
  for e in $EXCLUDE; do
    [ "$e" = "$f" ] && return 0
  done
  return 1
}

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

bad=0
for f in core/*.c core/*.cpp; do
  [ -f "$f" ] || continue
  is_excluded "$f" && continue
  strip_comments "$f" > "$tmp/stripped"
  grep -nE "$PATTERN" "$tmp/stripped" | while IFS=: read -r lineno _; do
    text=$(sed -n "${lineno}p" "$f")
    allowed=0
    save_ifs=$IFS
    IFS='
'
    for entry in $ALLOW; do
      afile=$(printf '%s\n' "$entry" | cut -d'|' -f1)
      astr=$(printf '%s\n' "$entry" | cut -d'|' -f2)
      if [ "$afile" = "$f" ]; then
        case "$text" in
          *"$astr"*) allowed=1 ;;
        esac
      fi
    done
    IFS=$save_ifs
    if [ "$allowed" -eq 0 ]; then
      printf '%s:%s: %s\n' "$f" "$lineno" "$text"
      echo 1 >> "$tmp/bad"
    fi
  done
done

if [ -s "$tmp/bad" ]; then
  echo "formatcheck: sprintf/strcpy/vsprintf not in the allowlist (see tools/formatcheck.sh); use xpp_snprintf/XPP_SPRINTF or xpp_strlcpy/XPP_STRCPY (core/xpp_io.h)"
  exit 1
fi
echo "formatcheck ok: no un-allowlisted sprintf/strcpy/vsprintf in core/"
