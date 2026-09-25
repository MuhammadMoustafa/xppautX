# Counts unsafe-C idioms per file/category, reading every core/*.cpp and
# core/*.h directly. tools/unsafecheck.sh calls this loaded together with
# tools/strip_comments.awk, so both scripts' main rules run for every
# line in the order given:
#   awk -f tools/strip_comments.awk -f tools/unsafe_count.awk core/*.cpp core/*.h
# strip_comments.awk's rule sets its (global) `out` to the current line
# with comments blanked, and prints it; this script's rule then uses that
# same `out` for the actual counting, so comments are stripped only once
# and each file is read only once. strip_comments.awk's own printed lines
# are noise for our purposes; every line this script emits is prefixed
# "U\t" so tools/unsafecheck.sh can filter them out with `grep '^U\t'`
# (a source line can never start with a tab).
#
# Output: "U\t<file>\t<category>\t<count>" (only nonzero categories),
# then "U\tGRAND\t<category>\t<count>" (5 lines) and
# "U\tGRANDTOTAL\t<n>" (1 line) at the end. tools/unsafecheck.sh does the
# sorting/table formatting.
FNR == 1 {
  files[FILENAME] = 1
}
{
  content = out

  # memory: xpp_malloc/xpp_calloc/xpp_realloc/xpp_free/xpp_strdup, and
  # new[]/delete[] (core/xpp_mem.h's own idioms; RAII containers are the
  # eventual replacement).
  if (index(content, "xpp_malloc"))  counts[FILENAME, "memory"] += gsub(/(^|[^A-Za-z0-9_])xpp_malloc[ \t]*\(/, "&", content)
  if (index(content, "xpp_calloc"))  counts[FILENAME, "memory"] += gsub(/(^|[^A-Za-z0-9_])xpp_calloc[ \t]*\(/, "&", content)
  if (index(content, "xpp_realloc")) counts[FILENAME, "memory"] += gsub(/(^|[^A-Za-z0-9_])xpp_realloc[ \t]*\(/, "&", content)
  if (index(content, "xpp_free"))    counts[FILENAME, "memory"] += gsub(/(^|[^A-Za-z0-9_])xpp_free[ \t]*\(/, "&", content)
  if (index(content, "xpp_strdup"))  counts[FILENAME, "memory"] += gsub(/(^|[^A-Za-z0-9_])xpp_strdup[ \t]*\(/, "&", content)
  if (index(content, "new"))    counts[FILENAME, "memory"] += gsub(/(^|[^A-Za-z0-9_])new[ \t]+[A-Za-z_][A-Za-z0-9_:<>]*[ \t]*\[/, "&", content)
  if (index(content, "delete")) counts[FILENAME, "memory"] += gsub(/(^|[^A-Za-z0-9_])delete[ \t]*\[[ \t]*\]/, "&", content)

  # buffers: fixed-size "char name[N];" declarations (locals or members).
  if (index(content, "char") && index(content, "["))
    counts[FILENAME, "buffers"] += gsub(/(^|[^A-Za-z0-9_])char[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\[/, "&", content)

  # text: xpp_io.h's own safe copiers/formatters, and the raw calls they
  # replace (strcpy/strcat/strncpy/sprintf/strtok are already 0 per
  # tools/formatcheck.sh/sourcecheck; counted here too so a regression
  # shows up in the same ratchet), plus printf-style fprintf/vfprintf.
  if (index(content, "xpp_strlcpy"))  counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])xpp_strlcpy[ \t]*\(/, "&", content)
  if (index(content, "xpp_strlcat"))  counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])xpp_strlcat[ \t]*\(/, "&", content)
  if (index(content, "xpp_snprintf")) counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])xpp_snprintf[ \t]*\(/, "&", content)
  if (index(content, "XPP_SPRINTF"))  counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])XPP_SPRINTF[ \t]*\(/, "&", content)
  if (index(content, "XPP_STRCPY"))   counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])XPP_STRCPY[ \t]*\(/, "&", content)
  if (index(content, "XPP_STRCAT"))   counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])XPP_STRCAT[ \t]*\(/, "&", content)
  if (index(content, "strcpy"))       counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])strcpy[ \t]*\(/, "&", content)
  if (index(content, "strcat"))       counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])strcat[ \t]*\(/, "&", content)
  if (index(content, "strncpy"))      counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])strncpy[ \t]*\(/, "&", content)
  if (index(content, "sprintf"))      counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])sprintf[ \t]*\(/, "&", content)
  if (index(content, "strtok"))       counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])strtok[ \t]*\(/, "&", content)
  if (index(content, "fprintf"))      counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])fprintf[ \t]*\(/, "&", content)
  if (index(content, "vfprintf"))     counts[FILENAME, "text"] += gsub(/(^|[^A-Za-z0-9_])vfprintf[ \t]*\(/, "&", content)

  # files: fopen/fclose/fscanf/fgets/sscanf/feof (core/xpp_io.h's line
  # and token readers, and xpp::Writer, are the replacement).
  if (index(content, "fopen"))  counts[FILENAME, "files"] += gsub(/(^|[^A-Za-z0-9_])fopen[ \t]*\(/, "&", content)
  if (index(content, "fclose")) counts[FILENAME, "files"] += gsub(/(^|[^A-Za-z0-9_])fclose[ \t]*\(/, "&", content)
  if (index(content, "fscanf")) counts[FILENAME, "files"] += gsub(/(^|[^A-Za-z0-9_])fscanf[ \t]*\(/, "&", content)
  if (index(content, "fgets"))  counts[FILENAME, "files"] += gsub(/(^|[^A-Za-z0-9_])fgets[ \t]*\(/, "&", content)
  if (index(content, "sscanf")) counts[FILENAME, "files"] += gsub(/(^|[^A-Za-z0-9_])sscanf[ \t]*\(/, "&", content)
  if (index(content, "feof"))   counts[FILENAME, "files"] += gsub(/(^|[^A-Za-z0-9_])feof[ \t]*\(/, "&", content)

  # casts: a C-style cast, "(type)"/"(type *)"/"(type **)" immediately
  # before an identifier, a digit or "(". type is a fixed list of
  # builtins and the core's own typedefs cast to (xpp_types.h's
  # XppWinId, auto_f2c.h's integer/real/doublereal/logical,
  # llnltyps.h's N_Vector, xpp_win32.h's DWORD). This is a heuristic, not
  # a parse: it undercounts a cast to a project struct/typedef not in
  # this list, and does not try every spacing; it is meant to not grow,
  # not to reach 0.
  if (index(content, "(") &&
      (index(content, "int") || index(content, "char") || index(content, "void") ||
       index(content, "double") || index(content, "float") || index(content, "long") ||
       index(content, "short") || index(content, "unsigned") || index(content, "signed") ||
       index(content, "size_t") || index(content, "real") || index(content, "logical") ||
       index(content, "XppWinId") || index(content, "N_Vector") || index(content, "DWORD")))
    counts[FILENAME, "casts"] += gsub(/\([ \t]*(const[ \t]+)?(unsigned[ \t]+|signed[ \t]+)?(void|char|int|short|long|float|double|size_t|doublereal|integer|real|logical|XppWinId|N_Vector|DWORD)([ \t]+(int|long))?[ \t]*\**[ \t]*\)[ \t]*[A-Za-z0-9_(]/, "&", content)
}
END {
  ncats = 5
  cats[1] = "memory"; cats[2] = "buffers"; cats[3] = "text"
  cats[4] = "files";  cats[5] = "casts"
  for (fn in files) {
    for (i = 1; i <= ncats; i++) {
      v = counts[fn, cats[i]] + 0
      if (v > 0) {
        print "U\t" fn "\t" cats[i] "\t" v
        grand[cats[i]] += v
        grandtotal += v
      }
    }
  }
  print "U\tGRAND\tmemory\t" grand["memory"] + 0
  print "U\tGRAND\tbuffers\t" grand["buffers"] + 0
  print "U\tGRAND\ttext\t" grand["text"] + 0
  print "U\tGRAND\tfiles\t" grand["files"] + 0
  print "U\tGRAND\tcasts\t" grand["casts"] + 0
  print "U\tGRANDTOTAL\t" grandtotal + 0
}
