/* The settings syntax (setx.h, W284): .odex's options syntax read by the
   same tokenizer and block grammar, plus lists. What Out writes reads back
   as the same values; every error names its line and column and the line as
   written; a key that is given twice, a value that is none, a group nested
   deeper than max_depth, a list longer than the reader's maximum and a key
   nothing asked for are refused (nothing allocated from a count, no
   recursion past the bound: the file may be one a user was sent). */
#include "xpptest.h"
#include "setx.h"
#include "odex.h"

#include <cmath>
#include <string>
#include <vector>

namespace {

/* the error of text read as a complete file through read(Block &), "" when it reads */
template <class F> std::string error_of(std::string_view text, F read, bool complete = true)
{
    const auto r = xpp::setx::read_block("test", "t.setx", text, complete, read);
    return r ? std::string() : r.error().text();
}

} // namespace

int main()
{
    using namespace xpp::setx;

    /* A 4 MiB punctuation-only file is refused before the token vector can grow without bound. */
    const std::string punctuation(4u * 1024 * 1024, ',');
    const auto too_many = read_block("test", "punctuation.setx", punctuation, true, [](Block &) { return 0; });
    CHECK(!too_many);
    if (!too_many) {
        CHECK(too_many.error().place.file == "punctuation.setx" && too_many.error().place.line == 1);
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
    out.comment("a comment");
    out.begin("numerics");
    out.real("dt", 0.05);
    out.real("tiny", 1e-300);
    out.real("negative", -2.5);
    out.whole("n", -7);
    out.word("method", "runge_kutta");
    out.word("spaced", "two words"); /* not a bare word: quoted */
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
        std::string method, spaced, path;
        n->real("dt", dt);
        n->real("tiny", tiny);
        n->real("negative", negative);
        n->whole("n", whole);
        n->text("method", method);
        n->text("spaced", spaced);
        n->text("path", path);
        n->done();
        CHECK(dt == 0.05 && tiny == 1e-300 && negative == -2.5 && whole == -7);
        CHECK(method == "runge_kutta" && spaced == "two words" && path == "sub dir/a \"b\" \\c.dat");
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
    CHECK(error_of("a: 1,\n  a: 2,\n", whole_a) == "t.setx:2:3: `a` is given twice (the first is at line 1)");
    CHECK(error_of("a: x,\n", whole_a) == "t.setx:1:4: `a`: `x` is not a whole number");
    CHECK(error_of("a: 1.5,\n", whole_a) == "t.setx:1:4: `a`: `1.5` is not a whole number");
    CHECK(error_of("a: 1, b: 2,\n", whole_a) == "t.setx:1:7: `b` is not a setting of the file");
    CHECK(error_of("b: 1,\n", whole_a) == "t.setx:1:1: the file has no `a`");
    CHECK(error_of("a: ,\n", whole_a).starts_with("t.setx:1:4:"));
    CHECK(error_of("a: 1 b: 2,\n", whole_a).starts_with("t.setx:1:6: expected `,`"));
    CHECK(error_of("a: { b: 1\n", whole_a).find("expected `}` to close the block, found the end of the file") != std::string::npos);
    CHECK(error_of("a: [1, 2\n", whole_a).starts_with("t.setx:"));
    CHECK(error_of("a: \"never closed\n", whole_a).find("never closed") != std::string::npos);
    /* partial: an absent key is left, a key nothing asks for is still refused */
    CHECK(error_of("", whole_a, false).empty());
    CHECK(error_of("z: 1,\n", whole_a, false).find("`z` is not a setting") != std::string::npos);
    /* a flag is 0 or 1, a finite number is not inf or nan */
    CHECK(error_of("a: 2,\n", [](Block &b) { bool f = false; b.flag("a", f); return 0; }).find("2 is not 0 or 1") != std::string::npos);
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
    TEST_REPORT("test_setx");
}
