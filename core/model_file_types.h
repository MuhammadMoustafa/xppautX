#pragma once
/* A model's files as a Model holds them and a session file saves them
   (model_files.h has the functions). C++ only. */
#include <string>
#include <vector>

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

} // namespace xpp
