/* The JSON reader the protocol's command lines and keymap.json share: it
   reads text in place (a pointer into it), with no tree, strict where a
   file's text is checked (js_problem). Split out of json_io.cpp (W211) so a
   core module that is no part of the front end (xpp_keymap) reads JSON with
   it too, never with a second reader. */
#include "json_reader.h"
#include "xpp_io.h"
#include "xpp_mem.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace xpp::json {

/* ---- a small JSON reader for flat command objects ------------------------ */

const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

const char *skip_value(const char *p)
{
    int depth = 0;
    p = skip_ws(p);
    do {
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) p++;
                p++;
            }
            if (*p) p++;
        } else if (*p == '[' || *p == '{') {
            depth++;
            p++;
        } else if (*p == ']' || *p == '}') {
            if (depth == 0) return p;
            depth--;
            p++;
        } else if (*p == ',' && depth == 0) {
            return p;
        } else if (*p) {
            p++;
        }
        if (depth == 0 && (*p == ',' || *p == '}' || *p == ']')) return p;
    } while (*p);
    return p;
}

namespace {

/* Where valid_value stopped, and why, for js_problem. */
struct Stop {
    const char *at = nullptr;
    const char *why = nullptr;
};

constexpr const char *NOT_JSON = "is not valid JSON";
constexpr const char *TOO_DEEP = "is nested too deeply";
constexpr const char *TRAILING = "has text after its value";

/* Bound recursive validation of untrusted JSON (a recording, a keymap file). */
constexpr int JSON_MAX_DEPTH = 64;

/* nothing is read past a NUL, so a stop is always inside the text */
const char *stop(Stop &st, const char *p, const char *why)
{
    st.at = p;
    st.why = why;
    return nullptr;
}

/* one JSON value at p, strictly (RFC 8259, nesting at most JSON_MAX_DEPTH
   deep): past it, or NULL when it is not one (st says where and why) */
const char *valid_value(const char *p, int depth, Stop &st)
{
    p = skip_ws(p);
    if (depth > JSON_MAX_DEPTH) return stop(st, p, TOO_DEEP);
    if (*p == '{' || *p == '[') {
        const char close = *p == '{' ? '}' : ']';
        p = skip_ws(p + 1);
        if (*p == close) return p + 1;
        for (;;) {
            if (close == '}') {
                if (*p != '"') return stop(st, p, NOT_JSON);
                if (!(p = valid_value(p, depth + 1, st))) return nullptr;
                p = skip_ws(p);
                if (*p++ != ':') return stop(st, p - 1, NOT_JSON);
            }
            if (!(p = valid_value(p, depth + 1, st))) return nullptr;
            p = skip_ws(p);
            if (*p == close) return p + 1;
            if (*p++ != ',') return stop(st, p - 1, NOT_JSON);
            p = skip_ws(p);
        }
    }
    if (*p == '"') {
        for (p++; *p != '"'; p++) {
            if (static_cast<unsigned char>(*p) < 0x20) return stop(st, p, NOT_JSON); /* the end, or a raw control character */
            if (*p == '\\') {
                p++;
                if (*p == 'u') {
                    for (int i = 1; i <= 4; i++)
                        if (!std::isxdigit(static_cast<unsigned char>(p[i]))) return stop(st, p, NOT_JSON);
                    p += 4;
                } else if (!*p || !std::strchr("\"\\/bfnrt", *p)) {
                    return stop(st, p, NOT_JSON);
                }
            }
        }
        return p + 1;
    }
    for (const char *word : {"true", "false", "null"})
        if (std::strncmp(p, word, std::strlen(word)) == 0) return p + std::strlen(word);
    const char *start = p;
    if (*p == '-') p++;
    if (*p == '0') p++;
    else if (*p >= '1' && *p <= '9')
        while (*p >= '0' && *p <= '9') p++;
    else return stop(st, p, NOT_JSON);
    if (*p == '.') {
        if (!(*++p >= '0' && *p <= '9')) return stop(st, p, NOT_JSON);
        while (*p >= '0' && *p <= '9') p++;
    }
    if (*p == 'e' || *p == 'E') {
        if (*++p == '+' || *p == '-') p++;
        if (!(*p >= '0' && *p <= '9')) return stop(st, p, NOT_JSON);
        while (*p >= '0' && *p <= '9') p++;
    }
    return p > start ? p : stop(st, p, NOT_JSON);
}

} // namespace

const char *js_problem(const char *text, const char *&at)
{
    Stop st;
    const char *end = valid_value(text, 0, st);
    if (end && *skip_ws(end) == '\0') return nullptr;
    if (end) stop(st, skip_ws(end), TRAILING);
    at = st.at;
    return st.why;
}

bool js_valid(const char *text)
{
    const char *at;
    return js_problem(text, at) == nullptr;
}

/* value of member key in the object at obj, or NULL */
const char *js_find(const char *obj, const char *key)
{
    const char *p = skip_ws(obj);
    std::string_view want(key);
    if (*p != '{') return nullptr;
    p++;
    for (;;) {
        p = skip_ws(p);
        if (*p != '"') return nullptr;
        const char *k = ++p;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) p++;
            p++;
        }
        if (!*p) return nullptr;
        bool match = std::string_view(k, static_cast<size_t>(p - k)) == want;
        p = skip_ws(p + 1);
        if (*p != ':') return nullptr;
        p = skip_ws(p + 1);
        if (match) return p;
        p = skip_ws(skip_value(p));
        if (*p != ',') return nullptr;
        p++;
    }
}

std::string_view js_raw(const char *v)
{
    if (!v) return {};
    const char *end = skip_value(v);
    while (end > v && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) end--;
    return std::string_view(v, static_cast<size_t>(end - v));
}

std::string js_object_without(const char *obj, std::initializer_list<std::string_view> drop)
{
    std::string out = "{";
    const char *p = skip_ws(obj);
    if (*p == '{') p++;
    for (;;) {
        p = skip_ws(p);
        if (*p != '"') break;
        const char *k = p++;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) p++;
            p++;
        }
        if (!*p) break;
        const std::string_view key(k + 1, static_cast<size_t>(p - k - 1));
        const char *colon = skip_ws(p + 1);
        if (*colon != ':') break;
        const char *v = skip_ws(colon + 1);
        if (std::find(drop.begin(), drop.end(), key) == drop.end()) {
            if (out.size() > 1) out += ',';
            out.append(k, static_cast<size_t>(p + 1 - k));
            out += ':';
            out += js_raw(v);
        }
        p = skip_ws(skip_value(v));
        if (*p != ',') break;
        p++;
    }
    return out + "}";
}

bool js_string(const char *v, std::string &out)
{
    try {
        return xpp::json_decode_string(v, out, std::string::npos, /*strict=*/false);
    } catch (...) {
        xpp::out_of_memory("reading a command");
    }
}

const char *js_elem(const char *arr, int i)
{
    if (!arr || *arr != '[') return nullptr;
    const char *p = skip_ws(arr + 1);
    if (*p == ']') return nullptr;
    while (i-- > 0) {
        p = skip_ws(skip_value(p));
        if (*p != ',') return nullptr;
        p = skip_ws(p + 1);
    }
    return p;
}

bool js_member(const char *&cursor, std::string &key, const char *&value)
{
    const char *p = skip_ws(cursor);
    if (*p == '{' || *p == ',') p++;
    p = skip_ws(p);
    if (*p != '"' || !js_string(p, key)) return false;
    const char *q = p + 1; /* the key's closing quote (skip_value would run on through the value) */
    while (*q && *q != '"') q += (*q == '\\' && q[1]) ? 2 : 1;
    if (!*q) return false;
    p = skip_ws(q + 1);
    if (*p != ':') return false;
    value = skip_ws(p + 1);
    cursor = skip_value(value);
    return true;
}

int js_line(const char *text, const char *at)
{
    return 1 + static_cast<int>(std::count(text, at, '\n'));
}

} // namespace xpp::json
