/* A recording's file, name.recx: see recx.h. */
#include "recx.h"
#include "xpp_sha256.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace xpp::recx {

namespace {

/* the lines of bytes, each without its "\n" or "\r\n" (none for no bytes;
   a last line without its "\n" is a line too) */
std::vector<std::string_view> lines_of(std::string_view bytes)
{
    std::vector<std::string_view> out;
    while (!bytes.empty()) {
        const size_t nl = bytes.find('\n');
        std::string_view line = bytes.substr(0, nl);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        out.push_back(line);
        if (nl == std::string_view::npos) break;
        bytes.remove_prefix(nl + 1);
    }
    return out;
}

/* a file's line as its section holds it: "@end" and a line starting with
   "@@" get one more "@", so that "@end" alone ends the section */
std::string escaped(std::string_view line)
{
    if (line == "@end" || line.starts_with("@@")) return "@" + std::string(line);
    return std::string(line);
}

} // namespace

void add_file(Recording &r, ModelFile file)
{
    if (std::find(r.files.begin(), r.files.end(), file) == r.files.end()) r.files.push_back(std::move(file));
}

std::string fingerprint(const std::vector<std::string> &hashed)
{
    Sha256 sha;
    for (const std::string &line : hashed) {
        sha.update(line.data(), line.size());
        sha.update("\n", 1);
    }
    return sha.hex();
}

std::string text(const Recording &r)
{
    std::string out;
    std::vector<std::string> hashed;
    auto add = [&out](std::string_view line) {
        out += line;
        out += '\n';
    };
    auto add_hashed = [&](std::string line) {
        add(line);
        hashed.push_back(std::move(line));
    };
    add(format_line);
    add("program: " + r.program);
    add("model: " + r.model);
    add("recorded: " + r.recorded);
    for (const ModelFile &f : r.files) {
        add("");
        add_hashed("@file " + f.name);
        for (std::string_view line : lines_of(f.bytes)) add_hashed(escaped(line));
        add_hashed("@end");
    }
    add("");
    add("@steps");
    for (const Step &s : r.steps) {
        for (std::string_view line : lines_of(s.note)) add(line.empty() ? "#" : "# " + std::string(line));
        add_hashed(s.line);
    }
    add("");
    add("fingerprint: " + fingerprint(hashed));
    return out;
}

} // namespace xpp::recx
