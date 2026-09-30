#ifndef XPP_RECX_H
#define XPP_RECX_H
/* A recording, name.recx (W59, docs/protocol.md "Recordings"): one plain
   text file, openable and editable in any editor, that holds what a
   session did and nothing it computed (a replay computes it again):

     xppautx-recording 1
     program: xppautX <version>
     model: <the model's file, as it names it>
     recorded: <when the recording began, UTC>

     @file <name>
     <the file's lines>
     @end
     ...                      (the model's files, then every other text
                               file the session read)
     @steps
     # <the note shown above the next step, any number of # lines>
     {"step":"Initialconds → Go","keys":["i","g"]}
     ...

     fingerprint: <SHA-256 of the files and the steps, never the notes>

   A line of a file that is "@end" or starts with "@@" is written with one
   more "@" in front (a reader takes it off). The fingerprint is the
   SHA-256 (xpp_sha256.h), as 64 lowercase hex digits, of every line of
   the @file sections (their "@file" and "@end" lines included) and every
   step line, in the file's order, each followed by "\n": the header, the
   blank lines between sections, the # notes and the fingerprint line are
   not in it, so a note edited afterwards keeps it, and a step or a file
   changed does not. This header is the format's pure part (recx.cpp, no
   I/O; tests/test_recx.cpp); what goes into one is json_record.cpp's.
   C++ only. */
#include <string>
#include <string_view>
#include <vector>

#include "model_files.h"

namespace xpp::recx {

inline constexpr std::string_view extension = ".recx";
inline constexpr std::string_view format_line = "xppautx-recording 1";

/* a step: its note (its lines separated by "\n", "" for none) and the
   step itself, one JSON object on one line */
struct Step {
    std::string note;
    std::string line;
};

struct Recording {
    std::string program;  /* "xppautX 8.1" (xpp_version_string) */
    std::string model;    /* the model's file, as it names itself (Model::this_file) */
    std::string recorded; /* when it began: 2026-09-30T10:14:02Z */
    std::vector<ModelFile> files;
    std::vector<Step> steps;
};

/* file into r's files, unless one of that name with those bytes is there
   already: a file read again with other bytes (a model edited and
   reloaded) is another section, after the first */
void add_file(Recording &r, ModelFile file);

/* r as its file's text, the fingerprint last */
std::string text(const Recording &r);

/* the fingerprint of the lines it covers (above), each without its "\n" */
std::string fingerprint(const std::vector<std::string> &hashed);

} // namespace xpp::recx

#endif
