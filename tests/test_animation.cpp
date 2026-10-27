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
    char arg0[] = "test_animation", arg1[] = "examples/ode/lecar.ode";
    char *argv[] = {arg0, arg1, nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    xpp::Session &s = xpp::client_session();
    const char *good = "build/test_animation_good.ani", *bad = "build/test_animation_bad.ani";
    put(good, "speed 27\ndimension -1;-2;3;4\npermanent\nline 0;0;1;1;$RED;2\n"
              "grab .5;.5;.1\n{iapp=1}\n{iapp=2;runnow=0}\ndone\n");
    CHECK(xpp::get_ani_file(s, good) == 1);
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
        try {
            xpp::get_ani_file(s, bad);
            CHECK(false);
        } catch (const xpp::Error &e) {
            CHECK(e.where == "animation" && e.place.file == bad && e.place.line == line);
            CHECK(e.place.source == source);
        }
        CHECK(s.animation.commands.data() == commands && s.animation.ncom == 1);
        CHECK(s.animation.grabs.data() == grabs && s.animation.grabs.size() == 1);
        CHECK(s.animation.grabs.size() == 1 && s.animation.grabs[0].start.events.size() == 1 && s.animation.grabs[0].end.events.size() == 1);
        CHECK(s.animation.vcr.file == good && s.animation.vcr.ok == 1 && s.animation.vcr.pos == 7);
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
    CHECK(xpp::get_ani_file(s, bad) == 1);
    CHECK(s.animation.vcr.file == bad && s.animation.ncom == 3 && s.animation.speed == 8);
    CHECK(s.animation.grabs.empty() && s.animation.vcr.pos == 0 && s.animation.grab_flag == 0);
    CHECK(s.ani_shown.frame.empty() && s.ani_shown.prims.empty());
    xpp::files::remove(good);
    xpp::files::remove(bad);
    TEST_REPORT("animation");
}
