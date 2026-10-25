/* The settings syntax's typed reader and writer: see setx.h. The grammar
   itself is the .odex reader's (odex_parse.cpp). */
#include "setx.h"

#include <algorithm>
#include <cmath>

#include "odex.h"
#include "xpp_io.h"

namespace xpp::setx {

Result<Value> read_tree(std::string where, std::string file, std::string_view text)
{
    return odex::read_settings_tree(where, file, text);
}

void Document::fail(int line, int col, std::string what) const
{
    Place place = line_place(file_, text_, line);
    place.col = col;
    throw ReadFailed{Error{where_, std::move(what), std::move(place)}};
}

/* ---- Block ---- */

const Member *Block::find(std::string_view key) const
{
    for (const Member &m : group_->members)
        if (m.key == key) return &m;
    return nullptr;
}

const Member *Block::take(std::string_view key)
{
    const Member *m = find(key);
    if (!m) {
        if (complete_) doc_->fail(*group_, xpp::format("{} has no `{}`", name_, key));
        return nullptr;
    }
    read_[static_cast<std::size_t>(m - group_->members.data())] = true;
    return m;
}

const Value &Block::scalar(const Member &m, std::string_view what) const
{
    if (m.value.kind == Value::Kind::group || m.value.kind == Value::Kind::list)
        doc_->fail(m.value, xpp::format("`{}` takes {}, found a {}", m.key, what, m.value.kind == Value::Kind::group ? "group" : "list"));
    return m.value;
}

namespace {

/* the numbers a list of them puts on a line before it goes on to the next */
constexpr std::size_t numbers_per_line = 8;

/* the word v reads as a number (into x) */
bool word_number(const Value &v, double &x) { return v.kind == Value::Kind::word && xpp::parse_number(v.text, x); }

} // namespace

bool Block::number(std::string_view key, double &v)
{
    const Member *m = take(key);
    if (!m) return false;
    double x = 0;
    if (!word_number(scalar(*m, "a number"), x)) doc_->fail(m->value, xpp::format("`{}`: `{}` is not a number", key, m->value.text));
    v = x;
    return true;
}

bool Block::real(std::string_view key, double &v)
{
    const Member *m = take(key);
    if (!m) return false;
    double x = 0;
    if (!word_number(scalar(*m, "a number"), x) || !std::isfinite(x))
        doc_->fail(m->value, xpp::format("`{}`: `{}` is not a finite number", key, m->value.text));
    v = x;
    return true;
}

bool Block::whole(std::string_view key, int &v)
{
    const Member *m = take(key);
    if (!m) return false;
    int x = 0;
    if (scalar(*m, "a whole number").kind != Value::Kind::word || !xpp::parse_int(m->value.text, x))
        doc_->fail(m->value, xpp::format("`{}`: `{}` is not a whole number", key, m->value.text));
    v = x;
    return true;
}

bool Block::flag(std::string_view key, bool &v)
{
    const Member *m = take(key);
    if (!m) return false;
    if (scalar(*m, "true or false").kind != Value::Kind::word || (m->value.text != "true" && m->value.text != "false"))
        doc_->fail(m->value, xpp::format("`{}`: `{}` is not true or false", key, m->value.text));
    v = m->value.text == "true";
    return true;
}

bool Block::text(std::string_view key, std::string &v)
{
    const Member *m = take(key);
    if (!m) return false;
    v = scalar(*m, "a string").text;
    return true;
}

std::optional<Block> Block::group(std::string_view key)
{
    const Member *m = take(key);
    if (!m) return std::nullopt;
    if (m->value.kind != Value::Kind::group) doc_->fail(m->value, xpp::format("`{}` takes a {{ group }}", key));
    return Block(*doc_, m->value, complete_, std::string(key));
}

bool Block::groups(std::string_view key, std::vector<Block> &out, std::size_t max)
{
    const Member *m = take(key);
    if (!m) return false;
    if (m->value.kind != Value::Kind::list) doc_->fail(m->value, xpp::format("`{}` takes a [ list ]", key));
    if (m->value.items.size() > max)
        doc_->fail(m->value, xpp::format("`{}` has {} entries: at most {}", key, m->value.items.size(), max));
    out.clear();
    out.reserve(m->value.items.size());
    std::size_t k = 0;
    for (const Value &item : m->value.items) {
        if (item.kind != Value::Kind::group) doc_->fail(item, xpp::format("entry {} of `{}` is not a {{ group }}", k + 1, key));
        out.emplace_back(*doc_, item, complete_, xpp::format("{} {}", key, k + 1));
        k++;
    }
    return true;
}

namespace {

/* the items of the list member m (at most max), each a word (or a string
   when strings): the list read into out through convert(item) */
template <class T, class F>
bool read_list(const Document &doc, const Member &m, std::vector<T> &out, std::size_t max, bool strings, F convert)
{
    if (m.value.kind != Value::Kind::list) doc.fail(m.value, xpp::format("`{}` takes a [ list ]", m.key));
    if (m.value.items.size() > max) doc.fail(m.value, xpp::format("`{}` has {} entries: at most {}", m.key, m.value.items.size(), max));
    std::vector<T> items;
    for (const Value &item : m.value.items) {
        if (item.kind == Value::Kind::group || item.kind == Value::Kind::list || (item.kind == Value::Kind::string && !strings))
            doc.fail(item, xpp::format("`{}` holds {}", m.key, strings ? "words and strings" : "bare values"));
        items.push_back(convert(item));
    }
    out = std::move(items);
    return true;
}

} // namespace

bool Block::reals(std::string_view key, std::vector<double> &out, std::size_t max)
{
    const Member *m = take(key);
    return m && read_list(*doc_, *m, out, max, false, [&](const Value &v) {
        double x = 0;
        if (!word_number(v, x) || !std::isfinite(x)) doc_->fail(v, xpp::format("`{}`: `{}` is not a finite number", key, v.text));
        return x;
    });
}

bool Block::reals(std::string_view key, std::span<double> out)
{
    std::vector<double> got;
    if (!reals(key, got, out.size())) return false;
    if (got.size() != out.size())
        doc_->fail(find(key)->value, xpp::format("`{}` has {} numbers: {} are due", key, got.size(), out.size()));
    std::ranges::copy(got, out.begin());
    return true;
}

bool Block::texts(std::string_view key, std::vector<std::string> &out, std::size_t max)
{
    const Member *m = take(key);
    return m && read_list(*doc_, *m, out, max, true, [](const Value &v) { return v.text; });
}

std::vector<const Member *> Block::members()
{
    std::vector<const Member *> all;
    for (const Member &m : group_->members) all.push_back(&m);
    read_.assign(read_.size(), true);
    return all;
}

double Block::number_of(const Member &m) const
{
    double x = 0;
    if (!word_number(scalar(m, "a number"), x)) doc_->fail(m.value, xpp::format("`{}`: `{}` is not a number", m.key, m.value.text));
    return x;
}

std::string Block::text_of(const Member &m) const { return scalar(m, "a string").text; }

const Member *Block::member(std::string_view key) { return take(key); }

void Block::done() const
{
    for (std::size_t k = 0; k < read_.size(); k++)
        if (!read_[k]) doc_->fail(group_->members[k], xpp::format("`{}` is not a setting of {}", group_->members[k].key, name_));
}

/* ---- Out ---- */

bool is_bare_word(std::string_view text)
{
    if (text.empty()) return false;
    for (const char c : text) {
        const bool name = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if (!name && c != '.' && c != '-' && c != '+') return false;
    }
    return true;
}

void Out::put(std::string_view text) { out_ += text; }

void Out::entry(std::string_view key)
{
    if (!odex::is_name(key) && !bad_) bad_ = xpp::format("`{}` cannot be a key of the settings syntax", key);
    out_.append(static_cast<std::size_t>(depth_) * 2, ' ');
    out_ += key;
    out_ += ": ";
}

void Out::quote(std::string_view text)
{
    out_ += '"';
    for (const char c : text) {
        if (c == '\n' || c == '\r') {
            if (!bad_) bad_ = "a newline cannot be written in a string";
            continue;
        }
        if (c == '"' || c == '\\') out_ += '\\';
        out_ += c;
    }
    out_ += '"';
}

void Out::real(std::string_view key, double v)
{
    entry(key);
    put(xpp::number(v));
    put(",\n");
}

void Out::whole(std::string_view key, long long v)
{
    entry(key);
    put(std::to_string(v));
    put(",\n");
}

void Out::word(std::string_view key, std::string_view text_)
{
    if (!is_bare_word(text_)) return text(key, text_);
    entry(key);
    put(text_);
    put(",\n");
}

void Out::text(std::string_view key, std::string_view text_)
{
    entry(key);
    quote(text_);
    put(",\n");
}

void Out::reals(std::string_view key, std::span<const double> v)
{
    entry(key);
    put("[");
    for (std::size_t k = 0; k < v.size(); k++) {
        if (k > 0 && k % numbers_per_line == 0) {
            put(",\n");
            out_.append(static_cast<std::size_t>(depth_ + 1) * 2, ' ');
        } else if (k > 0) {
            put(", ");
        }
        put(xpp::number(v[k]));
    }
    put("],\n");
}

void Out::texts(std::string_view key, std::span<const std::string> v)
{
    entry(key);
    put("[");
    for (std::size_t k = 0; k < v.size(); k++) {
        put(k == 0 ? "" : ", ");
        if (is_bare_word(v[k])) put(v[k]);
        else quote(v[k]);
    }
    put("],\n");
}

void Out::begin(std::string_view key)
{
    entry(key);
    put("{\n");
    depth_++;
}

void Out::end()
{
    depth_--;
    out_.append(static_cast<std::size_t>(depth_) * 2, ' ');
    put("},\n");
}

void Out::begin_list(std::string_view key)
{
    entry(key);
    put("[\n");
    depth_++;
}

void Out::end_list()
{
    depth_--;
    out_.append(static_cast<std::size_t>(depth_) * 2, ' ');
    put("],\n");
}

void Out::begin_item()
{
    out_.append(static_cast<std::size_t>(depth_) * 2, ' ');
    put("{\n");
    depth_++;
}

void Out::end_item() { end(); }

void Out::comment(std::string_view text_)
{
    out_.append(static_cast<std::size_t>(depth_) * 2, ' ');
    put("# ");
    put(text_);
    put("\n");
}

Result<std::string> Out::result(std::string where) const
{
    if (bad_) return fail(std::move(where), *bad_);
    return out_;
}

} // namespace xpp::setx
