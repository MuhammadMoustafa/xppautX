#pragma once

#include "comline.h"
#include "xpp_error.h"

#include <expected>
#include <functional>
#include <optional>
#include <vector>
namespace xpp { namespace prefs { struct Preferences; } struct SavedModel; struct LoadServices; class FileSystem; }

namespace xpp {

struct Session; /* session.h */

/* The load, as the front ends ask for it: declared here, at the model's
   level, and done by xpp_batch.cpp, the program's start, which knows every
   module the load sets up. The readers and converters below the application
   never call it: they are handed it (ModelLoader, below). */

/* what a load gives: the Session it loaded (its Model the Session's),
   or what is wrong and where */
using Loaded = std::expected<Session *, Error>;
/* The shared start of every mode (the program, --silent, --convert,
   File > Open model, a unit test): options, the ODE file, numerics
   set-up; batch forces batch mode. fs is the file system the model is read from and keeps for later (Model::fs). The Session loaded (the client's in
   the session list from now on, session.h), else what is wrong and where
   (the one before stays; the caller exits with status 1 when there is
   nothing to go on with). saved: a model saved in an
   autoX or session file, whose files the load reads instead of the disk's
   (model_files.h); cl names its first file. check, when given, reads
   what such a file adds against the loaded Session before the load keeps
   it (xpp_saved_check, xpp_session.h): what it finds wrong fails the load
   as a model that does not load does. What is wrong is written to the
   log here (Error::text()), once. */
Loaded load_model(FileSystem &fs, CommandLine cl, int batch, const SavedModel *saved = nullptr,
                  const std::function<std::optional<Error>(Session &)> &check = {},
                  const Result<prefs::Preferences> *read_prefs = nullptr);
/* Import and validate in a temporary Model and Session, using the shared
   setup. Never commits; the converter alone supplies the .ode reader. */
/* A loaded model translated (translate_model, W260's exact Jacobian when the
   numerics ask for it): the warnings the compiler gave, each with its place,
   or the error of a model that asks for an exact Jacobian it has none of.
   A load logs the warnings; --check reports them. */
Result<std::vector<Error>> compile_loaded(Session &s);

/* A load in a Model that is discarded. services, when given, are the
   "Allow always" answers it works with (model_file_types.h). */
Result<> inspect_model(FileSystem &fs, const CommandLine &cl, const std::function<void(Session &)> &read,
                      const std::function<std::optional<Error>(Session &)> &check = {},
                      const SavedModel *saved = nullptr, const LoadServices *services = nullptr);

/* The load a converter, a checker or a front end runs a model through, handed to it by its
   caller (the program's start, a unit test: program_loader): the load sets up
   every module, so what is below the application cannot call it. */
struct ModelLoader {
  Loaded (*load)(FileSystem &fs, CommandLine cl, int batch, const SavedModel *saved,
                 const std::function<std::optional<Error>(Session &)> &check, const Result<prefs::Preferences> *read_prefs);
  Result<> (*inspect)(FileSystem &fs, const CommandLine &cl, const std::function<void(Session &)> &read,
                      const std::function<std::optional<Error>(Session &)> &check, const SavedModel *saved, const LoadServices *services);
  void (*batch_start)(Session &s);
  /* the .odex reader: a model's file read into the Session being loaded (inspect's `read` for an .odex) */
  void (*read_odex)(Session &s);
};

/* After a load with no interface (--silent, a unit test): s's browser,
   graphs and colors set up. What --silent then runs is its built-in
   script (json_silent.cpp). */
void batch_start(Session &s);

} // namespace xpp
