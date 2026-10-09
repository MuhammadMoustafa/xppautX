/* The bundled examples: see xpp_examples.h. */
#include "xpp_examples.h"

#include <algorithm>

#include "browse.h"
#include "json_error.h"
#include "odex.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_ui.h"

namespace xpp::examples {

namespace {

constexpr std::string_view WHERE = "open example"; /* the Error's `where` */

std::string folder_of(std::string_view parent)
{
    return parent.empty() ? std::string() : files::join(parent, FOLDER);
}

} // namespace

Listing list()
{
    Listing out;
    out.folder = folder_of(files::program_dir());
    if (out.folder.empty()) {
        out.reason = "the program's own folder is not known, so its examples cannot be found";
        return out;
    }
    std::vector<files::DirEntry> entries;
    if (!files::list_dir(out.folder, entries)) {
        out.reason = xpp::format("there is no examples folder at {}", out.folder);
        return out;
    }
    for (const files::DirEntry &e : entries)
        if (!e.folder && files::name_ok(e.name) && files::has_extension(e.name, odex::extension)) out.names.push_back(e.name);
    std::ranges::sort(out.names);
    if (out.names.size() > MAX_EXAMPLES) out.names.resize(MAX_EXAMPLES);
    if (out.names.empty()) out.reason = xpp::format("the folder {} has no {} models", out.folder, odex::extension);
    return out;
}

std::string json()
{
    const Listing l = list();
    std::string out = "{\"folder\":";
    json_append_string(out, l.folder);
    out += ",\"names\":[";
    for (std::size_t i = 0; i < l.names.size(); i++) {
        if (i) out += ',';
        json_append_string(out, l.names[i]);
    }
    out += ']';
    if (!l.reason.empty()) {
        out += ",\"reason\":";
        json_append_string(out, l.reason);
    }
    return out + "}";
}

Result<std::string> install(std::string_view name)
{
    const Listing l = list();
    if (std::ranges::find(l.names, name) == l.names.end())
        return fail(std::string(WHERE), xpp::format("\"{}\" is not one of the bundled examples", name), command_place());
    const std::string source = files::join(l.folder, name);
    std::string bytes;
    xpp::UniqueFile in(files::open_read_within(source, l.folder));
    if (!in || !read_bytes(in.get(), bytes, MAX_EXAMPLE_BYTES))
        return fail(std::string(WHERE), xpp::format("cannot read the example {} (or it is longer than {} bytes)", source, MAX_EXAMPLE_BYTES), command_place());
    const std::string folder = folder_of(files::config_dir());
    if (folder.empty()) return std::unexpected(no_config_folder(WHERE, FOLDER));
    if (!files::make_dirs(folder)) return fail(std::string(WHERE), xpp::format("cannot make the folder {}", folder), command_place());
    const std::string target = files::join(folder, name);
    Result<> opened;
    Writer w = open_writer_asking(target, true, &opened);
    if (!w) return opened ? Result<std::string>(std::string()) : std::unexpected(opened.error());
    if (!w.write(bytes)) {
        w.abort();
        return fail(std::string(WHERE), xpp::format("cannot write {}", target), command_place());
    }
    if (Result<> done = w.commit(); !done) return std::unexpected(done.error());
    return target;
}

} // namespace xpp::examples
