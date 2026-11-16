#pragma once
/* A model's files as a Model holds them and a session file saves them
   (model_files.h has the functions). C++ only. */
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "export_services.h"
#include "xpp_error.h"

namespace xpp {

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

/* the user's answer "Allow always" for one model file and one path it names
   outside its folder (both absolute; the path as the program resolved it,
   files::resolve_path): a changed path is another question */
struct OutsideApproval {
    std::string model, path;
    bool operator==(const OutsideApproval &) const = default;
};

/* What the program gives a load (maintainer, 2026-11-14; W737): the "Allow always" answers, data
   in, and an interface the loader owns for what comes out, and the registry's check of a command value (a model's
   button names one: `check_command`, which the program fills, never the registry itself). The program's
   start fills it from the preferences (xpp_prefs.h: load_services): `kept`
   the answers kept before (or `unreadable`, why they cannot be read), and
   `keep` what remembers one more at the moment the user gives it; the
   loader reaches no file of the preferences itself. A Model with no `keep`
   (a unit test) remembers an answer only in `kept`. */
struct LoadServices {
    std::vector<OutsideApproval> kept;
    std::optional<Error> unreadable;
    std::function<Result<>(const OutsideApproval &)> keep;
    std::function<Result<>(std::string_view value)> check_command;
    /* the data and picture formats the export writes, and the operations that write a picture out (export_services.h) */
    ExportServices exports;
};

} // namespace xpp
