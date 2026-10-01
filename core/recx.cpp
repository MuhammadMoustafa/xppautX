/* A recording's file, name.recx: see recx.h. */
#include "recx.h"
#include "model_files.h"
#include "xpp_io.h"
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

/* a file line as its section holds it, the other way */
std::string_view unescaped(std::string_view line)
{
    if (line.starts_with("@@")) line.remove_prefix(1);
    return line;
}

constexpr std::string_view file_start = "@file ", binary_start = "@binary ", snapshot_line = "@snapshot",
                           end_line = "@end", steps_line = "@steps", fingerprint_start = "fingerprint: ";
constexpr size_t base64_width = 76;

/* bytes in base64, 76 digits a line, into out */
void base64_lines(std::string_view bytes, std::vector<std::string> &out)
{
    std::string digits;
    base64_append(digits, bytes);
    for (size_t i = 0; i < digits.size(); i += base64_width) out.push_back(digits.substr(i, base64_width));
}

/* a file's section, its first and "@end" lines included, as lines */
void section_lines(const ModelFile &f, std::vector<std::string> &out)
{
    if (is_model_text(f.bytes)) {
        out.push_back(std::string(file_start) + f.name);
        for (std::string_view line : lines_of(f.bytes)) out.push_back(escaped(line));
    } else {
        out.push_back(std::string(binary_start) + f.name);
        base64_lines(f.bytes, out);
    }
    out.emplace_back(end_line);
}

} // namespace

size_t add_file(Recording &r, ModelFile file)
{
    const auto at = std::find(r.files.begin(), r.files.end(), file);
    if (at != r.files.end()) return static_cast<size_t>(at - r.files.begin());
    r.files.push_back(std::move(file));
    return r.files.size() - 1;
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
    add(format_line);
    add("program: " + r.program);
    add("model: " + r.model);
    add("recorded: " + r.recorded);
    add("");
    hashed.emplace_back(snapshot_line);
    base64_lines(r.snapshot, hashed);
    hashed.emplace_back(end_line);
    for (const std::string &line : hashed) add(line);
    for (const ModelFile &f : r.files) {
        add("");
        const size_t first = hashed.size();
        section_lines(f, hashed);
        for (size_t i = first; i < hashed.size(); i++) add(hashed[i]);
    }
    add("");
    add(steps_line);
    for (const Step &s : r.steps) {
        for (std::string_view line : lines_of(s.note)) add(line.empty() ? "#" : "# " + std::string(line));
        add(s.line);
        hashed.push_back(s.line);
    }
    add("");
    add(std::string(fingerprint_start) + (r.fingerprint.empty() ? fingerprint(hashed) : r.fingerprint));
    return out;
}

std::optional<Read> read(std::string_view text, std::string &error)
{
    const std::vector<std::string_view> lines = lines_of(text);
    Read got;
    Recording &r = got.rec;
    std::vector<std::string> hashed;
    size_t i = 0;
    auto fail = [&error, &i](std::string_view why) {
        error = xpp::format("line {}: {}", i + 1, why);
        return std::nullopt;
    };
    if (lines.empty() || lines[0] != format_line) return fail(xpp::format("not a recording (it does not begin \"{}\")", format_line));
    for (i = 1; i < lines.size() && !lines[i].empty() && !lines[i].starts_with('@'); i++) {
        const std::string_view l = lines[i];
        if (l.starts_with("program: ")) r.program = l.substr(9);
        else if (l.starts_with("model: ")) r.model = l.substr(7);
        else if (l.starts_with("recorded: ")) r.recorded = l.substr(10);
    }
    bool snapshot = false;
    for (; i < lines.size() && lines[i] != steps_line; i++) {
        const std::string_view l = lines[i];
        if (l.empty()) continue;
        const bool is_snapshot = l == snapshot_line && !snapshot;
        const bool binary = is_snapshot || l.starts_with(binary_start);
        if (!binary && !l.starts_with(file_start))
            return fail(xpp::format("\"{}\" where a @snapshot, @file or @binary section or @steps was due", l));
        if (!snapshot && !is_snapshot) return fail(xpp::format("no {} section before the files: a recording begins with the session's state", snapshot_line));
        ModelFile f;
        f.name = is_snapshot ? std::string("the snapshot") : std::string(l.substr(binary ? binary_start.size() : file_start.size()));
        hashed.emplace_back(l);
        const size_t first = i;
        std::string digits;
        for (i++; i < lines.size() && lines[i] != end_line; i++) {
            hashed.emplace_back(lines[i]);
            if (binary) {
                digits += lines[i];
            } else {
                f.bytes += unescaped(lines[i]);
                f.bytes += '\n';
            }
        }
        if (i == lines.size()) {
            i = first;
            return fail(xpp::format("the section of {} has no {}", f.name, end_line));
        }
        hashed.emplace_back(end_line);
        if (binary && !base64_decode_append(f.bytes, digits)) {
            i = first;
            return fail(xpp::format("the section of {} is not base64", f.name));
        }
        if (is_snapshot) {
            r.snapshot = std::move(f.bytes);
            snapshot = true;
        } else {
            r.files.push_back(std::move(f));
        }
    }
    if (i == lines.size()) return fail(xpp::format("no {} line", steps_line));
    if (!snapshot) return fail(xpp::format("no {} section: a recording begins with the session's state", snapshot_line));
    std::string note;
    bool noted = false;
    for (i++; i < lines.size(); i++) {
        const std::string_view l = lines[i];
        if (l.empty()) continue;
        if (l.starts_with(fingerprint_start)) {
            r.fingerprint = l.substr(fingerprint_start.size());
            break;
        }
        if (l.starts_with('#')) {
            std::string_view n = l.substr(1);
            if (n.starts_with(' ')) n.remove_prefix(1);
            if (noted) note += '\n';
            note += n;
            noted = true;
            continue;
        }
        r.steps.push_back({std::move(note), std::string(l)});
        hashed.emplace_back(l);
        note.clear();
        noted = false;
    }
    got.intact = !r.fingerprint.empty() && r.fingerprint == fingerprint(hashed);
    if (r.fingerprint.empty()) r.fingerprint = "missing"; /* written again so: a note saved does not make one */
    return got;
}

} // namespace xpp::recx
