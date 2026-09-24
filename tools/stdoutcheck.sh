#!/bin/sh
# The core never prints to stdout or stderr directly (issue: W11): in
# --server mode stdout carries the JSON protocol, so a stray printf
# corrupts it, and every message belongs in xpp_log (core/xpp_log.h) so
# --verbose/--debug and -logfile control it and browser mode can show it
# in the page's log panel. tools/verify.sh runs this right after
# utf8check. Usage: tools/stdoutcheck.sh
cd "$(dirname "$0")/.." || exit 1

# Comments are stripped (see strip_comments below) before matching, so a
# printf left inside dead/commented-out code does not need an entry here.
# Each entry is "file|substring of the matching line|why it stays direct".
# The substring only has to be unique enough to identify that one call;
# it is matched literally (grep -F), so a line-number shift needs no edit
# here.
ALLOW="core/comline.c|XPPAUT Version %g.%g|the -version flag's own text, like --help
core/xppautx_main.c|printf(\"xppautX %s\\n\", XPPAUTX_VERSION)|--version text the VS Code extension reads
core/xpp_http.cpp|printf(\"XPP: %s\\n\", page_url)|the XPP: address line xppautX prints in browser mode
core/xppautx_main.c|printf(\"%s%s%s\", usage_head|the --help text"

PATTERN='(^|[^a-zA-Z_])(printf|puts|putchar|vprintf)[ \t]*\(|v?f(printf|puts|putc|write)[ \t]*\((stdout|stderr)|std::(cout|cerr)'

# Strip // and /* */ comments (tracking block comments across lines) and
# the contents of "..." string literals, but keep line numbers aligned
# with the source so grep -n below reports real line numbers.
strip_comments() {
  awk '
    BEGIN { in_comment = 0 }
    {
      line = $0; out = ""; n = length(line); in_str = 0; i = 1
      while (i <= n) {
        c = substr(line, i, 1); c2 = substr(line, i, 2)
        if (in_comment) {
          if (c2 == "*/") { in_comment = 0; out = out "  "; i += 2; continue }
          out = out " "; i += 1; continue
        }
        if (!in_str && c2 == "/*") { in_comment = 1; out = out "  "; i += 2; continue }
        if (!in_str && c2 == "//") { i = n + 1; continue }
        if (c == "\"") { in_str = !in_str; out = out c; i += 1; continue }
        if (in_str && c == "\\") { out = out c substr(line, i+1, 1); i += 2; continue }
        out = out c; i += 1
      }
      print out
    }
  ' "$1"
}

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

bad=0
for f in core/*.c core/*.cpp; do
  [ -f "$f" ] || continue
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
  echo "stdoutcheck: direct stdout/stderr output not in the allowlist (see tools/stdoutcheck.sh)"
  exit 1
fi
echo "stdoutcheck ok: no un-allowlisted direct stdout/stderr output in core/"
