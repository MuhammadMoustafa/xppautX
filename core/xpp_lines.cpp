/* core/xpp_lines.h's implementation: text read by lines. Nothing here touches a file. */
#include "xpp_lines.h"

#include <utility>

namespace xpp {

/* ---- Lines: a file of ours, read (W125) ---- */

/* s split at each sep (empty fields kept) */
std::vector<std::string_view> split_fields(std::string_view s, char sep)
{
    std::vector<std::string_view> f;
    while (true) {
        const std::size_t i = s.find(sep);
        f.push_back(s.substr(0, i));
        if (i == std::string_view::npos) return f;
        s = s.substr(i + 1);
    }
}

std::vector<std::string_view> split_lines(std::string_view text)
{
    std::vector<std::string_view> lines;
    while (!text.empty()) {
        const std::size_t nl = text.find('\n');
        std::string_view l = text.substr(0, nl);
        if (!l.empty() && l.back() == '\r') l.remove_suffix(1);
        lines.push_back(l);
        if (nl == std::string_view::npos) break;
        text.remove_prefix(nl + 1);
    }
    return lines;
}

std::string_view nth_line(std::string_view text, int n)
{
    for (int i = 1; !text.empty(); i++) {
        const std::size_t nl = text.find('\n');
        std::string_view line = text.substr(0, nl);
        if (i == n) {
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            return line;
        }
        if (nl == std::string_view::npos) break;
        text.remove_prefix(nl + 1);
    }
    return {};
}

Place line_place(std::string file, std::string_view text, int n)
{
    return Place{std::move(file), n, 0, std::string(nth_line(text, n))};
}

Lines::Lines(std::string where, std::string file, std::string_view text)
    : where_(std::move(where)), file_(std::move(file)), text_(text), lines_(split_lines(text_))
{
}

std::string_view Lines::text(int n) const noexcept
{
    return n >= 1 && static_cast<std::size_t>(n) <= lines_.size() ? lines_[static_cast<std::size_t>(n) - 1]
                                                                    : std::string_view();
}

Error Lines::error(int n, std::string what) const
{
    return Error{where_, std::move(what), Place{file_, n, 0, std::string(text(n))}};
}

void Lines::fail(int n, std::string what) const { throw ReadFailed{error(n, std::move(what))}; }

std::string_view Lines::next(std::string_view what)
{
    if (at_end()) {
        std::string cause = "the file ends here";
        what = trim_blanks(what);
        if (!what.empty()) cause += xpp::format(", before {}", what);
        fail(line() + 1, std::move(cause));
    }
    return lines_[next_++];
}

namespace {

/* the next line's first word through parse (parse_int or parse_number),
   kind naming what it must be */
template <class T>
T first_word(Lines &lines, std::string_view what, const char *kind, bool (*parse)(std::string_view, T &))
{
    const std::string_view line = lines.next(what);
    const std::string_view text = trim_blanks(line);
    T value{};
    if (!parse(text.substr(0, text.find_first_of(" \t")), value)) {
        const std::string_view name = trim_blanks(what);
        lines.fail(xpp::format("\"{}\" is not {}{}", line, kind, name.empty() ? std::string() : xpp::format(" ({})", name)));
    }
    return value;
}

} // namespace

int Lines::whole(std::string_view what, bool named)
{
    const int value = first_word(*this, what, "a whole number", parse_int);
    if (named) check_name(what);
    return value;
}

double Lines::real(std::string_view what, bool named)
{
    const double value = first_word(*this, what, "a number", parse_number);
    if (named) check_name(what);
    return value;
}

std::string_view Lines::value_name() const
{
    const std::string_view value = trim_blanks(text(line()));
    const std::size_t end = value.find_first_of(" \t");
    return end == std::string_view::npos ? std::string_view() : trim_blanks(value.substr(end));
}

void Lines::check_name(std::string_view what) const
{
    const std::string_view name = value_name();
    if (name != trim_blanks(what))
        fail(xpp::format("name \"{}\" differs from \"{}\"", name, trim_blanks(what)));
}

void Lines::heading(std::string_view heading)
{
    const std::string_view line = next(heading);
    if (!line.starts_with("#")) fail(xpp::format("\"{}\" is not the heading \"{}\"", line, trim_blanks(heading)));
}

void Lines::end()
{
    while (!at_end())
        if (!trim_blanks(next()).empty()) fail(xpp::format("\"{}\" after the end of what the file holds", text(line())));
}



} // namespace xpp
