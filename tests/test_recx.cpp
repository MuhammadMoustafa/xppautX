/* A recording's file (recx.h, W59a): the header, each file as a section
   (a line that would end it written with one more "@"), the steps with
   their notes as # lines, and a fingerprint of the files and the steps
   that a note does not change and a step does. */
#include "xpptest.h"
#include "recx.h"
#include "xpp_sha256.h"

#include <optional>
#include <string>

namespace {

xpp::recx::Recording sample()
{
    xpp::recx::Recording r;
    r.program = "xppautX dev";
    r.model = "lecar.ode";
    r.recorded = "2026-09-30T10:14:02Z";
    r.snapshot = std::string("PK\x05\x06", 4); /* the session, a zip (W59d) */
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
    const std::string hashed = "@snapshot\nUEsFBg==\n@end\n@file lecar.ode\nx'=-x\n@@end\n@@@ twice\n@ total=10\ndone\n@end\n"
                               "{\"step\":\"Initialconds → Go\",\"keys\":[\"i\",\"g\"]}\n{\"step\":\"Erase\",\"keys\":[\"e\"]}\n";
    const std::string want = "xppautx-recording 1\nprogram: xppautX dev\nmodel: lecar.ode\nrecorded: 2026-09-30T10:14:02Z\n\n"
                             "@snapshot\nUEsFBg==\n@end\n\n@file lecar.ode\nx'=-x\n@@end\n@@@ twice\n@ total=10\ndone\n@end\n\n@steps\n"
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

    /* read back: the same recording, intact; a note edited keeps it so,
       and the text written again with it keeps what the file said */
    std::string error;
    r = sample();
    xpp::recx::add_file(r, {"lecar.autox", std::string("PK\x03\x04\0binary\xff", 12)}); /* a zip: @binary */
    const std::string file = xpp::recx::text(r);
    CHECK(file.find("\n@binary lecar.autox\nUEsDBABiaW5hcnn/\n@end\n") != std::string::npos);
    std::optional<xpp::recx::Read> back = xpp::recx::read(file, error);
    CHECK(back && back->intact);
    CHECK(back->rec.files.size() == 2 && back->rec.files[1].bytes == r.files[1].bytes);
    CHECK_STR(back->rec.files[0].bytes.c_str(), "x'=-x\n@end\n@@ twice\n@ total=10\ndone\n");
    CHECK(back->rec.steps.size() == 2);
    CHECK(back->rec.snapshot == r.snapshot);
    CHECK_STR(back->rec.steps[0].note.c_str(), "First run.\n\nIt settles.");
    CHECK_STR(xpp::recx::text(back->rec).c_str(), file.c_str());
    std::string edited = file;
    edited.replace(edited.find("# It settles."), 13, "# It fires and settles.");
    back = xpp::recx::read(edited, error);
    CHECK(back && back->intact);
    edited.replace(edited.find("[\"e\"]"), 5, "[\"x\"]");
    back = xpp::recx::read(edited, error);
    CHECK(back && !back->intact); /* a step changed: it still reads */
    back->rec.steps[0].note = "Saved again.";
    CHECK(!xpp::recx::read(xpp::recx::text(back->rec), error)->intact); /* saving a note does not mend it */
    CHECK(!xpp::recx::read("xppautx-recording 1\n\n@snapshot\nUEsFBg==\n@end\n@file a.ode\nx'=1\n", error));
    CHECK_STR(error.c_str(), "line 6: the section of a.ode has no @end");
    /* no fallback: a file without the session it began from is not a recording */
    CHECK(!xpp::recx::read("xppautx-recording 1\n\n@file a.ode\nx'=1\n@end\n@steps\n", error));
    CHECK_STR(error.c_str(), "line 3: no @snapshot section before the files: a recording begins with the session's state");
    CHECK(!xpp::recx::read("xppautx-recording 1\n\n@steps\n", error));
    CHECK_STR(error.c_str(), "line 3: no @snapshot section: a recording begins with the session's state");
    CHECK(!xpp::recx::read("not one\n", error));
    TEST_REPORT("test_recx");
}
