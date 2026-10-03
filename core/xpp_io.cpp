/* core/xpp_io.h's implementation: the file handles (LineReader,
   TokenReader, Writer) and xpp::format/xpp::vformat. Nothing here throws:
   a failed allocation is xpp::out_of_memory's. */
#include "xpp_io.h"
#include "xpp_files.h"
#include "xpp_log.h"
#include "xpp_mem.h"

#include <iterator>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <utility>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

/* ===================================================================
   The file half: line reader, token reader, writer (issue: W11 step 3,
   W7b). See xpp_io.h for the contract. */

/* A reader reads the stream of: owned when it opened the file itself
   (closed with the reader), attached when it was handed one; with
   neither, the lines are text's, from pos on (LineReader::of_text) */
struct xpp::LineReader::State {
    xpp::UniqueFile owned;
    std::FILE *attached = nullptr;
    std::string line;
    std::string text;
    size_t pos = 0;
    std::FILE *fp() const noexcept { return owned ? owned.get() : attached; }
};

struct xpp::TokenReader::State {
    std::FILE *attached = nullptr;
    std::FILE *fp() const noexcept { return attached; }
    std::string_view text;
    std::size_t pos = 0;
    bool text_mode = false;
    bool has_input() const noexcept { return fp() || text_mode; }
    int get() noexcept
    {
        return text_mode ? (pos < text.size() ? static_cast<unsigned char>(text[pos++]) : EOF) : std::fgetc(fp());
    }
    /* c, the last character read, back to the stream (one at a time) */
    void unget(int c) noexcept
    {
        if (c == EOF) return;
        if (text_mode) --pos;
        else std::ungetc(c, fp());
    }
};

/* the temp file being written, and the file it replaces at commit */
struct xpp::Writer::State {
    xpp::UniqueFile fp;
    std::string tmp, target;
};

namespace {

/* ---- lines -------------------------------------------------------- */

/* Reads one line via fgetc, growing `line` without limit; strips a
   trailing \r so \n and \r\n both end a line. Returns false only at a
   real end of file (nothing at all was read) so an unterminated final
   line still comes back once, not twice. */
bool read_line(std::FILE *fp, std::string &line)
{
    line.clear();
    int c;
    bool any = false;
    while ((c = std::fgetc(fp)) != EOF) {
        any = true;
        if (c == '\n') break;
        line.push_back(static_cast<char>(c));
    }
    if (!any) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return true;
}

/* the next line of r's text, as read_line reads a file's */
bool text_line(xpp::LineReader::State &r)
{
    if (r.pos >= r.text.size()) return false;
    size_t nl = r.text.find('\n', r.pos);
    if (nl == std::string::npos) nl = r.text.size();
    r.line.assign(r.text, r.pos, nl - r.pos);
    r.pos = nl + 1;
    if (!r.line.empty() && r.line.back() == '\r') r.line.pop_back();
    return true;
}

/* ---- tokens --------------------------------------------------------- */

/* Skips leading whitespace, then collects the run of non-whitespace
   characters into `tok`. False at end of file before any token starts.
   The whitespace that ends the token goes back to the stream, as fscanf
   leaves it, so a line read after a token on an attached FILE* sees the
   same rest of the line. */
bool read_token(xpp::TokenReader::State &in, std::string &tok)
{
    tok.clear();
    int c;
    do {
        c = in.get();
    } while (c != EOF && std::isspace(static_cast<unsigned char>(c)));
    if (c == EOF) return false;
    while (c != EOF && !std::isspace(static_cast<unsigned char>(c))) {
        tok.push_back(static_cast<char>(c));
        c = in.get();
    }
    in.unget(c);
    return true;
}

/* the next token of fp converted by strto (strtod, strtof, or strtol in
   base 10): false at the end of the file or when it does not start with
   a number */
template <class T, class Convert>
bool read_number(xpp::TokenReader::State *in, T &out, Convert strto)
{
    std::string tok;
    try {
        if (!in || !in->has_input() || !read_token(*in, tok)) return false;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("reading a number");
    }
    char *end = nullptr;
    const auto v = strto(tok.c_str(), &end);
    if (end == tok.c_str()) return false;
    out = static_cast<T>(v);
    return true;
}

/* ---- writer's temp file ---------------------------------------------- */

std::atomic<unsigned> writer_serial{0};

long long writer_pid()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

/* A handle's new state (Ptr: the handle's std::unique_ptr<State, Free>),
   over the file path, opened here (closed with the reader), or over the
   stream fp the caller keeps; nullptr when path cannot be opened, fp is
   none, or there is no memory */
template <class Ptr>
Ptr new_state() noexcept
{
    try {
        return Ptr(new typename Ptr::element_type);
    } catch (const std::bad_alloc &) {
        return nullptr;
    }
}

template <class Ptr>
Ptr opened_state(std::string_view path) noexcept
{
    xpp::UniqueFile fp(xpp::files::open_stream(path, "r"));
    if (!fp) return nullptr;
    Ptr st = new_state<Ptr>();
    if (st) st->owned = std::move(fp);
    return st;
}

template <class Ptr>
Ptr attached_state(std::FILE *fp) noexcept
{
    if (!fp) return nullptr;
    Ptr st = new_state<Ptr>();
    if (st) st->attached = fp;
    return st;
}

template <class Ptr>
Ptr writer_open(std::string_view path, bool binary)
{
    if (path.empty() || !xpp::files::write_path_ok(path, true)) return nullptr;
    try {
        Ptr w = new_state<Ptr>();
        if (!w) throw std::bad_alloc();
        w->target = path;
        const auto [dir, base] = xpp::files::split_path(path);
        /* exclusive: a name some other run left behind is skipped */
        constexpr int temp_attempts = 100; /* skip exclusive names left by older processes */
        for (int tries = 0; tries < temp_attempts && !w->fp; tries++) {
            w->tmp = std::format("{}{}.{}.tmp-{}-{}", dir, dir.empty() ? "" : "/", base, writer_pid(),
                                 writer_serial.fetch_add(1, std::memory_order_relaxed));
            w->fp.reset(xpp::files::create_new(w->tmp, binary));
            if (!w->fp && errno != EEXIST) break;
        }
        if (!w->fp) return nullptr;
        return w;
    } catch (...) {
        xpp::log_printf(XPP_LOG_ERROR, "out of memory opening %.*s for writing\n", static_cast<int>(path.size()),
                        path.data());
        return nullptr;
    }
}

/* the temp file closed and removed (Windows cannot remove an open file) */
template <class Ptr>
void writer_discard(Ptr w)
{
    w->fp.reset();
    xpp::files::remove(w->tmp);
}

} // namespace

namespace xpp {

/* ---- LineReader ------------------------------------------------------ */

void LineReader::Free::operator()(State *s) const noexcept { delete s; }

LineReader::LineReader(std::string_view path) noexcept : state_(opened_state<decltype(state_)>(path)) {}

LineReader LineReader::of_text(std::string text) noexcept
{
    LineReader l;
    l.state_ = new_state<decltype(l.state_)>();
    if (l.state_) l.state_->text = std::move(text);
    return l;
}

LineReader LineReader::attach(FILE *fp) noexcept
{
    LineReader l;
    l.state_ = attached_state<decltype(l.state_)>(fp);
    return l;
}

std::optional<std::string_view> LineReader::next()
{
    if (!state_) return std::nullopt;
    State &r = *state_;
    if (!(r.fp() ? read_line(r.fp(), r.line) : text_line(r))) return std::nullopt;
    return std::string_view(r.line);
}

std::string LineReader::line(int n)
{
    for (int i = 1; std::optional<std::string_view> l = next(); i++)
        if (i == n) return std::string(*l);
    return {};
}

/* ---- TokenReader ----------------------------------------------------- */

void TokenReader::Free::operator()(State *s) const noexcept { delete s; }

TokenReader TokenReader::attach(FILE *fp) noexcept
{
    TokenReader t;
    t.state_ = attached_state<decltype(t.state_)>(fp);
    return t;
}

TokenReader TokenReader::of_text(std::string_view text) noexcept
{
    TokenReader t;
    t.state_ = new_state<decltype(t.state_)>();
    if (t.state_) { t.state_->text = text; t.state_->text_mode = true; }
    return t;
}

bool TokenReader::at_end() noexcept
{
    if (!state_) return true;
    int c;
    do { c = state_->get(); } while (c != EOF && std::isspace(static_cast<unsigned char>(c)));
    state_->unget(c);
    return c == EOF;
}

bool TokenReader::read(double &x) noexcept
{
    return read_number(state_.get(), x, [](const char *s, char **e) { return std::strtod(s, e); });
}

bool TokenReader::read(int &x) noexcept
{
    return read_number(state_.get(), x, [](const char *s, char **e) { return std::strtol(s, e, 10); });
}

/* fscanf "%ld"'s own grammar, one character of lookahead pushed back as
   fscanf pushes it back, so a field that follows with no space between
   (AUTO's "%5ld" columns) is the next read's. */
bool TokenReader::read(long &x) noexcept
{
    if (!state_ || !state_->has_input()) return false;
    State &in = *state_;
    std::string num;
    int c;
    do {
        c = in.get();
    } while (c != EOF && std::isspace(static_cast<unsigned char>(c)));
    try {
        if (c == '+' || c == '-') {
            num.push_back(static_cast<char>(c));
            c = in.get();
        }
        while (c != EOF && std::isdigit(static_cast<unsigned char>(c))) {
            num.push_back(static_cast<char>(c));
            c = in.get();
        }
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("reading a number");
    }
    in.unget(c);
    if (num.empty() || !std::isdigit(static_cast<unsigned char>(num.back()))) return false;
    errno = 0;
    x = std::strtol(num.c_str(), nullptr, 10);
    return errno != ERANGE;
}

bool TokenReader::skip_line() noexcept
{
    if (!state_ || !state_->has_input()) return false;
    int c;
    while ((c = state_->get()) != EOF)
        if (c == '\n') return true;
    return false;
}

/* ---- Writer ----------------------------------------------------------- */

void Writer::Free::operator()(State *s) const noexcept { delete s; }

Writer::~Writer() { abort(); }

Writer &Writer::operator=(Writer &&o) noexcept
{
    if (this != &o) {
        abort();
        path_ = std::move(o.path_);
        state_ = std::move(o.state_);
    }
    return *this;
}

Writer::Writer(std::string_view path) noexcept : path_(path), state_(writer_open<decltype(state_)>(path, false)) {}

Writer Writer::binary(std::string_view path) noexcept
{
    Writer w;
    w.path_ = path;
    w.state_ = writer_open<decltype(w.state_)>(path, true);
    return w;
}

FILE *Writer::file() const noexcept { return state_ ? state_->fp.get() : nullptr; }

Result<> Writer::commit() noexcept
{
    if (!state_) return fail("writer", "cannot be written: no open temporary file", Place{path_});
    decltype(state_) w = std::move(state_);
    const bool failed = std::ferror(w->fp.get()) != 0;
    int closed = std::fclose(w->fp.release()); /* its result: a write that failed at the flush */
    if (failed || closed != 0) {
        const Error error{"writer", "cannot be written: the write failed", Place{w->target}};
        writer_discard(std::move(w));
        return std::unexpected(error);
    }
    if (files::replace_file(w->tmp, w->target) != 0) {
        const Error error{"writer", "cannot be replaced", Place{w->target}};
        writer_discard(std::move(w));
        return std::unexpected(error);
    }
    return {};
}

void Writer::abort() noexcept
{
    if (state_) writer_discard(std::move(state_));
}

void format_failed(const char *file, int line) noexcept
{
    xpp::log_printf(XPP_LOG_ERROR, "out of memory formatting a string at %s:%d\n", file, line);
    std::exit(1);
}

std::string vformat(std::string_view fmt, std::format_args args) noexcept
{
    try {
        return std::vformat(fmt, args);
    } catch (...) {
        format_failed(__FILE__, __LINE__);
    }
}

bool parse_number(std::string_view text, double &value)
{
    if (text.empty() || std::isspace(static_cast<unsigned char>(text[0])) ||
        text.find_first_of("xX") != std::string_view::npos)
        return false;
    const std::string s(text); /* strtod needs the NUL */
    char *end = nullptr;
    errno = 0;
    const double v = std::strtod(s.c_str(), &end);
    /* ERANGE on a subnormal result too, which is in range: only an
       overflow or an underflow to 0 is out of it */
    if (end != s.c_str() + s.size() || (errno == ERANGE && (v == 0 || std::isinf(v)))) return false;
    value = v;
    return true;
}

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

Place line_place(std::string file, std::string_view text, int n)
{
    const std::vector<std::string_view> lines = split_lines(text);
    const std::string source = n >= 1 && static_cast<std::size_t>(n) <= lines.size() ? std::string(lines[static_cast<std::size_t>(n) - 1]) : std::string();
    return Place{std::move(file), n, 0, source};
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

void Lines::check_name(std::string_view what) const
{
    const std::string_view value = trim_blanks(text(line()));
    const std::size_t end = value.find_first_of(" \t");
    const std::string_view name = end == std::string_view::npos ? std::string_view() : trim_blanks(value.substr(end));
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

bool parse_int(std::string_view text, int &value)
{
    int v = 0;
    const std::from_chars_result r = std::from_chars(text.data(), text.data() + text.size(), v);
    if (text.empty() || r.ec != std::errc() || r.ptr != text.data() + text.size()) return false;
    value = v;
    return true;
}

void vformat_append(std::string &out, std::string_view fmt, std::format_args args) noexcept
{
    try {
        std::vformat_to(std::back_inserter(out), fmt, args);
    } catch (...) {
        format_failed(__FILE__, __LINE__);
    }
}

namespace {

constexpr std::string_view kHex = "0123456789abcdef";

void append_u00(std::string &out, unsigned char c)
{
    out += "\\u00";
    out += kHex[c >> 4];
    out += kHex[c & 15];
}

void append_utf8(std::string &out, unsigned cp)
{
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xc0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xe0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    } else {
        out += static_cast<char>(0xf0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    }
}

/* the UTF-8 sequence length a leading byte announces, 0 if it cannot
   start one (a continuation byte, or a byte no valid UTF-8 uses) */
int utf8_len(unsigned char c0)
{
    if ((c0 & 0xe0) == 0xc0) return 2;
    if ((c0 & 0xf0) == 0xe0) return 3;
    if ((c0 & 0xf8) == 0xf0) return 4;
    return 0;
}

} // namespace

void json_append_string(std::string &out, const char *s)
{
    json_append_string(out, s ? std::string_view(s) : std::string_view());
}

void json_append_string(std::string &out, std::string_view s)
{
    out += '"';
    json_encode_string(out, s);
    out += '"';
}

void json_encode_string(std::string &out, std::string_view s)
{
    size_t i = 0, n = s.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
            i++;
        } else if (c == '\n') { out += "\\n"; i++; }
        else if (c == '\t') { out += "\\t"; i++; }
        else if (c == '\r') { out += "\\r"; i++; }
        else if (c == '\b') { out += "\\b"; i++; }
        else if (c == '\f') { out += "\\f"; i++; }
        else if (c < 0x20) { append_u00(out, c); i++; }
        else if (c < 0x80) { out += static_cast<char>(c); i++; }
        else {
            int len = utf8_len(c);
            bool valid = len > 0 && i + static_cast<size_t>(len) <= n;
            unsigned cp = c & (0xffu >> (len + 1));
            for (int k = 1; valid && k < len; k++) {
                unsigned char cc = static_cast<unsigned char>(s[i + static_cast<size_t>(k)]);
                if ((cc & 0xc0) != 0x80) { valid = false; break; }
                cp = (cp << 6) | (cc & 0x3f);
            }
            if (valid) {
                /* reject overlong encodings, surrogates and out-of-range
                   code points: not a byte sequence a UTF-8 encoder would
                   ever produce, so not passed through as one */
                if (len == 2 && cp < 0x80) valid = false;
                else if (len == 3 && cp < 0x800) valid = false;
                else if (len == 4 && cp < 0x10000) valid = false;
                else if (cp > 0x10ffff) valid = false;
                else if (cp >= 0xd800 && cp <= 0xdfff) valid = false;
            }
            if (valid) {
                out.append(s.data() + i, static_cast<size_t>(len));
                i += static_cast<size_t>(len);
            } else {
                append_u00(out, c);
                i++;
            }
        }
    }
}

namespace {

/* one \uXXXX's 4 hex digits at *v (which must point just past the 'u'),
   advancing v; -1 on a bad digit */
int read_hex4(const char *&v)
{
    unsigned u = 0;
    for (int i = 0; i < 4; i++) {
        unsigned char d = static_cast<unsigned char>(*v);
        int digit;
        if (d >= '0' && d <= '9') digit = d - '0';
        else if ((d | 32) >= 'a' && (d | 32) <= 'f') digit = (d | 32) - 'a' + 10;
        else return -1;
        u = u * 16 + static_cast<unsigned>(digit);
        v++;
    }
    return static_cast<int>(u);
}

} // namespace

bool json_decode_string(const char *v, std::string &out, size_t max, bool strict)
{
    out.clear();
    if (!v || *v != '"') return false;
    v++;
    auto push_byte = [&](char ch) {
        if (out.size() + 1 < max) out += ch;
    };
    auto push_cp = [&](unsigned cp) {
        std::string tmp;
        append_utf8(tmp, cp);
        for (char ch : tmp) push_byte(ch);
    };
    while (*v && *v != '"') {
        unsigned char c = static_cast<unsigned char>(*v);
        if (c < 0x20) {
            if (strict) return false;
            push_byte(static_cast<char>(c));
            v++;
            continue;
        }
        if (c != '\\') {
            push_byte(static_cast<char>(c));
            v++;
            continue;
        }
        v++;
        unsigned char e = static_cast<unsigned char>(*v);
        if (!e) break; /* a trailing backslash: stop, like the old readers did */
        switch (e) {
        case '"': push_byte('"'); v++; break;
        case '\\': push_byte('\\'); v++; break;
        case '/': push_byte('/'); v++; break;
        case 'n': if (strict) return false; push_byte('\n'); v++; break;
        case 't': if (strict) return false; push_byte('\t'); v++; break;
        case 'r': if (strict) return false; push_byte('\r'); v++; break;
        case 'b': if (strict) return false; push_byte('\b'); v++; break;
        case 'f': if (strict) return false; push_byte('\f'); v++; break;
        case 'u': {
            v++;
            int u1 = read_hex4(v);
            if (u1 < 0) { if (strict) return false; break; }
            unsigned cp = static_cast<unsigned>(u1);
            if (u1 >= 0xd800 && u1 <= 0xdbff) {
                /* a high surrogate: a following \uDCxx completes the pair */
                const char *save = v;
                if (v[0] == '\\' && v[1] == 'u') {
                    const char *v2 = v + 2;
                    int u2 = read_hex4(v2);
                    if (u2 >= 0xdc00 && u2 <= 0xdfff) {
                        cp = 0x10000u + ((static_cast<unsigned>(u1) - 0xd800u) << 10) +
                             (static_cast<unsigned>(u2) - 0xdc00u);
                        v = v2;
                    } else {
                        v = save;
                        if (strict) return false;
                    }
                } else if (strict) return false;
            } else if (u1 >= 0xdc00 && u1 <= 0xdfff) {
                if (strict) return false; /* a lone low surrogate */
            }
            if (strict && cp < 0x20) return false; /* \u0000 etc: a control character too */
            if (cp >= 0xd800 && cp <= 0xdfff) cp = 0xfffd; /* unpaired: U+FFFD, loose mode only */
            push_cp(cp);
            break;
        }
        default:
            if (strict) return false;
            push_byte(static_cast<char>(e)); /* an unknown escape: copied literally */
            v++;
            break;
        }
    }
    return true;
}

int base64_value(int c) noexcept
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

namespace {
constexpr std::string_view B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

bool Base64Decoder::feed(char c)
{
    if (c == '=') {
        pad_++;
        return true;
    }
    const int d = base64_value(static_cast<unsigned char>(c));
    if (d < 0 || pad_) return false;
    q_[n_++] = d;
    if (n_ == 4) {
        out_ += static_cast<char>(q_[0] << 2 | q_[1] >> 4);
        out_ += static_cast<char>(q_[1] << 4 | q_[2] >> 2);
        out_ += static_cast<char>(q_[2] << 6 | q_[3]);
        n_ = 0;
    }
    return true;
}

bool Base64Decoder::finish()
{
    if (n_ == 1 || pad_ > 2) return false;
    if (n_ >= 2) out_ += static_cast<char>(q_[0] << 2 | q_[1] >> 4);
    if (n_ == 3) out_ += static_cast<char>(q_[1] << 4 | q_[2] >> 2);
    n_ = 0;
    return true;
}

void base64_append(std::string &out, std::string_view bytes)
{
    const std::size_t n = bytes.size(), at = out.size();
    /* the digits written in place: a group of three bytes, four digits */
    out.resize(at + (n + 2) / 3 * 4);
    char *o = out.data() + at;
    const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(bytes[i]); };
    std::size_t i = 0;
    for (; i + 3 <= n; i += 3) {
        const unsigned a = byte(i), b = byte(i + 1), c = byte(i + 2);
        *o++ = B64[a >> 2];
        *o++ = B64[(a & 3) << 4 | b >> 4];
        *o++ = B64[(b & 15) << 2 | c >> 6];
        *o++ = B64[c & 63];
    }
    if (n - i == 1) {
        const unsigned a = byte(i);
        *o++ = B64[a >> 2];
        *o++ = B64[(a & 3) << 4];
        *o++ = '=';
        *o++ = '=';
    } else if (n - i == 2) {
        const unsigned a = byte(i), b = byte(i + 1);
        *o++ = B64[a >> 2];
        *o++ = B64[(a & 3) << 4 | b >> 4];
        *o++ = B64[(b & 15) << 2];
        *o++ = '=';
    }
}

bool base64_decode_append(std::string &out, std::string_view text)
{
    Base64Decoder d(out);
    for (char c : text)
        if (!d.feed(c)) return false;
    return d.finish();
}

} // namespace xpp
