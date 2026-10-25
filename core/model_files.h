#pragma once
/* A model's own files (W103): the .ode or .odex and every file its load
   reads -- the files it includes, its file tables, its options file,
   --anifile's animation. Their readers read them through here, by the name
   the model gives each (a path as the model writes it, relative to the
   working folder or to an including .odex's folder):

   - a model read from the disk: each file is read there, and recorded in
     the Model (Model::files) as it was read, so that an AUTO file (.snapx)
     or a session file (.snapx) saves the model whole (xpp_session.h);
   - a model saved in one of those: each file is the copy saved there and
     never the disk (Model::saved_in names the file), a file not saved
     being a file that is not there.

   The model itself is loaded by xpp::load_model (xpp_batch.h), given the
   saved files for a saved one. C++ only. */
#include "xpp_error.h"
#include "xpp_io.h"
#include "odex.h"

#include <string>
#include <string_view>
#include <vector>

namespace xpp {

struct Model; /* model.h */

/* a file of a model: its name, as the model reads it, and its bytes */
struct ModelFile {
    std::string name;
    std::string bytes;
    bool operator==(const ModelFile &) const = default;
};

/* a model saved in a file: its files, the model's own first, and the
   .snapx they are saved in (an absolute path) */
struct SavedModel {
    std::string in;
    std::vector<ModelFile> files;
};

/* the path of the file an include line of file `including` names: relative
   to the including file's folder (the model's, or an included file's for a
   nested include); an absolute one as it is. The one rule of .ode's
   #include and .odex's include; the --include flag is typed in the working
   folder and stays as typed. */
std::string include_path(const std::string &including, const std::string &name);

/* The answers of the question a model's path outside its folder asks
   (W286), as keys: Allow always (kept in the preferences for this model file
   and this path; a .ode uses its generated .odex), Allow once (this load),
   Refuse (or no answer). */
inline constexpr std::string_view OUTSIDE_PATH_KEYS = "aor";
inline constexpr std::string_view PATH_REFUSED = "path refused"; /* the `where` of the refusal's Error */
inline constexpr char OUTSIDE_PATH_ALWAYS = 'a', OUTSIDE_PATH_ONCE = 'o';

/* A path a model names (an include, an import, a table, an animation, an
   output or log file) may be read or written when it lies inside the model's
   folder, or outside it when the user allowed it: this asks (Choose: the
   dialog, --server's ask event, the terminal with no interface, where
   -y / --yes allows it once, printed), unless the pair of the model file and
   the path was kept before ("Allow always", xpp_prefs.h) or allowed in this
   load (Model::outside_paths). A link on the way, or a name that is not a
   plain one, is refused with no question. Call it, with the place that names
   the path (an empty Place: the command line), before reading name with the
   functions below, which refuse an outside path not approved; the error is
   the refusal, at `at`. */
Result<> approve_model_path(Model &m, const std::string &name, const Place &at);

/* m's file name, whole, into bytes: false (bytes empty) when absent,
   outside the model's folder, linked, or over the archive member byte limit. */
bool read_model_file(Model &m, const std::string &name, std::string &bytes);

/* m's file name opened for reading, as open_read opens a file,
   for a reader that takes a FILE *: empty when there is none. During a
   load only: a saved file is read from a copy of it in a scratch folder
   the load removes when it ends. */
UniqueFile open_model_file(Model &m, const std::string &name);

/* m's file name, a line at a time (LineReader): empty when there
   is none */
LineReader model_file_lines(Model &m, const std::string &name);

/* bytes can be a model's text: not a zip (an .snapx is opened
   as what it is, by its name) nor any other binary file (a NUL byte) */
bool is_model_text(std::string_view bytes);

/* m's own file (this_file) now holds bytes, a save of the model file having
   written them (model_save.h): its bytes as the load read them, its source
   lines and the line index errors quote from. */
void replace_model_text(Model &m, std::string bytes);
/* m.source: the model file's lines of text, each with its line end */
void set_model_source(Model &m, std::string_view text);

/* Finish the load's name index with diagnostic places and source lines. */
void index_model_places(Model &m);

/* Where a model line is, for an error it causes at run time (W140): the
   file of a statement's place (odex::Pos, its file one of
   Model::statement_files), its line and column and the line as written. */
Place model_place(const Model &m, const odex::Pos &pos);
/* where m defines name (any case): the equation, aux quantity, function,
   table, Markov variable, network or parameter line the parser read it
   from; where a load in progress is (xpp::Load::place(), session.h) when
   no line does, else an empty Place (a column the browser added) */
Place model_place(const Model &m, std::string_view name);
/* where m's statement number k (from 0) of kind is (its k-th boundary,
   say); an empty Place when it has fewer */
Place model_place(const Model &m, odex::Statement::Kind kind, int k);
/* line n of m's file name as m read it ("" when it has no such line) */
std::string model_source_line(const Model &m, const std::string &name, int n);

/* every line of m's file name as m read it, in order (empty when m has no such file) */
std::vector<std::string> model_source_lines(const Model &m, const std::string &name);

/* m as a title names it: its file, and the file it is saved in
   ("lecar.ode (saved in lecar.snapx)") */
std::string model_title(const Model &m);

} // namespace xpp

