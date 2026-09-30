/* A recording's file (recx.h, W59a): the header, each file as a section
   (a line that would end it written with one more "@"), the steps with
   their notes as # lines, and a fingerprint of the files and the steps
   that a note does not change and a step does. */
#include "xpptest.h"
#include "recx.h"
#include "xpp_sha256.h"

#include <string>

namespace {

xpp::recx::Recording sample()
{
    xpp::recx::Recording r;
    r.program = "xppautX dev";
    r.model = "lecar.ode";
    r.recorded = "2026-09-30T10:14:02Z";
    xpp::recx::add_file(r, {"lecar.ode", "x'=-x\r\n@end\n@@ twice\n@ total=10\ndone"});
    xpp::recx::add_file(r, {"lecar.ode", "x'=-x\r\n@end\n@@ twice\n@ total=10\ndone"}); /* the same: once */
    r.steps.push_back({"First run.\n\nIt settles.", R"({"step":"Initialconds → Go","keys":["i","g"]})"});
    r.steps.push_back({"", R"({"step":"Erase","keys":["e"]})"});
    return r;
}

std::string sha(const std::string &hashed)
{
    xpp::Sha256 s;
    s.update(hashed.data(), hashed.size());
    return s.hex();
}

} // namespace

int main()
{
    xpp::recx::Recording r = sample();
    CHECK(r.files.size() == 1);
    const std::string hashed = "@file lecar.ode\nx'=-x\n@@end\n@@@ twice\n@ total=10\ndone\n@end\n"
                               "{\"step\":\"Initialconds → Go\",\"keys\":[\"i\",\"g\"]}\n{\"step\":\"Erase\",\"keys\":[\"e\"]}\n";
    const std::string want = "xppautx-recording 1\nprogram: xppautX dev\nmodel: lecar.ode\nrecorded: 2026-09-30T10:14:02Z\n\n"
                             "@file lecar.ode\nx'=-x\n@@end\n@@@ twice\n@ total=10\ndone\n@end\n\n@steps\n"
                             "# First run.\n#\n# It settles.\n{\"step\":\"Initialconds → Go\",\"keys\":[\"i\",\"g\"]}\n"
                             "{\"step\":\"Erase\",\"keys\":[\"e\"]}\n\nfingerprint: "
                             + sha(hashed) + "\n";
    CHECK_STR(xpp::recx::text(r).c_str(), want.c_str());

    /* a note is not in the fingerprint; a step and a file are */
    const std::string fp = xpp::recx::text(r).substr(want.size() - 65);
    r.steps[1].note = "Clear the screen.";
    CHECK(xpp::recx::text(r).ends_with(fp));
    r.steps[1].line = R"({"step":"Erase","keys":["x"]})";
    CHECK(!xpp::recx::text(r).ends_with(fp));
    r = sample();
    xpp::recx::add_file(r, {"lecar.ode", "x'=-2*x\n"}); /* read again, changed: a second section */
    CHECK(r.files.size() == 2);
    CHECK(!xpp::recx::text(r).ends_with(fp));
    TEST_REPORT("test_recx");
}
