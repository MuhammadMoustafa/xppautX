/* W530 (#595): the registry's argument types, groups, typed accessor, server
   text form and hello description, each accepted and refused, over rows made
   here (the window resource's own are servercheck's registry section). A
   refusal's message names the argument and why. */
#include "load_args.h"
#include "xpptest.h"
#include "command_def.h"
#include "command_registry.h"
#include "xpp_batch.h"
#include "session.h"
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace xpp::command;

constexpr std::array CHOICES = {"a", "b"};
double end_time(const xpp::Session &) { return 10; }

xpp::Result<std::string> nothing(xpp::Session &, const Command &, const Arguments &, const xpp::Place &) { return std::string("{}"); }

constexpr std::array ARGUMENTS = {
    Argument{.name = "n", .type = Type::integer, .required = false, .min = constant(1), .max = constant(5)},
    Argument{.name = "x", .type = Type::real, .required = false, .max = session_quantity("end_time", end_time)},
    Argument{.name = "flag", .type = Type::boolean, .required = false},
    Argument{.name = "mode", .type = Type::choice, .required = false, .choices = CHOICES},
    Argument{.name = "label", .type = Type::text, .required = false, .max_length = 4},
    Argument{.name = "formula", .type = Type::expression, .required = false},
    Argument{.name = "par", .type = Type::model_name, .required = false, .names = NameSet::parameter},
    Argument{.name = "var", .type = Type::model_name, .required = false, .names = NameSet::variable},
    Argument{.name = "either", .type = Type::model_name, .required = false, .names = NameSet::either},
    Argument{.name = "any", .type = Type::model_name, .required = false, .names = NameSet::any},
    Argument{.name = "file", .type = Type::file, .required = false},
    Argument{.name = "path", .type = Type::path, .required = false},
    Argument{.name = "color", .type = Type::color, .required = false},
    Argument{.name = "reals", .type = Type::list, .required = false, .min = constant(0), .element = Type::real, .max_items = 3},
    Argument{.name = "values", .type = Type::map, .required = false, .names = NameSet::either, .element = Type::real, .max_items = 2},
    Argument{.name = "window", .type = Type::window, .required = false},
    Argument{.name = "new", .type = Type::new_name, .required = false},
    Argument{.name = "first", .type = Type::real, .required = false},
    Argument{.name = "second", .type = Type::real, .required = false},
    Argument{.name = "third", .type = Type::real, .required = false},
    Argument{.name = "must", .type = Type::real, .required = false}};
constexpr std::array ONE_OF = {"n", "x"};
constexpr std::array AT_MOST = {"flag", "mode"};
constexpr std::array TOGETHER = {"first", "second"};
constexpr std::array GROUPS = {Group{GroupRule::exactly_one, ONE_OF}, Group{GroupRule::at_most_one, AT_MOST},
                               Group{GroupRule::together, TOGETHER}};
constexpr Command ROW{"test.all", ARGUMENTS, XPP_KIND_VIEW, xpp::CommandCategory::Tools, "", "", "All the types.", {}, {}, false, nothing, {}};
constexpr Command GROUPED{"test.groups", ARGUMENTS, XPP_KIND_VIEW, xpp::CommandCategory::Tools, "", "", "Groups.", {}, {}, false, nothing, GROUPS};

const xpp::Place PLACE{"command", 1, 0, "line"};

xpp::Result<Arguments> check(xpp::Session *s, const std::string &json, bool paths = false, const Command &row = ROW)
{
    return validate(s, row, json.c_str(), PLACE, paths);
}

/* the line is refused with a message naming `needle` */
bool refused(xpp::Session *s, const std::string &json, const std::string &needle, bool paths = false, const Command &row = ROW)
{
    const auto result = check(s, json, paths, row);
    return !result && result.error().what.find(needle) != std::string::npos;
}

std::string cmd(const std::string &members) { return "{\"cmd\":\"test.all\"" + (members.empty() ? "" : "," + members) + "}"; }
std::string grouped(const std::string &members) { return "{\"cmd\":\"test.groups\",\"must\":1" + (members.empty() ? "" : "," + members) + "}"; }

std::string text_form(const std::string &words)
{
    const auto result = tokenize_command(ROW, words.c_str(), PLACE);
    return result ? *result : "ERROR " + result.error().what;
}

} // namespace

int main()
{
    char name[] = "test_command_registry", path[] = "examples/ode/lecar.odex";
    char *argv[] = {name, path, nullptr};
    const xpp::Loaded loaded = load_args(2, argv, 1);
    CHECK(loaded.has_value());
    if (!loaded) TEST_REPORT("command registry");
    xpp::Session &s = **loaded;
    xpp::batch_start(s);

    /* integer: whole, in range, exact */
    {
        const auto a = check(&s, cmd("\"n\":3"));
        CHECK(a && a->get<long long>("n") == 3);
        CHECK(refused(&s, cmd("\"n\":0"), "\"n\" must be at least 1"));
        CHECK(refused(&s, cmd("\"n\":6"), "\"n\" must be at most 5"));
        CHECK(refused(&s, cmd("\"n\":1.5"), "\"n\" must be a whole number"));
        CHECK(refused(&s, cmd("\"n\":1e1"), "\"n\" must be a whole number"));
        CHECK(refused(&s, cmd("\"n\":\"3\""), "\"n\" must be a whole number"));
        CHECK(refused(&s, cmd("\"n\":99999999999999999999"), "\"n\" must be a whole number"));
    }
    /* real: finite, a session quantity's bound only with a session */
    {
        const auto a = check(&s, cmd("\"x\":2.5"));
        CHECK(a && a->get<double>("x") == 2.5);
        CHECK(refused(&s, cmd("\"x\":1e999"), "\"x\" must be a finite number"));
        CHECK(refused(&s, cmd("\"x\":\"2\""), "\"x\" must be a finite number"));
        CHECK(refused(&s, cmd("\"x\":10.5"), "\"x\" must be at most 10"));
        CHECK(check(nullptr, cmd("\"x\":10.5")).has_value());
    }
    /* boolean, choice, text */
    {
        const auto a = check(&s, cmd("\"flag\":true,\"mode\":\"b\",\"label\":\"abcd\""));
        CHECK(a && a->get<bool>("flag") && a->get<std::string>("mode") == "b" && a->get<std::string>("label") == "abcd");
        CHECK(refused(&s, cmd("\"flag\":1"), "\"flag\" must be boolean"));
        CHECK(refused(&s, cmd("\"mode\":\"c\""), "Invalid choice \"c\" for mode"));
        CHECK(refused(&s, cmd("\"label\":\"abcde\""), "\"label\" must be text of at most 4 bytes"));
        CHECK(refused(&s, cmd("\"label\":7"), "\"label\" must be text"));
    }
    /* expression: compiled against the model, refused with the compiler's message */
    {
        CHECK(check(&s, cmd("\"formula\":\"v+2*w\"")).has_value());
        CHECK(refused(&s, cmd("\"formula\":\"v+\""), "\"formula\": Illegal formula"));
        CHECK(check(nullptr, cmd("\"formula\":\"v+\"")).has_value()); /* a shape check has no model */
    }
    /* names of a kind, in any case */
    {
        CHECK(check(&s, cmd("\"par\":\"IAPP\",\"var\":\"W\",\"either\":\"iapp\",\"any\":\"ica\"")).has_value());
        CHECK(refused(&s, cmd("\"par\":\"v\""), "no parameter named \"v\""));
        CHECK(refused(&s, cmd("\"var\":\"iapp\""), "no variable named \"iapp\""));
        CHECK(refused(&s, cmd("\"either\":\"zzz\""), "no parameter or variable named \"zzz\""));
        CHECK(refused(&s, cmd("\"any\":\"zzz\""), "no name named \"zzz\""));
    }
    /* file: a base name; path: --server's own input only */
    {
        CHECK(check(&s, cmd("\"file\":\"run.dat\"")).has_value());
        CHECK(refused(&s, cmd("\"file\":\"../run.dat\""), "\"file\""));
        CHECK(refused(&s, cmd("\"file\":\"a/b\""), "\"file\""));
        CHECK(refused(&s, cmd("\"file\":\".hidden\""), "\"file\""));
        const auto a = check(&s, cmd("\"path\":\"/tmp/a b.dat\""), true);
        CHECK(a && a->get<std::string>("path") == "/tmp/a b.dat");
        CHECK(refused(&s, cmd("\"path\":\"/tmp/a\""), "\"path\" is a path, which only --server's own input may give"));
        CHECK(refused(&s, cmd("\"path\":\"\""), "\"path\" must not be empty", true));
        CHECK(refused(nullptr, cmd("\"path\":\"/tmp/a\""), "\"path\" is a path")); /* a recording's shape check */
    }
    /* color: a CSS name, #rrggbb, #rrggbbaa or foreground */
    {
        const auto a = check(&s, cmd("\"color\":\"#ff000080\""));
        CHECK(a && a->get<xpp::Color>("color") == (xpp::Color{255, 0, 0, 128, false}));
        CHECK(check(&s, cmd("\"color\":\"red\"")).has_value());
        CHECK(check(&s, cmd("\"color\":\"foreground\"")).has_value());
        CHECK(refused(&s, cmd("\"color\":\"reddish\""), "\"color\" must be a CSS colour name"));
        CHECK(refused(&s, cmd("\"color\":3"), "\"color\" must be text"));
    }
    /* list: bounded, one level, each item typed and ranged */
    {
        const auto a = check(&s, cmd("\"reals\":[1,2.5]"));
        CHECK(a && a->list<double>("reals") == std::vector<double>({1, 2.5}));
        CHECK(check(&s, cmd("\"reals\":[]")).has_value());
        CHECK(refused(&s, cmd("\"reals\":[1,2,3,4]"), "\"reals\" must have at most 3 items"));
        CHECK(refused(&s, cmd("\"reals\":[1,[2]]"), "\"reals[2]\" holds single values"));
        CHECK(refused(&s, cmd("\"reals\":[1,\"x\"]"), "\"reals[2]\" must be a finite number"));
        CHECK(refused(&s, cmd("\"reals\":[-1]"), "\"reals[1]\" must be at least 0"));
        CHECK(refused(&s, cmd("\"reals\":2"), "\"reals\" must be a list"));
    }
    /* map: keys are the model's names, once each; values single and typed */
    {
        const auto a = check(&s, cmd("\"values\":{\"v\":1,\"IAPP\":2.5}"));
        const auto entries = a ? a->map<double>("values") : std::vector<std::pair<std::string, double>>{};
        CHECK(entries.size() == 2 && entries[0].first == "v" && entries[0].second == 1 && entries[1].first == "IAPP");
        CHECK(refused(&s, cmd("\"values\":{\"v\":1,\"V\":2}"), "names \"V\" twice"));
        CHECK(refused(&s, cmd("\"values\":{\"zzz\":1}"), "no parameter or variable named \"zzz\""));
        CHECK(refused(&s, cmd("\"values\":{\"v\":\"1\"}"), "\"values.v\" must be a finite number"));
        CHECK(refused(&s, cmd("\"values\":{\"v\":{\"w\":1}}"), "\"values.v\" holds single values"));
        CHECK(refused(&s, cmd("\"values\":{\"v\":1,\"w\":2,\"par\":3}"), "\"values\" must have at most 2 entries"));
        CHECK(refused(&s, cmd("\"values\":[1]"), "\"values\" must be a map"));
    }
    /* a window and a new window name (W353's) */
    {
        CHECK(check(&s, cmd("\"window\":\"graph_1\",\"new\":\"phase plane\"")).has_value());
        CHECK(refused(&s, cmd("\"window\":\"nowhere\""), "Unknown window \"nowhere\""));
        CHECK(refused(&s, cmd("\"new\":\"\""), ""));
    }
    /* the line itself: unknown, repeated and missing arguments, bad JSON */
    {
        CHECK(refused(&s, cmd("\"nope\":1"), "Unknown argument \"nope\""));
        CHECK(refused(&s, cmd("\"n\":1,\"n\":2"), "Repeated argument \"n\""));
        CHECK(refused(&s, "{\"cmd\":\"test.all\",", "Command"));
        CHECK(refused(&s, grouped("\"first\":1,\"second\":2").substr(0, 0) + "{\"cmd\":\"test.groups\"}", "", false, GROUPED));
    }
    /* groups: exactly one, at most one, together */
    {
        CHECK(check(&s, grouped("\"n\":1"), false, GROUPED).has_value());
        CHECK(refused(&s, grouped(""), "Exactly one of \"n\", \"x\" is required", false, GROUPED));
        CHECK(refused(&s, grouped("\"n\":1,\"x\":1"), "Exactly one of \"n\", \"x\" is required", false, GROUPED));
        CHECK(check(&s, grouped("\"n\":1,\"flag\":true"), false, GROUPED).has_value());
        CHECK(refused(&s, grouped("\"n\":1,\"flag\":true,\"mode\":\"a\""), "At most one of \"flag\", \"mode\" may be given", false, GROUPED));
        CHECK(check(&s, grouped("\"n\":1,\"first\":1,\"second\":2"), false, GROUPED).has_value());
        CHECK(refused(&s, grouped("\"n\":1,\"first\":1"), "\"first\", \"second\" go together: give all or none", false, GROUPED));
    }
    /* the accessor: optional values, and a handler's mistake is loud */
    {
        const auto a = check(&s, cmd("\"n\":2"));
        CHECK(a && a->has("n") && !a->has("x") && !a->optional<double>("x") && a->optional<long long>("n") == 2);
        CHECK(a && a->list<double>("reals").empty() && a->map<double>("values").empty());
        bool threw = false;
        try { (void)a->get<double>("x"); } catch (const std::logic_error &) { threw = true; }
        CHECK(threw);
    }
    /* the server text form, one rule per type */
    {
        CHECK_STR(text_form(" --n 3 --x 2.5 --flag true --mode b --label \"hi there\"").c_str(),
                  "{\"cmd\":\"test.all\",\"n\":3,\"x\":2.5,\"flag\":true,\"mode\":\"b\",\"label\":\"hi there\"}");
        CHECK_STR(text_form(" --reals [1 2.5 3] --values {\"v\" 1 \"w\" 2}").c_str(),
                  "{\"cmd\":\"test.all\",\"reals\":[1,2.5,3],\"values\":{\"v\":1,\"w\":2}}");
        CHECK_STR(text_form(" --reals [] --color \"#ff0000\" --formula \"v + 1\" --file \"a.dat\"").c_str(),
                  "{\"cmd\":\"test.all\",\"reals\":[],\"color\":\"#ff0000\",\"formula\":\"v + 1\",\"file\":\"a.dat\"}");
        CHECK(text_form(" --label hello").find("Text must be double-quoted") != std::string::npos);
        CHECK(text_form(" --n \"3\"").find("must be bare") != std::string::npos);
        CHECK(text_form(" --n abc").find("\"n\" must be a finite number") != std::string::npos);
        CHECK(text_form(" --flag yes").find("\"flag\" must be true or false") != std::string::npos);
        CHECK(text_form(" --reals [1 2").find("Missing ] after the items of \"reals\"") != std::string::npos);
        CHECK(text_form(" --reals [1 [2]]").find("hold single values") != std::string::npos);
        CHECK(text_form(" --reals 1").find("is written between [ and ]") != std::string::npos);
        CHECK(text_form(" --n [1]").find("is not what this argument takes") != std::string::npos);
        CHECK(text_form(" --values {v 1}").find("Map keys must be double-quoted") != std::string::npos);
        CHECK(text_form(" --values {\"v\"}").find("Missing value for \"v\"") != std::string::npos);
        CHECK(text_form(" 3").find("Arguments must be named") != std::string::npos);
        CHECK(text_form(" --reals [1]x").find("Expected whitespace after value") != std::string::npos);
        /* what the text form writes, the validator reads */
        const auto words = tokenize_command(ROW, " --reals [1 2] --values {\"v\" 1}", PLACE);
        CHECK(words && check(&s, *words).has_value());
    }
    /* hello and help describe each type with its range, kind, items and groups */
    {
        std::string out;
        append_command(out, ROW);
        for (const char *piece : {"\"type\":\"integer\",\"required\":false,\"max_length\":1024,\"choices\":[],\"min\":1,\"max\":5",
                                  "\"max_of\":\"end_time\"", "\"type\":\"name\"", "\"names\":\"parameter\"", "\"names\":\"either\"",
                                  "\"type\":\"list\"", "\"element\":\"real\",\"max_items\":3", "\"choices\":[\"a\",\"b\"]",
                                  "\"type\":\"expression\"", "\"type\":\"file\"", "\"type\":\"path\"", "\"type\":\"color\""})
            CHECK(out.find(piece) != std::string::npos);
        CHECK(out.find("\"groups\"") == std::string::npos);
        out.clear();
        append_command(out, GROUPED);
        CHECK(out.find("\"groups\":[{\"rule\":\"exactly_one\",\"arguments\":[\"n\",\"x\"]},{\"rule\":\"at_most_one\","
                       "\"arguments\":[\"flag\",\"mode\"]},{\"rule\":\"together\",\"arguments\":[\"first\",\"second\"]}]") != std::string::npos);
    }
    /* W522: a map's fixed pair is bounded, finite and has exactly two scalars, in both wire forms. */
    {
        const Command &search=*find("equilibrium.search");
        for(const char *bounds:{"{\"v\":[-1,1]}","{\"v\":[0,0]}"}){
            const std::string line=std::string("{\"cmd\":\"equilibrium.search\",\"guesses\":1,\"tolerance\":0.001,\"bounds\":")+bounds+'}';
            const auto args=validate(nullptr,search,line.c_str(),PLACE,false);
            CHECK((args && args->map<std::array<double,2>>("bounds").size()==1));
        }
        for(const char *bounds:{"{\"v\":[]}","{\"v\":[1]}","{\"v\":[1,2,3]}","{\"v\":[[1],2]}","{\"v\":[null,2]}"}){
            const std::string line=std::string("{\"cmd\":\"equilibrium.search\",\"guesses\":1,\"tolerance\":0.001,\"bounds\":")+bounds+'}';
            CHECK(!validate(nullptr,search,line.c_str(),PLACE,false));
        }
        const auto text=tokenize_command(search," --guesses 1 --tolerance 0.001 --bounds {\"v\" [-1 1]}",PLACE);
        CHECK(text && validate(nullptr,search,text->c_str(),PLACE,false));
    }
    /* W528: a map of key lists (keymap.set), a list of named events, a multi-line note and a one-of group of the session rows */
    {
        const Command &keymap_set=*find("keymap.set");
        const auto set=[&](const std::string &members){
            const std::string line="{\"cmd\":\"keymap.set\""+(members.empty()?"":","+members)+"}";
            return validate(nullptr,keymap_set,line.c_str(),PLACE,false);
        };
        const auto bindings=set("\"bindings\":{\"calculator\":[\"Ctrl+B\",\"F2\"],\"reload\":[]},\"pinned\":[\"calculator\"],\"preset\":\"xppaut\"");
        CHECK(bindings && bindings->map<std::vector<std::string>>("bindings").size()==2
              && bindings->map<std::vector<std::string>>("bindings")[0].second.size()==2
              && bindings->list<std::string>("pinned").size()==1 && bindings->get<std::string>("preset")=="xppaut");
        CHECK(set("").has_value()); /* every part is optional: what is left out stays */
        for(const char *bad:{"\"bindings\":{\"calculator\":\"Ctrl+B\"}","\"bindings\":{\"calculator\":[1]}","\"bindings\":[\"x\"]",
                             "\"bindings\":{\"c\":[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\",\"h\",\"i\"]}","\"preset\":\"vi\"","\"pinned\":[1]"})
            CHECK(!set(bad));
        const Command &subscribe=*find("events.subscribe");
        const auto events=[&](const std::string &members){
            const std::string line="{\"cmd\":\"events.subscribe\","+members+"}";
            return validate(nullptr,subscribe,line.c_str(),PLACE,false);
        };
        CHECK(events("\"events\":[\"series\",\"plots\"],\"encoding\":\"f32\",\"diagram\":false"));
        CHECK(events("\"events\":[]"));
        CHECK(!events("\"events\":[\"series\",\"nothing\"]") && !events("\"encoding\":\"f32\"") && !events("\"events\":[],\"encoding\":\"f64\""));
        const Command &note=*find("recording.add_note");
        const auto noted=[&](const std::string &text){
            const std::string line="{\"cmd\":\"recording.add_note\",\"note\":"+text+"}";
            return validate(nullptr,note,line.c_str(),PLACE,false);
        };
        CHECK(noted("\"First run.\\nThe cell\\tsettles.\""));
        CHECK(!noted("\"bell\\u0007\"") && !noted("\"x\\u0000\""));
        const Command &open_model=*find("session.open_model");
        const auto opened=[&](const std::string &members,bool paths){
            const std::string line="{\"cmd\":\"session.open_model\","+members+"}";
            return validate(nullptr,open_model,line.c_str(),PLACE,paths);
        };
        CHECK(opened("\"file\":\"/tmp/a.ode\"",true) && opened("\"example\":\"lecar.odex\"",false) && opened("\"recent\":0",false));
        CHECK(!opened("\"file\":\"/tmp/a.ode\"",false) && !opened("",true) && !opened("\"example\":\"a\",\"recent\":1",false) && !opened("\"recent\":-1",false));
        /* the text form carries a key list and a named list as it does any list */
        const auto words=tokenize_command(keymap_set," --bindings {\"calculator\" [\"Ctrl+B\"]} --pinned [\"calculator\"]",PLACE);
        CHECK(words && validate(nullptr,keymap_set,words->c_str(),PLACE,false));
    }
    /* the tools: the calculator works a formula of the model out, and a comment or a button the model lacks is refused */
    {
        const auto worked = run(s, "{\"cmd\":\"tool.calculator\",\"expression\":\"2*3+1\"}", PLACE, false);
        CHECK(worked && worked->find("\"value\":7") != std::string::npos);
        const auto at_phi = run(s, "{\"cmd\":\"tool.calculator\",\"expression\":\"phi*3\"}", PLACE, false);
        CHECK(at_phi && at_phi->find("\"value\":0.999") != std::string::npos);
        CHECK(!run(s, "{\"cmd\":\"tool.calculator\",\"expression\":\"2*\"}", PLACE, false));
        CHECK(!run(s, "{\"cmd\":\"tool.run_user_button\",\"index\":0}", PLACE, false));
        CHECK(!run(s, "{\"cmd\":\"tool.run_source_action\",\"index\":0}", PLACE, false));
        CHECK(!run(s, "{\"cmd\":\"tool.run_source_action\",\"index\":-1}", PLACE, false));
    }
    /* the registry joins the resources' tables */
    CHECK(find("help") && find("window.destroy") && !find("window.nope"));
    TEST_REPORT("command registry");
}
