/* The unit tests' one program: every tests/test_*.cpp registers a named test
   (xpptest.h TEST_CASE) and this runs them, in one process, in the order of
   their names. It is linked once with the whole core (W693: 79 programs, each
   linked with it, took minutes under ASan).

       test_all                  every test
       test_all NAME...          these tests, in the order given
       test_all --list           the names
       test_all --reverse        the same, from the last (no test depends on another)

   A test sets up and restores what it changes (an environment variable, the
   working folder); what this runs between tests is only what every test
   shares: the session list's one session, made fresh, the working folder, the
   input, the error count, the front end's table of calls (xpp::ui), the batch
   options and the program's description. */
#include "session.h"
#include "xpp_globals.h"
#include "xpp_log.h"
#include "xpp_ui.h"
#include "xpptest.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include <cstdio>
#include <system_error>

namespace {

struct Test {
    std::string name;
    int (*run)();
};

/* made on first use: a registration runs before main, in no known order */
std::vector<Test> &registry()
{
    static std::vector<Test> tests;
    return tests;
}

/* what every test shares, as the program starts: what one test changes in it, the next must not meet */
struct SharedState {
    std::filesystem::path home = std::filesystem::current_path();
    xpp::XppUi ui = xpp::ui;
    xpp::XppBatchOptions batch_options = xpp::batch_options;
    XppProgram program = ::program;

    void restore() const
    {
        xpp::ui = ui; /* a test installs its own table (set_ui): its handlers must not outlive it */
        xpp::batch_options = batch_options;
        ::program = program;
        std::error_code ignored;
        std::filesystem::current_path(home, ignored);
        /* no terminal answers a question: a test that needs an answer gives its own (test_options) */
        const char *none = test_null_device();
        if (!std::freopen(none, "r", stdin)) printf("cannot read %s as the input\n", none);
        xpp::Load fresh; /* the session list's one session, new */
        fresh.commit();
        xpp::log_settings.errors = 0;
    }
};

} // namespace

TestRegistration::TestRegistration(const char *name, int (*run)())
{
    registry().push_back({name, run});
}

int main(int argc, char **argv)
{
    std::vector<Test> &tests = registry();
    std::sort(tests.begin(), tests.end(), [](const Test &a, const Test &b) { return a.name < b.name; });
    std::vector<Test> chosen;
    bool reverse = false;
    for (int i = 1; i < argc; i++) {
        const std::string_view arg = argv[i];
        if (arg == "--list") {
            for (const Test &t : tests) printf("%s\n", t.name.c_str());
            return 0;
        }
        if (arg == "--reverse") {
            reverse = true;
            continue;
        }
        const auto found = std::find_if(tests.begin(), tests.end(), [&](const Test &t) { return t.name == arg; });
        if (found == tests.end()) {
            printf("unknown test \"%s\" (--list shows them)\n", argv[i]);
            return 2;
        }
        chosen.push_back(*found);
    }
    if (chosen.empty()) chosen = tests;
    if (reverse) std::reverse(chosen.begin(), chosen.end());

    const SharedState shared;
    int failed = 0;
    for (const Test &t : chosen) {
        shared.restore();
        if (t.run() != 0) {
            printf("FAIL %s\n", t.name.c_str());
            failed++;
        }
    }
    if (failed != 0) {
        printf("unit tests: %d of %zu failed\n", failed, chosen.size());
        return 1;
    }
    return 0;
}
