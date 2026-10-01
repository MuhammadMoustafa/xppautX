/* A model's own files: see model_files.h. */
#include "model_files.h"
#include "model.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_zip.h"

#include <algorithm>
#include <memory>
#include <string>

namespace xpp {

std::string include_path(const std::string &including, const std::string &name)
{
    const size_t slash = including.find_last_of("/\\");
    const bool absolute = !name.empty() && (name[0] == '/' || name[0] == '\\' || (name.size() > 1 && name[1] == ':'));
    return (slash == std::string::npos || absolute) ? name : including.substr(0, slash + 1) + name;
}

namespace {

/* the index of the model's file name in m.files, or -1 */
long file_index(const Model &m, const std::string &name)
{
    const auto it = std::find_if(m.files.begin(), m.files.end(), [&name](const ModelFile &f) { return f.name == name; });
    return it == m.files.end() ? -1 : it - m.files.begin();
}

} // namespace

bool read_model_file(Model &m, const std::string &name, std::string &bytes)
{
    bytes.clear();
    const long i = file_index(m, name);
    if (!m.saved_in.empty()) {
        if (i < 0) return false;
        bytes = m.files[static_cast<size_t>(i)].bytes;
        return true;
    }
    if (!read_bytes(name.c_str(), bytes)) return false;
    if (i < 0) m.files.push_back({name, bytes});
    return true;
}

UniqueFile open_model_file(Model &m, const std::string &name)
{
    std::string bytes;
    if (!read_model_file(m, name, bytes)) return UniqueFile();
    if (m.saved_in.empty()) return open_read(name.c_str());
    if (!m.saved_copies) m.saved_copies = std::make_shared<TempDir>();
    if (m.saved_copies->path().empty()) return UniqueFile();
    const std::string copy = m.saved_copies->file(std::to_string(file_index(m, name)));
    Writer w = Writer::binary(copy.c_str());
    if (!w || !w.write(bytes) || !w.commit()) return UniqueFile();
    return open_read(copy.c_str());
}

LineReader model_file_lines(Model &m, const std::string &name)
{
    std::string bytes;
    if (!read_model_file(m, name, bytes)) return LineReader();
    return LineReader::of_text(std::move(bytes));
}

bool is_model_text(std::string_view bytes)
{
    return !zip::is_zip(bytes) && bytes.find('\0') == std::string_view::npos;
}

std::string model_title(const Model &m)
{
    if (m.saved_in.empty()) return m.this_file;
    return m.this_file + " (saved in " + xpp::files::split_path(m.saved_in).second + ")";
}

} // namespace xpp
