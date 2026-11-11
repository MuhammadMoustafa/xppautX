/* The settings syntax (odex_block.h, W284): .odex's options syntax read by the
   same tokenizer and block grammar, plus lists. What Out writes reads back
   as the same values; every error names its line and column and the line as
   written; a key that is given twice, a value that is none, a group nested
   deeper than max_depth, a list longer than the reader's maximum and a key
   nothing asked for are refused (nothing allocated from a count, no
   recursion past the bound: the file may be one a user was sent). */
#include "xpptest.h"
#include "odex_block.h"
#include "odex.h"
#include "model.h"
#include "odex_write.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

/* the error of text read as a complete file through read(Block &), "" when it reads */
template <class F> std::string error_of(std::string_view text, F read, bool complete = true)
{
    const auto r = xpp::odex::read_block("test", "t.odex", text, complete, read);
    return r ? std::string() : r.error().text();
}

} // namespace

TEST_CASE("test_odex_block")
{
    using namespace xpp::odex;

    /* A 4 MiB punctuation-only file is refused before the token vector can grow without bound. */
    const std::string punctuation(4u * 1024 * 1024, ',');
    const auto too_many = read_block("test", "punctuation.odex", punctuation, true, [](Block &) { return 0; });
    CHECK(!too_many);
    if (!too_many) {
        CHECK(too_many.error().place.file == "punctuation.odex" && too_many.error().place.line == 1);
        CHECK(too_many.error().place.col == static_cast<int>(xpp::odex::max_file_tokens) + 1);
        CHECK(too_many.error().place.source == punctuation);
        CHECK(too_many.error().what.find("tokens") != std::string::npos);
    }

    /* The budget still admits both full 10000-segment nullcline coordinate lists. */
    constexpr std::size_t curve_coordinates = 40000; /* Four coordinates per allowed segment. */
    std::string curves;
    for (const char *key : {"x", "y"}) {
        curves += std::string(key) + ": [";
        for (std::size_t i = 0; i < curve_coordinates; ++i) curves += i ? ",0" : "0";
        curves += "],\n";
    }
    CHECK(error_of(curves, [=](Block &b) {
        std::vector<double> x, y;
        b.reals("x", x, curve_coordinates);
        b.reals("y", y, curve_coordinates);
        CHECK(x.size() == curve_coordinates && y.size() == curve_coordinates);
        return 0;
    }).empty());

    /* written, then read back: every kind of value */
    Out out;
    out.begin("numerics");
    out.real("dt", 0.05);
    out.real("tiny", 1e-300);
    out.real("negative", -2.5);
    out.whole("n", -7);
    out.word("method", "runge_kutta");
    out.text("spaced", "two words");
    out.text("one", "word"); /* user text is quoted even when it is one word */
    out.text("path", "sub dir/a \"b\" \\c.dat");
    out.end();
    const std::vector<double> numbers{1, 2.5, -3, 4, 5, 6, 7, 8, 9, 10};
    out.reals("list", numbers);
    out.begin_list("items");
    for (int i = 0; i < 2; i++) {
        out.begin_item();
        out.whole("id", i);
        out.end_item();
    }
    out.end_list();
    const xpp::Result<std::string> text = out.result("test");
    CHECK(text.has_value());
    if (!text) return 1;
    const auto read_all = [](Block &b) {
        std::optional<Block> n = b.group("numerics");
        CHECK(n.has_value());
        double dt = 0, tiny = 0, negative = 0;
        int whole = 0;
        std::string method, spaced, path, one;
        n->real("dt", dt);
        n->real("tiny", tiny);
        n->real("negative", negative);
        n->whole("n", whole);
        n->word("method", method);
        n->text("spaced", spaced);
        n->text("one", one);
        n->text("path", path);
        n->done();
        CHECK(dt == 0.05 && tiny == 1e-300 && negative == -2.5 && whole == -7);
        CHECK(method == "runge_kutta" && spaced == "two words" && one == "word" && path == "sub dir/a \"b\" \\c.dat");
        std::vector<double> list;
        CHECK(b.reals("list", list, 10) && list.size() == 10 && list[1] == 2.5 && list[2] == -3);
        std::vector<Block> items;
        CHECK(b.groups("items", items, 2) && items.size() == 2);
        int id = -1;
        items[1].whole("id", id);
        CHECK(id == 1);
        return 0;
    };
    CHECK(error_of(*text, read_all).empty());

    /* the options syntax: a trailing comma, comments of both kinds, CRLF */
    CHECK(error_of("# comment\r\na: 1, /* block */ b: { c: 2, },\r\n", [](Block &b) {
        int a = 0, c = 0;
        b.whole("a", a);
        std::optional<Block> g = b.group("b");
        g->whole("c", c);
        g->done();
        CHECK(a == 1 && c == 2);
        return 0;
    }).empty());

    /* errors at the line and column, the line as written */
    const auto whole_a = [](Block &b) {
        int a = 0;
        b.whole("a", a);
        return 0;
    };
    CHECK(error_of("a: 1,\n  a: 2,\n", whole_a) == "t.odex:2:3: `a` is given twice (the first is at line 1)");
    CHECK(error_of("a: x,\n", whole_a) == "t.odex:1:4: `a`: `x` is not a whole number");
    CHECK(error_of("a: 1.5,\n", whole_a) == "t.odex:1:4: `a`: `1.5` is not a whole number");
    CHECK(error_of("a: 1, b: 2,\n", whole_a) == "t.odex:1:7: `b` is not a setting of the file");
    CHECK(error_of("b: 1,\n", whole_a) == "t.odex:1:1: the file has no `a`");
    CHECK(error_of("a: ,\n", whole_a).starts_with("t.odex:1:4:"));
    CHECK(error_of("a: 1 b: 2,\n", whole_a).starts_with("t.odex:1:6: expected `,`"));
    CHECK(error_of("a: { b: 1\n", whole_a).find("expected `}` to close the block, found the end of the file") != std::string::npos);
    CHECK(error_of("a: [1, 2\n", whole_a).starts_with("t.odex:"));
    CHECK(error_of("a: \"never closed\n", whole_a).find("never closed") != std::string::npos);
    /* text is always a string, a choice or a name never one */
    const auto text_a = [](Block &b) {
        std::string a;
        b.text("a", a);
        return 0;
    };
    const auto word_a = [](Block &b) {
        std::string a;
        b.word("a", a);
        return 0;
    };
    CHECK(error_of("a: \"one\",\n", text_a).empty());
    CHECK(error_of("a: one,\n", text_a) == "t.odex:1:4: `a` takes text in double quotes: write \"one\"");
    CHECK(error_of("a: one,\n", word_a).empty());
    CHECK(error_of("a: \"one\",\n", word_a) == "t.odex:1:4: `a` takes a word, never in quotes: write one");
    /* partial: an absent key is left, a key nothing asks for is still refused */
    CHECK(error_of("", whole_a, false).empty());
    CHECK(error_of("z: 1,\n", whole_a, false).find("`z` is not a setting") != std::string::npos);
    /* a flag is true or false, a finite number is not inf or nan */
    CHECK(error_of("a: 2,\n", [](Block &b) { bool f = false; b.flag("a", f); return 0; }).find("`2` is not true or false") != std::string::npos);
    CHECK(error_of("a: nan,\n", [](Block &b) { double v = 0; b.real("a", v); return 0; }).find("not a finite number") != std::string::npos);
    CHECK(error_of("a: nan,\n", [](Block &b) { double v = 0; b.number("a", v); CHECK(std::isnan(v)); return 0; }).empty());
    CHECK(error_of("a: -inf,\n", [](Block &b) { double v = 0; b.number("a", v); CHECK(std::isinf(v) && v < 0); return 0; }).empty());

    /* bounds: a list longer than the reader's maximum, a group nested too deep */
    CHECK(error_of("a: [1, 2, 3],\n", [](Block &b) { std::vector<double> v; b.reals("a", v, 2); return 0; }).find("3 entries: at most 2") != std::string::npos);
    std::string deep = "a: ";
    for (int i = 0; i < max_depth + 2; i++) deep += "{ k: ";
    deep += "1";
    for (int i = 0; i < max_depth + 2; i++) deep += " }";
    deep += ",\n";
    CHECK(error_of(deep, whole_a, false).find("nested at most") != std::string::npos);
    /* The token cap refuses a million nested groups even before the depth check. */
    std::string many = "a: ";
    for (int i = 0; i < 1000000; i++) many += "{k:";
    CHECK(error_of(many, whole_a, false).find("tokens") != std::string::npos);
    /* an exact list of numbers wants its count */
    CHECK(error_of("rm: [1, 2, 3],\n", [](Block &b) { std::vector<double> v(9); b.reals("rm", std::span<double>(v)); return 0; }).find("3 numbers: 9 are due") != std::string::npos);

    /* a text a line cannot hold is an error when written, not a file that does not read */
    Out bad;
    bad.text("a", "two\nlines");
    CHECK(!bad.result("test").has_value());
    Out bad_key;
    bad_key.whole("not a key", 1);
    CHECK(!bad_key.result("test").has_value());

    /* the session and preferences statements (W322): the one reader reads them in a file of their own kind only, so a
       model has no `session`, a session file has nothing but its statements (no import reads a file from it) and
       the preferences file nothing but its own */
    using xpp::odex::FileKind;
    const auto state_error = [](FileKind kind, const std::string &text) {
        const auto r = xpp::odex::read_state_file(kind, "test", "s.odex", text);
        return r ? std::string() : r.error().text();
    };
    const std::string session = "version 4\npar a = 1\ninit x = 2\noptions { integration: { dt: 0.1 } }\nsession { model: \"m.odex\", run: { t: 0 } }\n";
    CHECK(state_error(FileKind::session, session).empty());
    const auto file = xpp::odex::read_state_file(FileKind::session, "test", "s.odex", session);
    CHECK(file && file->group.members.size() == 2 && file->group.members[0].key == "model" && file->parsed.statements.size() == 4);
    CHECK(state_error(FileKind::session, "version 4\nsession { a: 1 }\nsession { b: 2 }\n") == "s.odex:3:1: `session` is written once in a file (the first is at line 2)");
    CHECK(state_error(FileKind::session, "version 4\nx' = -x\n").starts_with("s.odex:2:1: a session file holds `par`, `init`, `options { ... }` and `session { ... }` only, found"));
    CHECK(state_error(FileKind::session, "version 4\nimport \"/etc/passwd\"\n").starts_with("s.odex:2:1: a session file holds"));
    CHECK(state_error(FileKind::session, "version 4\npreferences { }\n").starts_with("s.odex:2:1: a session file holds"));
    CHECK(state_error(FileKind::session, "session { }\n").starts_with("s.odex:1:1: a session file starts with `version 4`"));
    CHECK(state_error(FileKind::session, "version 3\nsession { }\n").find("version 3: this program reads session files of version 4") != std::string::npos);
    CHECK(state_error(FileKind::preferences, "version 4\npreferences { threads: auto }\n").empty());
    CHECK(state_error(FileKind::preferences, "version 4\nsession { a: 1 }\n").starts_with("s.odex:2:1: the preferences file holds `preferences { ... }` only"));
    CHECK(state_error(FileKind::preferences, "version 4\npar a = 1\n").starts_with("s.odex:2:1: the preferences file holds"));
    CHECK(state_error(FileKind::session, "version 4\nsession { a: { b: { c: { d: { e: { f: { g: { h: { i: 1 } } } } } } } } }\n").find("nested at most 8 deep") != std::string::npos);
    const auto model = std::make_unique<xpp::Model>();
    const auto in_model = xpp::odex::read_statements(*model, "version 4\nx' = -x\nsession { a: 1 }\n", "m.odex");
    CHECK(!in_model && in_model.error().place.line == 3 && in_model.error().what == "`session { ... }` is accepted only in a session file, not in a model");
    const auto prefs_in_model = xpp::odex::read_statements(*model, "version 4\npreferences { }\n", "m.odex");
    CHECK(!prefs_in_model && prefs_in_model.error().what == "`preferences { ... }` is accepted only in the preferences file, not in a model");
    CHECK(xpp::odex::read_statements(*model, "version 4\nsession = 1\n", "m.odex").has_value()); /* a name, not the statement: `session` is no reserved word */
    TEST_REPORT("test_odex_block");
}
