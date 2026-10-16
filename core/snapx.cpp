/* The session file's pure part: see snapx.h. */
#include "snapx.h"
#include "xpp_io.h"

#include "xpp_files.h"
#include "setx.h"

namespace xpp::snapx {

namespace {

/* the manifest's group, as session.setx names it */
constexpr std::string_view manifest_group = "session";

} // namespace

bool account_memory(std::size_t &used, std::size_t count, std::size_t bytes_each)
{
    if (used > zip::archive_bytes_limit || (bytes_each && count > (zip::archive_bytes_limit - used) / bytes_each)) return false;
    used += count * bytes_each;
    return true;
}

bool memory_groups(setx::Block &block, std::string_view key, std::vector<setx::Block> &out,
                   std::size_t bytes_each, std::size_t &used)
{
    const setx::Member *m = block.member(key);
    if (m && m->value.kind == setx::Value::Kind::list && !account_memory(used, m->value.items.size(), bytes_each))
        block.fail(*m, xpp::format("too many {} for the session memory limit", key));
    return block.groups(key, out, zip::archive_bytes_limit / bytes_each);
}

Result<std::string> manifest_text(const Manifest &m)
{
    setx::Out out;
    out.begin(manifest_group);
    out.whole("version", m.version);
    out.text("model", m.model_name);
    if (!m.anifile.empty()) out.text("anifile", m.anifile);
    out.flag("data", m.data);
    out.end();
    return out.result("session");
}

xpp::Result<Manifest> parse_manifest(std::string file, std::string_view text)
{
    return setx::read_block("session", std::move(file), text, true, [](setx::Block &file_block) {
        Manifest m;
        setx::Block g = *file_block.group(manifest_group);
        g.whole("version", m.version);
        if (m.version != format_version)
            g.fail(*g.member("version"), xpp::format("version {}: this program reads session files of version {}", m.version, format_version));
        g.text("model", m.model_name);
        if (!odex::is_odex(m.model_name)) g.fail(*g.member("model"), "a session stores an .odex model only; .ode is refused");
        if (g.has("anifile")) g.text("anifile", m.anifile);
        g.flag("data", m.data);
        g.done();
        return m;
    });
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

std::string with_extension(std::string_view name, std::string_view ext)
{
    std::string n(name);
    if (!xpp::files::has_extension(n, ext)) n += ext;
    return n;
}

bool is_session_file(std::string_view path) { return xpp::files::has_extension(path, extension); }

std::string session_file_name(std::string_view name) { return with_extension(name, extension); }

} // namespace xpp::snapx
