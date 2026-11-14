/* core/xpp_text.h's implementation: text formatting, number and word parsers, JSON strings and base64.
   Nothing here throws: a failed allocation is xpp::out_of_memory's. */
#include "xpp_text.h"
#include "xpp_log.h"
#include "xpp_mem.h"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <string>

namespace xpp {

void format_failed(const char *file, int line) noexcept
{
    xpp::log(XPP_LOG_ERROR, "out of memory formatting a string at {}:{:d}\n", file, line);
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


namespace {
template<class Integer>
bool parse_whole(std::string_view text, Integer &value)
{
    Integer v = 0;
    const std::from_chars_result r = std::from_chars(text.data(), text.data() + text.size(), v);
    if (text.empty() || r.ec != std::errc() || r.ptr != text.data() + text.size()) return false;
    value = v;
    return true;
}
} // namespace

bool parse_bool(std::string_view text, bool &value)
{
    if (text != "true" && text != "false") return false;
    value = text == "true";
    return true;
}

bool parse_int(std::string_view text, int &value) { return parse_whole(text, value); }
bool parse_uint64(std::string_view text, std::uint64_t &value) { return parse_whole(text, value); }

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

/* a byte a JSON string holds as it is: printable ASCII but the quote and the backslash */
bool plain_ascii(unsigned char c)
{
    return c >= 0x20 && c < 0x80 && c != '"' && c != '\\';
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
        /* a run of bytes that need no escape is copied at once */
        const size_t start = i;
        while (i < n && plain_ascii(static_cast<unsigned char>(s[i]))) i++;
        out.append(s.data() + start, i - start);
        if (i == n) break;
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

/* ---- per-user settings files ------------------------------------------ */

} // namespace xpp
