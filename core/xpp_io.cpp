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
    xpp::UniqueFile owned;
    std::FILE *attached = nullptr;
    std::FILE *fp() const noexcept { return owned ? owned.get() : attached; }
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
bool read_token(std::FILE *fp, std::string &tok)
{
    tok.clear();
    int c;
    do {
        c = std::fgetc(fp);
    } while (c != EOF && std::isspace(static_cast<unsigned char>(c)));
    if (c == EOF) return false;
    while (c != EOF && !std::isspace(static_cast<unsigned char>(c))) {
        tok.push_back(static_cast<char>(c));
        c = std::fgetc(fp);
    }
    if (c != EOF) std::ungetc(c, fp);
    return true;
}

/* the next token of fp converted by strto (strtod, strtof, or strtol in
   base 10): false at the end of the file or when it does not start with
   a number */
template <class T, class Convert>
bool read_number(std::FILE *fp, T &out, Convert strto)
{
    std::string tok;
    try {
        if (!fp || !read_token(fp, tok)) return false;
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
    if (path.empty()) {
        xpp::log(XPP_LOG_ERROR, "Writer: no destination path given\n");
        return nullptr;
    }
    try {
        Ptr w = new_state<Ptr>();
        if (!w) throw std::bad_alloc();
        w->target = path;
        const auto [dir, base] = xpp::files::split_path(path);
        /* exclusive: a name some other run left behind is skipped */
        for (int tries = 0; tries < 100 && !w->fp; tries++) {
            w->tmp = std::format("{}{}.{}.tmp-{}-{}", dir, dir.empty() ? "" : "/", base, writer_pid(),
                                 writer_serial.fetch_add(1, std::memory_order_relaxed));
            w->fp.reset(xpp::files::create_new(w->tmp, binary));
            if (!w->fp && errno != EEXIST) break;
        }
        if (!w->fp) {
            xpp::log(XPP_LOG_ERROR, "Writer: cannot create a temp file for {}\n", path);
            return nullptr;
        }
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

/* ---- TokenReader ----------------------------------------------------- */

void TokenReader::Free::operator()(State *s) const noexcept { delete s; }

TokenReader::TokenReader(std::string_view path) noexcept : state_(opened_state<decltype(state_)>(path)) {}

TokenReader TokenReader::attach(FILE *fp) noexcept
{
    TokenReader t;
    t.state_ = attached_state<decltype(t.state_)>(fp);
    return t;
}

bool TokenReader::read(double &x) noexcept
{
    return state_ && read_number(state_->fp(), x, [](const char *s, char **e) { return std::strtod(s, e); });
}

/* strtof, not (float)strtod: fscanf "%f"/"%g" rounds the decimal
   straight to float, and rounding through double first can differ. */
bool TokenReader::read(float &x) noexcept
{
    return state_ && read_number(state_->fp(), x, [](const char *s, char **e) { return std::strtof(s, e); });
}

bool TokenReader::read(int &x) noexcept
{
    return state_ && read_number(state_->fp(), x, [](const char *s, char **e) { return std::strtol(s, e, 10); });
}

/* fscanf "%ld"'s own grammar, one character of lookahead pushed back as
   fscanf pushes it back, so a field that follows with no space between
   (AUTO's "%5ld" columns) is the next read's. */
bool TokenReader::read(long &x) noexcept
{
    std::FILE *fp = state_ ? state_->fp() : nullptr;
    if (!fp) return false;
    std::string num;
    int c;
    do {
        c = std::fgetc(fp);
    } while (c != EOF && std::isspace(static_cast<unsigned char>(c)));
    try {
        if (c == '+' || c == '-') {
            num.push_back(static_cast<char>(c));
            c = std::fgetc(fp);
        }
        while (c != EOF && std::isdigit(static_cast<unsigned char>(c))) {
            num.push_back(static_cast<char>(c));
            c = std::fgetc(fp);
        }
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("reading a number");
    }
    if (c != EOF) std::ungetc(c, fp);
    if (num.empty() || !std::isdigit(static_cast<unsigned char>(num.back()))) return false;
    x = std::strtol(num.c_str(), nullptr, 10);
    return true;
}

bool TokenReader::skip_line() noexcept
{
    std::FILE *fp = state_ ? state_->fp() : nullptr;
    if (!fp) return false;
    int c;
    while ((c = std::fgetc(fp)) != EOF)
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
        state_ = std::move(o.state_);
    }
    return *this;
}

Writer::Writer(std::string_view path) noexcept : state_(writer_open<decltype(state_)>(path, false)) {}

Writer Writer::binary(std::string_view path) noexcept
{
    Writer w;
    w.state_ = writer_open<decltype(w.state_)>(path, true);
    return w;
}

FILE *Writer::file() const noexcept { return state_ ? state_->fp.get() : nullptr; }

bool Writer::commit() noexcept
{
    if (!state_) return false;
    decltype(state_) w = std::move(state_);
    int closed = std::fclose(w->fp.release()); /* its result: a write that failed at the flush */
    if (closed != 0) {
        xpp::log(XPP_LOG_ERROR, "Writer: write failed for {}\n", w->target);
        writer_discard(std::move(w));
        return false;
    }
    if (files::replace_file(w->tmp, w->target) != 0) {
        xpp::log(XPP_LOG_ERROR, "Writer: cannot replace {}\n", w->target);
        writer_discard(std::move(w));
        return false;
    }
    return true;
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

int lines_read(FILE *fp)
{
    const long pos = std::ftell(fp);
    if (pos <= 0 || std::fseek(fp, 0, SEEK_SET) != 0) return 0;
    int lines = 0, last = '\n';
    for (long k = 0; k < pos; k++) {
        const int c = std::fgetc(fp);
        if (c == EOF) break;
        if (c == '\n') lines++;
        last = c;
    }
    if (last != '\n') lines++;
    std::fseek(fp, pos, SEEK_SET);
    return lines;
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
