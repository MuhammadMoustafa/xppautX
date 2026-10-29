/* W507: a refused animation replaces no part of the working one, including
   its filename, grabs and already displayed frame. */
#include "xpptest.h"
#include "load_args.h"
#include "session.h"
#include "aniparse.h"
#include "ani_data.h"
#include "xpp_io.h"
#include "xpp_files.h"
#include "xpp_ui.h"
#include "xpp_log.h"
#include "getvar.h"
#include <string>

namespace {
void put(const char *path, const std::string &text)
{
    xpp::Writer w(path);
    CHECK(w && w.write(text) && w.commit());
}
}

int main()
{
    xpp::XppUi test_ui{};
    test_ui.err_msg = [](const xpp::Error &e) { throw e; };
    xpp::set_ui(&test_ui);
    char arg0[] = "test_animation", arg1[] = "examples/ode/lecar.odex";
    char *argv[] = {arg0, arg1, nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    xpp::Session &s = xpp::client_session();
    const char *good = "build/test_animation_good.ani", *bad = "build/test_animation_bad.ani";
    const std::string good_native = "build/test_animation_good.anix", bad_native = "build/test_animation_bad.anix";
    xpp::files::remove(good_native);
    xpp::files::remove(bad_native);
    put(good, "speed 27\ndimension -1;-2;3;4\npermanent\nline 0;0;1;1;$RED;2\n"
              "grab .5;.5;.1\n{iapp=1}\n{iapp=2;runnow=0}\ndone\n");
    CHECK(xpp::load_chosen_animation(s, good).has_value());
    CHECK(s.animation.ncom == 1 && s.animation.grabs.size() == 1);
    if (s.animation.commands.empty() || s.animation.grabs.empty()) TEST_REPORT("animation");
    const auto *commands = s.animation.commands.data();
    const auto *grabs = s.animation.grabs.data();
    s.animation.vcr.pos = 7;
    s.animation.grab_flag = 1;
    s.ani_shown.frame = "previous frame";
    s.ani_shown.prims = "previous primitives";
    auto refused = [&](const std::string &text, int line, const std::string &source) {
        put(bad, text);
        const xpp::Result<> loaded = xpp::load_chosen_animation(s, bad);
        CHECK(!loaded);
        if (!loaded) {
            CHECK(loaded.error().where == "animation" && loaded.error().place.file == bad && loaded.error().place.line == line);
            CHECK(loaded.error().place.source == source);
        }
        CHECK(s.animation.commands.data() == commands && s.animation.ncom == 1);
        CHECK(s.animation.grabs.data() == grabs && s.animation.grabs.size() == 1);
        CHECK(s.animation.grabs.size() == 1 && s.animation.grabs[0].start.events.size() == 1 && s.animation.grabs[0].end.events.size() == 1);
        CHECK(s.animation.vcr.file == good_native && s.animation.vcr.ok == 1 && s.animation.vcr.pos == 7);
        CHECK(s.animation.speed == 27 && s.animation.xlo == -1 && s.animation.ylo == -2
              && s.animation.xhi == 3 && s.animation.yhi == 4 && s.animation.grab_flag == 1);
        CHECK(s.ani_shown.frame == "previous frame" && s.ani_shown.prims == "previous primitives");
    };
    refused("speed 4\ndimension 0;0;10;10\nline 0;0;1;1\ngrab .2;.2;.1\n{iapp=3}\n{iapp=4}\n"
            "line 0;0;1;1;$RED;bad\n", 7, "line 0;0;1;1;$RED;bad");
    refused("grab .2;.2;.1\n{iapp=3}\n{runnow=bad}\n", 3, "{runnow=bad}");
    refused("grab .2;.2;.1\n{iapp=3}\n", 3, "");
    refused("line 0;0;\\\n1;1\nspeed bad\n", 3, "speed bad");
    refused("line [0..1000000000];0;1;1\n", 1, "line [0..1000000000];0;1;1");
    refused("line [0..999999999999999999999];0;1;1\n", 1, "line [0..999999999999999999999];0;1;1");
    refused("line [j];0;1;1\n", 1, "line [j];0;1;1");
    refused("line [1;0;1;1\n", 1, "line [1;0;1;1");
    refused("line [2147483647..2147483647];0;[j+1];1\n", 1, "line [2147483647..2147483647];0;[j+1];1");
    put(bad, "speed 8\nline [1..3];0;1;1\ndone\n");
    CHECK(xpp::load_chosen_animation(s, bad).has_value());
    CHECK(s.animation.vcr.file == bad_native && s.animation.ncom == 3 && s.animation.speed == 8);
    CHECK(s.animation.grabs.empty() && s.animation.vcr.pos == 0 && s.animation.grab_flag == 0);
    CHECK(s.ani_shown.frame.empty() && s.ani_shown.prims.empty());
    xpp::files::remove(good);
    xpp::files::remove(bad);
    xpp::files::remove(good_native);
    xpp::files::remove(bad_native);
    const std::string native =
        "model: \"lecar.odex\"\n"
        "options { speed: 2, bounds: { x: [-1, 2], y: [-2, 3] } }\n"
        "permanent { text { x: 0, y: 0, text: \"hello\", color: \"blue\", size: 2, font: symbol } }\n"
        "line { x1: v, y1: w, x2: 1, y2: 1, color: \"black\", width: 3, dash: dotted }\n"
        "line { x1: .1, y1: .2, relative: true }\n"
        "circle { x: v, y: w, radius: .1, fill: w }\n"
        "rectangle { x1: 0, y1: 0, x2: 1, y2: 1, fill: \"red\" }\n"
        "ellipse { x: 0, y: 0, radius_x: .1, radius_y: .2, color: \"blue\" }\n"
        "trail { x: v, y: w, length: 4 }\n"
        "value { x: 0, y: 0, label: \"t = \", value: t }\n"
        "nullcline { variable: \"v\", x1: 0, y1: 0, x2: 1, y2: 1 }\n"
        "grab { x: v, y: w, start: { iapp: 1 }, end: { iapp: 2 }, run: false }\n"
        "line { x1: j, y1: 0, x2: j+1, y2: 1 } for j in 1..5 by 2\n";
    auto parsed = xpp::read_animation(s, native, "test.anix");
    if (!parsed) xpp::log(XPP_LOG_WARN, "{}\n", parsed.error().text());
    CHECK(parsed.has_value());
    if (parsed) CHECK(parsed->ncom == 12 && parsed->grabs.size() == 1 && parsed->speed == 2);
    auto invalid = [&](const std::string &text, int line) {
        auto result = xpp::read_animation(s, text, "bad.anix");
        CHECK(!result.has_value());
        if (!result) {
            CHECK(result.error().where == "animation" && result.error().place.file == "bad.anix");
            CHECK(result.error().place.line == line && !result.error().place.source.empty());
        }
    };
    invalid("line { x1: 0, y1: 0, x2: 1, y2: 1, bogus: 2 }", 1);
    invalid("line { x1: 0, y1: 0, x2: 1 }", 1);
    invalid("line { x1: 0, y1: 0, x2: 1, y2: 1, width: \"thick\" }", 1);
    invalid("line { x1: 0, y1: 0, x2: 1, y2: 1, relative: 1 }", 1);
    invalid("line { x1: 0, y1: 0, x2: 1, y2: 1, color: \"no_such_color\" }", 1);
    invalid("line { x1: 0, y1: 0,\n x2: unknown_name, y2: 1 }", 2);
    invalid("line { x1: 0, y1: 0, x2: 1, y2: 1, width: 2, width: 3 }", 1);
    invalid("permanent {\n permanent { } }", 2);
    invalid("options { bounds: { x: [1, 0], y: [0, 1] } }", 1);
    invalid("model: \"../lecar.odex\"", 1);
    invalid("model: \"other.odex\"", 1);
    invalid("trail { x: 0, y: 0, length: 1000000000 }", 1);
    invalid("line { x1: j, y1: 0, x2: 1, y2: 1 } for j in 1..100001", 1);
    invalid("line { x1: j, y1: 0, x2: 1, y2: 1 } for j in 1..2 by 0", 1);
    invalid("line { x1: 0, y1: 0, x2: 1, y2: 1 } for v in 1..2", 1);
    invalid("grab { x: 0, y: 0, end: { unknown_name: 1 } }", 1);
    invalid("text { x: 0, y: 0, text: unquoted }", 1);
    invalid("options { unknown: 1 }", 1);
    invalid("line { x1: " + std::string(300, '(') + "0" + std::string(300, ')') + ", y1: 0, x2: 1, y2: 1 }", 1);
    std::string chain = "0";
    for (int i = 0; i < 1000; ++i) chain += "+1";
    invalid("line { x1: " + chain + ", y1: 0, x2: 1, y2: 1 }", 1);
    invalid("trail { x: 0, y: 0,\n length: 0 }", 2);
    invalid("line { x1: 0, y1: 0, x2: 1,\n y2: \"bad\" }", 2);
    invalid("options {\n speed: 1001 }", 2);
    invalid("nullcline { x1: 0, y1: 0, x2: 1, y2: 1,\n variable: \"missing\" }", 2);
    invalid("nullcline { x1: 0, y1: 0, x2: 1, y2: 1,\n variable: \"iapp\" }", 2);
    invalid("nullcline { x1: 0, y1: 0, x2: 1, y2: 1, variable: \"v\",\n source: invalid }", 2);
    /* Every legacy command, including range expansion, gets the same frame
       data after conversion. No pixel or timing comparison is involved. */
    std::string legacy;
    CHECK(xpp::read_bytes("tools/gui_test.ani", legacy, 100000));
    legacy.insert(legacy.rfind("end"), "line [1..3];0;[j+1];1;$BLUE;1\n");
    auto converted = xpp::convert_animation(s, legacy, "test.ani");
    if (!converted) xpp::log(XPP_LOG_WARN, "{}\n", converted.error().text());
    CHECK(converted.has_value());
    auto imported = xpp::read_animation(s, legacy, "test.ani");
    auto roundtrip = converted ? xpp::read_animation(s, *converted, "test.anix") : xpp::Result<xpp::AnimationState>{std::unexpected(converted.error())};
    CHECK(imported.has_value() && roundtrip.has_value());
    xpp::ani_data_init([](std::string_view) {});
    auto permanent = xpp::read_animation(s, "permanent { line { x1: 0, y1: 0, x2: 1, y2: 1, color: t } }", "permanent.anix");
    CHECK(permanent.has_value());
    if (permanent) {
        s.animation = std::move(*permanent);
        xpp::set_ani_perm(s);
        xpp::render_ani(s);
        const auto primitives = s.ani_shown.prims;
        setvar(s, 0, 1);
        xpp::render_ani(s);
        CHECK(s.ani_shown.prims == primitives);
    }
    if (imported && roundtrip) {
        CHECK(imported->grabs.size() == roundtrip->grabs.size());
        for (int frame = 0; frame < 3; ++frame) {
            s.animation = *imported;
            xpp::set_ani_perm(s);
            xpp::render_ani(s);
            const auto before = s.ani_shown.frame;
            s.animation = *roundtrip;
            xpp::set_ani_perm(s);
            xpp::render_ani(s);
            CHECK(s.ani_shown.frame == before);
        }
    }
    TEST_REPORT("animation");
}
