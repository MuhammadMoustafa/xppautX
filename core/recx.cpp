/* A recording's file, name.recx: see recx.h. */
#include "recx.h"
#include "model_files.h"
#include "setx.h"
#include "snapx.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_sha256.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace xpp::recx {

namespace {

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
        for (std::string_view line : split_lines(f.bytes)) out.push_back(escaped(line));
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

Result<std::string> text(const Recording &r)
{
    std::string out;
    std::vector<std::string> hashed;
    auto add = [&out](std::string_view line) {
        out += line;
        out += '\n';
    };
    setx::Out header;
    header.begin(header_group);
    header.whole("version", format_version);
    header.text("program", r.program);
    header.text("model", r.model);
    header.text("recorded", r.recorded);
    header.end();
    Result<std::string> head = header.result("recording");
    if (!head) return std::unexpected(head.error());
    out += *head;
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
        for (std::string_view line : split_lines(s.note)) add(line.empty() ? "#" : "# " + std::string(line));
        add(s.line);
        hashed.push_back(s.line);
    }
    add("");
    add(std::string(fingerprint_start) + (r.fingerprint.empty() ? fingerprint(hashed) : r.fingerprint));
    return out;
}

Result<Read> read(std::string_view text, std::string file)
{
    return read_lines("recording", std::move(file), text, [](Lines &l) {
        Read got;
        Recording &r = got.rec;
        std::vector<std::string> hashed;
        /* the header: the lines before the first section's, in the settings syntax */
        std::string header;
        std::string_view first_section;
        while (!l.at_end()) {
            const std::string_view line = l.next();
            if (line.starts_with('@')) {
                first_section = line;
                break;
            }
            header += line;
            header += '\n';
        }
        /* text that is no settings at all is not a recording: said so (a header that parses and is wrong is told what is) */
        if (Result<setx::Value> syntax = setx::read_tree("recording", l.file(), header); !syntax) {
            syntax.error().what = xpp::format("not a recording: {}", syntax.error().what);
            throw ReadFailed{syntax.error()};
        }
        Result<int> parsed = setx::read_block("recording", l.file(), header, true, [&r](setx::Block &file) {
            setx::Block g = *file.group(header_group);
            int version = 0;
            g.whole("version", version);
            if (version != format_version)
                g.fail(*g.member("version"), xpp::format("version {}: this program reads recordings of version {}", version, format_version));
            g.text("program", r.program);
            g.text("model", r.model);
            if (!odex::is_odex(r.model)) g.fail(*g.member("model"), "a recording stores an .odex model only; .ode is refused");
            g.text("recorded", r.recorded);
            g.done();
            return 0;
        });
        if (!parsed) throw ReadFailed{parsed.error()};
        if (first_section.empty()) l.fail("no @snapshot section: a recording begins with the session's state");
        bool snapshot = false;
        for (std::string_view h = first_section; h != steps_line; h = l.next(steps_line)) {
            if (h.empty()) continue;
            const bool is_snapshot = h == snapshot_line && !snapshot;
            const bool binary = is_snapshot || h.starts_with(binary_start);
            if (!binary && !h.starts_with(file_start))
                l.fail(xpp::format("\"{}\" where a @snapshot, @file or @binary section or @steps was due", h));
            if (!snapshot && !is_snapshot)
                l.fail(xpp::format("no {} section before the files: a recording begins with the session's state", snapshot_line));
            ModelFile f;
            f.name = is_snapshot ? std::string("the snapshot") : std::string(h.substr(binary ? binary_start.size() : file_start.size()));
            if (!is_snapshot && xpp::files::has_extension(f.name, ".ode"))
                l.fail("a recording cannot carry .ode model text");
            hashed.emplace_back(h);
            const int first = l.line();
            std::string digits;
            for (;;) {
                if (l.at_end()) l.fail(first, xpp::format("the section of {} has no {}", f.name, end_line));
                const std::string_view line = l.next();
                if (line == end_line) break;
                hashed.emplace_back(line);
                if (binary) {
                    digits += line;
                } else {
                    f.bytes += unescaped(line);
                    f.bytes += '\n';
                }
            }
            hashed.emplace_back(end_line);
            if (binary && !base64_decode_append(f.bytes, digits)) l.fail(first, xpp::format("the section of {} is not base64", f.name));
            if (is_snapshot) {
                r.snapshot = std::move(f.bytes);
                r.snapshot_at = first;
                snapshot = true;
            } else {
                r.files.push_back(std::move(f));
            }
        }
        if (!snapshot) l.fail(xpp::format("no {} section: a recording begins with the session's state", snapshot_line));
        std::string note;
        bool noted = false;
        for (;;) {
            const std::string_view h = l.next("its fingerprint");
            if (h.empty()) continue;
            if (h.starts_with(fingerprint_start)) {
                if (noted) l.fail("the fingerprint after a note: a note is shown above the step after it");
                r.fingerprint = h.substr(fingerprint_start.size());
                break;
            }
            if (h.starts_with('#')) {
                std::string_view n = h.substr(1);
                if (n.starts_with(' ')) n.remove_prefix(1);
                if (noted) note += '\n';
                note += n;
                noted = true;
                continue;
            }
            r.steps.push_back({std::move(note), std::string(h), l.line()});
            hashed.emplace_back(h);
            note.clear();
            noted = false;
        }
        l.end();
        got.intact = r.fingerprint == fingerprint(hashed);
        return got;
    });
}

} // namespace xpp::recx
