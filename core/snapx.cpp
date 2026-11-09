/* The session file's pure part: see snapx.h. */
#include "snapx.h"
#include "xpp_io.h"

#include "xpp_files.h"
#include "odex_write.h"

namespace xpp::snapx {

bool account_memory(std::size_t &used, std::size_t count, std::size_t bytes_each)
{
    if (used > zip::archive_bytes_limit || (bytes_each && count > (zip::archive_bytes_limit - used) / bytes_each)) return false;
    used += count * bytes_each;
    return true;
}

bool memory_groups(odex::Block &block, std::string_view key, std::vector<odex::Block> &out,
                   std::size_t bytes_each, std::size_t &used)
{
    const odex::Member *m = block.member(key);
    if (m && m->value.kind == odex::Value::Kind::list && !account_memory(used, m->value.items.size(), bytes_each))
        block.fail(*m, xpp::format("too many {} for the session memory limit", key));
    return block.groups(key, out, zip::archive_bytes_limit / bytes_each);
}

void write_manifest(odex::Out &out, const Manifest &m)
{
    out.text("model", m.model_name);
    if (!m.anifile.empty()) out.text("animation", m.anifile);
    out.flag("data", m.data);
}

Manifest read_manifest(odex::Block &session)
{
    Manifest m;
    session.text("model", m.model_name);
    if (!odex::is_odex(m.model_name)) session.fail(*session.member("model"), "a session stores an .odex model only; .ode is refused");
    if (session.has("animation")) {
        session.text("animation", m.anifile);
        if (!files::has_extension(m.anifile, ".anix")) session.fail(*session.member("animation"), "a session stores an .anix animation only");
    }
    session.flag("data", m.data);
    return m;
}

Result<std::string> manifest_text(const Manifest &m)
{
    odex::Out out;
    out.begin_statement("session");
    write_manifest(out, m);
    out.end_statement();
    Result<std::string> body = out.result("session");
    if (!body) return std::unexpected(body.error());
    return odex::version_statement() + "\n" + *body;
}

xpp::Result<Manifest> parse_manifest(std::string file, std::string_view text)
{
    Result<odex::StateFile> parsed = odex::read_state_file(odex::FileKind::session, "session", std::move(file), std::string(text));
    if (!parsed) return std::unexpected(parsed.error());
    try {
        odex::Block session = parsed->block(true, "the session");
        return read_manifest(session);
    } catch (ReadFailed &e) {
        return std::unexpected(std::move(e.error));
    }
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
