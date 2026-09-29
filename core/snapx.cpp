/* The session file's pure part: see snapx.h. */
#include "snapx.h"
#include "xpp_sha256.h"

#include <charconv>
#include <cctype>

namespace xpp::snapx {

namespace {

constexpr std::string_view first_line = "xppautX session ";

void add_line(std::string &o, std::string_view key, std::string_view value)
{
    o += key;
    if (!value.empty()) {
        o += ' ';
        o += value;
    }
    o += '\n';
}

std::string joined(const std::vector<std::string> &names)
{
    std::string o;
    for (const std::string &n : names) {
        if (!o.empty()) o += ' ';
        o += n;
    }
    return o;
}

std::vector<std::string> words(std::string_view s)
{
    std::vector<std::string> w;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ') i++;
        const std::size_t j = s.find(' ', i);
        const std::size_t e = j == std::string_view::npos ? s.size() : j;
        if (e > i) w.emplace_back(s.substr(i, e - i));
        i = e;
    }
    return w;
}

bool number(std::string_view s, int &v)
{
    const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    return r.ec == std::errc() && r.ptr == s.data() + s.size();
}

bool ends_with_nocase(std::string_view s, std::string_view end)
{
    if (s.size() < end.size()) return false;
    s = s.substr(s.size() - end.size());
    for (std::size_t i = 0; i < end.size(); i++)
        if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(end[i])))
            return false;
    return true;
}

} // namespace

std::string manifest_text(const Manifest &m)
{
    std::string o;
    o += first_line;
    o += std::to_string(m.version);
    o += '\n';
    add_line(o, "model", m.model);
    add_line(o, "name", m.model_name);
    add_line(o, "sha256", m.sha256);
    add_line(o, "node", std::to_string(m.node));
    add_line(o, "markov", std::to_string(m.nmarkov));
    add_line(o, "vars", joined(m.vars));
    add_line(o, "pars", joined(m.pars));
    add_line(o, "data", m.data ? "1" : "0");
    return o;
}

std::optional<Manifest> parse_manifest(std::string_view text)
{
    Manifest m;
    bool first = true;
    while (!text.empty()) {
        const std::size_t nl = text.find('\n');
        std::string_view line = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view() : text.substr(nl + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (first) {
            if (!line.starts_with(first_line) || !number(line.substr(first_line.size()), m.version) || m.version != 1)
                return std::nullopt;
            first = false;
            continue;
        }
        if (line.empty()) continue;
        const std::size_t sp = line.find(' ');
        const std::string_view key = line.substr(0, sp);
        const std::string_view value = sp == std::string_view::npos ? std::string_view() : line.substr(sp + 1);
        if (key == "model") m.model = value;
        else if (key == "name") m.model_name = value;
        else if (key == "sha256") m.sha256 = value;
        else if (key == "node") {
            if (!number(value, m.node)) return std::nullopt;
        } else if (key == "markov") {
            if (!number(value, m.nmarkov)) return std::nullopt;
        } else if (key == "vars") m.vars = words(value);
        else if (key == "pars") m.pars = words(value);
        else if (key == "data") m.data = value == "1";
        /* a key a later writer adds is left for it */
    }
    if (first) return std::nullopt;
    return m;
}

std::string fingerprint(std::span<const std::string> files)
{
    Sha256 h;
    for (const std::string &f : files) {
        const std::string length = std::to_string(f.size()) + '\n';
        h.update(length.data(), length.size());
        h.update(f.data(), f.size());
    }
    return h.hex();
}

bool is_session_file(std::string_view path) { return ends_with_nocase(path, extension); }

std::string session_file_name(std::string_view name)
{
    std::string n(name);
    if (!is_session_file(n)) n += extension;
    return n;
}

} // namespace xpp::snapx
