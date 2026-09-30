/* core/xpp_io.h's implementation: the file half (line reader, token
   reader, writer) and xpp::format/xpp::vformat. No exception crosses
   into C (CLAUDE.md, "C and C++"). */
#include "xpp_io.h"
#include "xpp_files.h"
#include "xpp_log.h"

#include <iterator>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
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

/* fp is the stream read; owned holds it too when the reader opened it
   itself (and closes it with the reader), and is empty when attached;
   without fp, the lines are text's, from pos on (xpp::line_reader_of_text) */
struct XppLineReader {
    std::FILE *fp = nullptr;
    xpp::UniqueFile owned;
    std::string line;
    std::string text;
    size_t pos = 0;
};

struct XppTokenReader {
    std::FILE *fp = nullptr;
    xpp::UniqueFile owned;
};

struct XppWriter {
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

/* path split at the last slash (either kind: a checkout may build on
   either platform); dir is empty for a bare file name (today's model
   files: always relative to the working directory). */
void split_path(const std::string &path, std::string &dir, std::string &base)
{
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        dir.clear();
        base = path;
    } else {
        dir = path.substr(0, pos);
        base = path.substr(pos + 1);
    }
}

} // namespace

XppLineReader *xpp_line_reader_open(const char *path)
{
    xpp::UniqueFile fp(xpp_files_open_stream(path, "r"));
    if (!fp) return nullptr;
    try {
        XppLineReader *r = new XppLineReader;
        r->fp = fp.get();
        r->owned = std::move(fp);
        return r;
    } catch (const std::bad_alloc &) {
        return nullptr;
    }
}

XppLineReader *xpp_line_reader_attach(FILE *fp)
{
    if (!fp) return nullptr;
    try {
        XppLineReader *r = new XppLineReader;
        r->fp = fp;
        return r;
    } catch (const std::bad_alloc &) {
        return nullptr;
    }
}

XppLineReader *xpp::line_reader_of_text(std::string text) noexcept
{
    try {
        XppLineReader *r = new XppLineReader;
        r->text = std::move(text);
        return r;
    } catch (const std::bad_alloc &) {
        return nullptr;
    }
}

namespace {

/* the next line of r's text, as read_line reads a file's */
bool text_line(XppLineReader &r)
{
    if (r.pos >= r.text.size()) return false;
    size_t nl = r.text.find('\n', r.pos);
    if (nl == std::string::npos) nl = r.text.size();
    r.line.assign(r.text, r.pos, nl - r.pos);
    r.pos = nl + 1;
    if (!r.line.empty() && r.line.back() == '\r') r.line.pop_back();
    return true;
}

} // namespace

const char *xpp_line_reader_next(XppLineReader *r, size_t *len)
{
    if (!r || !(r->fp ? read_line(r->fp, r->line) : text_line(*r))) {
        if (len) *len = 0;
        return nullptr;
    }
    if (len) *len = r->line.size();
    return r->line.c_str();
}

void xpp_line_reader_close(XppLineReader *r) { delete r; }

XppTokenReader *xpp_token_reader_open(const char *path)
{
    xpp::UniqueFile fp(xpp_files_open_stream(path, "r"));
    if (!fp) return nullptr;
    try {
        XppTokenReader *r = new XppTokenReader;
        r->fp = fp.get();
        r->owned = std::move(fp);
        return r;
    } catch (const std::bad_alloc &) {
        return nullptr;
    }
}

XppTokenReader *xpp_token_reader_attach(FILE *fp)
{
    if (!fp) return nullptr;
    try {
        XppTokenReader *r = new XppTokenReader;
        r->fp = fp;
        return r;
    } catch (const std::bad_alloc &) {
        return nullptr;
    }
}

int xpp_token_reader_double(XppTokenReader *r, double *out)
{
    std::string tok;
    if (!r || !r->fp || !read_token(r->fp, tok)) return 0;
    char *end = nullptr;
    double v = std::strtod(tok.c_str(), &end);
    if (end == tok.c_str()) return 0;
    *out = v;
    return 1;
}

/* strtof, not (float)strtod: fscanf "%f"/"%g" rounds the decimal
   straight to float, and rounding through double first can differ. */
int xpp_token_reader_float(XppTokenReader *r, float *out)
{
    std::string tok;
    if (!r || !r->fp || !read_token(r->fp, tok)) return 0;
    char *end = nullptr;
    float v = std::strtof(tok.c_str(), &end);
    if (end == tok.c_str()) return 0;
    *out = v;
    return 1;
}

int xpp_token_reader_int(XppTokenReader *r, int *out)
{
    std::string tok;
    if (!r || !r->fp || !read_token(r->fp, tok)) return 0;
    char *end = nullptr;
    long v = std::strtol(tok.c_str(), &end, 10);
    if (end == tok.c_str()) return 0;
    *out = static_cast<int>(v);
    return 1;
}

/* fscanf "%ld"'s own grammar, one character of lookahead pushed back as
   fscanf pushes it back, so a field that follows with no space between
   (AUTO's "%5ld" columns) is the next read's. */
int xpp_token_reader_long(XppTokenReader *r, long *out)
{
    if (!r || !r->fp) return 0;
    std::string num;
    int c;
    do {
        c = std::fgetc(r->fp);
    } while (c != EOF && std::isspace(static_cast<unsigned char>(c)));
    if (c == '+' || c == '-') {
        num.push_back(static_cast<char>(c));
        c = std::fgetc(r->fp);
    }
    while (c != EOF && std::isdigit(static_cast<unsigned char>(c))) {
        num.push_back(static_cast<char>(c));
        c = std::fgetc(r->fp);
    }
    if (c != EOF) std::ungetc(c, r->fp);
    if (num.empty() || !std::isdigit(static_cast<unsigned char>(num.back()))) return 0;
    *out = std::strtol(num.c_str(), nullptr, 10);
    return 1;
}

int xpp_token_reader_skip_line(XppTokenReader *r)
{
    if (!r || !r->fp) return 0;
    int c;
    while ((c = std::fgetc(r->fp)) != EOF)
        if (c == '\n') return 1;
    return 0;
}

namespace {

/* xpp_token_reader_string's own truncating copy (never overflows buf,
   NUL-terminates, warns once if the token does not fit): the general
   xpp_strlcpy this used to call was retired at W48 once nothing in core
   called it directly any more, but this one caller still needs the same
   safe, non-overflowing copy fscanf "%s" itself never was. */
void copy_token(char *buf, size_t bufsize, const std::string &tok)
{
    size_t n = tok.size() < bufsize - 1 ? tok.size() : bufsize - 1;
    if (n > 0) std::memcpy(buf, tok.data(), n);
    buf[n] = '\0';
    if (tok.size() >= bufsize) {
        static std::mutex m;
        static bool warned = false;
        std::lock_guard<std::mutex> lock(m);
        if (!warned) {
            warned = true;
            xpp::log(XPP_LOG_WARN, "xpp_token_reader_string: wanted {} bytes, buffer is {}: truncated\n",
                     tok.size() + 1, bufsize);
        }
    }
}

} // namespace

int xpp_token_reader_string(XppTokenReader *r, char *buf, size_t bufsize)
{
    std::string tok;
    if (!r || !r->fp || !read_token(r->fp, tok)) return 0;
    if (bufsize > 0) copy_token(buf, bufsize, tok);
    return 1;
}

void xpp_token_reader_close(XppTokenReader *r) { delete r; }

namespace {

XppWriter *writer_open(const char *path, int how)
{
    if (!path || !*path) {
        xpp_log(XPP_LOG_ERROR, "xpp_writer_open: no destination path given\n");
        return nullptr;
    }
    try {
        std::unique_ptr<XppWriter> w = std::make_unique<XppWriter>();
        w->target = path;
        if (how == XPP_WRITE_APPEND) {
            w->fp.reset(xpp_files_open_stream(path, "a"));
            if (!w->fp) {
                xpp::log(XPP_LOG_ERROR, "xpp_writer_open: cannot open {} to append to it\n", path);
                return nullptr;
            }
            return w.release();
        }
        std::string dir, base;
        split_path(path, dir, base);
        /* exclusive: a name some other run left behind is skipped */
        for (int tries = 0; tries < 100 && !w->fp; tries++) {
            w->tmp = std::format("{}{}.{}.tmp-{}-{}", dir, dir.empty() ? "" : "/", base, writer_pid(),
                                 writer_serial.fetch_add(1, std::memory_order_relaxed));
            w->fp.reset(xpp_files_create_new(w->tmp.c_str(), how == XPP_WRITE_BINARY));
            if (!w->fp && errno != EEXIST) break;
        }
        if (!w->fp) {
            xpp::log(XPP_LOG_ERROR, "xpp_writer_open: cannot create a temp file for {}\n", path);
            return nullptr;
        }
        return w.release();
    } catch (...) {
        xpp::log(XPP_LOG_ERROR, "out of memory opening {} for writing\n", path);
        return nullptr;
    }
}

/* the temp file closed and removed (Windows cannot remove an open file);
   an append's own file only closed */
void writer_discard(XppWriter *w)
{
    w->fp.reset();
    if (!w->tmp.empty()) xpp_files_remove(w->tmp.c_str());
    delete w;
}

} // namespace

XppWriter *xpp_writer_open(const char *path) { return writer_open(path, XPP_WRITE_TEXT); }

XppWriter *xpp_writer_open_as(const char *path, int how) { return writer_open(path, how); }

FILE *xpp_writer_file(XppWriter *w) { return w ? w->fp.get() : nullptr; }

int xpp_writer_printf(XppWriter *w, const char *fmt, ...)
{
    if (!w || !w->fp) return -1;
    va_list ap;
    va_start(ap, fmt);
    int r = std::vfprintf(w->fp.get(), fmt, ap);
    va_end(ap);
    if (r < 0) xpp_log(XPP_LOG_ERROR, "xpp_writer_printf: write failed for %s\n", w->target.c_str());
    return r;
}

int xpp_writer_commit(XppWriter *w)
{
    if (!w) return -1;
    int closed = std::fclose(w->fp.release()); /* its result: a write that failed at the flush */
    if (closed != 0) {
        xpp_log(XPP_LOG_ERROR, "xpp_writer_commit: write failed for %s\n", w->target.c_str());
        writer_discard(w);
        return -1;
    }
    if (!w->tmp.empty() && xpp_files_replace_file(w->tmp.c_str(), w->target.c_str()) != 0) {
        xpp_log(XPP_LOG_ERROR, "xpp_writer_commit: cannot replace %s\n", w->target.c_str());
        writer_discard(w);
        return -1;
    }
    delete w;
    return 0;
}

void xpp_writer_abort(XppWriter *w)
{
    if (w) writer_discard(w);
}

namespace xpp {
void format_failed(const char *file, int line) noexcept
{
    xpp_log(XPP_LOG_ERROR, "out of memory formatting a string at %s:%d\n", file, line);
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

void Base64Encoder::push(unsigned char byte)
{
    q_[n_++] = byte;
    if (n_ < 3) return;
    out_ += B64[q_[0] >> 2];
    out_ += B64[(q_[0] & 3) << 4 | q_[1] >> 4];
    out_ += B64[(q_[1] & 15) << 2 | q_[2] >> 6];
    out_ += B64[q_[2] & 63];
    n_ = 0;
}

void Base64Encoder::finish()
{
    if (n_ == 1) {
        out_ += B64[q_[0] >> 2];
        out_ += B64[(q_[0] & 3) << 4];
        out_ += "==";
    } else if (n_ == 2) {
        out_ += B64[q_[0] >> 2];
        out_ += B64[(q_[0] & 3) << 4 | q_[1] >> 4];
        out_ += B64[(q_[1] & 15) << 2];
        out_ += '=';
    }
    n_ = 0;
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
    out.reserve(out.size() + (bytes.size() + 2) / 3 * 4);
    Base64Encoder e(out);
    for (char c : bytes) e.push(static_cast<unsigned char>(c));
    e.finish();
}

bool base64_decode_append(std::string &out, std::string_view text)
{
    Base64Decoder d(out);
    for (char c : text)
        if (!d.feed(c)) return false;
    return d.finish();
}

} // namespace xpp
