/* recent.txt: see xpp_recent.h. */
#include "xpp_recent.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "json_error.h"
#include "xpp_files.h"
#include "xpp_io.h"

namespace xpp::recent {

namespace {

constexpr std::string_view WHERE = "recent models"; /* the Error's `where` */

bool has_control(std::string_view text)
{
    return std::ranges::any_of(text, [](char c) { return static_cast<unsigned char>(c) < ' ' || c == 0x7f; });
}

} // namespace

Result<std::vector<std::string>> parse_recent(std::string_view text, std::string file)
{
    if (text.size() > MAX_FILE_BYTES)
        return fail_reading(std::string(WHERE), xpp::format("the file is longer than {} bytes", MAX_FILE_BYTES), std::move(file));
    return read_lines(std::string(WHERE), std::move(file), text, [](Lines &l) {
        std::vector<std::string> entries;
        while (!l.at_end()) {
            const std::string_view line = l.next("a model's path");
            if (entries.size() == MAX_ENTRIES) l.fail(xpp::format("more than {} models are listed", MAX_ENTRIES));
            if (line.empty()) l.fail("the line is empty: one absolute path to a model on each line");
            if (line.size() > MAX_PATH_BYTES) l.fail(xpp::format("the path is longer than {} bytes", MAX_PATH_BYTES));
            if (has_control(line)) l.fail("the path has a control character");
            if (!files::is_absolute(line)) l.fail(xpp::format("\"{}\" is not an absolute path", line));
            if (std::ranges::find(entries, line) != entries.end()) l.fail(xpp::format("\"{}\" is listed twice", line));
            entries.emplace_back(line);
        }
        return entries;
    });
}

std::string serialize_recent(const std::vector<std::string> &entries)
{
    std::string out;
    for (const std::string &entry : entries) out += entry + "\n";
    return out;
}

std::string recent_path() { return files::config_path(FILE_NAME); }

Result<std::vector<std::string>> load_recent()
{
    const Result<std::optional<std::string>> text = read_config_file(WHERE, FILE_NAME, MAX_FILE_BYTES);
    if (!text) return std::unexpected(text.error());
    if (!*text) return std::vector<std::string>{};
    return parse_recent(**text, recent_path());
}

Result<> note(std::string_view model)
{
    /* a recording playing is not the user opening models: it keeps no list of theirs */
    if (files::serving_reads()) return {};
    Result<std::vector<std::string>> entries = load_recent();
    if (!entries) return std::unexpected(entries.error());
    std::erase(*entries, model);
    entries->insert(entries->begin(), std::string(model));
    if (entries->size() > MAX_ENTRIES) entries->resize(MAX_ENTRIES);
    return write_config_file(WHERE, FILE_NAME, serialize_recent(*entries));
}

std::string start_json(const Result<std::vector<std::string>> &loaded)
{
    std::string out = "{\"path\":";
    json_append_string(out, recent_path());
    out += xpp::format(",\"limit\":{},\"recent\":[", MAX_ENTRIES);
    if (loaded) {
        bool first = true;
        for (const std::string &entry : *loaded) {
            if (!first) out += ',';
            first = false;
            out += "{\"path\":";
            json_append_string(out, entry);
            out += files::exists(entry) ? ",\"missing\":false}" : ",\"missing\":true}";
        }
    }
    out += ']';
    if (!loaded) json::append_error(out, loaded.error());
    return out + "}";
}

} // namespace xpp::recent
