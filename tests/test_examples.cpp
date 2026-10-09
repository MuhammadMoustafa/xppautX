/* The bundled examples (W233, core/xpp_examples.h): the program's own folder
   is found, a test binary has no examples/ beside it so the listing is
   empty with its reason shown (one rule, no search elsewhere), and no name
   from the page opens anything outside the listing: a path, "..", an
   absolute path or a name that is not there is refused before any file is
   read or written, in a config folder of its own (XPP_CONFIG_DIR). The
   protocol side (hello, the copy, the Replace question) is
   tools/servercheck.py's start screen section. */
#include "xpptest.h"
#include "xpp_examples.h"
#include "xpp_files.h"
#include "xpp_io.h"

#include <cstdlib>
#include <filesystem>
#include <string>

int main(void)
{
    const std::string program = xpp::files::program_dir();
    CHECK(!program.empty() && xpp::files::is_dir(program));
    const xpp::examples::Listing listing = xpp::examples::list();
    CHECK(listing.folder == xpp::files::join(program, xpp::examples::FOLDER));
    CHECK(listing.names.empty() && !listing.reason.empty());
    const std::string json = xpp::examples::json();
    CHECK(json.find("\"names\":[]") != std::string::npos && json.find("\"reason\":") != std::string::npos);

    xpp::TempDir temp;
    CHECK(!temp.path().empty());
    const std::string config = temp.path() + "/config";
#ifdef _WIN32
    CHECK(_putenv_s(xpp::files::CONFIG_DIR_ENV, config.c_str()) == 0);
#else
    CHECK(setenv(xpp::files::CONFIG_DIR_ENV, config.c_str(), 1) == 0);
#endif
    for (const char *name : {"lecar.odex", "../lecar.odex", "..", "", "/etc/passwd", "C:\\x.odex", "a/b.odex", "lecar.ode"}) {
        const xpp::Result<std::string> copied = xpp::examples::install(name);
        CHECK(!copied && copied.error().what.find("not one of the bundled examples") != std::string::npos);
    }
    CHECK(!std::filesystem::exists(config)); /* a refused name made nothing */
    TEST_REPORT("examples");
}
