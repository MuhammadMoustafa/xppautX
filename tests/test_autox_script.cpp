/* An autoX script's file (autox_script.h, W306): the runs read whole, each checked against autox.run's own validator, every fault
   at its file and line; a run's line as it is sent (the start by type and index, the settings by their names). */
#include "xpptest.h"
#include "autox_script.h"

#include <string>

namespace {

const std::string good =
    "version: 4,\n"
    "model: \"ab.odex\",\n"
    "diagram: \"branch.csv\",\n"
    "runs: [\n"
    "  { name: \"steady\", kind: equilibrium, replace_diagram: true, parameters: [\"p0\"], settings: { step_size: 0.02, follow_all_branches: false } },\n"
    "  { name: \"cycles\", kind: periodic, start: { run: \"steady\", type: hopf, index: 2 }, parameters: [\"p0\"], settings: { step_size: -0.01 } },\n"
    "]\n";

std::string error(const std::string &text)
{
    const xpp::Result<xpp::AutoxScript> read = xpp::autox_script_read(text, "a.autox");
    return read ? std::string() : read.error().text();
}

} // namespace

TEST_CASE("test_autox_script")
{
    const xpp::Result<xpp::AutoxScript> script = xpp::autox_script_read(good, "a.autox");
    CHECK(script.has_value());
    if (script) {
        CHECK(script->model == "ab.odex" && script->diagram_file == "branch.csv" && script->runs.size() == 2);
        CHECK(script->runs[0].replaces && !script->runs[0].from && script->runs[0].parameters == std::vector<std::string>{"p0"});
        CHECK(xpp::autox_script_command(script->runs[0], {}, 0) ==
              R"({"cmd":"autox.run","kind":"equilibrium","replace_diagram":true,"step_size":0.02,"follow_all_branches":false})");
        CHECK(script->runs[1].from && script->runs[1].from->run == "steady" && script->runs[1].from->type == "hopf" && script->runs[1].from->index == 2);
        CHECK(xpp::autox_script_command(script->runs[1], "hopf", 5) == R"({"cmd":"autox.run","kind":"periodic","step_size":-0.01,"type":"hopf","index":5})");
        CHECK(xpp::autox_script_axes_command(script->runs[1]) == R"({"cmd":"autox.configure_axes","parameter1":"p0"})");
    }
    auto replaced = [&](const std::string &from, const std::string &to) {
        std::string text = good;
        text.replace(text.find(from), from.size(), to);
        return text;
    };
    CHECK(error(replaced("version: 4", "version: 5")).starts_with("a.autox:1:"));
    CHECK(error(replaced("version: 4", "version: 5")).find("version 5: this program reads autoX scripts of version 4") != std::string::npos);
    CHECK(error(replaced("model: \"ab.odex\"", "model: \"../ab.odex\"")).starts_with("a.autox:1:1: an autoX script names its model"));
    CHECK(error(replaced("model: \"ab.odex\"", "model: \"ab.ode\"")).starts_with("a.autox:1:1: an autoX script names its model"));
    CHECK(error(replaced("branch.csv", "branch.dat")).starts_with("a.autox:3:"));
    CHECK(error(replaced("name: \"cycles\"", "name: \"steady\"")).find("there is already a run named \"steady\"") != std::string::npos);
    CHECK(error(replaced("run: \"steady\"", "run: \"later\"")).find("\"later\" is not a run before this one") != std::string::npos);
    CHECK(error(replaced("type: hopf", "type: HB")).find("\"HB\" is not a point type") != std::string::npos);
    CHECK(error(replaced("index: 2", "index: 0")).find("the index counts from 1") != std::string::npos);
    CHECK(error(replaced("settings: { step_size: -0.01 }", "settings: { kind: bvp }")).find("kind is not a setting") != std::string::npos);
    CHECK(error(replaced("step_size: -0.01", "step_size: -0.01, step_size: 0.5")).find("given twice") != std::string::npos);
    CHECK(error(replaced("step_size: -0.01", "step_size: wide")).find("step_size") != std::string::npos); /* the command's own validator */
    CHECK(error(replaced("kind: periodic", "kind: wobble")).find("wobble") != std::string::npos);
    CHECK(error(replaced("parameters: [\"p0\"], settings: { step_size: -0.01 }", "parameters: [], settings: { step_size: -0.01 }")).find("parameters names") != std::string::npos);
    CHECK(error("version: 4,\nmodel: \"ab.odex\",\nruns: [],\n").find("lists its runs") != std::string::npos);
    CHECK(error(replaced("version: 4,", "version: 4,\nextra: 1,")).find("extra") != std::string::npos);
    TEST_REPORT("test_autox_script");
}
