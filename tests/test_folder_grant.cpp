/* W781, W794: the folder of the path the OS save dialog returned for several files (xpp_inbox.h authorize_folder) lets a command's `file` be a full path directly
   in that folder, written by the usual save path, and nothing else: no other folder, no subfolder, no "..", no link,
   no root, and nothing once it is replaced or released. The grant is what the window's host table calls with the
   dialog's answer; a command line cannot make one. */
#include "load_args.h"
#include "xpptest.h"
#include "command_registry.h"
#include "xpp_files.h"
#include "xpp_inbox.h"
#include "xpp_path.h"
#include "xpp_text.h"
#include <filesystem>
#include <string>

namespace {

const xpp::Place PLACE{"command", 1, 0, "line"};

/* the path escaped: a Windows one has backslashes */
std::string line_for(const std::string &file)
{
    std::string line = R"({"cmd":"session.save_as","file":)";
    xpp::json_append_string(line, file);
    return line + "}";
}

} // namespace

TEST_CASE("test_folder_grant")
{
    char name[] = "test_folder_grant", path[] = "examples/ode/lecar.odex";
    char *argv[] = {name, path, nullptr};
    const xpp::Loaded loaded = load_args(2, argv, 1);
    CHECK(loaded.has_value());
    if (!loaded) TEST_REPORT("folder grant");
    xpp::Session &s = **loaded;
    xpp::batch_start(s);

    const std::filesystem::path root = xpp::files::make_temp_dir().value_or(std::string());
    const std::filesystem::path granted = root / "granted", other = root / "other";
    std::filesystem::create_directories(granted / "sub");
    std::filesystem::create_directories(other);
    const std::string dir = granted.string(), far = other.string();
    const auto ran = [&](const std::string &file) { return xpp::command::run(s, line_for(file).c_str(), PLACE, false).has_value(); };

    /* before any pick a full path is refused, as it always was */
    CHECK(!ran(dir + "/a.snapx") && !std::filesystem::exists(granted / "a.snapx"));
    CHECK(!xpp::inbox::in_granted_folder(dir + "/a.snapx"));

    CHECK(xpp::inbox::authorize_folder(dir + "/fig1")); /* the path picked: its folder is granted */
    CHECK(xpp::inbox::in_granted_folder(dir + "/a.snapx"));
    CHECK(ran(dir + "/a.snapx") && std::filesystem::exists(granted / "a.snapx"));
    CHECK(ran(dir + "/b.snapx") && std::filesystem::exists(granted / "b.snapx")); /* several files, one pick */
    CHECK(ran("c.snapx") && std::filesystem::exists("c.snapx")); /* base names are what they were */
    std::filesystem::remove("c.snapx");

    /* what the grant does not cover */
    CHECK(!ran(far + "/a.snapx") && !std::filesystem::exists(other / "a.snapx"));
    CHECK(!ran(dir + "/sub/a.snapx") && !std::filesystem::exists(granted / "sub" / "a.snapx"));
    CHECK(!ran(dir + "/../other/a.snapx") && !std::filesystem::exists(other / "a.snapx"));
    CHECK(!ran(dir + "/../granted/a.snapx"));
    CHECK(!ran(dir + "x/a.snapx") && !ran(dir + "/.hidden") && !ran(dir + "/a..snapx") && !ran(dir + "/"));
    CHECK(!ran(dir + "/a:b.snapx") && !ran(dir + "/a\\\\b.snapx"));

    /* a link in the folder is never written through */
    std::error_code ec;
    std::filesystem::create_symlink(other / "target.snapx", granted / "link.snapx", ec);
    if (!ec) {
        ran(dir + "/link.snapx"); /* the save shows its refusal ("not a plain file") */
        CHECK(!std::filesystem::exists(other / "target.snapx") && std::filesystem::is_symlink(granted / "link.snapx"));
    }

    /* a root is no folder to grant; a relative path neither; a refused pick leaves the grant as it was */
    CHECK(!xpp::inbox::authorize_folder("/fig1") && !xpp::inbox::authorize_folder("//fig1") && !xpp::inbox::authorize_folder("rel/fig1"));
    CHECK(!xpp::inbox::authorize_folder("fig1") && !xpp::inbox::authorize_folder(""));
    CHECK(xpp::inbox::in_granted_folder(dir + "/a.snapx"));

    /* the next pick replaces the grant; release ends it */
    CHECK(xpp::inbox::authorize_folder(far + "/fig2"));
    CHECK(!ran(dir + "/d.snapx") && ran(far + "/d.snapx") && std::filesystem::exists(other / "d.snapx"));
    xpp::inbox::release_folder();
    CHECK(!ran(far + "/e.snapx") && !std::filesystem::exists(other / "e.snapx") && !xpp::inbox::in_granted_folder(far + "/e.snapx"));

    xpp::files::remove_temp_dir(root.string());
    TEST_REPORT("folder grant");
}
