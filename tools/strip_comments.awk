# Prints a C/C++ source with its // and /* */ comments (tracked across
# lines) blanked out, line for line, so grep -n on the result reports the
# source's own line numbers; a /* or // inside a "..." literal is not taken
# for a comment. For the source checks (formatcheck.sh, alloccheck.sh):
#   awk -f tools/strip_comments.awk core/x.c
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
