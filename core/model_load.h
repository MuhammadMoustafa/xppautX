#pragma once

#include "comline.h"
#include "xpp_error.h"

#include <expected>
#include <functional>
#include <optional>
#include <vector>
namespace xpp { namespace prefs { struct Preferences; } struct SavedModel; }

namespace xpp {

struct Session; /* session.h */

/* The load, as the readers and the front ends ask for it: declared here, at
   the model's level, and done by xpp_batch.cpp, the program's start, which
   knows every module the load sets up. */

/* what a load gives: the Session it loaded (its Model the Session's),
   or what is wrong and where */
using Loaded = std::expected<Session *, Error>;
/* The shared start of every mode (the program, --silent, --convert,
   File > Open model, a unit test): options, the ODE file, numerics
   set-up; batch forces batch mode. The Session loaded (the client's in
   the session list from now on, session.h), else what is wrong and where
   (the one before stays; the caller exits with status 1 when there is
   nothing to go on with). saved: a model saved in an
   autoX or session file, whose files the load reads instead of the disk's
   (model_files.h); cl names its first file. check, when given, reads
   what such a file adds against the loaded Session before the load keeps
   it (xpp_saved_check, xpp_session.h): what it finds wrong fails the load
   as a model that does not load does. What is wrong is written to the
   log here (Error::text()), once. */
Loaded load_model(CommandLine cl, int batch, const SavedModel *saved = nullptr,
                  const std::function<std::optional<Error>(Session &)> &check = {},
                  const Result<prefs::Preferences> *read_prefs = nullptr);
/* Import and validate in a temporary Model and Session, using the shared
   setup. Never commits; the converter alone supplies the .ode reader. */
/* A loaded model translated (translate_model, W260's exact Jacobian when the
   numerics ask for it): the warnings the compiler gave, each with its place,
   or the error of a model that asks for an exact Jacobian it has none of.
   A load logs the warnings; --check reports them. */
Result<std::vector<Error>> compile_loaded(Session &s);

Result<> inspect_model(const CommandLine &cl, const std::function<void(Session &)> &read,
                      const std::function<std::optional<Error>(Session &)> &check = {},
                      const SavedModel *saved = nullptr);

/* After a load with no interface (--silent, a unit test): s's browser,
   graphs and colors set up. What --silent then runs is its built-in
   script (json_silent.cpp). */
void batch_start(Session &s);

} // namespace xpp
