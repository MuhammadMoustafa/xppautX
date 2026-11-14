#pragma once
/* A model's own files (W103): the .ode or .odex and every file its load
   reads -- the files it includes, its file tables, its options file,
   its animation (the animation option's .anix). Their readers read them through here, by the name
   the model gives each (a path as the model writes it, relative to the
   working folder or to an including .odex's folder):

   - a model read from the disk: each file is read there, and recorded in
     the Model (ModelRecord::files) as it was read, so that an autoX file (.snapx)
     or a session file (.snapx) saves the model whole (xpp_session.h);
   - a model saved in one of those: each file is the copy saved there and
     never the disk (Model::saved_in names the file), a file not saved
     being a file that is not there.

   The model itself is loaded by xpp::load_model (model_load.h), given the
   saved files for a saved one. C++ only. */
#include "xpp_error.h"
#include "xpp_io.h"
#include "model_file_types.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace xpp {

struct Model; /* model.h */

/* the path of the file an include line of file `including` names: relative
   to the including file's folder (the model's, or an included file's for a
   nested include); an absolute one as it is. The one rule of .ode's
   #include and .odex's include; a name in the working
   folder and stays as typed. */
std::string include_path(const std::string &including, const std::string &name);
/* the name that include_path(including, name) turns into path: the including file's folder dropped from it; a path outside that
   folder as it is */
std::string relative_to_model(const std::string &including, const std::string &path);

/* The answers of the question a model's path outside its folder asks
   (W286), as keys: Allow always (kept in the preferences for this model file
   and this path; a .ode uses its generated .odex), Allow once (this load),
   Refuse (or no answer). */
inline constexpr std::string_view OUTSIDE_PATH_KEYS = "aor", OUTSIDE_PATH_KEYS_ONCE_ONLY = "or";
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

/* The user's standing answers behind "Allow always": kept in the preferences
   (xpp_prefs.cpp, the application's), asked here, below the Session. */
namespace prefs {
/* whether the user answered "Allow always" for model naming path (a bad file
   is the error) */
Result<bool> outside_path_approved(std::string_view model, std::string_view path);
/* the user's "Allow always": remembered first, the oldest answer dropped when
   MAX_OUTSIDE_APPROVALS are kept, a repeated one moved to the front */
Result<> approve_outside_path(std::string_view model, std::string_view path);
} // namespace prefs

/* m's file name, whole, into bytes: false (bytes empty) when absent,
   outside the model's folder, linked, or over the archive member byte limit. */
bool read_model_file(Model &m, const std::string &name, std::string &bytes);
/* A validated companion picked after loading, recorded for session saving.
   Replace its earlier bytes, checking the same aggregate archive budget. */
Result<> remember_model_file(Model &m, const std::string &name, std::string bytes);

/* m's file name opened for reading, as open_read opens a file,
   for a reader that takes a FILE *: empty when there is none. During a
   load only: the checked and recorded bytes are read from a copy in a
   scratch folder the load removes when it ends. */
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

/* a Model of its own to parse a file in without touching m (a save's edited text, a file of values to
   load): the folder, the paths the user allowed and (for a saved model) the files are m's, so an
   import is read, and an outside path asked about, as m's load read it */
std::unique_ptr<Model> scratch_model(const Model &m);

/* Finish the load's name index with diagnostic places and source lines. */
void index_model_places(Model &m);

/* line n of m's file name as m read it ("" when it has no such line) */
std::string model_source_line(const Model &m, const std::string &name, int n);

/* every line of m's file name as m read it, in order (empty when m has no such file) */
std::vector<std::string> model_source_lines(const Model &m, const std::string &name);

/* m as a title names it: its file, and the file it is saved in
   ("lecar.ode (saved in lecar.snapx)") */
std::string model_title(const Model &m);

} // namespace xpp

