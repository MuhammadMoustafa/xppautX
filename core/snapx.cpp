/* The session file's pure part: see snapx.h. */
#include "snapx.h"
#include "xpp_io.h"

#include <cctype>

namespace xpp::snapx {

namespace {

/* "xppautX <kind> " before the version */
std::string first_line(std::string_view kind)
{
    std::string o = "xppautX ";
    o += kind;
    o += ' ';
    return o;
}

void add_line(std::string &o, std::string_view key, std::string_view value)
{
    o += key;
    if (!value.empty()) {
        o += ' ';
        o += value;
    }
    o += '\n';
}

} // namespace

std::string manifest_text(const Manifest &m, std::string_view kind)
{
    std::string o;
    o += first_line(kind);
    o += std::to_string(m.version);
    o += '\n';
    add_line(o, "name", m.model_name);
    if (!m.anifile.empty()) add_line(o, "anifile", m.anifile);
    if (kind == session_kind) add_line(o, "data", m.data ? "1" : "0");
    return o;
}

std::expected<Manifest, std::string> parse_manifest(std::string_view text, std::string_view kind)
{
    Manifest m;
    const std::string head = first_line(kind);
    int number = 0;
    bool named = false;
    while (!text.empty()) {
        const std::size_t nl = text.find('\n');
        std::string_view line = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view() : text.substr(nl + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (++number == 1) {
            if (!line.starts_with(head) || !xpp::parse_int(line.substr(head.size()), m.version) || m.version != 1)
                return std::unexpected(xpp::format("it does not begin \"{}1\"", head));
            continue;
        }
        if (line.empty()) continue;
        const std::size_t sp = line.find(' ');
        const std::string_view key = line.substr(0, sp);
        const std::string_view value = sp == std::string_view::npos ? std::string_view() : line.substr(sp + 1);
        if (key == "name") {
            m.model_name = value;
            named = true;
        } else if (key == "anifile")
            m.anifile = value;
        else if (key == "data" && kind == session_kind && (value == "0" || value == "1"))
            m.data = value == "1";
        else
            return std::unexpected(xpp::format("its line {} is not one it has: \"{}\"", number, line));
    }
    if (number == 0) return std::unexpected(std::string("it is empty"));
    if (!named || m.model_name.empty()) return std::unexpected(std::string("it names no model"));
    return m;
}

void add_model_members(std::vector<zip::Entry> &entries, std::span<const ModelFile> files)
{
    for (const ModelFile &f : files) entries.push_back({std::string(model_folder) + f.name, f.bytes});
}

std::optional<std::vector<ModelFile>> model_members(const std::vector<zip::Entry> &entries, std::string_view model_name)
{
    std::vector<ModelFile> files;
    bool own = false;
    for (const zip::Entry &e : entries) {
        if (!e.name.starts_with(model_folder)) continue;
        std::string name = e.name.substr(model_folder.size());
        own = own || name == model_name;
        files.push_back({std::move(name), e.bytes});
    }
    if (!own || model_name.empty()) return std::nullopt;
    return files;
}

bool has_extension(std::string_view path, std::string_view ext)
{
    if (path.size() < ext.size()) return false;
    path = path.substr(path.size() - ext.size());
    for (std::size_t i = 0; i < ext.size(); i++)
        if (std::tolower(static_cast<unsigned char>(path[i])) != std::tolower(static_cast<unsigned char>(ext[i])))
            return false;
    return true;
}

std::string with_extension(std::string_view name, std::string_view ext)
{
    std::string n(name);
    if (!has_extension(n, ext)) n += ext;
    return n;
}

bool is_session_file(std::string_view path) { return has_extension(path, extension); }

std::string session_file_name(std::string_view name) { return with_extension(name, extension); }

} // namespace xpp::snapx
