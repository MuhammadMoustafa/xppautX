#ifndef XPP_RECX_H
#define XPP_RECX_H
/* A recording, name.recx (W59, docs/protocol.md "Recordings"): one plain
   text file, openable and editable in any editor, that holds what a
   session did and nothing it computed (a replay computes it again):

     xppautx-recording 1
     program: xppautX <version>
     model: <the model's file, as it names it>
     recorded: <when the recording began, UTC>

     @snapshot
     <the session file's bytes (.snapx) in base64, 76 digits a line>
     @end

     @file <name>
     <the file's lines>
     @end
     @binary <name>
     <the file's bytes in base64, 76 digits a line>
     @end
     ...                      (the model's files, then every other file
                               the session read: text as @file, any
                               other (an .autox, a .snapx) as @binary)
     @steps
     # <the note shown above the next step, any number of # lines>
     {"step":"Initialconds → Go","keys":["i","g"]}
     ...

     fingerprint: <SHA-256 of the files and the steps, never the notes>

   A line of a file that is "@end" or starts with "@@" is written with one
   more "@" in front (a reader takes it off). The @snapshot section (W59d)
   is the session as it was when Record was pressed: what Save session
   writes (xpp_session.h), without the data table, which the replay
   computes again; the player starts from it, and a file without it is
   not a recording. The fingerprint is the
   SHA-256 (xpp_sha256.h), as 64 lowercase hex digits, of every line of
   the sections (their first and "@end" lines included) and every
   step line, in the file's order, each followed by "\n": the header, the
   blank lines between sections, the # notes and the fingerprint line are
   not in it, so a note edited afterwards keeps it, and a step or a file
   changed does not. This header is the format's pure part (recx.cpp, no
   I/O; tests/test_recx.cpp); what goes into one is json_record.cpp's,
   and its replay json_player.cpp's.
   C++ only. */
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "model_files.h"
#include "xpp_error.h"

namespace xpp::recx {

inline constexpr std::string_view extension = ".recx";
inline constexpr std::string_view format_line = "xppautx-recording 1";

/* a step: its note (its lines separated by "\n", "" for none) and the
   step itself, one JSON object on one line */
struct Step {
    std::string note;
    std::string line;
    int at = 0; /* the line of the file it is on (a read recording's), for its errors */
};

struct Recording {
    std::string program;  /* "xppautX 8.1" (xpp_version_string) */
    std::string model;    /* the model's file, as it names itself (Model::this_file) */
    std::string recorded; /* when it began: 2026-09-30T10:14:02Z */
    /* the session when the recording began: a .snapx's bytes, without
       the data table (xpp_session_snapshot) */
    std::string snapshot;
    std::vector<ModelFile> files;
    std::vector<Step> steps;
    /* the fingerprint a read file ends with (empty when it has none);
       empty for a recording being made, whose text() computes it */
    std::string fingerprint;
};

/* file into r's files, unless one of that name with those bytes is there
   already: a file read again with other bytes (a model edited and
   reloaded) is another section, after the first. The section's index. */
size_t add_file(Recording &r, ModelFile file);

/* r as its file's text, the fingerprint last: r.fingerprint when it has
   one (a read recording whose notes were edited keeps what it said),
   else the fingerprint of what r holds */
std::string text(const Recording &r);

/* a recording's file read back */
struct Read {
    Recording rec;
    /* the fingerprint matches the files and the steps: nothing but the
       notes was changed after the recording was made */
    bool intact = false;
};

/* the text of a .recx file (named file, for its errors), read whole: the
   error at the line that is wrong when it is not one (its first line, a
   header line missing, given twice or not one of them, no @snapshot, a
   section without its @end, no @steps, a @binary or @snapshot section
   that is not base64, no fingerprint line, a line after it) */
Result<Read> read(std::string_view text, std::string file);

/* the fingerprint of the lines it covers (above), each without its "\n" */
std::string fingerprint(const std::vector<std::string> &hashed);

} // namespace xpp::recx

#endif
