/* The text a save of values writes into a model file (core/odex_write.h, W320): a new
   set, a set's part replaced, the defaults rewritten, a set renamed or deleted. The
   rest of the file stays byte for byte, each result parses and checks as a model, and a
   text that would not load is an error. Where the lines went is LineMap's. */
#include "xpptest.h"
#include "model.h"
#include "odex.h"
#include "odex_write.h"

#include <memory>
#include <string>

namespace {

using xpp::odex::Values;
using xpp::odex::ValueKind;

const std::string file = "t.odex";

/* a, b are parameters; x, y states */
xpp::odex::ValueKind classify(std::string_view name)
{
    return name == "a" || name == "b" ? ValueKind::parameter : name == "x" || name == "y" ? ValueKind::state : ValueKind::other;
}

std::unique_ptr<xpp::Model> scratch() { return std::make_unique<xpp::Model>(); }

/* the text after an edit, "" and a failure noted when it is an error */
std::string result(const xpp::Result<xpp::odex::Edited> &r, std::string *error = nullptr)
{
    if (!r) {
        if (error) *error = r.error().text();
        return "";
    }
    return r->text;
}

Values parameters(std::initializer_list<std::pair<std::string, std::string>> list)
{
    Values v;
    v.parameters = true;
    v.parameter_values.assign(list.begin(), list.end());
    return v;
}

Values states(std::initializer_list<std::pair<std::string, std::string>> list)
{
    Values v;
    v.states = true;
    v.state_values.assign(list.begin(), list.end());
    return v;
}

Values group(std::string name, std::initializer_list<std::pair<std::string, std::string>> list)
{
    Values v;
    v.groups.push_back(name);
    for (const auto &[key, value] : list) v.settings.push_back({name, key, value});
    return v;
}

const std::string model =
    "# model\n"
    "x' = -a*x + b   # decay\n"
    "y' = -y\n"
    "par a = 1, b = 2\n"
    "init x = 0.5, y = 2*3\n"
    "\n"
    "options {\n"
    "  integration: { dt: 0.02, total_time: 9 },\n"
    "}\n"
    "\n"
    "# regimes\n"
    "set \"one\" { a: 10 }\n"
    "set \"two\" { a: 5, x: 0.9, options: { integration: { dt: 0.1 }, plot: { xlo: -3 } } }\n";

} // namespace

int main()
{
    using namespace xpp::odex;

    /* the layout the converter writes */
    CHECK(braced({"a: 1", "b: 2"}, 4) == "{ a: 1, b: 2 }");
    CHECK(options_block({}).empty());
    CHECK(options_block({{"integration", "dt: 0.1"}}) == "options {\n  integration: { dt: 0.1 },\n}\n");
    CHECK(set_block("s", {"a: 1"}, {{"plot", "xlo: 1"}}) == "set \"s\" { a: 1, options: { plot: { xlo: 1 } } }");
    std::string many;
    std::vector<std::string> items;
    for (int i = 0; i < 40; i++) items.push_back("name" + std::to_string(i) + ": 1.5");
    many = braced(items, 4);
    CHECK(many.starts_with("{\n") && many.ends_with("\n    }") && many.find("name39") != std::string::npos);

    /* where lines went */
    {
        LineMap map;
        map.changes.push_back({3, 2, 5}); /* lines 3-5 became 5 lines more... old 3..5 -> new 3..7 */
        CHECK(map.map(2) == 2);
        CHECK(map.map(3) == 3);
        CHECK(map.map(4) == 4);
        CHECK(map.map(5) == 5);
        CHECK(map.map(6) == 9);
        map.changes.push_back({10, 1, 0}); /* old lines 10-11 became none */
        CHECK(map.map(12) == 12 + 3 - 1);
    }

    /* a new set goes at the end, the file as it was before it */
    {
        auto m = scratch();
        const std::string out = result(add_set(*m, model, file, "three", parameters({{"a", "7"}, {"b", "2.5"}})));
        CHECK(out.starts_with(model));
        CHECK(out.substr(model.size()) == "\nset \"three\" { a: 7, b: 2.5 }\n");
        /* no newline at the end of the file, and a CRLF file */
        const std::string bare = result(add_set(*m, "x' = -x\nset \"s\" { }", file, "other", parameters({{"a", "1"}}), true));
        CHECK(bare == "x' = -x\nset \"s\" { }\n\nset \"other\" { a: 1 }\n");
        const std::string crlf = result(add_set(*m, "x' = -x\r\npar a = 1\r\n", file, "other", parameters({{"a", "2"}, {"b", "3"}}), true));
        CHECK(crlf.starts_with("x' = -x\r\npar a = 1\r\n"));
        CHECK(crlf.find("\r\n\r\nset \"other\" { a: 2, b: 3 }\r\n") != std::string::npos);
        /* the name is checked like any model name, and is not already a set */
        std::string error;
        CHECK(result(add_set(*m, model, file, "one", parameters({{"a", "1"}})), &error).empty() && error.find("already a set") != std::string::npos);
        CHECK(result(add_set(*m, model, file, "two words", parameters({}) ), &error).empty() && error.find("not a valid name") != std::string::npos);
        CHECK(result(add_set(*m, model, file, "set", parameters({}) ), &error).empty() && error.find("not a valid name") != std::string::npos);
        /* a value the model could not read back is refused, the text unchanged */
        CHECK(result(add_set(*m, model, file, "bad", parameters({{"a", "nan"}})), &error).empty() && !error.empty());
    }

    /* a part of a set replaced */
    {
        auto m = scratch();
        const std::string out = result(replace_in_set(*m, model, file, "two", states({{"x", "0.1"}, {"y", "0.2"}}), classify));
        const std::size_t at = model.find("set \"two\"");
        CHECK(out.substr(0, at) == model.substr(0, at));
        CHECK(out.substr(at).find("a: 5, x: 0.1, y: 0.2") != std::string::npos);
        CHECK(out.substr(at).find("options: { integration: { dt: 0.1 }, plot: { xlo: -3 } }") != std::string::npos);
        /* a group replaced, the others and the values kept */
        const std::string g = result(replace_in_set(*m, model, file, "two", group("plot", {{"xlo", "1"}, {"xhi", "2"}}), classify));
        CHECK(g.substr(g.find("set \"two\"")).find("a: 5, x: 0.9, options: { integration: { dt: 0.1 }, plot: { xlo: 1, xhi: 2 } }") != std::string::npos);
        /* a kind emptied: its entries go and the others stay */
        Values none;
        none.parameters = true;
        const std::string e = result(replace_in_set(*m, model, file, "two", none, classify));
        CHECK(e.find("set \"two\" { x: 0.9, options: {") != std::string::npos);
        /* a group added to a set that had none */
        const std::string added = result(replace_in_set(*m, model, file, "one", group("integration", {{"dt", "0.3"}}), classify));
        CHECK(added.find("set \"one\" { a: 10, options: { integration: { dt: 0.3 } } }") != std::string::npos);
        std::string error;
        CHECK(result(replace_in_set(*m, model, file, "nosuch", none, classify), &error).empty() && error.find("no set nosuch") != std::string::npos);
    }

    /* rename and delete */
    {
        auto m = scratch();
        const std::string out = result(rename_set(*m, model, file, "one", "uno"));
        CHECK(out == model.substr(0, model.find("set \"one\"")) + "set \"uno\"" + model.substr(model.find("set \"one\"") + 9));
        std::string error;
        CHECK(result(rename_set(*m, model, file, "one", "two"), &error).empty() && error.find("already a set") != std::string::npos);
        CHECK(result(rename_set(*m, model, file, "one", "a b"), &error).empty() && !error.empty());
        const xpp::Result<Edited> deleted = delete_set(*m, model, file, "one");
        CHECK(deleted && deleted->text == model.substr(0, model.find("set \"one\"")) + model.substr(model.find("set \"two\"")));
        CHECK(deleted && deleted->lines.map(13) == 12);
        const std::string last = result(delete_set(*m, model, file, "two"));
        CHECK(last == model.substr(0, model.find("set \"two\"")));
        CHECK(result(delete_set(*m, model, file, "nosuch"), &error).empty());
    }

    /* the defaults */
    {
        auto m = scratch();
        const std::string par = result(set_defaults(*m, model, file, parameters({{"a", "4"}})));
        CHECK(par == model.substr(0, model.find("par a = 1")) + "par a = 4, b = 2" + model.substr(model.find("par a = 1") + 16));
        /* an init that is a formula becomes the number; the equal one stays as written */
        const std::string ic = result(set_defaults(*m, model, file, states({{"y", "3"}})));
        CHECK(ic.find("init x = 0.5, y = 3\n") != std::string::npos);
        CHECK(result(set_defaults(*m, model, file, states({{"y", "6"}}))) == model);
        /* a parameter written as a formula stays as written while its value is the same */
        const std::string quotient = "x' = -a*x\npar a = 1/30\n";
        CHECK(result(set_defaults(*m, quotient, file, parameters({{"a", "0.03333333333333333"}}))) == quotient);
        CHECK(result(set_defaults(*m, quotient, file, parameters({{"a", "0.5"}}))) == "x' = -a*x\npar a = 0.5\n");
        /* the same value written differently is not rewritten */
        CHECK(result(set_defaults(*m, model, file, parameters({{"a", "1.0"}}))) == model);
        /* a state with no init line gets one */
        const std::string fresh = result(set_defaults(*m, "x' = -x\ny' = -y\npar a = 1\ninit x = 1\n", file, states({{"y", "2"}})));
        CHECK(fresh == "x' = -x\ny' = -y\npar a = 1\ninit x = 1\ninit y = 2\n");
        /* a group of the options block replaced, the others as written */
        const std::string opt = result(set_defaults(*m, model, file, group("integration", {{"dt", "0.05"}})));
        CHECK(opt.find("options {\n  integration: { dt: 0.05 },\n}\n") != std::string::npos);
        const std::string more = result(set_defaults(*m, model, file, group("plot", {{"xlo", "1"}})));
        CHECK(more.find("options {\n  integration: { dt: 0.02, total_time: 9 },\n  plot: { xlo: 1 },\n}\n") != std::string::npos);
        /* a file with no options block gets one at its end */
        const std::string block = result(set_defaults(*m, "x' = -x\npar a = 1\n", file, group("integration", {{"dt", "0.5"}})));
        CHECK(block == "x' = -x\npar a = 1\n\noptions {\n  integration: { dt: 0.5 },\n}\n");
        /* a default that no line of this file declares cannot be written here */
        std::string error;
        CHECK(result(set_defaults(*m, "x' = -x\npar a[1..2] = 1\n", file, parameters({{"a", "3"}})), &error).empty());
        CHECK(result(set_defaults(*m, model, file, parameters({{"c", "3"}})), &error).empty() && error.find("declared at") != std::string::npos);
    }

    /* text that would not load is an error at its line */
    {
        auto m = scratch();
        CHECK(check_text(*m, model, file));
        CHECK(!check_text(*m, "x' = -x\nset \"s\" { nosuch: 1 }\n", file));
        CHECK(check_text(*m, "set \"s\" { a: 1 }\n", file, true));
        const auto bad = check_text(*m, "x' = \n", file);
        CHECK(!bad && bad.error().place.file == file && bad.error().place.line >= 1);
    }

    CHECK(export_header("m.odex", "2026-10-14T10:00:00Z") == "# Saved from m.odex at 2026-10-14T10:00:00Z\n");
    TEST_REPORT("odex_write");
}
